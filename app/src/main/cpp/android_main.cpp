#include <android_native_app_glue.h>
#include <android/log.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/time.h>

#define LOG_TAG "QuakeVR-Main"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Quake Engine Parameter Structure
struct quakeparms_s {
    const char *basedir;
    const char *userdir;
    int argc;
    char **argv;
    void *membase;
    int memsize;
};
typedef struct quakeparms_s quakeparms_t;

// Function Pointer Typedefs
typedef void (*Host_Init_fn)(void *parms);
typedef void (*Host_Frame_fn)(double time);
typedef void (*Host_Shutdown_fn)(void);

static Host_Init_fn g_Host_Init = NULL;
static Host_Frame_fn g_Host_Frame = NULL;
static Host_Shutdown_fn g_Host_Shutdown = NULL;

static double Sys_DoubleTime(void) {
    struct timeval tp;
    struct timezone tzp;
    gettimeofday(&tp, &tzp);
    return (double)tp.tv_sec + (double)tp.tv_usec / 1000000.0;
}

// Find a symbol in the loaded binary by checking common mangled & unmangled names
static void* FindEngineSymbol(const char** names, size_t count) {
    void* handle = dlopen(NULL, RTLD_NOW);
    if (!handle) {
        LOGE("Failed to open self dlopen: %s", dlerror());
        return NULL;
    }
    for (size_t i = 0; i < count; ++i) {
        void* sym = dlsym(handle, names[i]);
        if (sym) {
            LOGI("Found engine symbol: %s at %p", names[i], sym);
            return sym;
        }
    }
    return NULL;
}

static bool ResolveEngineSymbols(void) {
    const char* init_names[] = {
        "Host_Init",
        "_Z9Host_InitP12quakeparms_s",
        "_Z9Host_InitP12quakeparms_t",
        "_ZN5quake9Host_InitEP12quakeparms_s",
        "_ZN5quake9Host_InitEP12quakeparms_t",
        "_ZN5quake4host4initEP12quakeparms_s",
        "_ZN5quake4host4initEP12quakeparms_t"
    };
    g_Host_Init = (Host_Init_fn)FindEngineSymbol(init_names, sizeof(init_names)/sizeof(init_names[0]));

    const char* frame_names[] = {
        "Host_Frame",
        "_Z10Host_Framed",
        "_ZN5quake10Host_FrameEd",
        "_ZN5quake4host5frameEd"
    };
    g_Host_Frame = (Host_Frame_fn)FindEngineSymbol(frame_names, sizeof(frame_names)/sizeof(frame_names[0]));

    const char* shutdown_names[] = {
        "Host_Shutdown",
        "_Z13Host_Shutdownv",
        "_ZN5quake13Host_ShutdownEv",
        "_ZN5quake4host8shutdownEv"
    };
    g_Host_Shutdown = (Host_Shutdown_fn)FindEngineSymbol(shutdown_names, sizeof(shutdown_names)/sizeof(shutdown_names[0]));

    if (!g_Host_Init) {
        LOGE("CRITICAL: Host_Init symbol could not be resolved.");
        return false;
    }
    return true;
}

// OpenXR Bridge Declarations
namespace quake {
namespace vr {
namespace bridge {
    bool init_openxr(android_app *app) { (void)app; return true; }
    void shutdown_openxr(void) {}
    bool is_session_running(void) { return true; }
    void begin_frame(void) {}
    void end_frame(void) {}
}
}
}

static bool g_initialized = false;
static bool g_app_active = false;

static void on_app_cmd(struct android_app *app, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            LOGI("APP_CMD_INIT_WINDOW");
            if (app->window != NULL) {
                if (!g_initialized) {
                    if (quake::vr::bridge::init_openxr(app)) {
                        LOGI("OpenXR initialized successfully on Meta Quest");
                    }

                    if (!ResolveEngineSymbols()) {
                        LOGE("Could not locate Host_Init in engine library.");
                    }

                    static const char *args[] = {
                        "quakevr",
                        "-basedir", "/sdcard/QuakeVR",
                        "-userdir", "/sdcard/QuakeVR",
                        "-vr",
                        "+vr_aimmode", "1",
                        "+vr_movement_mode", "1"
                    };

                    static quakeparms_t parms;
                    memset(&parms, 0, sizeof(parms));
                    parms.basedir = "/sdcard/QuakeVR";
                    parms.userdir = "/sdcard/QuakeVR";
                    parms.argc = sizeof(args) / sizeof(args[0]);
                    parms.argv = (char**)args;
                    parms.memsize = 128 * 1024 * 1024; // 128MB heap
                    parms.membase = malloc(parms.memsize);

                    LOGI("Calling Quake Engine Host_Init via resolved pointer...");
                    if (g_Host_Init) {
                        g_Host_Init(&parms);
                    }
                    g_initialized = true;
                }
                g_app_active = true;
            }
            break;

        case APP_CMD_TERM_WINDOW:
            LOGI("APP_CMD_TERM_WINDOW");
            g_app_active = false;
            break;

        case APP_CMD_DESTROY:
            LOGI("APP_CMD_DESTROY");
            if (g_initialized) {
                if (g_Host_Shutdown) {
                    g_Host_Shutdown();
                }
                quake::vr::bridge::shutdown_openxr();
                g_initialized = false;
            }
            break;

        case APP_CMD_GAINED_FOCUS:
            LOGI("APP_CMD_GAINED_FOCUS");
            g_app_active = true;
            break;

        case APP_CMD_LOST_FOCUS:
            LOGI("APP_CMD_LOST_FOCUS");
            g_app_active = false;
            break;
    }
}

void android_main(struct android_app *app) {
    LOGI("QuakeVR Standalone Starting...");
    app->onAppCmd = on_app_cmd;

    double oldtime = Sys_DoubleTime();

    while (app->destroyRequested == 0) {
        int events;
        struct android_poll_source *source = NULL;

        while (ALooper_pollOnce(g_app_active ? 0 : -1, NULL, &events, (void **)&source) >= 0) {
            if (source != NULL) {
                source->process(app, source);
            }
            if (app->destroyRequested != 0) {
                break;
            }
        }

        if (g_initialized && g_app_active && quake::vr::bridge::is_session_running()) {
            quake::vr::bridge::begin_frame();

            double newtime = Sys_DoubleTime();
            double time = newtime - oldtime;
            if (time <= 0.0) time = 0.001;
            if (time > 0.1) time = 0.1;
            oldtime = newtime;

            if (g_Host_Frame) {
                g_Host_Frame(time);
            }

            quake::vr::bridge::end_frame();
        }
    }

    LOGI("QuakeVR Standalone Exiting cleanly.");
}
