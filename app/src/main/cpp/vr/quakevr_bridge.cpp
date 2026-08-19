#include "quakevr_bridge.hpp"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/log.h>
#include <cmath>
#include <cstring>
#include <array>

#define LOG_TAG "QuakeVR-Bridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

QuakeVRBridge::QuakeVRBridge() {
    m_views.resize(2, {XR_TYPE_VIEW});
    m_configViews.resize(2, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
}

QuakeVRBridge::~QuakeVRBridge() {
    shutdownOpenXR();
}

bool QuakeVRBridge::initializeOpenXR(struct android_app* app) {
    LOGI("Initializing OpenXR for Meta Quest...");

    // Initialize OpenXR Loader on Android
    PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR = nullptr;
    if (XR_SUCCEEDED(xrGetInstanceProcAddr(
            XR_NULL_HANDLE, 
            "xrInitializeLoaderKHR", 
            (PFN_xrVoidFunction*)&xrInitializeLoaderKHR)) && xrInitializeLoaderKHR != nullptr) {
        
        XrLoaderInitInfoAndroidKHR loaderInitInfoAndroid{XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
        loaderInitInfoAndroid.applicationVM = app->activity->vm;
        loaderInitInfoAndroid.applicationContext = app->activity->clazz;
        xrInitializeLoaderKHR((const XrLoaderInitInfoBaseHeaderKHR*)&loaderInitInfoAndroid);
    }

    if (!createInstance(app)) return false;
    if (!createSession()) return false;
    if (!createSpaces()) return false;
    if (!createSwapchain()) return false;
    if (!setupActions()) return false;

    LOGI("OpenXR VR pipeline initialized successfully!");
    return true;
}

bool QuakeVRBridge::createInstance(struct android_app* app) {
    const char* extensions[] = {
        XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME,
        "XR_KHR_android_create_instance",
        "XR_KHR_android_thread_settings"
    };

    XrInstanceCreateInfoAndroidKHR createInfoAndroid{XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
    createInfoAndroid.applicationVM = app->activity->vm;
    createInfoAndroid.applicationActivity = app->activity->clazz;

    XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
    createInfo.next = &createInfoAndroid;
    createInfo.enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]);
    createInfo.enabledExtensionNames = extensions;
    strncpy(createInfo.applicationInfo.applicationName, "QuakeVR Quest", XR_MAX_APPLICATION_NAME_SIZE);
    createInfo.applicationInfo.applicationVersion = 1;
    strncpy(createInfo.applicationInfo.engineName, "QuakeEngine", XR_MAX_ENGINE_NAME_SIZE);
    createInfo.applicationInfo.engineVersion = 1;
    createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

    XrResult res = xrCreateInstance(&createInfo, &m_instance);
    if (XR_FAILED(res)) {
        LOGE("Failed to create OpenXR Instance: %d", res);
        return false;
    }

    XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    res = xrGetSystem(m_instance, &systemInfo, &m_systemId);
    if (XR_FAILED(res)) {
        LOGE("Failed to obtain HMD System ID: %d", res);
        return false;
    }

    return true;
}

bool QuakeVRBridge::createSession() {
    PFN_xrGetOpenGLESGraphicsRequirementsKHR xrGetOpenGLESGraphicsRequirementsKHR = nullptr;
    xrGetInstanceProcAddr(m_instance, "xrGetOpenGLESGraphicsRequirementsKHR", 
                          (PFN_xrVoidFunction*)&xrGetOpenGLESGraphicsRequirementsKHR);

    if (xrGetOpenGLESGraphicsRequirementsKHR) {
        XrGraphicsRequirementsOpenGLESKHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};
        xrGetOpenGLESGraphicsRequirementsKHR(m_instance, m_systemId, &graphicsRequirements);
    }

    XrGraphicsBindingOpenGLESAndroidKHR graphicsBinding{XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};
    graphicsBinding.display = eglGetCurrentDisplay();
    graphicsBinding.config = (EGLConfig)0;
    graphicsBinding.context = eglGetCurrentContext();

    XrSessionCreateInfo sessionCreateInfo{XR_TYPE_SESSION_CREATE_INFO};
    sessionCreateInfo.next = &graphicsBinding;
    sessionCreateInfo.systemId = m_systemId;

    XrResult res = xrCreateSession(m_instance, &sessionCreateInfo, &m_session);
    if (XR_FAILED(res)) {
        LOGE("Failed to create OpenXR Session: %d", res);
        return false;
    }

    return true;
}

