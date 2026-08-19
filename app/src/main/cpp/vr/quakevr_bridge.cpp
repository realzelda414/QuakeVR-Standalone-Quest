#include "quakevr_bridge.hpp"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/log.h>
#include <cmath>
#include <cstring>

#define LOG_TAG "QuakeVR-Bridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define XR_CHECK(x) do { \
    XrResult res = (x); \
    if (XR_FAILED(res)) { \
        LOGE("OpenXR error %d at %s:%d", (int)res, __FILE__, __LINE__); \
        return false; \
    } \
} while(0)

QuakeVROpenXRBridge& QuakeVROpenXRBridge::getInstance() {
    static QuakeVROpenXRBridge instance;
    return instance;
}

bool QuakeVROpenXRBridge::initialize(struct android_app* app) {
    LOGI("Initializing OpenXR for Meta Quest 3...");

    // 1. Initialize OpenXR Loader for Android
    PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR = nullptr;
    xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR", 
        (PFN_xrVoidFunction*)&xrInitializeLoaderKHR);

    if (xrInitializeLoaderKHR) {
        XrLoaderInitInfoAndroidKHR loaderInitInfoAndroid{XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
        loaderInitInfoAndroid.applicationVM = app->activity->vm;
        loaderInitInfoAndroid.applicationContext = app->activity->clazz;
        xrInitializeLoaderKHR((const XrLoaderInitInfoBaseHeaderKHR*)&loaderInitInfoAndroid);
    }

    // 2. Create OpenXR Instance with OpenGL ES & Android extensions
    const char* enabledExtensions[] = {
        XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME,
        XR_KHR_ANDROID_CREATE_INSTANCE_HELPER_EXTENSION_NAME,
        "XR_FB_display_refresh_rate"
    };

    XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
    strcpy(createInfo.applicationInfo.applicationName, "QuakeVR-Quest3");
    createInfo.applicationInfo.applicationVersion = 1;
    strcpy(createInfo.applicationInfo.engineName, "QuakeSpasm-VR");
    createInfo.applicationInfo.engineVersion = 1;
    createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
    createInfo.enabledExtensionCount = 3;
    createInfo.enabledExtensionNames = enabledExtensions;

    XrInstanceCreateInfoAndroidKHR createInfoAndroid{XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
    createInfoAndroid.applicationVM = app->activity->vm;
    createInfoAndroid.applicationActivity = app->activity->clazz;
    createInfo.next = &createInfoAndroid;

    XR_CHECK(xrCreateInstance(&createInfo, &m_instance));

    // 3. Get System for VR HMD (Quest 3 Form Factor)
    XrSystemGetInfo systemGetInfo{XR_TYPE_SYSTEM_GET_INFO};
    systemGetInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XR_CHECK(xrGetSystem(m_instance, &systemGetInfo, &m_systemId));

    // Request 90Hz / 120Hz display refresh rate on Quest 3
    PFN_xrRequestDisplayRefreshRateFB xrRequestDisplayRefreshRateFB = nullptr;
    xrGetInstanceProcAddr(m_instance, "xrRequestDisplayRefreshRateFB",
        (PFN_xrVoidFunction*)&xrRequestDisplayRefreshRateFB);

    // 4. Query OpenGL ES Graphics Requirements
    PFN_xrGetOpenGLESGraphicsRequirementsKHR xrGetOpenGLESGraphicsRequirementsKHR = nullptr;
    xrGetInstanceProcAddr(m_instance, "xrGetOpenGLESGraphicsRequirementsKHR",
        (PFN_xrVoidFunction*)&xrGetOpenGLESGraphicsRequirementsKHR);

    XrGraphicsRequirementsOpenGLESKHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};
    if (xrGetOpenGLESGraphicsRequirementsKHR) {
        xrGetOpenGLESGraphicsRequirementsKHR(m_instance, m_systemId, &graphicsRequirements);
    }

    // 5. Create Session with EGL context
    XrGraphicsBindingOpenGLESAndroidKHR graphicsBinding{XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};
    graphicsBinding.display = eglGetCurrentDisplay();
    graphicsBinding.config = (EGLConfig)0;
    graphicsBinding.context = eglGetCurrentContext();

    XrSessionCreateInfo sessionCreateInfo{XR_TYPE_SESSION_CREATE_INFO};
    sessionCreateInfo.next = &graphicsBinding;
    sessionCreateInfo.systemId = m_systemId;
    XR_CHECK(xrCreateSession(m_instance, &sessionCreateInfo, &m_session));

    if (xrRequestDisplayRefreshRateFB) {
        xrRequestDisplayRefreshRateFB(m_session, 90.0f); // Default to silky-smooth 90Hz
    }

    // 6. Create Reference Spaces (Stage / Local / View)
    XrReferenceSpaceCreateInfo spaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE; // Room-scale 6DoF
    spaceCreateInfo.poseInReferenceSpace.orientation.w = 1.0f;
    XR_CHECK(xrCreateReferenceSpace(m_session, &spaceCreateInfo, &m_appSpace));

    spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    XR_CHECK(xrCreateReferenceSpace(m_session, &spaceCreateInfo, &m_headSpace));

    // 7. Initialize Quest Touch Controller Action Sets
    xrStringToPath(m_instance, "/user/hand/left", &m_leftHandPath);
    xrStringToPath(m_instance, "/user/hand/right", &m_rightHandPath);

    XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
    strcpy(actionSetInfo.actionSetName, "gameplay");
    strcpy(actionSetInfo.localizedActionSetName, "Quake VR Gameplay");
    XR_CHECK(xrCreateActionSet(m_instance, &actionSetInfo, &m_actionSet));

    // Subpath array for both hands
    XrPath handSubpaths[] = { m_leftHandPath, m_rightHandPath };

    // Grip Pose (for weapon physics & holsters)
    XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
    actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
    strcpy(actionInfo.actionName, "hand_pose_grip");
    strcpy(actionInfo.localizedActionName, "Hand Pose Grip");
    actionInfo.countSubactionPaths = 2;
    actionInfo.subactionPaths = handSubpaths;
    XR_CHECK(xrCreateAction(m_actionSet, &actionInfo, &m_poseGripAction));

    // Aim Pose (for gun barrel raycasting)
    strcpy(actionInfo.actionName, "hand_pose_aim");
    strcpy(actionInfo.localizedActionName, "Hand Pose Aim");
    XR_CHECK(xrCreateAction(m_actionSet, &actionInfo, &m_poseAimAction));

    // Trigger (Shooting)
    actionInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
    strcpy(actionInfo.actionName, "trigger_value");
    strcpy(actionInfo.localizedActionName, "Trigger Value");
    XR_CHECK(xrCreateAction(m_actionSet, &actionInfo, &m_triggerAction));

    // Grip (Grabbing / Holstering / Two-handed stabilizing)
    strcpy(actionInfo.actionName, "grip_value");
    strcpy(actionInfo.localizedActionName, "Grip Value");
    XR_CHECK(xrCreateAction(m_actionSet, &actionInfo, &m_gripAction));

    // Thumbsticks (Locomotion & Snap Turn)
    actionInfo.actionType = XR_ACTION_TYPE_VECTOR2F_INPUT;
    strcpy(actionInfo.actionName, "move_stick");
    strcpy(actionInfo.localizedActionName, "Move Stick");
    XR_CHECK(xrCreateAction(m_actionSet, &actionInfo, &m_moveStickAction));

    // Haptics
    actionInfo.actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT;
    strcpy(actionInfo.actionName, "haptic_pulse");
    strcpy(actionInfo.localizedActionName, "Haptic Feedback");
    XR_CHECK(xrCreateAction(m_actionSet, &actionInfo, &m_hapticAction));

    // Bind to Meta Quest Touch Profile
    XrPath touchProfilePath;
    xrStringToPath(m_instance, "/interaction_profiles/oculus/touch_controller", &touchProfilePath);

    std::vector<XrActionSuggestedBinding> bindings;
    auto addBinding = [&](XrAction action, const char* pathStr) {
        XrPath path;
        xrStringToPath(m_instance, pathStr, &path);
        bindings.push_back({action, path});
    };

    addBinding(m_poseGripAction, "/user/hand/left/input/grip/pose");
    addBinding(m_poseGripAction, "/user/hand/right/input/grip/pose");
    addBinding(m_poseAimAction, "/user/hand/left/input/aim/pose");
    addBinding(m_poseAimAction, "/user/hand/right/input/aim/pose");
    addBinding(m_triggerAction, "/user/hand/left/input/trigger/value");
    addBinding(m_triggerAction, "/user/hand/right/input/trigger/value");
    addBinding(m_gripAction, "/user/hand/left/input/squeeze/value");
    addBinding(m_gripAction, "/user/hand/right/input/squeeze/value");
    addBinding(m_moveStickAction, "/user/hand/left/input/thumbstick");
    addBinding(m_moveStickAction, "/user/hand/right/input/thumbstick");
    addBinding(m_hapticAction, "/user/hand/left/output/haptic");
    addBinding(m_hapticAction, "/user/hand/right/output/haptic");

    XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    suggestedBindings.interactionProfile = touchProfilePath;
    suggestedBindings.suggestedBindings = bindings.data();
    suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
    XR_CHECK(xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings));

    // Create Action Spaces for hands
    XrActionSpaceCreateInfo actionSpaceInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
    actionSpaceInfo.action = m_poseGripAction;
    actionSpaceInfo.poseInActionSpace.orientation.w = 1.0f;
    actionSpaceInfo.subactionPath = m_leftHandPath;
    XR_CHECK(xrCreateActionSpace(m_session, &actionSpaceInfo, &m_leftHandGripSpace));

    actionSpaceInfo.subactionPath = m_rightHandPath;
    XR_CHECK(xrCreateActionSpace(m_session, &actionSpaceInfo, &m_rightHandGripSpace));

    // Attach Action Set to Session
    XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attachInfo.countActionSets = 1;
    attachInfo.actionSets = &m_actionSet;
    XR_CHECK(xrAttachSessionActionSets(m_session, &attachInfo));

    // 8. Create Double-Eye Swapchains (GLES 3.2 Texture Arrays for Multiview)
    uint32_t viewCount = 0;
    xrEnumerateViewConfigurationViews(m_instance, m_systemId, 
        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewCount, nullptr);
    m_viewConfigs.resize(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
    xrEnumerateViewConfigurationViews(m_instance, m_systemId,
        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, viewCount, &viewCount, m_viewConfigs.data());

    m_views.resize(viewCount, {XR_TYPE_VIEW});

    // Create Texture Array Swapchain for Single-Pass Multiview
    XrSwapchainCreateInfo swapchainInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    swapchainInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
    swapchainInfo.format = GL_SRGB8_ALPHA8;
    swapchainInfo.sampleCount = 1;
    swapchainInfo.width = m_viewConfigs[0].recommendedImageRectWidth;
    swapchainInfo.height = m_viewConfigs[0].recommendedImageRectHeight;
    swapchainInfo.faceCount = 1;
    swapchainInfo.arraySize = 2; // Left & Right Eye Multiview Layers
    swapchainInfo.mipCount = 1;
    XR_CHECK(xrCreateSwapchain(m_session, &swapchainInfo, &m_colorSwapchain));

    uint32_t imageCount = 0;
    xrEnumerateSwapchainImages(m_colorSwapchain, 0, &imageCount, nullptr);
    m_swapchainImages.resize(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR});
    xrEnumerateSwapchainImages(m_colorSwapchain, imageCount, &imageCount, 
        (XrSwapchainImageBaseHeader*)m_swapchainImages.data());

    // Generate FBOs
    m_framebuffers.resize(imageCount);
    m_depthBuffers.resize(imageCount);
    glGenFramebuffers(imageCount, m_framebuffers.data());
    glGenRenderbuffers(imageCount, m_depthBuffers.data());

    for (uint32_t i = 0; i < imageCount; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffers[i]);
        glBindRenderbuffer(GL_RENDERBUFFER, m_depthBuffers[i]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, swapchainInfo.width, swapchainInfo.height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthBuffers[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_swapchainImages[i].image, 0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    LOGI("QuakeVR OpenXR Subsystem successfully initialized. Viewport: %dx%d per eye", 
        swapchainInfo.width, swapchainInfo.height);
    return true;
}

void QuakeVROpenXRBridge::pollEvents() {
    XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
    while (xrPollEvent(m_instance, &event) == XR_SUCCESS) {
        if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            auto sessionStateChanged = (XrEventDataSessionStateChanged*)&event;
            if (sessionStateChanged->state == XR_SESSION_STATE_READY) {
                XrSessionBeginInfo beginInfo{XR_TYPE_SESSION_BEGIN_INFO};
                beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                xrBeginSession(m_session, &beginInfo);
                m_sessionRunning = true;
            } else if (sessionStateChanged->state == XR_SESSION_STATE_STOPPING) {
                xrEndSession(m_session);
                m_sessionRunning = false;
            }
        }
        event.type = XR_TYPE_EVENT_DATA_BUFFER;
    }
}

