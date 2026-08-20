#pragma once

#if defined(__ANDROID__) || defined(ANDROID)
    #include <unistd.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <math.h>
    #include <GLES3/gl32.h>
    #include <GLES3/gl3ext.h>
    #include <EGL/egl.h>
    #include <EGL/eglext.h>

    #ifndef GLM_FORCE_SWIZZLE
    #define GLM_FORCE_SWIZZLE
    #endif
    #ifndef GLM_ENABLE_EXPERIMENTAL
    #define GLM_ENABLE_EXPERIMENTAL
    #endif

    #ifdef __cplusplus
    extern "C" {
    #endif

    // Desktop OpenGL type aliases
    #ifndef GLdouble
    typedef double GLdouble;
    #endif
    #ifndef GLclampd
    typedef double GLclampd;
    #endif

    // Depth Range alias for GLES3
    #define glDepthRange(n, f) glDepthRangef((GLfloat)(n), (GLfloat)(f))

    // ARB Buffer & Texture aliases to standard GLES3
    #define glBindBufferARB glBindBuffer
    #define glDeleteBuffersARB glDeleteBuffers
    #define glGenBuffersARB glGenBuffers
    #define glIsBufferARB glIsBuffer
    #define glBufferDataARB glBufferData
    #define glBufferSubDataARB glBufferSubData
    #define glGetBufferSubDataARB glGetBufferSubData
    #define glMapBufferARB glMapBufferRange
    #define glUnmapBufferARB glUnmapBuffer
    #define glGetBufferParameterivARB glGetBufferParameteriv
    #define glGetBufferPointervARB glGetBufferPointerv
    #define glActiveTextureARB glActiveTexture
    #define glClientActiveTextureARB glActiveTexture
    #define glMultiTexCoord2fARB(target, s, t)

    #define GL_TEXTURE0_ARB 0x84C0
    #define GL_TEXTURE1_ARB 0x84C1
    #define GL_TEXTURE2_ARB 0x84C2
    #define GL_TEXTURE3_ARB 0x84C3
    #define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE

    // Desktop glGetTexImage stub for GLES3
    inline void glGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, void* pixels) {
        (void)target; (void)level; (void)format; (void)type; (void)pixels;
    }

    // Legacy OpenGL Immediate Mode & Matrix Constants
    #define GL_QUADS 0x0007
    #define GL_QUAD_STRIP 0x0008
    #define GL_POLYGON 0x0009
    #define GL_MODELVIEW 0x1700
    #define GL_PROJECTION 0x1701
    #define GL_TEXTURE 0x1702
    #define GL_MATRIX_MODE 0x0BA0
    #define GL_MODELVIEW_MATRIX 0x0BA6
    #define GL_PROJECTION_MATRIX 0x0BA7
    #define GL_TEXTURE_MATRIX 0x0BA8
    #define GL_FOG 0x0B60
    #define GL_FOG_MODE 0x0B65
    #define GL_FOG_DENSITY 0x0B62
    #define GL_FOG_COLOR 0x0B66
    #define GL_FOG_INDEX 0x0B61
    #define GL_FOG_START 0x0B63
    #define GL_FOG_END 0x0B64
    #define GL_EXP 0x0800
    #define GL_EXP2 0x0801
    #define GL_ALPHA_TEST 0x0BC0
    #define GL_GREATER 0x0204
    #define GL_TEXTURE_ENV 0x2300
    #define GL_TEXTURE_ENV_MODE 0x2200
    #define GL_MODULATE 0x2100
    #define GL_DECAL 0x2101
    #define GL_REPLACE 0x1E01
    #define GL_ADD 0x0104
    #define GL_PERSPECTIVE_CORRECTION_HINT 0x0C50
    #define GL_GENERATE_MIPMAP 0x8191

    // Shading, Color Format, and Polygon Offset constants
    #define GL_FLAT 0x1D00
    #define GL_SMOOTH 0x1D01
    #define GL_BGRA 0x80E1
    #define GL_POLYGON_OFFSET_LINE 0x2A02
    #define GL_POLYGON_OFFSET_POINT 0x2A01
    #define GL_POINT_SMOOTH 0x0B10
    #define GL_LINE_SMOOTH 0x0B20

    // Texture Formats and Compressed Formats
    #define GL_UNSIGNED_INT_8_8_8_8_REV 0x8367
    #define GL_UNSIGNED_INT_8_8_8_8 0x8035
    #define GL_LUMINANCE8 0x8040
    #define GL_LUMINANCE8_ALPHA8 0x8045
    #define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
    #define GL_COMPRESSED_RGB_S3TC_DXT1_EXT 0x83F0
    #define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
    #define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
    #define GL_COMPRESSED_RED_RGTC1 0x8DBB
    #define GL_COMPRESSED_SIGNED_RED_RGTC1 0x8DBC
    #define GL_COMPRESSED_RG_RGTC2 0x8DBD
    #define GL_COMPRESSED_SIGNED_RG_RGTC2 0x8DBE
    #define GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT 0x8E8E
    #define GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT 0x8E8F
    #define GL_COMPRESSED_RGBA_BPTC_UNORM 0x8E8C
    #define GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM 0x8E8D
    #define GL_COMPRESSED_RGBA_ASTC_4x4_KHR 0x93B0
    #define GL_COMPRESSED_RGBA_ASTC_5x4_KHR 0x93B1
    #define GL_COMPRESSED_RGBA_ASTC_5x5_KHR 0x93B2
    #define GL_COMPRESSED_RGBA_ASTC_6x5_KHR 0x93B3
    #define GL_COMPRESSED_RGBA_ASTC_6x6_KHR 0x93B4
    #define GL_COMPRESSED_RGBA_ASTC_8x5_KHR 0x93B5
    #define GL_COMPRESSED_RGBA_ASTC_8x6_KHR 0x93B6
    #define GL_COMPRESSED_RGBA_ASTC_8x8_KHR 0x93B7
    #define GL_COMPRESSED_RGBA_ASTC_10x5_KHR 0x93B8
    #define GL_COMPRESSED_RGBA_ASTC_10x6_KHR 0x93B9
    #define GL_COMPRESSED_RGBA_ASTC_10x8_KHR 0x93BA
    #define GL_COMPRESSED_RGBA_ASTC_10x10_KHR 0x93BB
    #define GL_COMPRESSED_RGBA_ASTC_12x10_KHR 0x93BC
    #define GL_COMPRESSED_RGBA_ASTC_12x12_KHR 0x93BD

    // Legacy Stubs for Immediate Mode Calls
    inline void glBegin(GLenum mode) { (void)mode; }
    inline void glEnd(void) {}
    inline void glVertex2f(GLfloat x, GLfloat y) { (void)x; (void)y; }
    inline void glVertex3f(GLfloat x, GLfloat y, GLfloat z) { (void)x; (void)y; (void)z; }
    inline void glVertex3fv(const GLfloat* v) { (void)v; }
    inline void glTexCoord2f(GLfloat s, GLfloat t) { (void)s; (void)t; }
    inline void glTexCoord2fv(const GLfloat* v) { (void)v; }
    inline void glColor3f(GLfloat r, GLfloat g, GLfloat b) { (void)r; (void)g; (void)b; }
    inline void glColor3fv(const GLfloat* v) { (void)v; }
    inline void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { (void)r; (void)g; (void)b; (void)a; }
    inline void glColor4fv(const GLfloat* v) { (void)v; }
    inline void glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a) { (void)r; (void)g; (void)b; (void)a; }
    inline void glColor4ubv(const GLubyte* v) { (void)v; }
    inline void glNormal3f(GLfloat x, GLfloat y, GLfloat z) { (void)x; (void)y; (void)z; }
    inline void glNormal3fv(const GLfloat* v) { (void)v; }
    inline void glMatrixMode(GLenum mode) { (void)mode; }
    inline void glPushMatrix(void) {}
    inline void glPopMatrix(void) {}
    inline void glLoadIdentity(void) {}
    inline void glLoadMatrixf(const GLfloat* m) { (void)m; }
    inline void glMultMatrixf(const GLfloat* m) { (void)m; }
    inline void glTranslatef(GLfloat x, GLfloat y, GLfloat z) { (void)x; (void)y; (void)z; }
    inline void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) { (void)angle; (void)x; (void)y; (void)z; }
    inline void glScalef(GLfloat x, GLfloat y, GLfloat z) { (void)x; (void)y; (void)z; }
    inline void glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) { (void)l; (void)r; (void)b; (void)t; (void)n; (void)f; }
    inline void glFrustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) { (void)l; (void)r; (void)b; (void)t; (void)n; (void)f; }
    inline void glAlphaFunc(GLenum func, GLclampf ref) { (void)func; (void)ref; }
    inline void glTexEnvf(GLenum target, GLenum pname, GLfloat param) { (void)target; (void)pname; (void)param; }
    inline void glTexEnvi(GLenum target, GLenum pname, GLint param) { (void)target; (void)pname; (void)param; }
    inline void glFogf(GLenum pname, GLfloat param) { (void)pname; (void)param; }
    inline void glFogi(GLenum pname, GLint param) { (void)pname; (void)param; }
    inline void glFogfv(GLenum pname, const GLfloat* params) { (void)pname; (void)params; }
    inline void glShadeModel(GLenum mode) { (void)mode; }
    inline void glPolygonMode(GLenum face, GLenum mode) { (void)face; (void)mode; }
    inline void glPointSize(GLfloat size) { (void)size; }
    #define GL_POINT 0x1B00
    #define GL_LINE 0x1B01
    #define GL_FILL 0x1B02

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

    #ifdef __cplusplus
    }
    #endif
#else
    #include_next <GL/glew.h>
#endif
