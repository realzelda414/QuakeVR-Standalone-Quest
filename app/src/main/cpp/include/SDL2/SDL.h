#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef uint64_t Uint64;
typedef int8_t Sint8;
typedef int16_t Sint16;
typedef int32_t Sint32;
typedef int64_t Sint64;
typedef int SDL_bool;
#define SDL_TRUE 1
#define SDL_FALSE 0

typedef struct SDL_Window SDL_Window;
typedef void* SDL_GLContext;
typedef uint32_t SDL_AudioDeviceID;
typedef uint16_t SDL_AudioFormat;

#define AUDIO_U8        0x0008
#define AUDIO_S8        0x8008
#define AUDIO_U16LSB    0x0010
#define AUDIO_S16LSB    0x8010
#define AUDIO_S16SYS    AUDIO_S16LSB
#define AUDIO_S16       AUDIO_S16LSB

typedef void (*SDL_AudioCallback)(void *userdata, Uint8 *stream, int len);

typedef struct SDL_AudioSpec {
    int freq;
    SDL_AudioFormat format;
    Uint8 channels;
    Uint8 silence;
    Uint16 samples;
    Uint16 padding;
    Uint32 size;
    SDL_AudioCallback callback;
    void *userdata;
} SDL_AudioSpec;

typedef struct SDL_DisplayMode {
    Uint32 format;
    int w;
    int h;
    int refresh_rate;
    void *driverdata;
} SDL_DisplayMode;

#define SDL_BUTTON_LEFT 1
#define SDL_BUTTON_MIDDLE 2
#define SDL_BUTTON_RIGHT 3
#define SDL_BUTTON_X1 4
#define SDL_BUTTON_X2 5
#define SDL_BUTTON(X) (1 << ((X)-1))
#define SDL_BUTTON_LMASK SDL_BUTTON(SDL_BUTTON_LEFT)
#define SDL_BUTTON_MMASK SDL_BUTTON(SDL_BUTTON_MIDDLE)
#define SDL_BUTTON_RMASK SDL_BUTTON(SDL_BUTTON_RIGHT)

inline Uint32 SDL_GetMouseState(int *x, int *y) {
    if (x) *x = 0;
    if (y) *y = 0;
    return 0;
}

inline Uint32 SDL_GetRelativeMouseState(int *x, int *y) {
    if (x) *x = 0;
    if (y) *y = 0;
    return 0;
}

inline void SDL_WarpMouseInWindow(SDL_Window *window, int x, int y) {
    (void)window; (void)x; (void)y;
}

inline int SDL_SetRelativeMouseMode(SDL_bool enabled) {
    (void)enabled;
    return 0;
}

inline SDL_bool SDL_GetRelativeMouseMode(void) {
    return (SDL_bool)0;
}

inline SDL_AudioDeviceID SDL_OpenAudioDevice(const char *device, int iscapture, const SDL_AudioSpec *desired, SDL_AudioSpec *obtained, int allowed_changes) {
    (void)device; (void)iscapture; (void)desired; (void)obtained; (void)allowed_changes;
    return 0;
}

inline void SDL_CloseAudioDevice(SDL_AudioDeviceID dev) {
    (void)dev;
}

inline void SDL_PauseAudioDevice(SDL_AudioDeviceID dev, int pause_on) {
    (void)dev; (void)pause_on;
}

#ifdef __cplusplus
}
#endif
