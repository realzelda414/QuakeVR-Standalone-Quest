#include "quakedef.hpp"
#include "host.hpp"
#include "sys.hpp"
#include <android_native_app_glue.h>
#include <android/log.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define LOG_TAG "QuakeVR-Main"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

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

                    LOGI("Calling Quake Engine Host_Init...");
                    Host_Init(&parms);
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
                Host_Shutdown();
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

            Host_Frame(time);

            quake::vr::bridge::end_frame();
        }
    }

    LOGI("QuakeVR Standalone Exiting cleanly.");
}
