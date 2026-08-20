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

extern "C" {

// Standard Quake types
typedef int qboolean;
#define qtrue 1
#define qfalse 0

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

typedef struct usercmd_s usercmd_t;
typedef struct sizebuf_s sizebuf_t;

// Global engine state
qboolean isDedicated = qfalse;
viddef_t vid = { 1920, 1080, 0, 1920 * 4, 4, 0, 640, 480, 0, 1, 0 };

// Time
double Sys_DoubleTime(void) {
    struct timeval tp;
    struct timezone tzp;
    gettimeofday(&tp, &tzp);
    return (double)tp.tv_sec + (double)tp.tv_usec / 1000000.0;
}

// Print & Error
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

// Video / Input Stubs
void VID_Lock(void) {}
void VID_Unlock(void) {}
void IN_UpdateGrabs(void) {}
void IN_Move(usercmd_t *cmd) { (void)cmd; }

// VOIP Stubs
void S_Voip_Transmit(unsigned char flags, sizebuf_t *sb) { (void)flags; (void)sb; }
void S_Voip_MapChange(void) {}
void S_Voip_Parse(void) {}

} // extern "C"
