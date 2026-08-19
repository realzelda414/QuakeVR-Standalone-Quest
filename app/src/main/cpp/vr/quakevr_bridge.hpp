#pragma once

#include <android/native_window.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

#define XR_USE_PLATFORM_ANDROID 1
#define XR_USE_GRAPHICS_API_OPENGL_ES 1

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <vector>
#include <string>
#include <memory>

// QuakeVR Math structures compatible with Vittorio Romeo's engine
struct Vec3 {
    float x{0.0f}, y{0.0f}, z{0.0f};
    Vec3() = default;
    Vec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
};

struct Quat {
    float x{0.0f}, y{0.0f}, z{0.0f}, w{1.0f};
};

struct Mat4x4 {
    float m[16];
};

struct VRControllerState {
    bool isTracked{false};
    Vec3 position;
    Quat rotation;
    Vec3 linearVelocity;
    Vec3 angularVelocity;

    // Inputs
    float trigger{0.0f};
    float grip{0.0f};
    float thumbstickX{0.0f};
    float thumbstickY{0.0f};
    bool primaryButton{false};   // A on Right, X on Left
    bool secondaryButton{false}; // B on Right, Y on Left
    bool thumbstickClick{false};
    bool menuButton{false};
};

struct HolsterAnchor {
    enum class Type { LeftHip, RightHip, Chest, BackLeft, BackRight };
    Type type;
    Vec3 offsetFromHmd;
    float grabRadius{0.18f}; // 18cm grab zone
    int boundWeaponId{-1};
};

class QuakeVROpenXRBridge {
public:
    static QuakeVROpenXRBridge& getInstance();

    // Lifecycle
    bool initialize(struct android_app* app);
    void shutdown();
    void pollEvents();

    // Frame synchronization
    bool beginFrame(int& renderWidth, int& renderHeight);
    void bindEyeFramebuffer(int eyeIndex);
    void endFrame();

    // Tracking & Poses (Replacing OpenVR GetDeviceToAbsoluteTrackingPose)
    void updateTracking();
    Mat4x4 getEyeViewMatrix(int eyeIndex) const;
    Mat4x4 getEyeProjectionMatrix(int eyeIndex, float nearZ = 4.0f, float farZ = 8192.0f) const;
    
    // Hands & Controllers (Vittorio Romeo Holsters & Two-Hand Aiming)
    const VRControllerState& getLeftController() const { return m_leftHand; }
    const VRControllerState& getRightController() const { return m_rightHand; }
    void triggerHaptic(int handIndex, float durationSeconds, float frequency, float amplitude);

    // Holster system integration
    Vec3 getHolsterPosition(HolsterAnchor::Type type) const;
    bool checkHolsterGrab(int handIndex, HolsterAnchor::Type type) const;

    // OpenXR State accessors
    XrSession getSession() const { return m_session; }
    XrSpace getAppSpace() const { return m_appSpace; }
    bool isSessionRunning() const { return m_sessionRunning; }

private:
    QuakeVROpenXRBridge() = default;
    ~QuakeVROpenXRBridge() = default;

    XrInstance m_instance{XR_NULL_HANDLE};
    XrSession m_session{XR_NULL_HANDLE};
    XrSpace m_appSpace{XR_NULL_HANDLE};
    XrSpace m_headSpace{XR_NULL_HANDLE};
    XrSpace m_leftHandGripSpace{XR_NULL_HANDLE};
    XrSpace m_rightHandGripSpace{XR_NULL_HANDLE};
    XrSpace m_leftHandAimSpace{XR_NULL_HANDLE};
    XrSpace m_rightHandAimSpace{XR_NULL_HANDLE};

    XrSystemId m_systemId{XR_NULL_SYSTEM_ID};
    bool m_sessionRunning{false};

    // Swapchains for OpenGLES
    std::vector<XrViewConfigurationView> m_viewConfigs;
    std::vector<XrView> m_views;
    XrSwapchain m_colorSwapchain{XR_NULL_HANDLE};
    std::vector<XrSwapchainImageOpenGLESKHR> m_swapchainImages;
    std::vector<GLuint> m_framebuffers;
    std::vector<GLuint> m_depthBuffers;
    uint32_t m_currentSwapchainIndex{0};

    // Actions & Input
    XrActionSet m_actionSet{XR_NULL_HANDLE};
    XrAction m_poseGripAction{XR_NULL_HANDLE};
    XrAction m_poseAimAction{XR_NULL_HANDLE};
    XrAction m_triggerAction{XR_NULL_HANDLE};
    XrAction m_gripAction{XR_NULL_HANDLE};
    XrAction m_moveStickAction{XR_NULL_HANDLE};
    XrAction m_turnStickAction{XR_NULL_HANDLE};
    XrAction m_primaryBtnAction{XR_NULL_HANDLE};
    XrAction m_secondaryBtnAction{XR_NULL_HANDLE};
    XrAction m_hapticAction{XR_NULL_HANDLE};

    XrPath m_leftHandPath{XR_NULL_PATH};
    XrPath m_rightHandPath{XR_NULL_PATH};

    VRControllerState m_leftHand;
    VRControllerState m_rightHand;
    Vec3 m_hmdPosition;
    Quat m_hmdRotation;

    XrFrameState m_frameState{XR_TYPE_FRAME_STATE};
};