void QuakeVROpenXRBridge::updateTracking() {
    if (!m_sessionRunning) return;

    // Sync Action Sets
    XrActiveActionSet activeActionSet{m_actionSet, XR_NULL_PATH};
    XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
    syncInfo.countActiveActionSets = 1;
    syncInfo.activeActionSets = &activeActionSet;
    xrSyncActions(m_session, &syncInfo);

    // Read Hand Poses (Left & Right 6DoF)
    auto updateHand = [&](XrSpace space, XrPath path, VRControllerState& state) {
        XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};
        xrLocateSpace(space, m_appSpace, m_frameState.predictedDisplayTime, &location);

        if ((location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) &&
            (location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)) {
            state.isTracked = true;
            state.position = Vec3(location.pose.position.x, location.pose.position.y, location.pose.position.z);
            state.rotation = {location.pose.orientation.x, location.pose.orientation.y, 
                              location.pose.orientation.z, location.pose.orientation.w};
        } else {
            state.isTracked = false;
        }

        // Read Trigger & Grip
        XrActionStateFloat triggerState{XR_TYPE_ACTION_STATE_FLOAT};
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO, nullptr, m_triggerAction, path};
        xrGetActionStateFloat(m_session, &getInfo, &triggerState);
        state.trigger = triggerState.currentState;

        getInfo.action = m_gripAction;
        XrActionStateFloat gripState{XR_TYPE_ACTION_STATE_FLOAT};
        xrGetActionStateFloat(m_session, &getInfo, &gripState);
        state.grip = gripState.currentState;

        // Read Thumbstick
        getInfo.action = m_moveStickAction;
        XrActionStateVector2f stickState{XR_TYPE_ACTION_STATE_VECTOR2F};
        xrGetActionStateVector2f(m_session, &getInfo, &stickState);
        state.thumbstickX = stickState.currentState.x;
        state.thumbstickY = stickState.currentState.y;
    };

    updateHand(m_leftHandGripSpace, m_leftHandPath, m_leftHand);
    updateHand(m_rightHandGripSpace, m_rightHandPath, m_rightHand);

    // Locate HMD Head
    XrSpaceLocation headLoc{XR_TYPE_SPACE_LOCATION};
    xrLocateSpace(m_headSpace, m_appSpace, m_frameState.predictedDisplayTime, &headLoc);
    m_hmdPosition = Vec3(headLoc.pose.position.x, headLoc.pose.position.y, headLoc.pose.position.z);
    m_hmdRotation = {headLoc.pose.orientation.x, headLoc.pose.orientation.y, 
                     headLoc.pose.orientation.z, headLoc.pose.orientation.w};
}

