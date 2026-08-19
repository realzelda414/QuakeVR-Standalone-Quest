#pragma once

#include <android/native_window.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

#ifndef XR_USE_PLATFORM_ANDROID
#define XR_USE_PLATFORM_ANDROID 1
#endif

#ifndef XR_USE_GRAPHICS_API_OPENGL_ES
#define XR_USE_GRAPHICS_API_OPENGL_ES 1
#endif

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <array>
#include <vector>
#include <string>
#include <memory>

struct VRInputState {
    float leftStickX = 0.0f;
    float leftStickY = 0.0f;
    float rightStickX = 0.0f;
    float rightStickY = 0.0f;
    
    bool triggerLeft = false;
    bool triggerRight = false;
    bool gripLeft = false;
    bool gripRight = false;
    bool buttonA = false;
    bool buttonB = false;
    bool buttonX = false;
    bool buttonY = false;
    bool menuButton = false;

    // Head pose (translation and quaternion)
    float headPos[3] = {0.0f, 0.0f, 0.0f};
    float headOri[4] = {0.0f, 0.0f, 0.0f, 1.0f};

    // Hands pose
    float handLeftPos[3] = {0.0f, 0.0f, 0.0f};
    float handLeftOri[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float handRightPos[3] = {0.0f, 0.0f, 0.0f};
    float handRightOri[4] = {0.0f, 0.0f, 0.0f, 1.0f};
};

struct VREyeView {
    float projMatrix[16];
    float viewMatrix[16];
    uint32_t width;
    uint32_t height;
    GLuint textureId;
    GLuint fboId;
};

class QuakeVRBridge {
public:
    QuakeVRBridge();
    ~QuakeVRBridge();

    bool initializeOpenXR(struct android_app* app);
    void shutdownOpenXR();

    bool beginFrame(VREyeView eyeViews[2]);
    void endFrame();

    void pollEvents();
    void updateTrackingAndInput(VRInputState& inputState);

    bool isSessionRunning() const { return m_sessionRunning; }

private:
    bool createInstance(struct android_app* app);
    bool createSession();
    bool createSpaces();
    bool createSwapchain();
    bool setupActions();

    XrInstance m_instance = XR_NULL_HANDLE;
    XrSession m_session = XR_NULL_HANDLE;
    XrSystemId m_systemId = XR_NULL_SYSTEM_ID;
    XrSpace m_appSpace = XR_NULL_HANDLE;
    XrSpace m_headSpace = XR_NULL_HANDLE;
    XrSpace m_leftHandSpace = XR_NULL_HANDLE;
    XrSpace m_rightHandSpace = XR_NULL_HANDLE;
    XrSwapchain m_swapchain = XR_NULL_HANDLE;

    XrSessionState m_sessionState = XR_SESSION_STATE_UNKNOWN;
    bool m_sessionRunning = false;

    // Actions
    XrActionSet m_actionSet = XR_NULL_HANDLE;
    XrAction m_handPoseAction = XR_NULL_HANDLE;
    XrAction m_moveAction = XR_NULL_HANDLE;
    XrAction m_lookAction = XR_NULL_HANDLE;
    XrAction m_fireAction = XR_NULL_HANDLE;
    XrAction m_jumpAction = XR_NULL_HANDLE;
    XrAction m_menuAction = XR_NULL_HANDLE;

    XrPath m_leftHandPath = XR_NULL_PATH;
    XrPath m_rightHandPath = XR_NULL_PATH;

    uint32_t m_width = 1832;
    uint32_t m_height = 1920;
    std::vector<XrSwapchainImageOpenGLESKHR> m_swapchainImages;
    std::vector<GLuint> m_framebuffers;

    XrFrameState m_frameState{XR_TYPE_FRAME_STATE};
    std::vector<XrView> m_views;
    std::vector<XrViewConfigurationView> m_configViews;
};
