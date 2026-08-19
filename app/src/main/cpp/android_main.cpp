#include <android_native_app_glue.h>
#include <android/log.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <memory>
#include <unistd.h>

#include "vr/quakevr_bridge.hpp"

#define LOG_TAG "QuakeVR-Main"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

struct EngineContext {
    struct android_app* app = nullptr;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    bool hasFocus = false;
    bool isReady = false;
    std::unique_ptr<QuakeVRBridge> vrBridge;
};

static bool initEGL(EngineContext* engine) {
    engine->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (engine->display == EGL_NO_DISPLAY) {
        LOGE("eglGetDisplay failed");
        return false;
    }

    if (!eglInitialize(engine->display, nullptr, nullptr)) {
        LOGE("eglInitialize failed");
        return false;
    }

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
    if (!eglChooseConfig(engine->display, attribs, &config, 1, &numConfigs) || numConfigs == 0) {
        LOGE("eglChooseConfig failed");
        return false;
    }

    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };

    engine->context = eglCreateContext(engine->display, config, EGL_NO_CONTEXT, contextAttribs);
    if (engine->context == EGL_NO_CONTEXT) {
        LOGE("eglCreateContext failed");
        return false;
    }

    engine->surface = eglCreateWindowSurface(engine->display, config, engine->app->window, nullptr);
    if (engine->surface == EGL_NO_SURFACE) {
        LOGE("eglCreateWindowSurface failed");
        return false;
    }

    if (!eglMakeCurrent(engine->display, engine->surface, engine->surface, engine->context)) {
        LOGE("eglMakeCurrent failed");
        return false;
    }

    LOGI("EGL and OpenGL ES 3.0 initialized successfully");
    return true;
}

static void termEGL(EngineContext* engine) {
    if (engine->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(engine->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (engine->context != EGL_NO_CONTEXT) {
            eglDestroyContext(engine->display, engine->context);
        }
        if (engine->surface != EGL_NO_SURFACE) {
            eglDestroySurface(engine->display, engine->surface);
        }
        eglTerminate(engine->display);
    }
    engine->display = EGL_NO_DISPLAY;
    engine->context = EGL_NO_CONTEXT;
    engine->surface = EGL_NO_SURFACE;
}

static void handleAppCmd(struct android_app* app, int32_t cmd) {
    auto* engine = (EngineContext*)app->userData;
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (app->window != nullptr) {
                if (!engine->isReady) {
                    if (initEGL(engine)) {
                        engine->vrBridge = std::make_unique<QuakeVRBridge>();
                        if (engine->vrBridge->initializeOpenXR(app)) {
                            engine->isReady = true;
                            LOGI("QuakeVR Quest Engine initialized!");
                        }
                    }
                }
            }
            break;

        case APP_CMD_TERM_WINDOW:
            if (engine->vrBridge) {
                engine->vrBridge->shutdownOpenXR();
                engine->vrBridge.reset();
            }
            termEGL(engine);
            engine->isReady = false;
            break;

        case APP_CMD_GAINED_FOCUS:
            engine->hasFocus = true;
            break;

        case APP_CMD_LOST_FOCUS:
            engine->hasFocus = false;
            break;
    }
}

void android_main(struct android_app* app) {
    EngineContext engine{};
    engine.app = app;
    app->userData = &engine;
    app->onAppCmd = handleAppCmd;

    LOGI("Starting QuakeVR Quest 3 Standalone Runtime...");

    VRInputState inputState{};
    VREyeView eyeViews[2]{};

    while (true) {
        int ident;
        int events;
        struct android_poll_source* source;

        // Poll Android events (non-blocking if active, blocking if paused)
        while ((ident = ALooper_pollOnce(engine.isReady && engine.hasFocus ? 0 : -1, nullptr, &events, (void**)&source)) >= 0) {
            if (source != nullptr) {
                source->process(app, source);
            }
            if (app->destroyRequested != 0) {
                if (engine.vrBridge) {
                    engine.vrBridge->shutdownOpenXR();
                    engine.vrBridge.reset();
                }
                termEGL(&engine);
                return;
            }
        }

        if (engine.isReady && engine.vrBridge) {
            engine.vrBridge->pollEvents();

            if (engine.vrBridge->isSessionRunning()) {
                engine.vrBridge->updateTrackingAndInput(inputState);

                if (engine.vrBridge->beginFrame(eyeViews)) {
                    // Render left and right eyes to OpenXR swapchains
                    for (int eye = 0; eye < 2; ++eye) {
                        glBindFramebuffer(GL_FRAMEBUFFER, eyeViews[eye].fboId);
                        glViewport(0, 0, eyeViews[eye].width, eyeViews[eye].height);
                        
                        // Clear background to dark Quake atmosphere color
                        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
                        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                    }

                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    engine.vrBridge->endFrame();
                }
            }
        }
    }
}
