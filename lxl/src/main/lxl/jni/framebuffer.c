/**
 * Created by: Layer
 * Copyright (c) 2026 Layer.
 * For use under LGPL-3.0
 */

#include "proc.h"
#include "egl.h"
#include <string.h>

static framebuffer_t* get_framebuffer(GLenum target) {
    GLuint fb;
    switch (target) {
        case GL_FRAMEBUFFER:
        case GL_DRAW_FRAMEBUFFER: fb = current_context->draw_framebuffer; break;
        case GL_READ_FRAMEBUFFER: fb = current_context->read_framebuffer; break;
        default: return NULL;
    }
    return unordered_map_get(current_context->framebuffer_map, (void*)(uintptr_t)fb);
}

static inline GLuint get_attachment_idx(GLenum attachment) {
    if (attachment >= GL_COLOR_ATTACHMENT0 && attachment < GL_COLOR_ATTACHMENT0 + MAX_FBTARGETS)
        return attachment - GL_COLOR_ATTACHMENT0;
    return (GLuint)-1;
}

static inline GLenum map_attachment(framebuffer_t* fb, GLenum attachment) {
    for (GLsizei i = 0; i < fb->nbuffers; i++) {
        if (fb->virt_drawbuffers[i] == attachment)
            return GL_COLOR_ATTACHMENT0 + i;
    }
    return GL_NONE;
}

static void rebind_framebuffer(GLenum target, framebuffer_t* fb, GLenum virt_attachment) {
    GLuint idx = get_attachment_idx(virt_attachment);
    if (idx == (GLuint)-1) return;
    GLenum phys_att = map_attachment(fb, virt_attachment);
    if (phys_att == GL_NONE) return;

    GLuint obj = fb->color_objects[idx];
    GLint level = fb->color_levels[idx];
    GLint layer = fb->color_layers[idx];

    switch (fb->color_targets[idx]) {
        case GL_NONE:
            es3_functions.glFramebufferRenderbuffer(target, phys_att, GL_RENDERBUFFER, 0);
            break;
        case GL_RENDERBUFFER:
            es3_functions.glFramebufferRenderbuffer(target, phys_att, GL_RENDERBUFFER, obj);
            break;
        case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER:
            es3_functions.glFramebufferTextureLayer(target, phys_att, obj, level, layer);
            break;
        default:
            es3_functions.glFramebufferTexture2D(target, phys_att, fb->color_targets[idx], obj, level);
            break;
    }
}

void glClearBufferiv(GLenum buffer, GLint drawBuffer, const GLint* value) {
    framebuffer_t* fb = get_framebuffer(GL_DRAW_FRAMEBUFFER);
    if (fb && buffer == GL_COLOR) {
        GLenum att = map_attachment(fb, GL_COLOR_ATTACHMENT0 + drawBuffer);
        if (att != GL_NONE)
            drawBuffer = att - GL_COLOR_ATTACHMENT0;
    }
    es3_functions.glClearBufferiv(buffer, drawBuffer, value);
}

void glClearBufferuiv(GLenum buffer, GLint drawBuffer, const GLuint* value) {
    framebuffer_t* fb = get_framebuffer(GL_DRAW_FRAMEBUFFER);
    if (fb && buffer == GL_COLOR) {
        GLenum att = map_attachment(fb, GL_COLOR_ATTACHMENT0 + drawBuffer);
        if (att != GL_NONE)
            drawBuffer = att - GL_COLOR_ATTACHMENT0;
    }
    es3_functions.glClearBufferuiv(buffer, drawBuffer, value);
}

void glClearBufferfv(GLenum buffer, GLint drawBuffer, const GLfloat* value) {
    framebuffer_t* fb = get_framebuffer(GL_DRAW_FRAMEBUFFER);
    if (fb && buffer == GL_COLOR) {
        GLenum att = map_attachment(fb, GL_COLOR_ATTACHMENT0 + drawBuffer);
        if (att != GL_NONE)
            drawBuffer = att - GL_COLOR_ATTACHMENT0;
    }
    es3_functions.glClearBufferfv(buffer, drawBuffer, value);
}

void glDrawBuffers(GLsizei n, const GLenum* buffers) {
    if (!current_context) return;
    framebuffer_t* fb = get_framebuffer(GL_DRAW_FRAMEBUFFER);
    if (!fb) {
        es3_functions.glDrawBuffers(n, buffers);
        return;
    }
    fb->nbuffers = n;
    memcpy(fb->virt_drawbuffers, buffers, n * sizeof(GLenum));

    GLenum phys[MAX_DRAWBUFFERS];
    for (GLsizei i = 0; i < n; i++) {
        rebind_framebuffer(GL_DRAW_FRAMEBUFFER, fb, buffers[i]);
        phys[i] = (buffers[i] != GL_NONE) ? (GL_COLOR_ATTACHMENT0 + i) : GL_NONE;
    }
    es3_functions.glDrawBuffers(n, phys);
}

void glDrawBuffer(GLenum buffer) {
    glDrawBuffers(1, &buffer);
}

