cmake_minimum_required(VERSION 3.22.1)
project(quakevr_quest3 C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# Compiler flags for Snapdragon XR2 Gen 2 (ARM64-v8a)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -ffast-math -flto -fexceptions -frtti")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -ffast-math -flto")

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/vr
    ${CMAKE_CURRENT_SOURCE_DIR}/engine
    ${ANDROID_NDK}/sources/android/native_app_glue
)

# OpenXR Loader Prebuilt Integration
add_library(openxr_loader SHARED IMPORTED)
set_target_properties(openxr_loader PROPERTIES IMPORTED_LOCATION
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/${ANDROID_ABI}/libopenxr_loader.so
)

# Android native glue
add_library(native_app_glue STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)

# Main Native QuakeVR Executable / Shared Library
add_library(quakevr SHARED
    android_main.cpp
    vr/quakevr_bridge.cpp
    # Quake engine sources will be included here:
    # engine/gl_draw.c
    # engine/gl_mesh.c
    # engine/gl_rmain.c
    # engine/host.c
    # engine/cl_input.c
    # engine/vr_physics.cpp
)

# System and VR Libraries
target_link_libraries(quakevr
    native_app_glue
    openxr_loader
    GLESv3
    EGL
    android
    log
    m
    z
)
