cmake_minimum_required(VERSION 3.22.1)

project(quakevr C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define Android and OpenGL ES OpenXR platform flags globally
add_definitions(
    -DXR_USE_PLATFORM_ANDROID=1
    -DXR_USE_GRAPHICS_API_OPENGL_ES=1
)

# Optimization flags for Snapdragon XR2 (Quest 3)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -ffast-math -flto")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -ffast-math -flto -fexceptions -frtti")

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/vr
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${ANDROID_NDK}/sources/android/native_app_glue
)

# Native App Glue static library
add_library(native_app_glue STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)

# Prebuilt OpenXR Loader library for arm64-v8a
add_library(openxr_loader SHARED IMPORTED)
set_target_properties(openxr_loader PROPERTIES
    IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/libs/${ANDROID_ABI}/libopenxr_loader.so
)

# QuakeVR Core Shared Library (Only real existing files)
add_library(quakevr SHARED
    android_main.cpp
    vr/quakevr_bridge.cpp
)

# Link all required Android system libraries, EGL/GLESv3, and OpenXR
target_link_libraries(quakevr
    native_app_glue
    openxr_loader
    android
    log
    EGL
    GLESv3
    m
    z
)
