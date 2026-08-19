#include <android_native_app_glue.h>
#include <android/log.h>
#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include <chrono>
#include <memory>
#include <vector>
#include <string>

#include "vr/quakevr_bridge.hpp"

#define LOG_TAG "QuakeVR-Native"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Quake Engine C/C++ externs
extern "C" {
    void Host_Init(int argc, char** argv);
    void Host_Frame(float time);
    void Host_Shutdown(void);
    void COM_AddGameDirectory(const char* dir);
    void Key_Event(int key, int down);
}

// Global App State
struct EngineState {
    struct android_app* app{nullptr};
    EGLDisplay display{EGL_NO_DISPLAY};
    EGLSurface surface{EGL_NO_SURFACE};
    EGLContext context{EGL_NO_CONTEXT};
    
    std::unique_ptr<quakevr::QuakeVRBridge> vrBridge;
    bool openxrReady{false};
    bool quakeInitialized{false};
    bool hasFocus{false};
};

static void initEGL(EngineState* state) {
    state->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(state->display, nullptr, nullptr);

    const EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
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
    state->surface = eglCreateWindowSurface(state->display, config, state->app->window, nullptr);
    eglMakeCurrent(state->display, state->surface, state->surface, state->context);
    
    LOGI("EGL & OpenGL ES 3.2 initialized successfully for Meta Quest.");
}

static void termEGL(EngineState* state) {
    if (state->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(state->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (state->context != EGL_NO_CONTEXT) {
            eglDestroyContext(state->display, state->context);
        }
        if (state->surface != EGL_NO_SURFACE) {
            eglDestroySurface(state->display, state->surface);
        }
        eglTerminate(state->display);
    }
    state->display = EGL_NO_DISPLAY;
    state->context = EGL_NO_CONTEXT;
    state->surface = EGL_NO_SURFACE;
}

static void initQuakeEngine(EngineState* state) {
    if (state->quakeInitialized) return;

    LOGI("Booting Quake Engine with Quest base directory...");

    // Setup launch arguments for Quake
    // Directs the file system to read from /sdcard/QuakeVR/id1
    const char* quakeArgs[] = {
        "quakevr",
        "-basedir", "/sdcard/QuakeVR",
        "-game", "id1",
        "+nosound", "0",
        "+gl_multiview", "1"
    };
    int argc = sizeof(quakeArgs) / sizeof(quakeArgs[0]);

    // Initialize Quake Subsystems (zone memory, cvars, filesystem, sound, renderer)
    Host_Init(argc, (char**)quakeArgs);

    // Ensure explicit Quest search directories
    COM_AddGameDirectory("/sdcard/QuakeVR/id1");
    COM_AddGameDirectory("/storage/emulated/0/QuakeVR/id1");

    state->quakeInitialized = true;
    LOGI("Quake Engine initialized and ready to render.");
}

static void handleAppCmd(struct android_app* app, int32_t cmd) {
    auto* state = static_cast<EngineState*>(app->userData);
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (app->window != nullptr) {
                initEGL(state);
                
                // Initialize OpenXR Subsystem
                state->vrBridge = std::make_unique<quakevr::QuakeVRBridge>(app);
                if (state->vrBridge->initializeOpenXR(state->display, state->context)) {
                    state->openxrReady = true;
                    initQuakeEngine(state);
                } else {
                    LOGE("Failed to initialize OpenXR on Meta Quest.");
                }
            }
            break;

        case APP_CMD_TERM_WINDOW:
            if (state->quakeInitialized) {
                Host_Shutdown();
                state->quakeInitialized = false;
            }
            state->vrBridge.reset();
            state->openxrReady = false;
            termEGL(state);
            break;

        case APP_CMD_GAINED_FOCUS:
            state->hasFocus = true;
            break;

        case APP_CMD_LOST_FOCUS:
            state->hasFocus = false;
            break;
    }
}

void android_main(struct android_app* app) {
    EngineState state{};
    state.app = app;
    app->userData = &state;
    app->onAppCmd = handleAppCmd;

    LOGI("QuakeVR Standalone Quest starting native main loop...");

    auto prevTime = std::chrono::high_resolution_clock::now();

    while (true) {
        int events;
        struct android_poll_source* source;

        // Poll Android and OpenXR events non-blocking when active
        while (ALooper_pollOnce(state.openxrReady && state.hasFocus ? 0 : -1,
                                nullptr, &events, (void**)&source) >= 0) {
            if (source != nullptr) {
                source->process(app, source);
            }
            if (app->destroyRequested != 0) {
                if (state.quakeInitialized) {
                    Host_Shutdown();
                }
                termEGL(&state);
                LOGI("QuakeVR Standalone Quest native activity destroyed.");
                return;
            }
        }

        if (!state.openxrReady || !state.quakeInitialized) {
            continue;
        }

        // Calculate delta time for Quake physics and simulation
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - prevTime).count();
        prevTime = currentTime;

        // Cap delta time to prevent physics anomalies
        if (deltaTime > 0.1f) deltaTime = 0.1f;

        // 1. OpenXR Event & Input Poll
        quakevr::VRInputState inputState{};
        state->vrBridge->pollEvents();
        state->vrBridge->updateTrackingAndInput(inputState);

        // 2. Advance Quake Engine Frame
        Host_Frame(deltaTime);

        // 3. Render Stereoscopic Views to Quest Headset Swapchains
        std::vector<quakevr::EyeView> eyeViews;
        if (state->vrBridge->beginFrame(eyeViews)) {
            for (size_t eye = 0; eye < eyeViews.size(); ++eye) {
                glBindFramebuffer(GL_FRAMEBUFFER, eyeViews[eye].fboId);
                glViewport(0, 0, eyeViews[eye].width, eyeViews[eye].height);
                
                // Clear and render Quake world for this eye
                glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                // Enable depth testing for 3D world rendering
                glEnable(GL_DEPTH_TEST);
                glDepthFunc(GL_LEQUAL);
            }

            state->vrBridge->endFrame();
        }
    }
}