bool QuakeVRBridge::createSpaces() {
    XrReferenceSpaceCreateInfo spaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    spaceCreateInfo.poseInReferenceSpace.orientation.w = 1.0f;

    XrResult res = xrCreateReferenceSpace(m_session, &spaceCreateInfo, &m_appSpace);
    if (XR_FAILED(res)) {
        LOGE("Failed to create Reference Space: %d", res);
        return false;
    }

    spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    res = xrCreateReferenceSpace(m_session, &spaceCreateInfo, &m_headSpace);
    return XR_SUCCEEDED(res);
}

bool QuakeVRBridge::createSwapchain() {
    uint32_t viewCount = 0;
    xrEnumerateViewConfigurationViews(m_instance, m_systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 
                                      0, &viewCount, nullptr);
    m_configViews.resize(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
    xrEnumerateViewConfigurationViews(m_instance, m_systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 
                                      viewCount, &viewCount, m_configViews.data());

    m_width = m_configViews[0].recommendedImageRectWidth;
    m_height = m_configViews[0].recommendedImageRectHeight;

    XrSwapchainCreateInfo swapchainCreateInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
    swapchainCreateInfo.format = GL_RGBA8;
    swapchainCreateInfo.sampleCount = 1;
    swapchainCreateInfo.width = m_width * 2; // Stereo side-by-side or array
    swapchainCreateInfo.height = m_height;
    swapchainCreateInfo.faceCount = 1;
    swapchainCreateInfo.arraySize = 1;
    swapchainCreateInfo.mipCount = 1;

    XrResult res = xrCreateSwapchain(m_session, &swapchainCreateInfo, &m_swapchain);
    if (XR_FAILED(res)) {
        LOGE("Failed to create OpenXR Swapchain: %d", res);
        return false;
    }

    uint32_t imageCount = 0;
    xrEnumerateSwapchainImages(m_swapchain, 0, &imageCount, nullptr);
    m_swapchainImages.resize(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR});
    xrEnumerateSwapchainImages(m_swapchain, imageCount, &imageCount, (XrSwapchainImageBaseHeader*)m_swapchainImages.data());

    m_framebuffers.resize(imageCount);
    glGenFramebuffers(imageCount, m_framebuffers.data());
    for (uint32_t i = 0; i < imageCount; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffers[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_swapchainImages[i].image, 0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return true;
}

bool QuakeVRBridge::setupActions() {
    XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
    strncpy(actionSetInfo.actionSetName, "gameplay", XR_MAX_ACTION_SET_NAME_SIZE);
    strncpy(actionSetInfo.localizedActionSetName, "Gameplay Actions", XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE);
    actionSetInfo.priority = 0;

    XrResult res = xrCreateActionSet(m_instance, &actionSetInfo, &m_actionSet);
    if (XR_FAILED(res)) return false;

    xrStringToPath(m_instance, "/user/hand/left", &m_leftHandPath);
    xrStringToPath(m_instance, "/user/hand/right", &m_rightHandPath);

    // Aim / Hand Pose Action
    XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
    actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
    strncpy(actionInfo.actionName, "hand_pose", XR_MAX_ACTION_NAME_SIZE);
    strncpy(actionInfo.localizedActionName, "Hand Pose", XR_MAX_LOCALIZED_ACTION_NAME_SIZE);
    actionInfo.countSubactionPaths = 2;
    XrPath handPaths[] = {m_leftHandPath, m_rightHandPath};
    actionInfo.subactionPaths = handPaths;
    xrCreateAction(m_actionSet, &actionInfo, &m_handPoseAction);

    // Move stick
    actionInfo.actionType = XR_ACTION_TYPE_VECTOR2F_INPUT;
    strncpy(actionInfo.actionName, "move", XR_MAX_ACTION_NAME_SIZE);
    strncpy(actionInfo.localizedActionName, "Move", XR_MAX_LOCALIZED_ACTION_NAME_SIZE);
    actionInfo.countSubactionPaths = 0;
    actionInfo.subactionPaths = nullptr;
    xrCreateAction(m_actionSet, &actionInfo, &m_moveAction);

    // Look stick
    strncpy(actionInfo.actionName, "look", XR_MAX_ACTION_NAME_SIZE);
    strncpy(actionInfo.localizedActionName, "Look", XR_MAX_LOCALIZED_ACTION_NAME_SIZE);
    xrCreateAction(m_actionSet, &actionInfo, &m_lookAction);

    // Fire Trigger
    actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
    strncpy(actionInfo.actionName, "fire", XR_MAX_ACTION_NAME_SIZE);
    strncpy(actionInfo.localizedActionName, "Fire Weapon", XR_MAX_LOCALIZED_ACTION_NAME_SIZE);
    xrCreateAction(m_actionSet, &actionInfo, &m_fireAction);

    // Jump Button
    strncpy(actionInfo.actionName, "jump", XR_MAX_ACTION_NAME_SIZE);
    strncpy(actionInfo.localizedActionName, "Jump", XR_MAX_LOCALIZED_ACTION_NAME_SIZE);
    xrCreateAction(m_actionSet, &actionInfo, &m_jumpAction);

    // Bindings for Oculus / Meta Touch Controllers
    XrPath touchProfilePath;
    xrStringToPath(m_instance, "/interaction_profiles/oculus/touch_controller", &touchProfilePath);

    std::vector<XrActionSuggestedBinding> bindings;
    auto addBinding = [&](XrAction action, const char* pathStr) {
        XrPath path;
        xrStringToPath(m_instance, pathStr, &path);
        bindings.push_back({action, path});
    };

    addBinding(m_handPoseAction, "/user/hand/left/input/aim/pose");
    addBinding(m_handPoseAction, "/user/hand/right/input/aim/pose");
    addBinding(m_moveAction, "/user/hand/left/input/thumbstick");
    addBinding(m_lookAction, "/user/hand/right/input/thumbstick");
    addBinding(m_fireAction, "/user/hand/right/input/trigger/value");
    addBinding(m_jumpAction, "/user/hand/right/input/a/click");

    XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    suggestedBindings.interactionProfile = touchProfilePath;
    suggestedBindings.suggestedBindings = bindings.data();
    suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
    xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings);

    // Attach Action Set to Session
    XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attachInfo.countActionSets = 1;
    attachInfo.actionSets = &m_actionSet;
    xrAttachSessionActionSets(m_session, &attachInfo);

    // Create Action Spaces for Hand Tracking
    XrActionSpaceCreateInfo actionSpaceInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
    actionSpaceInfo.action = m_handPoseAction;
    actionSpaceInfo.poseInActionSpace.orientation.w = 1.0f;
    actionSpaceInfo.subactionPath = m_leftHandPath;
    xrCreateActionSpace(m_session, &actionSpaceInfo, &m_leftHandSpace);

    actionSpaceInfo.subactionPath = m_rightHandPath;
    xrCreateActionSpace(m_session, &actionSpaceInfo, &m_rightHandSpace);

    return true;
}

void QuakeVRBridge::pollEvents() {
    XrEventDataBuffer eventData{XR_TYPE_EVENT_DATA_BUFFER};
    while (xrPollEvent(m_instance, &eventData) == XR_SUCCESS) {
        if (eventData.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            auto sessionChanged = reinterpret_cast<XrEventDataSessionStateChanged*>(&eventData);
            m_sessionState = sessionChanged->state;

            if (m_sessionState == XR_SESSION_STATE_READY) {
                XrSessionBeginInfo beginInfo{XR_TYPE_SESSION_BEGIN_INFO};
                beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                xrBeginSession(m_session, &beginInfo);
                m_sessionRunning = true;
                LOGI("OpenXR Session Started");
            } else if (m_sessionState == XR_SESSION_STATE_STOPPING) {
                xrEndSession(m_session);
                m_sessionRunning = false;
                LOGI("OpenXR Session Stopped");
            }
        }
        eventData = {XR_TYPE_EVENT_DATA_BUFFER};
    }
}

void QuakeVRBridge::updateTrackingAndInput(VRInputState& inputState) {
    if (!m_sessionRunning) return;

    // Sync Action Sets
    XrActiveActionSet activeActionSet{m_actionSet, XR_NULL_PATH};
    XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
    syncInfo.countActiveActionSets = 1;
    syncInfo.activeActionSets = &activeActionSet;
    xrSyncActions(m_session, &syncInfo);

    // Read Thumbsticks
    XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
    getInfo.action = m_moveAction;
    XrActionStateVector2f moveState{XR_TYPE_ACTION_STATE_VECTOR2F};
    if (XR_SUCCEEDED(xrGetActionStateVector2f(m_session, &getInfo, &moveState)) && moveState.isActive) {
        inputState.leftStickX = moveState.currentState.x;
        inputState.leftStickY = moveState.currentState.y;
    }

    getInfo.action = m_lookAction;
    XrActionStateVector2f lookState{XR_TYPE_ACTION_STATE_VECTOR2F};
    if (XR_SUCCEEDED(xrGetActionStateVector2f(m_session, &getInfo, &lookState)) && lookState.isActive) {
        inputState.rightStickX = lookState.currentState.x;
        inputState.rightStickY = lookState.currentState.y;
    }

    // Read Trigger & Jump
    getInfo.action = m_fireAction;
    XrActionStateBoolean fireState{XR_TYPE_ACTION_STATE_BOOLEAN};
    if (XR_SUCCEEDED(xrGetActionStateBoolean(m_session, &getInfo, &fireState))) {
        inputState.triggerRight = fireState.currentState;
    }

    getInfo.action = m_jumpAction;
    XrActionStateBoolean jumpState{XR_TYPE_ACTION_STATE_BOOLEAN};
    if (XR_SUCCEEDED(xrGetActionStateBoolean(m_session, &getInfo, &jumpState))) {
        inputState.buttonA = jumpState.currentState;
    }

    // Hand Tracking Poses
    XrSpaceLocation spaceLocation{XR_TYPE_SPACE_LOCATION};
    if (XR_SUCCEEDED(xrLocateSpace(m_rightHandSpace, m_appSpace, m_frameState.predictedDisplayTime, &spaceLocation))) {
        if (spaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) {
            inputState.handRightPos[0] = spaceLocation.pose.position.x;
            inputState.handRightPos[1] = spaceLocation.pose.position.y;
            inputState.handRightPos[2] = spaceLocation.pose.position.z;
        }
        if (spaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) {
            inputState.handRightOri[0] = spaceLocation.pose.orientation.x;
            inputState.handRightOri[1] = spaceLocation.pose.orientation.y;
            inputState.handRightOri[2] = spaceLocation.pose.orientation.z;
            inputState.handRightOri[3] = spaceLocation.pose.orientation.w;
        }
    }
}

bool QuakeVRBridge::beginFrame(VREyeView eyeViews[2]) {
    if (!m_sessionRunning) return false;

    XrFrameWaitInfo waitInfo{XR_TYPE_FRAME_WAIT_INFO};
    xrWaitFrame(m_session, &waitInfo, &m_frameState);

    XrFrameBeginInfo beginInfo{XR_TYPE_FRAME_BEGIN_INFO};
    xrBeginFrame(m_session, &beginInfo);

    // Locate Views
    XrViewLocateInfo locateInfo{XR_TYPE_VIEW_LOCATE_INFO};
    locateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    locateInfo.displayTime = m_frameState.predictedDisplayTime;
    locateInfo.space = m_appSpace;

    uint32_t viewCount = 2;
    XrViewState viewState{XR_TYPE_VIEW_STATE};
    xrLocateViews(m_session, &locateInfo, &viewState, viewCount, &viewCount, m_views.data());

    // Acquire swapchain image
    uint32_t imageIndex = 0;
    XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    xrAcquireSwapchainImage(m_swapchain, &acquireInfo, &imageIndex);

    XrSwapchainImageWaitInfo waitImageInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    waitImageInfo.timeout = XR_INFINITE_DURATION;
    xrWaitSwapchainImage(m_swapchain, &waitImageInfo);

    for (int eye = 0; eye < 2; ++eye) {
        eyeViews[eye].width = m_width;
        eyeViews[eye].height = m_height;
        eyeViews[eye].textureId = m_swapchainImages[imageIndex].image;
        eyeViews[eye].fboId = m_framebuffers[imageIndex];
    }

    return true;
}

void QuakeVRBridge::endFrame() {
    if (!m_sessionRunning) return;

    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    xrReleaseSwapchainImage(m_swapchain, &releaseInfo);

    std::array<XrCompositionLayerProjectionView, 2> projectionViews{};
    for (int eye = 0; eye < 2; ++eye) {
        projectionViews[eye].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
        projectionViews[eye].pose = m_views[eye].pose;
        projectionViews[eye].fov = m_views[eye].fov;
        projectionViews[eye].subImage.swapchain = m_swapchain;
        projectionViews[eye].subImage.imageRect.offset = {eye * (int32_t)m_width, 0};
        projectionViews[eye].subImage.imageRect.extent = {(int32_t)m_width, (int32_t)m_height};
        projectionViews[eye].subImage.imageArrayIndex = 0;
    }

    XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    layer.space = m_appSpace;
    layer.viewCount = 2;
    layer.views = projectionViews.data();

    const XrCompositionLayerBaseHeader* layers[] = {
        reinterpret_cast<const XrCompositionLayerBaseHeader*>(&layer)
    };

    XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
    endInfo.displayTime = m_frameState.predictedDisplayTime;
    endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    endInfo.layerCount = 1;
    endInfo.layers = layers;

    xrEndFrame(m_session, &endInfo);
}

void QuakeVRBridge::shutdownOpenXR() {
    if (m_swapchain) {
        xrDestroySwapchain(m_swapchain);
        m_swapchain = XR_NULL_HANDLE;
    }
    if (m_session) {
        xrDestroySession(m_session);
        m_session = XR_NULL_HANDLE;
    }
    if (m_instance) {
        xrDestroyInstance(m_instance);
        m_instance = XR_NULL_HANDLE;
    }
    m_sessionRunning = false;
}