Vec3 QuakeVROpenXRBridge::getHolsterPosition(HolsterAnchor::Type type) const {
    // Calculates dynamic body holster slots anchored to HMD yaw & torso height
    // (Preserving Vittorio Romeo's physical body inventory mechanics)
    float yaw = 2.0f * std::atan2(m_hmdRotation.y, m_hmdRotation.w);
    float cosY = std::cos(yaw);
    float sinY = std::sin(yaw);

    Vec3 forward{sinY, 0.0f, -cosY};
    Vec3 right{cosY, 0.0f, sinY};

    switch (type) {
        case HolsterAnchor::Type::RightHip:
            return Vec3(
                m_hmdPosition.x + right.x * 0.22f - forward.x * 0.05f,
                m_hmdPosition.y - 0.48f,
                m_hmdPosition.z + right.z * 0.22f - forward.z * 0.05f
            );
        case HolsterAnchor::Type::LeftHip:
            return Vec3(
                m_hmdPosition.x - right.x * 0.22f - forward.x * 0.05f,
                m_hmdPosition.y - 0.48f,
                m_hmdPosition.z - right.z * 0.22f - forward.z * 0.05f
            );
        case HolsterAnchor::Type::Chest:
            return Vec3(
                m_hmdPosition.x + forward.x * 0.12f,
                m_hmdPosition.y - 0.22f,
                m_hmdPosition.z + forward.z * 0.12f
            );
        case HolsterAnchor::Type::BackRight:
            return Vec3(
                m_hmdPosition.x + right.x * 0.18f - forward.x * 0.20f,
                m_hmdPosition.y - 0.10f,
                m_hmdPosition.z + right.z * 0.18f - forward.z * 0.20f
            );
        case HolsterAnchor::Type::BackLeft:
            return Vec3(
                m_hmdPosition.x - right.x * 0.18f - forward.x * 0.20f,
                m_hmdPosition.y - 0.10f,
                m_hmdPosition.z - right.z * 0.18f - forward.z * 0.20f
            );
    }
    return m_hmdPosition;
}

