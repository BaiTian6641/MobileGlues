#pragma once
#include <GLES3/gl32.h>
#include <GLES2/gl2ext.h>

struct BufferApi {
    void (*gen)(GLsizei, GLuint*);
    void (*remove)(GLsizei, const GLuint*);
    void (*bind)(GLenum, GLuint);
    void (*bind_range)(GLenum, GLuint, GLuint, GLintptr, GLsizeiptr);
    void (*storage)(GLenum, GLsizeiptr, const void*, GLbitfield);
    void* (*map)(GLenum, GLintptr, GLsizeiptr, GLbitfield);
    GLboolean (*unmap)(GLenum);
    void (*flush)(GLenum, GLintptr, GLsizeiptr);
    void (*parameter)(GLenum, GLenum, GLint*);
};
