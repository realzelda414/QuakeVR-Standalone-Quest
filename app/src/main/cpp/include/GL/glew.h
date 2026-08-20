#pragma once

#if defined(__ANDROID__) || defined(ANDROID)
    #include <GLES3/gl32.h>
    #include <GLES3/gl3ext.h>
    #include <EGL/egl.h>
    #include <EGL/eglext.h>

    // Desktop OpenGL type aliases for OpenGL ES 3.2
    #ifndef GLdouble
    typedef double GLdouble;
    #endif

    #ifndef GLclampd
    typedef double GLclampd;
    #endif

    // GLEW dummy constants / macros for GLES3
    #define GLEW_OK 0
    #define GLEW_NO_ERROR 0
    typedef int GLenum_glew;

    inline int glewInit(void) { return GLEW_OK; }
    inline const char* glewGetErrorString(int) { return "No error"; }
    inline int glewIsSupported(const char*) { return 1; }

    #define GLEW_VERSION_1_1 1
    #define GLEW_VERSION_1_2 1
    #define GLEW_VERSION_1_3 1
    #define GLEW_VERSION_1_4 1
    #define GLEW_VERSION_1_5 1
    #define GLEW_VERSION_2_0 1
    #define GLEW_VERSION_3_0 1
    #define GLEW_ARB_multitexture 1
    #define GLEW_ARB_texture_compression 1
    #define GLEW_EXT_texture_filter_anisotropic 1
#else
    #include_next <GL/glew.h>
#endif