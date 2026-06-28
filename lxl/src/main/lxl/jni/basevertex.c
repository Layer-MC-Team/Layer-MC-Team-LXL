/**
 * Created by: Layer
 * Copyright (c) 2026 Layer.
 * For use under LGPL-3.0
 */
#include <stdlib.h>
#include <GLES3/gl31.h>
#include "proc.h"
#include "egl.h"
#include "main.h"

typedef struct {
    GLuint count;
    GLuint instanceCount;
    GLuint firstIndex;
    GLint baseVertex;
    GLuint reservedMustBeZero;
} indirect_pass_t;

void basevertex_init(context_t* context) {
    basevertex_renderer_t *renderer = &context->basevertex;
    if (context->drawelementsbasevertex != NULL) {
        printf("LXL: BaseVertex using host driver\n");
        return;
    }
    if (!context->es31) {
        printf("LXL: BaseVertex requires ES 3.1\n");
        return;
    }
    es3_functions.glGenBuffers(1, &renderer->indirectRenderBuffer);
    GLenum error = es3_functions.glGetError();
    if (error != GL_NO_ERROR) {
        printf("LXL: Failed to init indirect buffer: %x\n", error);
        return;
    }
    es3_functions.glBindBuffer(GL_DRAW_INDIRECT_BUFFER, renderer->indirectRenderBuffer);
    es3_functions.glBufferData(GL_DRAW_INDIRECT_BUFFER, 1024 * 1024, NULL, GL_STREAM_DRAW);
    renderer->ready = true;
    renderer->cached_passes = NULL;
    renderer->cached_capacity = 0;
}

// 全局可见，供 multidraw.c 等文件使用
GLint type_bytes(GLenum type) {
    switch (type) {
        case GL_UNSIGNED_BYTE: return 1;
        case GL_UNSIGNED_SHORT: return 2;
        case GL_UNSIGNED_INT: return 4;
        default: return -1;
    }
}

static void restore_state(GLuint element_buffer) {
    es3_functions.glBindBuffer(GL_DRAW_INDIRECT_BUFFER,
        current_context->bound_buffers[get_buffer_index(GL_DRAW_INDIRECT_BUFFER)]);
}

void glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type,
                              const void *indices, GLint basevertex) {
    if (!current_context) return;
    if (current_context->drawelementsbasevertex != NULL) {
        current_context->drawelementsbasevertex(mode, count, type, indices, basevertex);
        return;
    }
    basevertex_renderer_t *renderer = &current_context->basevertex;
    if (!renderer->ready) return;
    GLint elementbuffer;
    es3_functions.glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &elementbuffer);
    if (elementbuffer == 0) {
        printf("LXL: BaseVertex without element buffer unsupported\n");
        return;
    }
    GLint typeBytes = type_bytes(type);
    if (typeBytes < 0) return;
    uintptr_t idxPtr = (uintptr_t)indices;
    if (idxPtr % typeBytes != 0) {
        printf("LXL: misaligned indices\n");
        return;
    }
    indirect_pass_t pass;
    pass.count = count;
    pass.instanceCount = 1;
    pass.firstIndex = idxPtr / typeBytes;
    pass.baseVertex = basevertex;
    pass.reservedMustBeZero = 0;

    es3_functions.glBindBuffer(GL_DRAW_INDIRECT_BUFFER, renderer->indirectRenderBuffer);
    es3_functions.glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, sizeof(pass), &pass);
    es3_functions.glDrawElementsIndirect(mode, type, 0);
    restore_state(elementbuffer);
}

void glMultiDrawElementsBaseVertex(GLenum mode, const GLsizei *count, GLenum type,
                                   const void * const *indices, GLsizei drawcount,
                                   const GLint *basevertex) {
    if (!current_context) return;
    if (current_context->drawelementsbasevertex != NULL) {
        for (GLsizei i = 0; i < drawcount; i++)
            current_context->drawelementsbasevertex(mode, count[i], type, indices[i], basevertex[i]);
        return;
    }
    basevertex_renderer_t *renderer = &current_context->basevertex;
    if (!renderer->ready) return;
    GLint elementbuffer;
    es3_functions.glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &elementbuffer);
    if (elementbuffer == 0) {
        printf("LXL: MultiBaseVertex without element buffer unsupported\n");
        return;
    }
    GLint typeBytes = type_bytes(type);
    if (typeBytes < 0) return;

    if (renderer->cached_capacity < drawcount) {
        GLsizei new_cap = drawcount * 2;
        indirect_pass_t *new_ptr = (indirect_pass_t*)realloc(renderer->cached_passes, new_cap * sizeof(indirect_pass_t));
        if (!new_ptr) return;
        renderer->cached_passes = new_ptr;
        renderer->cached_capacity = new_cap;
    }
    indirect_pass_t *passes = (indirect_pass_t*)renderer->cached_passes;
    for (GLsizei i = 0; i < drawcount; i++) {
        uintptr_t idxPtr = (uintptr_t)indices[i];
        if (idxPtr % typeBytes != 0) {
            printf("LXL: misaligned indices in draw %d\n", i);
            return;
        }
        passes[i].count = count[i];
        passes[i].instanceCount = 1;
        passes[i].firstIndex = idxPtr / typeBytes;
        passes[i].baseVertex = basevertex[i];
        passes[i].reservedMustBeZero = 0;
    }
    size_t totalSize = drawcount * sizeof(indirect_pass_t);
    es3_functions.glBindBuffer(GL_DRAW_INDIRECT_BUFFER, renderer->indirectRenderBuffer);
    GLint bufSize;
    es3_functions.glGetBufferParameteriv(GL_DRAW_INDIRECT_BUFFER, GL_BUFFER_SIZE, &bufSize);
    if (bufSize < totalSize) {
        es3_functions.glBufferData(GL_DRAW_INDIRECT_BUFFER, totalSize, NULL, GL_STREAM_DRAW);
    }
    es3_functions.glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, totalSize, passes);
    if (current_context->multidraw_indirect) {
        es3_functions.glMultiDrawElementsIndirectEXT(mode, type, 0, drawcount, 0);
    } else {
        for (GLsizei i = 0; i < drawcount; i++)
            es3_functions.glDrawElementsIndirect(mode, type, (void*)(i * sizeof(indirect_pass_t)));
    }
    restore_state(elementbuffer);
}