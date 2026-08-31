#include "q_sound.hpp"
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <android/log.h>
#include <pthread.h>
#include <atomic>
#include <deque>
#include <cstdlib>
#include <cstring>

#define AUDIO_TAG "QuakeVR-Audio"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, AUDIO_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, AUDIO_TAG, __VA_ARGS__)

// OpenSL ES objects
static SLObjectItf engineObject = nullptr;
static SLEngineItf engineEngine = nullptr;
static SLObjectItf outputMixObject = nullptr;
static SLObjectItf bqPlayerObject = nullptr;
static SLPlayItf bqPlayerPlay = nullptr;
static SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue = nullptr;

// DMA shared pointer used by engine
volatile dma_t* shm_local = nullptr; // local alias for shm (extern in q_sound)

// bookkeeping counters
static std::atomic<int> submitted_chunks{0}; // number of chunks submitted
static std::atomic<int> played_chunks{0};    // number of chunks played (consumed via callback)
static pthread_mutex_t submit_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t submit_cond = PTHREAD_COND_INITIALIZER;

// pending buffers queue: we allocate a small buffer for each submission and free it in callback
static std::deque<void*> pending_buffers;

// parameters we choose
static const int DEFAULT_SAMPLE_RATE = 44100; // Hz
static const int DEFAULT_CHANNELS = 2;        // stereo
static const int DEFAULT_SAMPLE_BITS = 16;    // bits
static const int DEFAULT_SUBMISSION_CHUNK = 2048; // mono samples per submission
static const int DEFAULT_BUFFER_CHUNKS = 8;   // how many submission chunks to allocate internally

// forward declarations
bool SNDDMA_Init(dma_t* dma);
int SNDDMA_GetDMAPos(void);
void SNDDMA_Shutdown(void);
void SNDDMA_LockBuffer(void);
void SNDDMA_Submit(void);
void SNDDMA_BlockSound(void);
void SNDDMA_UnblockSound(void);

extern volatile dma_t* shm; // declared in q_sound.hpp

// Helper: free oldest pending buffer (called from callback)
static void free_oldest_pending()
{
    pthread_mutex_lock(&submit_mutex);
    if(!pending_buffers.empty())
    {
        void* p = pending_buffers.front();
        pending_buffers.pop_front();
        free(p);
    }
    pthread_mutex_unlock(&submit_mutex);
}

// Buffer queue callback: called when a buffer finishes playing
static void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void* context)
{
    (void)bq; (void)context;
    // One chunk played
    played_chunks.fetch_add(1);
    // free the oldest pending buffer
    free_oldest_pending();
}

