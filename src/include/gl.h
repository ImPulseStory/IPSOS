#ifndef IPSOS_GL_H
#define IPSOS_GL_H

#include <stdint.h>
#include "virgl.h"

typedef uint32_t GLenum;
typedef uint32_t GLuint;
typedef int32_t GLint;
typedef int32_t GLsizei;
typedef int32_t GLsizeiptr;
typedef int32_t GLintptr;
typedef uint32_t GLbitfield;
typedef uint8_t GLboolean;
typedef float GLfloat;
typedef double GLdouble;

#define GL_FALSE 0u
#define GL_TRUE 1u
#define GL_NO_ERROR 0u
#define GL_INVALID_ENUM 0x0500u
#define GL_INVALID_VALUE 0x0501u
#define GL_INVALID_OPERATION 0x0502u
#define GL_OUT_OF_MEMORY 0x0505u
#define GL_COLOR_BUFFER_BIT 0x00004000u
#define GL_DEPTH_BUFFER_BIT 0x00000100u
#define GL_DEPTH_TEST 0x0B71u
#define GL_NEVER 0x0200u
#define GL_LESS 0x0201u
#define GL_EQUAL 0x0202u
#define GL_LEQUAL 0x0203u
#define GL_GREATER 0x0204u
#define GL_NOTEQUAL 0x0205u
#define GL_GEQUAL 0x0206u
#define GL_ALWAYS 0x0207u

#define GL_ARRAY_BUFFER 0x8892u
#define GL_ELEMENT_ARRAY_BUFFER 0x8893u
#define GL_STATIC_DRAW 0x88E4u
#define GL_DYNAMIC_DRAW 0x88E8u
#define GL_STREAM_DRAW 0x88E0u
#define GL_FLOAT 0x1406u
#define GL_UNSIGNED_BYTE 0x1401u
#define GL_UNSIGNED_SHORT 0x1403u
#define GL_UNSIGNED_INT 0x1405u
#define GL_POINTS 0x0000u
#define GL_LINES 0x0001u
#define GL_LINE_LOOP 0x0002u
#define GL_LINE_STRIP 0x0003u
#define GL_TRIANGLES 0x0004u
#define GL_TRIANGLE_STRIP 0x0005u
#define GL_TRIANGLE_FAN 0x0006u
#define GL_VERTEX_SHADER 0x8B31u
#define GL_FRAGMENT_SHADER 0x8B30u
#define GL_COMPILE_STATUS 0x8B81u
#define GL_LINK_STATUS 0x8B82u

/* IPSOS currently implements a VirGL-backed OpenGL-style subset. */
int glInit(void);
int glIsReady(void);
void glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void glClear(GLbitfield mask);
void glClearDepth(GLdouble depth);
void glEnable(GLenum capability);
void glDisable(GLenum capability);
void glDepthFunc(GLenum function);
void glDepthMask(GLboolean enabled);
void glViewport(GLint x, GLint y, GLsizei width, GLsizei height);
void glFlush(void);
GLenum glGetError(void);

/* Existing low-level VirGL bridge. */
int glCreateContext(uint32_t context_id, const char *name);
int glDestroyContext(uint32_t context_id);
int glCreateResource3D(uint32_t resource_id, uint32_t target, uint32_t format, uint32_t bind, uint32_t width, uint32_t height, uint32_t depth);
int glAttachResource(uint32_t context_id, uint32_t resource_id);
int glSubmitCommands(uint32_t context_id, const uint32_t *dwords, uint32_t count);

/* Buffer objects and vertex attributes. Pointer in glVertexAttribPointer is a byte offset. */
void glGenBuffers(GLsizei count, GLuint *buffers);
void glBindBuffer(GLenum target, GLuint buffer);
void glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data);
void glEnableVertexAttribArray(GLuint index);
void glDisableVertexAttribArray(GLuint index);
void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);

/* VirGL consumes TGSI shader text, not GLSL. This API name is intentionally explicit. */
GLuint glCreateShader(GLenum type);
int glShaderSourceTGSI(GLuint shader, const char *source);
int glCompileShader(GLuint shader);
GLuint glCreateProgram(void);
int glAttachShader(GLuint program, GLuint shader);
int glLinkProgram(GLuint program);
void glUseProgram(GLuint program);
void glDrawArrays(GLenum mode, GLint first, GLsizei count);
void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices);

#endif
