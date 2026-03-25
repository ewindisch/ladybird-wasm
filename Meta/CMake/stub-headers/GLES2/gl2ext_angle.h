/*
 * Stub GLES2/gl2ext_angle.h for EMSCRIPTEN/WASM builds.
 * Defines ANGLE-specific robust GL function stubs. WebGL is not used in the
 * 2D-canvas-only WASM build; these exist only to allow compilation.
 */

#pragma once

#include <GLES2/gl2ext.h>
#include <GLES3/gl31.h>

#ifdef __cplusplus
extern "C" {
#endif

/* GL_ANGLE_robust_client_memory function stubs */

static inline void glGetBooleanvRobustANGLE(GLenum pname, GLsizei bufSize, GLsizei* length, GLboolean* data) { (void)bufSize; (void)length; glGetBooleanv(pname, data); }
static inline void glGetIntegervRobustANGLE(GLenum pname, GLsizei bufSize, GLsizei* length, GLint* data) { (void)bufSize; (void)length; glGetIntegerv(pname, data); }
static inline void glGetFloatvRobustANGLE(GLenum pname, GLsizei bufSize, GLsizei* length, GLfloat* data) { (void)bufSize; (void)length; glGetFloatv(pname, data); }
static inline void glGetInteger64vRobustANGLE(GLenum pname, GLsizei bufSize, GLsizei* length, GLint64* data) { (void)bufSize; (void)length; glGetInteger64v(pname, data); }
static inline void glGetShaderivRobustANGLE(GLuint shader, GLenum pname, GLsizei bufSize, GLsizei* length, GLint* params) { (void)bufSize; (void)length; glGetShaderiv(shader, pname, params); }
static inline void glGetProgramivRobustANGLE(GLuint program, GLenum pname, GLsizei bufSize, GLsizei* length, GLint* params) { (void)bufSize; (void)length; glGetProgramiv(program, pname, params); }
static inline void glGetRenderbufferParameterivRobustANGLE(GLenum target, GLenum pname, GLsizei bufSize, GLsizei* length, GLint* params) { (void)bufSize; (void)length; glGetRenderbufferParameteriv(target, pname, params); }
static inline void glGetTexParameterfvRobustANGLE(GLenum target, GLenum pname, GLsizei bufSize, GLsizei* length, GLfloat* params) { (void)bufSize; (void)length; glGetTexParameterfv(target, pname, params); }
static inline void glGetTexParameterivRobustANGLE(GLenum target, GLenum pname, GLsizei bufSize, GLsizei* length, GLint* params) { (void)bufSize; (void)length; glGetTexParameteriv(target, pname, params); }
static inline void glGetVertexAttribfvRobustANGLE(GLuint index, GLenum pname, GLsizei bufSize, GLsizei* length, GLfloat* params) { (void)bufSize; (void)length; glGetVertexAttribfv(index, pname, params); }
static inline void glGetVertexAttribivRobustANGLE(GLuint index, GLenum pname, GLsizei bufSize, GLsizei* length, GLint* params) { (void)bufSize; (void)length; glGetVertexAttribiv(index, pname, params); }
static inline void glGetVertexAttribPointervRobustANGLE(GLuint index, GLenum pname, GLsizei bufSize, GLsizei* length, void** pointer) { (void)bufSize; (void)length; glGetVertexAttribPointerv(index, pname, pointer); }
static inline void glReadPixelsRobustANGLE(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, GLsizei* length, GLsizei* columns, GLsizei* rows, void* pixels) { (void)bufSize; (void)length; (void)columns; (void)rows; glReadPixels(x, y, width, height, format, type, pixels); }
static inline void glGetBufferParameterivRobustANGLE(GLenum target, GLenum pname, GLsizei bufSize, GLsizei* length, GLint* params) { (void)bufSize; (void)length; glGetBufferParameteriv(target, pname, params); }

/* Texture functions with buffer size */
static inline void glTexImage2DRobustANGLE(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, GLsizei dataSize, const void* pixels) { (void)dataSize; glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels); }
static inline void glTexSubImage2DRobustANGLE(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei dataSize, const void* pixels) { (void)dataSize; glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels); }
static inline void glCompressedTexImage2DRobustANGLE(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, GLsizei dataSize, const void* data) { (void)dataSize; glCompressedTexImage2D(target, level, internalformat, width, height, border, imageSize, data); }
static inline void glCompressedTexSubImage2DRobustANGLE(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, GLsizei dataSize, const void* data) { (void)dataSize; glCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, data); }

/* WebGL2 / GLES3 robust variants */
static inline void glTexImage3DRobustANGLE(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, GLsizei dataSize, const void* pixels) { (void)dataSize; glTexImage3D(target, level, internalformat, width, height, depth, border, format, type, pixels); }
static inline void glTexSubImage3DRobustANGLE(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, GLsizei dataSize, const void* pixels) { (void)dataSize; glTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels); }
static inline void glCompressedTexImage3DRobustANGLE(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, GLsizei dataSize, const void* data) { (void)dataSize; glCompressedTexImage3D(target, level, internalformat, width, height, depth, border, imageSize, data); }
static inline void glCompressedTexSubImage3DRobustANGLE(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, GLsizei dataSize, const void* data) { (void)dataSize; glCompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data); }

/* GLES3 robust variants */
static inline void glGetInternalformativRobustANGLE(GLenum target, GLenum internalformat, GLenum pname, GLsizei bufSize, GLsizei* length, GLint* params) { (void)length; glGetInternalformativ(target, internalformat, pname, bufSize, params); }
static inline void glGetActiveUniformBlockivRobustANGLE(GLuint program, GLuint uniformBlockIndex, GLenum pname, GLsizei bufSize, GLsizei* length, GLint* params) { (void)bufSize; (void)length; glGetActiveUniformBlockiv(program, uniformBlockIndex, pname, params); }
static inline void glGetQueryObjectuivRobustANGLE(GLuint id, GLenum pname, GLsizei bufSize, GLsizei* length, GLuint* params) { (void)bufSize; (void)length; glGetQueryObjectuiv(id, pname, params); }

/* ANGLE extension management */
static inline void glRequestExtensionANGLE(const char* name) { (void)name; }
static inline void glWaitUntilWorkScheduledANGLE(void) {}

#ifdef __cplusplus
}
#endif