bool SNDDMA_Init(dma_t* dma)
{
    if(!dma)
    {
        LOGE("SNDDMA_Init: dma is NULL");
        return false;
    }

    LOGI("SNDDMA_Init: initializing OpenSL ES audio backend");

    // fill dma parameters
    dma->channels = DEFAULT_CHANNELS;
    dma->samplebits = DEFAULT_SAMPLE_BITS;
    dma->signed8 = 0; // using signed 16-bit
    dma->speed = DEFAULT_SAMPLE_RATE;

    // total mono samples in buffer
    const int mono_samples = DEFAULT_SUBMISSION_CHUNK * DEFAULT_BUFFER_CHUNKS;
    dma->samples = mono_samples;
    dma->submission_chunk = DEFAULT_SUBMISSION_CHUNK;

    // allocate circular buffer (unsigned char*) sized: mono_samples * channels * (bits/8)
    const int bytes_per_sample = dma->channels * (dma->samplebits / 8);
    const size_t buffer_bytes = (size_t)dma->samples * bytes_per_sample;
    unsigned char* ring = (unsigned char*)malloc(buffer_bytes);
    if(!ring)
    {
        LOGE("SNDDMA_Init: failed to allocate audio ring buffer (%zu bytes)", buffer_bytes);
        return false;
    }
    memset(ring, 0, buffer_bytes);
    dma->buffer = ring; // engine will write into this buffer

    // set shared pointer so engine code can see the DMA buffer
    shm = dma;
    shm_local = dma;

    // create OpenSL ES engine
    SLresult result;
    result = slCreateEngine(&engineObject, 0, nullptr, 0, nullptr, nullptr);
    if(result != SL_RESULT_SUCCESS)
    {
        LOGE("slCreateEngine failed: %d", result);
        return false;
    }
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_TRUE);
    if(result != SL_RESULT_SUCCESS)
    {
        LOGE("engine Realize failed: %d", result);
        return false;
    }
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);
    if(result != SL_RESULT_SUCCESS)
    {
        LOGE("GetInterface ENGINE failed: %d", result);
        return false;
    }

    // create output mix
    const SLInterfaceID ids[0] = {};
    const SLboolean req[0] = {};
    result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 0, ids, req);
    if(result != SL_RESULT_SUCCESS)
    {
        LOGE("CreateOutputMix failed: %d", result);
        return false;
    }
    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_TRUE);
    if(result != SL_RESULT_SUCCESS)
    {
        LOGE("outputMix Realize failed: %d", result);
        return false;
    }

    // configure audio source
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDBUFFERQUEUE, 2};
    SLDataFormat_PCM format_pcm;
    format_pcm.formatType = SL_DATAFORMAT_PCM;
    format_pcm.numChannels = dma->channels;
    format_pcm.samplesPerSec = dma->speed * 1000; // OpenSL works in milliHz
    format_pcm.bitsPerSample = (SLuint16)dma->samplebits;
    format_pcm.containerSize = (SLuint16)dma->samplebits;
    format_pcm.channelMask = (dma->channels == 2) ? SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT : SL_SPEAKER_FRONT_CENTER;
    format_pcm.endianness = SL_BYTEORDER_LITTLEENDIAN;

    SLDataSource audioSrc = {&loc_bufq, &format_pcm};

    // configure audio sink
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
    SLDataSink audioSnk = {&loc_outmix, nullptr};

    // create audio player with buffer queue
    const SLInterfaceID ids1[] = {SL_IID_BUFFERQUEUE};
    const SLboolean req1[] = {SL_BOOLEAN_TRUE};
    result = (*engineEngine)->CreateAudioPlayer(engineEngine, &bqPlayerObject, &audioSrc, &audioSnk, 1, ids1, req1);
    if(result != SL_RESULT_SUCCESS)
    {
        LOGE("CreateAudioPlayer failed: %d", result);
        return false;
    }
    result = (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_TRUE);
    if(result != SL_RESULT_SUCCESS)
    {
        LOGE("bqPlayer Realize failed: %d", result);
        return false;
    }
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerPlay);
    if(result != SL_RESULT_SUCCESS)
    {
        LOGE("GetInterface PLAY failed: %d", result);
        return false;
    }
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE, &bqPlayerBufferQueue);
    if(result != SL_RESULT_SUCCESS)
    {
        LOGE("GetInterface BUFFERQUEUE failed: %d", result);
        return false;
    }

    // register callback
    result = (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, bqPlayerCallback, nullptr);
    if(result != SL_RESULT_SUCCESS)
    {
        LOGE("RegisterCallback failed: %d", result);
        return false;
    }

    // set player to playing
    result = (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);
    if(result != SL_RESULT_SUCCESS)
    {
        LOGE("SetPlayState PLAYING failed: %d", result);
        return false;
    }

    LOGI("OpenSL ES initialized: %d Hz, %d channels, %d bits, ring %d mono samples, chunk %d",
         dma->speed, dma->channels, dma->samplebits, dma->samples, dma->submission_chunk);

    submitted_chunks.store(0);
    played_chunks.store(0);

    return true;
}