bool QuakeVROpenXRBridge::checkHolsterGrab(int handIndex, HolsterAnchor::Type type) const {
    const auto& hand = (handIndex == 0) ? m_leftHand : m_rightHand;
    if (!hand.isTracked || hand.grip < 0.6f) return false;

    Vec3 holsterPos = getHolsterPosition(type);
    float dx = hand.position.x - holsterPos.x;
    float dy = hand.position.y - holsterPos.y;
    float dz = hand.position.z - holsterPos.z;
    float distSq = dx * dx + dy * dy + dz * dz;

    return distSq < (0.18f * 0.18f); // Within 18cm holster threshold
}

void QuakeVROpenXRBridge::triggerHaptic(int handIndex, float durationSeconds, float frequency, float amplitude) {
    XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
    vibration.duration = (XrDuration)(durationSeconds * 1e9f);
    vibration.frequency = frequency;
    vibration.amplitude = amplitude;

    XrHapticActionInfo hapticInfo{XR_TYPE_HAPTIC_ACTION_INFO};
    hapticInfo.action = m_hapticAction;
    hapticInfo.subactionPath = (handIndex == 0) ? m_leftHandPath : m_rightHandPath;

    xrApplyHapticFeedback(m_session, &hapticInfo, (XrHapticBaseHeader*)&vibration);
}

