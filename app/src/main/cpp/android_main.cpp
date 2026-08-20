#include <android_native_app_glue.h>
#include <android/log.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define LOG_TAG "QuakeVR-Quest"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Quake engine extern declarations
extern "C" {
    void Host_Init(int argc, char** argv);
    void Host_Frame(float time);
    void Host_Shutdown(void);
    
    // Platform stubs to replace desktop SDL clipboard/icon
    char* PL_GetClipboardData(void) { return nullptr; }
    void PL_SetWindowIcon(void) {}
}

// Forward declare VR Bridge functions from vr/quakevr_bridge.cpp
namespace quake::vr::bridge {
    bool init_openxr(struct android_app* app);
    void shutdown_openxr();
    void begin_frame();
    void end_frame();
    bool is_session_running();
}

static struct android_app* g_app = nullptr;
static bool g_initialized = false;

static void ensure_directories() {
    const char* paths[] = {
        "/sdcard/QuakeVR",
        "/sdcard/QuakeVR/id1"
    };
    for (const char* path : paths) {
        mkdir(path, 0777);
    }
}

static void on_app_cmd(struct android_app* app, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_START:
            LOGI("APP_CMD_START");
            break;
        case APP_CMD_RESUME:
            LOGI("APP_CMD_RESUME");
            break;
        case APP_CMD_PAUSE:
            LOGI("APP_CMD_PAUSE");
            break;
        case APP_CMD_STOP:
            LOGI("APP_CMD_STOP");
            break;
        case APP_CMD_DESTROY:
            LOGI("APP_CMD_DESTROY");
            if (g_initialized) {
                quake::vr::bridge::shutdown_openxr();
                Host_Shutdown();
                g_initialized = false;
            }
            break;
        case APP_CMD_INIT_WINDOW:
            LOGI("APP_CMD_INIT_WINDOW");
            if (app->window != nullptr && !g_initialized) {
                ensure_directories();
                
                // Initialize OpenXR on Quest
                if (quake::vr::bridge::init_openxr(app)) {
                    LOGI("OpenXR initialized successfully on Meta Quest");
                    
                    // Boot Quake engine with standalone Quest parameters
                    char* argv[] = {
                        (char*)"quakevr",
                        (char*)"-basedir",
                        (char*)"/sdcard/QuakeVR",
                        (char*)"+set",
                        (char*)"vr_enabled",
                        (char*)"1",
                        (char*)"+set",
                        (char*)"gl_clear",
                        (char*)"1",
                        nullptr
                    };
                    int argc = sizeof(argv) / sizeof(argv[0]) - 1;
                    
                    Host_Init(argc, argv);
                    g_initialized = true;
                    LOGI("Quake engine Host_Init complete!");
                } else {
                    LOGE("Failed to initialize OpenXR on Meta Quest");
                }
            }
            break;
        case APP_CMD_TERM_WINDOW:
            LOGI("APP_CMD_TERM_WINDOW");
            break;
    }
}

void android_main(struct android_app* app) {
    LOGI("QuakeVR Standalone Quest starting android_main...");
    g_app = app;
    app->onAppCmd = on_app_cmd;

    while (app->destroyRequested == 0) {
        int ident;
        int events;
        struct android_poll_source* source;

        // Poll native Android events
        while ((ident = ALooper_pollOnce(g_initialized ? 0 : -1, nullptr, &events, (void**)&source)) >= 0) {
            if (source != nullptr) {
                source->process(app, source);
            }
            if (app->destroyRequested != 0) {
                break;
            }
        }

        // Main game tick
        if (g_initialized && quake::vr::bridge::is_session_running()) {
            quake::vr::bridge::begin_frame();
            Host_Frame(0.011111f); // 90 FPS default frame step
            quake::vr::bridge::end_frame();
        }
    }
}
