#include <android_native_app_glue.h>
#include <android/log.h>
#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include "vr/quakevr_bridge.hpp"

#define LOG_TAG "QuakeVR-Main"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// Quake Engine C entry points (from QuakeSpasm)
extern "C" {
    void Host_Init(int argc, char** argv);
    void Host_Frame(float timeDelta);
    void Host_Shutdown();
}

struct EngineState {
    struct android_app* app{nullptr};
    EGLDisplay display{EGL_NO_DISPLAY};
    EGLSurface surface{EGL_NO_SURFACE};
    EGLContext context{EGL_NO_CONTEXT};
    bool isResumed{false};
    bool isInitialized{false};
};

static void initEGL(EngineState* state) {
    LOGI("Setting up EGL for OpenGL ES 3.2 on Quest 3...");
    state->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(state->display, nullptr, nullptr);

    const EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, // Headless PBuffer for OpenXR swapchain rendering
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_NONE
    };

    EGLConfig config;
    EGLint numConfigs;
    eglChooseConfig(state->display, attribs, &config, 1, &numConfigs);

    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };
    state->context = eglCreateContext(state->display, config, EGL_NO_CONTEXT, contextAttribs);

    const EGLint pbufferAttribs[] = {
        EGL_WIDTH, 16,
        EGL_HEIGHT, 16,
        EGL_NONE
    };
    state->surface = eglCreatePbufferSurface(state->display, config, pbufferAttribs);
    eglMakeCurrent(state->display, state->surface, state->surface, state->context);
}

static void handleAppCmd(struct android_app* app, int32_t cmd) {
    auto* state = (EngineState*)app->userData;
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (app->window != nullptr) {
                if (!state->isInitialized) {
                    initEGL(state);
                    QuakeVROpenXRBridge::getInstance().initialize(app);
                    
                    // Initialize Quake Engine with VR arguments
                    const char* argv[] = { "quake", "-basedir", "/sdcard/QuakeVR", "-vr" };
                    Host_Init(4, (char**)argv);
                    state->isInitialized = true;
                }
            }
            break;
        case APP_CMD_RESUME:
            state->isResumed = true;
            break;
        case APP_CMD_PAUSE:
            state->isResumed = false;
            break;
        case APP_CMD_DESTROY:
            Host_Shutdown();
            QuakeVROpenXRBridge::getInstance().shutdown();
            state->isInitialized = false;
            break;
    }
}

void android_main(struct android_app* app) {
    LOGI("QuakeVR Android NativeActivity Bootstrapping...");
    EngineState state{};
    state.app = app;
    app->userData = &state;
    app->onAppCmd = handleAppCmd;

    // Main Engine Game Loop
    while (true) {
        int events;
        struct android_poll_source* source;

        while ((ALooper_pollOnce(state.isResumed ? 0 : -1, nullptr, &events, (void**)&source)) >= 0) {
            if (source != nullptr) {
                source->process(app, source);
            }
            if (app->destroyRequested != 0) {
                return;
            }
        }

        if (state.isResumed && state.isInitialized) {
            QuakeVROpenXRBridge::getInstance().pollEvents();

            int renderW = 0, renderH = 0;
            if (QuakeVROpenXRBridge::getInstance().beginFrame(renderW, renderH)) {
                // Execute Quake Spasm frame tick
                Host_Frame(0.0111f); // ~90fps tick

                // Finish frame composition
                QuakeVROpenXRBridge::getInstance().endFrame();
            }
        }
    }
}