bool QuakeVROpenXRBridge::beginFrame(int& renderWidth, int& renderHeight) {
    if (!m_sessionRunning) return false;

    XrFrameWaitInfo waitInfo{XR_TYPE_FRAME_WAIT_INFO};
    xrWaitFrame(m_session, &waitInfo, &m_frameState);

    XrFrameBeginInfo beginInfo{XR_TYPE_FRAME_BEGIN_INFO};
    xrBeginFrame(m_session, &beginInfo);

    // Locate Views for Left and Right Eyes
    XrViewLocateInfo locateInfo{XR_TYPE_VIEW_LOCATE_INFO};
    locateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    locateInfo.displayTime = m_frameState.predictedDisplayTime;
    locateInfo.space = m_appSpace;

    uint32_t viewCount = 2;
    XrViewState viewState{XR_TYPE_VIEW_STATE};
    xrLocateViews(m_session, &locateInfo, &viewState, 2, &viewCount, m_views.data());

    // Acquire Swapchain Image
    XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    xrAcquireSwapchainImage(m_colorSwapchain, &acquireInfo, &m_currentSwapchainIndex);

    XrSwapchainImageWaitInfo imageWaitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    imageWaitInfo.timeout = XR_INFINITE_DURATION;
    xrWaitSwapchainImage(m_colorSwapchain, &imageWaitInfo);

    renderWidth = m_viewConfigs[0].recommendedImageRectWidth;
    renderHeight = m_viewConfigs[0].recommendedImageRectHeight;

    updateTracking();
    return true;
}

void QuakeVROpenXRBridge::bindEyeFramebuffer(int eyeIndex) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffers[m_currentSwapchainIndex]);
    glViewport(0, 0, m_viewConfigs[0].recommendedImageRectWidth, m_viewConfigs[0].recommendedImageRectHeight);
}

void QuakeVROpenXRBridge::endFrame() {
    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    xrReleaseSwapchainImage(m_colorSwapchain, &releaseInfo);

    // Prepare Composition Projection Layers
    std::array<XrCompositionLayerProjectionView, 2> projectionViews;
    for (int i = 0; i < 2; ++i) {
        projectionViews[i] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
        projectionViews[i].pose = m_views[i].pose;
        projectionViews[i].fov = m_views[i].fov;
        projectionViews[i].subImage.swapchain = m_colorSwapchain;
        projectionViews[i].subImage.imageRect.offset = {0, 0};
        projectionViews[i].subImage.imageRect.extent = {
            (int32_t)m_viewConfigs[0].recommendedImageRectWidth,
            (int32_t)m_viewConfigs[0].recommendedImageRectHeight
        };
        projectionViews[i].subImage.imageArrayIndex = (uint32_t)i;
    }

    XrCompositionLayerProjection projectionLayer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    projectionLayer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    projectionLayer.space = m_appSpace;
    projectionLayer.viewCount = 2;
    projectionLayer.views = projectionViews.data();

    const XrCompositionLayerBaseHeader* layers[] = {
        (const XrCompositionLayerBaseHeader*)&projectionLayer
    };

    XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
    endInfo.displayTime = m_frameState.predictedDisplayTime;
    endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    endInfo.layerCount = 1;
    endInfo.layers = layers;

    xrEndFrame(m_session, &endInfo);
}

void QuakeVROpenXRBridge::shutdown() {
    LOGI("Shutting down QuakeVR OpenXR Bridge...");
    if (m_colorSwapchain) xrDestroySwapchain(m_colorSwapchain);
    if (m_session) xrDestroySession(m_session);
    if (m_instance) xrDestroyInstance(m_instance);
}
