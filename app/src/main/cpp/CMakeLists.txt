cmake_minimum_required(VERSION 3.22.1)
project(quakevr C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

# Automatically download header-only GLM for Quest vector/matrix math
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 0.9.9.8
)
FetchContent_MakeAvailable(glm)

# Anchor to repository root
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
    -DGLM_ENABLE_EXPERIMENTAL=1
    -DGLM_FORCE_SWIZZLE=1
    -DGLEW_STATIC=1
    -DGLEW_NO_GLU=1
)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -include unistd.h -O3 -ffast-math -flto -Wno-format-security -Wno-error")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -include unistd.h -O3 -ffast-math -flto -fexceptions -frtti -Wno-format-security -Wno-error -Wno-deprecated-volatile")

# Include paths - local Quest compatibility headers first
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/vr
    ${REPO_ROOT}/Quake
    ${REPO_ROOT}/src
    ${glm_SOURCE_DIR}
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

# Exclude desktop, Windows-only backends, and directly-included source units
list(FILTER QUAKE_ROOT_SOURCES EXCLUDE REGEX "lodepng\\.(c|cpp)$")
list(FILTER QUAKE_ROOT_SOURCES EXCLUDE REGEX "sys_sdl.*\\.(c|cpp)$")
list(FILTER QUAKE_ROOT_SOURCES EXCLUDE REGEX "cd_sdl.*\\.(c|cpp)$")
list(FILTER QUAKE_ROOT_SOURCES EXCLUDE REGEX "snd_sdl.*\\.(c|cpp)$")
list(FILTER QUAKE_ROOT_SOURCES EXCLUDE REGEX "snd_voip.*\\.(c|cpp)$")
list(FILTER QUAKE_ROOT_SOURCES EXCLUDE REGEX "in_sdl.*\\.(c|cpp)$")
list(FILTER QUAKE_ROOT_SOURCES EXCLUDE REGEX "gl_vidsdl.*\\.(c|cpp)$")
list(FILTER QUAKE_ROOT_SOURCES EXCLUDE REGEX "vid_sdl.*\\.(c|cpp)$")
list(FILTER QUAKE_ROOT_SOURCES EXCLUDE REGEX "pl_linux.*\\.(c|cpp)$")
list(FILTER QUAKE_ROOT_SOURCES EXCLUDE REGEX "pl_win.*\\.(c|cpp)$")
list(FILTER QUAKE_ROOT_SOURCES EXCLUDE REGEX "sys_win.*\\.(c|cpp)$")
list(FILTER QUAKE_ROOT_SOURCES EXCLUDE REGEX "cd_win.*\\.(c|cpp)$")
list(FILTER QUAKE_ROOT_SOURCES EXCLUDE REGEX "in_win.*\\.(c|cpp)$")
list(FILTER QUAKE_ROOT_SOURCES EXCLUDE REGEX "vid_win.*\\.(c|cpp)$")
list(FILTER QUAKE_ROOT_SOURCES EXCLUDE REGEX "snd_win.*\\.(c|cpp)$")
list(FILTER QUAKE_ROOT_SOURCES EXCLUDE REGEX "net_win.*\\.(c|cpp)$")
list(FILTER QUAKE_ROOT_SOURCES EXCLUDE REGEX "net_wins.*\\.(c|cpp)$")
list(FILTER QUAKE_ROOT_SOURCES EXCLUDE REGEX "net_wipx.*\\.(c|cpp)$")

# Build the main QuakeVR Quest shared library
add_library(quakevr SHARED
    ${QUAKE_ROOT_SOURCES}
    android_main.cpp
    android_sys.cpp
    vr/quakevr_bridge.cpp
)

target_link_libraries(quakevr
    glm::glm
    native_app_glue
    openxr_loader
    android
    log
    EGL
    GLESv3
    m
    z
)
