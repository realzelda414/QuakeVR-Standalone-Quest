#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <android/log.h>

#define LOG_TAG "QuakeVR-Sys"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Quake Types
typedef int qboolean;
#define qtrue 1
#define qfalse 0

#define CVAR_ARCHIVE 1

typedef struct cvar_s {
    const char *name;
    const char *string;
    int flags;
    float value;
    struct cvar_s *next;
} cvar_t;

typedef struct {
    int width;
    int height;
    int colormask;
    int rowbytes;
    int pixelbytes;
    int buffer;
    int conwidth;
    int conheight;
    int direct;
    int aspect;
    int recalc_refdef;
} viddef_t;

struct usercmd_t;
struct sizebuf_t;

// Global Engine State
qboolean isDedicated = qfalse;
viddef_t vid = { 1920, 1080, 0, 1920 * 4, 4, 0, 640, 480, 0, 1, 0 };

// Video & GL Feature Flags (All supported natively in Quest GLES 3.2)
qboolean gl_texture_NPOT = qtrue;
qboolean gl_glsl_alias_able = qtrue;
qboolean gl_vbo_able = qtrue;
qboolean gl_glsl_gamma_able = qtrue;
qboolean gl_mtexable = qtrue;
float gl_max_anisotropy = 4.0f;
int gl_stencilbits = 8;

cvar_t vid_gamma = {"gamma", "1.0", CVAR_ARCHIVE, 1.0f, NULL};
cvar_t vid_contrast = {"contrast", "1.0", CVAR_ARCHIVE, 1.0f, NULL};
cvar_t sys_throttle = {"sys_throttle", "0.0", 0, 0.0f, NULL};

// VOIP Cvars
cvar_t sv_voip = {"sv_voip", "0", 0, 0.0f, NULL};
cvar_t sv_voip_echo = {"sv_voip_echo", "0", 0, 0.0f, NULL};
cvar_t cl_voip_test = {"cl_voip_test", "0", 0, 0.0f, NULL};
cvar_t cl_voip_vad_delay = {"cl_voip_vad_delay", "0", 0, 0.0f, NULL};
cvar_t cl_voip_capturingvol = {"cl_voip_capturingvol", "0", 0, 0.0f, NULL};
cvar_t cl_voip_showmeter = {"cl_voip_showmeter", "0", 0, 0.0f, NULL};
cvar_t cl_voip_play = {"cl_voip_play", "0", 0, 0.0f, NULL};
cvar_t cl_voip_micamp = {"cl_voip_micamp", "0", 0, 0.0f, NULL};
cvar_t cl_voip_ducking = {"cl_voip_ducking", "0", 0, 0.0f, NULL};
cvar_t cl_voip_noisefilter = {"cl_voip_noisefilter", "0", 0, 0.0f, NULL};
cvar_t cl_voip_autogain = {"cl_voip_autogain", "0", 0, 0.0f, NULL};
cvar_t cl_voip_opus_bitrate = {"cl_voip_opus_bitrate", "0", 0, 0.0f, NULL};
cvar_t cl_voip_send = {"cl_voip_send", "0", 0, 0.0f, NULL};

// Time & Sleep
double Sys_DoubleTime(void) {
    struct timeval tp;
    struct timezone tzp;
    gettimeofday(&tp, &tzp);
    return (double)tp.tv_sec + (double)tp.tv_usec / 1000000.0;
}

void Sys_Sleep(unsigned long msecs) {
    usleep(msecs * 1000);
}

// Print, Error & Quit
void Sys_Printf(const char *fmt, ...) {
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    LOGI("%s", buf);
}

void Sys_Error(const char *error, ...) {
    char buf[4096];
    va_list ap;
    va_start(ap, error);
    vsnprintf(buf, sizeof(buf), error, ap);
    va_end(ap);
    LOGE("SYS_ERROR: %s", buf);
    exit(1);
}

void Sys_Quit(void) {
    exit(0);
}

const char* Sys_ConsoleInput(void) {
    return NULL;
}

char* PL_GetClipboardData(void) {
    return NULL;
}

// POSIX File I/O
int Sys_FileOpenRead(const char *path, int *hndl) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        if (hndl) *hndl = -1;
        return -1;
    }
    if (hndl) *hndl = fd;
    struct stat st;
    if (fstat(fd, &st) == 0) {
        return (int)st.st_size;
    }
    return 0;
}

int Sys_FileOpenWrite(const char *path) {
    return open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
}

int Sys_FileOpenStdio(FILE *stream) {
    if (!stream) return -1;
    return fileno(stream);
}

int Sys_FileRead(int handle, void *dest, int count) {
    return (int)read(handle, dest, count);
}

int Sys_FileWrite(int handle, const void *data, int count) {
    return (int)write(handle, data, count);
}

void Sys_FileClose(int handle) {
    if (handle >= 0) {
        close(handle);
    }
}

void Sys_FileSeek(int handle, int position) {
    lseek(handle, position, SEEK_SET);
}

int Sys_FileTime(const char *path) {
    struct stat st;
    if (stat(path, &st) >= 0) {
        return (int)st.st_mtime;
    }
    return -1;
}

int Sys_mkdir(const char *path) {
    return mkdir(path, 0777);
}

// Video / Framebuffer Stubs (Handled by OpenXR Swapchain)
void VID_Init(void) {}
void VID_Shutdown(void) {}
void VID_Lock(void) {}
void VID_Unlock(void) {}
void VID_Toggle(void) {}
void VID_SyncCvars(void) {}
void GL_BeginRendering(int *x, int *y, int *width, int *height) {
    if (x) *x = 0;
    if (y) *y = 0;
    if (width) *width = vid.width;
    if (height) *height = vid.height;
}
void GL_EndRendering(void) {}

// Input Stubs (Handled by OpenXR Touch Controller Bridge)
void IN_Init(void) {}
void IN_Shutdown(void) {}
void IN_Commands(void) {}
void IN_UpdateGrabs(void) {}
void IN_UpdateInputMode(void) {}
void IN_Move(usercmd_t *cmd) { (void)cmd; }
void Sys_SendKeyEvents(void) {}

// Sound DMA Stubs
int SNDDMA_Init(void) { return 0; }
void* SNDDMA_LockBuffer(void) { return NULL; }
void SNDDMA_Submit(void) {}
void SNDDMA_Shutdown(void) {}

// VOIP Stubs
void S_Voip_Transmit(unsigned char flags, sizebuf_t *sb) { (void)flags; (void)sb; }
void S_Voip_MapChange(void) {}
void S_Voip_Parse(void) {}

// OpenVR Desktop SteamVR stub (bypassed on Quest OpenXR)
extern "C" void VR_ShutdownInternal(void) {}
