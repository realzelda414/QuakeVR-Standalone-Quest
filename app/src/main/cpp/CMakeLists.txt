cmake_minimum_required(VERSION 3.22.1)
project(quakevr C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)

# Engine Defines for Standalone Quest GLES 3.2 + OpenXR
add_definitions(
    -D__ANDROID__=1
    -DANDROID=1
    -DQUAKESPASM=1
    -DQUAKE_VR=1
    -DUSE_OPENGL=1
    -DUSE_GLES3=1
    -DXR_USE_PLATFORM_ANDROID=1
    -DXR_USE_GRAPHICS_API_OPENGL_ES=1
    -DGLEW_STATIC=1
    -DGLEW_NO_GLU=1
    -DGLM_FORCE_SWIZZLE=1
    -DGLM_ENABLE_EXPERIMENTAL=1
)

# Compiler flags for release performance on Quest ARM64
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -ffast-math -flto -fexceptions -frtti -Wno-format-security -Wno-error -Wno-deprecated-volatile")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -ffast-math -flto -Wno-format-security -Wno-error")

# Locate Quake and Project Root Paths
set(PROJ_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../..)
set(QUAKE_SRC_DIR ${PROJ_ROOT}/Quake)

# Include Directories (Including GLM and all headers)
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vr
    ${QUAKE_SRC_DIR}
    ${PROJ_ROOT}
    ${PROJ_ROOT}/include
    ${PROJ_ROOT}/src
    ${ANDROID_NDK}/sources/android/native_app_glue
)

# Android Native App Glue Library
add_library(native_app_glue STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)

# Collect all Quakespasm / QuakeVR Core Engine Sources
file(GLOB QUAKE_SOURCES
    ${QUAKE_SRC_DIR}/*.c
    ${QUAKE_SRC_DIR}/*.cpp
)

# Exclude desktop SDL video/sound sources (handled by android_sys.cpp & Quest OpenXR)
list(FILTER QUAKE_SOURCES EXCLUDE REGEX ".*gl_vidsdl\\.cpp$")
list(FILTER QUAKE_SOURCES EXCLUDE REGEX ".*snd_sdl\\.cpp$")
list(FILTER QUAKE_SOURCES EXCLUDE REGEX ".*sys_sdl\\.cpp$")
list(FILTER QUAKE_SOURCES EXCLUDE REGEX ".*cd_sdl\\.cpp$")

# Prebuilt OpenXR Loader from Quest SDK
add_library(openxr_loader SHARED IMPORTED)
set_target_properties(openxr_loader PROPERTIES
    IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/libs/${ANDROID_ABI}/libopenxr_loader.so
)

# Main Native Shared Library
add_library(quakevr SHARED
    ${QUAKE_SOURCES}
    android_main.cpp
    android_sys.cpp
    vr/quakevr_bridge.cpp
)

# Link Libraries
target_link_libraries(quakevr
    PRIVATE
    native_app_glue
    openxr_loader
    android
    log
    EGL
    GLESv3
    m
    z
    atomic
)
