# QuakeVR Meta Quest 3 Porting Package

This package contains the complete Android NDK & OpenXR bridge required to port Vittorio Romeo's QuakeVR PCVR mod to the standalone Meta Quest 3 (Snapdragon XR2 Gen 2).

## Included Files:
- `app/src/main/cpp/vr/quakevr_bridge.hpp` & `quakevr_bridge.cpp`: OpenXR subsystem & OpenVR translation layer
- `app/src/main/cpp/android_main.cpp`: NativeActivity lifecycle & EGL context
- `app/src/main/cpp/CMakeLists.txt`: NDK build configuration targeting arm64-v8a
- `app/src/main/AndroidManifest.xml`: Meta Horizon OS VR manifest
- `app/build.gradle.kts`: Gradle configuration
- `app/src/main/shaders/multiview_shader.vert`: GL_OVR_multiview2 single-pass shader

## Quick Start:
1. Open this directory in Android Studio.
2. Ensure Android NDK (r25+) is installed.
3. Build the APK: `./gradlew assembleRelease`
4. Install to Quest 3: `adb install -r app/build/outputs/apk/release/app-release.apk`
5. Push Quake files: `adb push PAK0.PAK /sdcard/QuakeVR/id1/`