int SNDDMA_GetDMAPos(void)
{
    if(!shm)
    {
        return 0;
    }
    // played_chunks represents how many submission chunks have finished
    int played = played_chunks.load();
    int pos = (played * shm->submission_chunk) % shm->samples; // mono samples
    return pos;
}

void SNDDMA_Shutdown(void)
{
    LOGI("SNDDMA_Shutdown: shutting down audio");

    // destroy player
    if(bqPlayerObject)
    {
        (*bqPlayerObject)->Destroy(bqPlayerObject);
        bqPlayerObject = nullptr;
        bqPlayerPlay = nullptr;
        bqPlayerBufferQueue = nullptr;
    }

    // destroy output mix
    if(outputMixObject)
    {
        (*outputMixObject)->Destroy(outputMixObject);
        outputMixObject = nullptr;
    }

    // destroy engine
    if(engineObject)
    {
        (*engineObject)->Destroy(engineObject);
        engineObject = nullptr;
        engineEngine = nullptr;
    }

    // free any pending allocated buffers
    pthread_mutex_lock(&submit_mutex);
    while(!pending_buffers.empty())
    {
        void* p = pending_buffers.front();
        pending_buffers.pop_front();
        free(p);
    }
    pthread_mutex_unlock(&submit_mutex);

    // free ring buffer
    if(shm && shm->buffer)
    {
        free((void*)shm->buffer);
        shm->buffer = nullptr;
    }
    shm = nullptr;
    shm_local = nullptr;
}

void SNDDMA_LockBuffer(void)
{
    pthread_mutex_lock(&submit_mutex);
}

void SNDDMA_Submit(void)
{
    if(!shm || !bqPlayerBufferQueue)
    {
        pthread_mutex_unlock(&submit_mutex);
        return;
    }

    // figure out where to copy from in the ring buffer
    static int submit_pos = 0; // mono samples
    const int samples = shm->samples;
    const int channels = shm->channels;
    const int monoChunk = shm->submission_chunk;
    const int bytesPerSample = channels * (shm->samplebits / 8);
    const size_t chunkBytes = (size_t)monoChunk * bytesPerSample;

    // allocate a temporary buffer and copy chunk (handle wraparound)
    unsigned char* tmp = (unsigned char*)malloc(chunkBytes);
    if(!tmp)
    {
        LOGE("SNDDMA_Submit: failed to allocate tmp buffer of %zu bytes", chunkBytes);
        pthread_mutex_unlock(&submit_mutex);
        return;
    }

    const unsigned char* ring = shm->buffer;
    int firstMono = submit_pos % samples;
    int firstBytes = firstMono * bytesPerSample;
    int tailMono = samples - firstMono;

    if(tailMono >= monoChunk)
    {
        // single memcpy
        memcpy(tmp, ring + firstBytes, chunkBytes);
    }
    else
    {
        // two-part copy
        size_t firstPartBytes = (size_t)tailMono * bytesPerSample;
        memcpy(tmp, ring + firstBytes, firstPartBytes);
        memcpy(tmp + firstPartBytes, ring, chunkBytes - firstPartBytes);
    }

    // enqueue tmp buffer; we will free it in the callback
    SLresult res = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, tmp, (SLuint32)chunkBytes);
    if(res != SL_RESULT_SUCCESS)
    {
        LOGE("SNDDMA_Submit: Enqueue failed: %d", res);
        free(tmp);
        pthread_mutex_unlock(&submit_mutex);
        return;
    }

    // push to pending list so we can free it in callback
    pending_buffers.push_back(tmp);

    submit_pos += monoChunk;
    submitted_chunks.fetch_add(1);

    pthread_mutex_unlock(&submit_mutex);
}

void SNDDMA_BlockSound(void)
{
    // stop enqueuing and optionally pause
    if(bqPlayerPlay)
    {
        (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PAUSED);
    }
}

void SNDDMA_UnblockSound(void)
{
    if(bqPlayerPlay)
    {
        (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);
    }
}
