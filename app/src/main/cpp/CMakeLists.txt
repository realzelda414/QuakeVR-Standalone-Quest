cmake_minimum_required(VERSION 3.22.1)
project(quakevr C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 20)

# Anchor to repository root (4 levels up from app/src/main/cpp/)
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../" ABSOLUTE)

add_definitions(
    -DANDROID=1
    -D__ANDROID__=1
    -DXR_USE_PLATFORM_ANDROID=1
    -DXR_USE_GRAPHICS_API_OPENGL_ES=1
    -DUSE_OPENGL=1
    -DUSE_GLES3=1
    -DQUAKE_VR=1
    -DQUAKESPASM=1
)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -ffast-math -flto")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -O3 -ffast-math -flto -fexceptions -frtti")

# Include directories from root Quake folder, vr bridge, and android headers
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/vr
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${REPO_ROOT}/Quake
    ${REPO_ROOT}/src
    ${ANDROID_NDK}/sources/android/native_app_glue
)

# OpenXR Prebuilt Loader
add_library(openxr_loader SHARED IMPORTED)
set_target_properties(openxr_loader PROPERTIES
    IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/libs/${ANDROID_ABI}/libopenxr_loader.so
)

# Native App Glue
add_library(native_app_glue STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)

# Pull all C and C++ source files from the root Quake/ directory
file(GLOB_RECURSE QUAKE_ROOT_SOURCES
    "${REPO_ROOT}/Quake/*.c"
    "${REPO_ROOT}/Quake/*.cpp"
)

# Exclude desktop-only platform backends (e.g. desktop SDL/Windows drivers)
list(FILTER QUAKE_ROOT_SOURCES EXCLUDE REGEX "sys_sdl\\.c$")
list(FILTER QUAKE_ROOT_SOURCES EXCLUDE REGEX "sys_win\\.c$")
list(FILTER QUAKE_ROOT_SOURCES EXCLUDE REGEX "sys_win\\.cpp$")
list(FILTER QUAKE_ROOT_SOURCES EXCLUDE REGEX "cd_win\\.c$")
list(FILTER QUAKE_ROOT_SOURCES EXCLUDE REGEX "in_win\\.c$")

# Build the main QuakeVR Quest shared library
add_library(quakevr SHARED
    ${QUAKE_ROOT_SOURCES}
    android_main.cpp
    vr/quakevr_bridge.cpp
)

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
