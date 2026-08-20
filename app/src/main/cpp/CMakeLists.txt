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

# Absolute path to repository root: QuakeVR-Standalone-Quest/
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
set(QUAKE_SRC_DIR "${REPO_ROOT}/Quake")

# Include Directories
include_directories(
    ${REPO_ROOT}
    ${REPO_ROOT}/glm
    ${QUAKE_SRC_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vr
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

# Exclude all Windows desktop and SDL desktop sources
list(FILTER QUAKE_SOURCES EXCLUDE REGEX ".*_win\\.(c|cpp)$")
list(FILTER QUAKE_SOURCES EXCLUDE REGEX ".*_win32\\.(c|cpp)$")
list(FILTER QUAKE_SOURCES EXCLUDE REGEX ".*_sdl.*\\.(c|cpp)$")
list(FILTER QUAKE_SOURCES EXCLUDE REGEX ".*gl_vidsdl\\.(c|cpp)$")
list(FILTER QUAKE_SOURCES EXCLUDE REGEX ".*snd_sdl\\.(c|cpp)$")
list(FILTER QUAKE_SOURCES EXCLUDE REGEX ".*sys_sdl\\.(c|cpp)$")
list(FILTER QUAKE_SOURCES EXCLUDE REGEX ".*cd_sdl\\.(c|cpp)$")
list(FILTER QUAKE_SOURCES EXCLUDE REGEX ".*in_sdl\\.(c|cpp)$")
list(FILTER QUAKE_SOURCES EXCLUDE REGEX ".*net_sdl\\.(c|cpp)$")
list(FILTER QUAKE_SOURCES EXCLUDE REGEX ".*net_wins\\.(c|cpp)$")
list(FILTER QUAKE_SOURCES EXCLUDE REGEX ".*net_wipx\\.(c|cpp)$")

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