GLenum glCheckFramebufferStatus(GLenum target) {
    if (!current_context) return GL_FRAMEBUFFER_UNDEFINED;
    GLenum status = es3_functions.glCheckFramebufferStatus(target);
    if (status == GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT) {
        framebuffer_t* fb = get_framebuffer(target);
        if (fb) {
            for (int i = 0; i < MAX_FBTARGETS; i++) {
                if (fb->color_targets[i] != GL_NONE || fb->color_objects[i] != 0)
                    return GL_FRAMEBUFFER_COMPLETE;
            }
        }
    }
    return status;
}

void glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget,
                            GLuint texture, GLint level) {
    if (!current_context) return;
    framebuffer_t* fb = get_framebuffer(target);
    GLuint idx = get_attachment_idx(attachment);
    if (!fb || idx == (GLuint)-1) {
        es3_functions.glFramebufferTexture2D(target, attachment, textarget, texture, level);
        return;
    }
    if (texture == 0) {
        fb->color_targets[idx] = GL_NONE;
    } else {
        fb->color_targets[idx] = textarget;
        fb->color_objects[idx] = texture;
        fb->color_levels[idx] = level;
    }
    rebind_framebuffer(target, fb, attachment);
}

void glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture,
                               GLint level, GLint layer) {
    if (!current_context) return;
    framebuffer_t* fb = get_framebuffer(target);
    GLuint idx = get_attachment_idx(attachment);
    if (!fb || idx == (GLuint)-1) {
        es3_functions.glFramebufferTextureLayer(target, attachment, texture, level, layer);
        return;
    }
    if (texture == 0) {
        fb->color_targets[idx] = GL_NONE;
    } else {
        fb->color_targets[idx] = GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER;
        fb->color_objects[idx] = texture;
        fb->color_levels[idx] = level;
        fb->color_layers[idx] = layer;
    }
    rebind_framebuffer(target, fb, attachment);
}

void glFramebufferRenderbuffer(GLenum target, GLenum attachment,
                               GLenum renderbuffertarget, GLuint renderbuffer) {
    if (!current_context) return;
    framebuffer_t* fb = get_framebuffer(target);
    GLuint idx = get_attachment_idx(attachment);
    if (!fb || idx == (GLuint)-1) {
        es3_functions.glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
        return;
    }
    if (renderbuffer == 0) {
        fb->color_targets[idx] = GL_NONE;
    } else {
        fb->color_targets[idx] = renderbuffertarget;
        fb->color_objects[idx] = renderbuffer;
    }
    rebind_framebuffer(target, fb, attachment);
}

void glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment,
                                           GLenum pname, GLint* params) {
    if (!current_context) return;
    framebuffer_t* fb = get_framebuffer(target);
    GLuint idx = get_attachment_idx(attachment);
    if (!fb || idx == (GLuint)-1) {
        es3_functions.glGetFramebufferAttachmentParameteriv(target, attachment, pname, params);
        return;
    }
    GLenum fb_target = fb->color_targets[idx];
    switch (pname) {
        case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
            *params = (fb_target == GL_NONE) ? GL_NONE :
                      (fb_target == GL_RENDERBUFFER) ? GL_RENDERBUFFER : GL_TEXTURE;
            break;
        case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
            *params = (fb_target == GL_NONE) ? 0 : (GLint)fb->color_objects[idx];
            break;
        case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
            *params = (fb_target == GL_NONE || fb_target == GL_RENDERBUFFER) ? 0 : fb->color_levels[idx];
            break;
        case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER:
            *params = (fb_target == GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER) ? fb->color_layers[idx] : 0;
            break;
        case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
            *params = (fb_target >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && fb_target <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z)
                      ? fb_target : 0;
            break;
        default:
            *params = 0;
            break;
    }
}

void glGenFramebuffers(GLsizei n, GLuint* framebuffers) {
    if (!current_context) return;
    es3_functions.glGenFramebuffers(n, framebuffers);
    for (GLsizei i = 0; i < n; i++) {
        framebuffer_t* fb = calloc(1, sizeof(framebuffer_t));
        fb->nbuffers = 1;
        fb->virt_drawbuffers[0] = GL_COLOR_ATTACHMENT0;
        unordered_map_put(current_context->framebuffer_map, (void*)(uintptr_t)framebuffers[i], fb);
    }
}

void glDeleteFramebuffers(GLsizei n, const GLuint* framebuffers) {
    if (!current_context) return;
    es3_functions.glDeleteFramebuffers(n, framebuffers);
    for (GLsizei i = 0; i < n; i++) {
        framebuffer_t* fb = unordered_map_remove(current_context->framebuffer_map,
                                                 (void*)(uintptr_t)framebuffers[i]);
        if (fb) free(fb);
    }
}

void glBindFramebuffer(GLenum target, GLuint framebuffer) {
    if (!current_context) return;
    es3_functions.glBindFramebuffer(target, framebuffer);
    switch (target) {
        case GL_FRAMEBUFFER:
            current_context->draw_framebuffer = current_context->read_framebuffer = framebuffer;
            break;
        case GL_READ_FRAMEBUFFER:
            current_context->read_framebuffer = framebuffer;
            break;
        case GL_DRAW_FRAMEBUFFER:
            current_context->draw_framebuffer = framebuffer;
            break;
    }
}