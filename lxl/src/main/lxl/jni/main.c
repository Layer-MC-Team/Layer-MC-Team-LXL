/**
 * Created by: Layer
 * Copyright (c) 2026 Layer.
 * For use under LGPL-3.0
 */

#include <stdio.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "GL/gl.h"
#include <GLES3/gl3.h>
#include "string_utils.h"
#include "proc.h"
#include "egl.h"
#include "glformats.h"
#include "main.h"
#include "swizzle.h"
#include "libraryinternal.h"
#include "env.h"

#define LXL_LOG(fmt, ...) printf("LXL: " fmt, ##__VA_ARGS__)
#define LXL_WARN(fmt, ...) printf("LXL WARNING: " fmt, ##__VA_ARGS__)

static inline bool isProxyTexture(GLenum target) {
    return target == GL_PROXY_TEXTURE_1D || target == GL_PROXY_TEXTURE_2D ||
           target == GL_PROXY_TEXTURE_3D || target == GL_PROXY_TEXTURE_RECTANGLE_ARB;
}

INTERNAL GLenum get_textarget_query_param(GLenum target) {
    static const GLenum map[] = {
        [GL_TEXTURE_2D] = GL_TEXTURE_BINDING_2D,
        [GL_TEXTURE_2D_MULTISAMPLE] = GL_TEXTURE_BINDING_2D_MULTISAMPLE,
        [GL_TEXTURE_2D_MULTISAMPLE_ARRAY] = GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY,
        [GL_TEXTURE_3D] = GL_TEXTURE_BINDING_3D,
        [GL_TEXTURE_2D_ARRAY] = GL_TEXTURE_BINDING_2D_ARRAY,
        [GL_TEXTURE_CUBE_MAP] = GL_TEXTURE_BINDING_CUBE_MAP,
        [GL_TEXTURE_CUBE_MAP_ARRAY] = GL_TEXTURE_BINDING_CUBE_MAP_ARRAY,
        [GL_TEXTURE_BUFFER] = GL_TEXTURE_BUFFER_BINDING,
    };
    if (target >= 0 && target < sizeof(map)/sizeof(map[0]))
        return map[target];
    return 0;
}

static inline int nlevel(int size, int level) {
    if (size) {
        size >>= level;
        if (!size) size = 1;
    }
    return size;
}

static bool texlevel_warning_issued = false;
static inline bool check_texlevelparameter(void) {
    if (current_context && current_context->es31)
        return true;
    if (!texlevel_warning_issued) {
        LXL_WARN("glGetTexLevelParameter* not supported below ES 3.1\n");
        texlevel_warning_issued = true;
    }
    return false;
}

static void proxy_getlevelparameter(GLenum target, GLint level, GLenum pname, GLint *params) {
    (void)target;
    switch (pname) {
        case GL_TEXTURE_WIDTH:  *params = nlevel(current_context->proxy_width, level); break;
        case GL_TEXTURE_HEIGHT: *params = nlevel(current_context->proxy_height, level); break;
        case GL_TEXTURE_INTERNAL_FORMAT: *params = current_context->proxy_intformat; break;
        default: *params = 0;
    }
}

int get_buffer_index(GLenum buffer) {
    switch (buffer) {
        case GL_ARRAY_BUFFER: return 0;
        case GL_COPY_READ_BUFFER: return 1;
        case GL_COPY_WRITE_BUFFER: return 2;
        case GL_PIXEL_PACK_BUFFER: return 3;
        case GL_PIXEL_UNPACK_BUFFER: return 4;
        case GL_TRANSFORM_FEEDBACK_BUFFER: return 5;
        case GL_UNIFORM_BUFFER: return 6;
        case GL_SHADER_STORAGE_BUFFER: return 7;
        case GL_DRAW_INDIRECT_BUFFER: return 8;
        default: return -1;
    }
}

int get_base_buffer_index(GLenum buffer) {
    switch (buffer) {
        case GL_ATOMIC_COUNTER_BUFFER: return 0;
        case GL_SHADER_STORAGE_BUFFER: return 1;
        case GL_TRANSFORM_FEEDBACK_BUFFER: return 2;
        case GL_UNIFORM_BUFFER: return 3;
        default: return -1;
    }
}

GLenum get_base_buffer_enum(int index) {
    switch (index) {
        case 0: return GL_ATOMIC_COUNTER_BUFFER;
        case 1: return GL_SHADER_STORAGE_BUFFER;
        case 2: return GL_TRANSFORM_FEEDBACK_BUFFER;
        case 3: return GL_UNIFORM_BUFFER;
        default: return (GLenum)-1;
    }
}

static basebuffer_binding_t* set_basebuffer(GLenum target, GLuint index, GLuint buffer) {
    int map_idx = get_base_buffer_index(target);
    if (map_idx < 0) return NULL;
    if (!buffer) {
        basebuffer_binding_t* old = unordered_map_remove(
            current_context->bound_basebuffers[map_idx], (void*)(uintptr_t)index);
        free(old);
        return NULL;
    }
    basebuffer_binding_t* binding = unordered_map_get(
        current_context->bound_basebuffers[map_idx], (void*)(uintptr_t)index);
    if (!binding) {
        binding = calloc(1, sizeof(basebuffer_binding_t));
        unordered_map_put(current_context->bound_basebuffers[map_idx],
                          (void*)(uintptr_t)index, binding);
    }
    binding->index = index;
    binding->buffer = buffer;
    return binding;
}

static bool noerror = false, debug_enabled = false;
static bool never_flush_buffers = true, coherent_dynamic_storage = true;

__attribute__((constructor)) static void init_globals(void) {
    noerror = env_istrue("LIBGL_NOERROR");
    debug_enabled = env_istrue("LXL_DEBUG");
    never_flush_buffers = env_istrue_d("LXL_NEVER_FLUSH_BUFFERS", true);
    coherent_dynamic_storage = env_istrue_d("LXL_COHERENT_DYNAMIC_STORAGE", true);
}

void glClearDepth(GLdouble depth) {
    if (current_context) es3_functions.glClearDepthf((GLfloat)depth);
}

void *glMapBuffer(GLenum target, GLenum access) {
    if (!current_context) return NULL;
    GLbitfield access_range = 0;
    switch (access) {
        case GL_READ_ONLY:  access_range = GL_MAP_READ_BIT; break;
        case GL_WRITE_ONLY: access_range = GL_MAP_WRITE_BIT; break;
        case GL_READ_WRITE: access_range = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT; break;
        default: return NULL;
    }
    GLint length = 0;
    es3_functions.glGetBufferParameteriv(target, GL_BUFFER_SIZE, &length);
    return es3_functions.glMapBufferRange(target, 0, length, access_range);
}

void glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat *params) {
    if (!current_context) return;
    if (isProxyTexture(target)) {
        GLint val; proxy_getlevelparameter(target, level, pname, &val);
        *params = (GLfloat)val; return;
    }
    if (check_texlevelparameter())
        es3_functions.glGetTexLevelParameterfv(target, level, pname, params);
}

void glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params) {
    if (!current_context) return;
    if (isProxyTexture(target)) {
        proxy_getlevelparameter(target, level, pname, params); return;
    }
    if (check_texlevelparameter())
        es3_functions.glGetTexLevelParameteriv(target, level, pname, params);
}

void glTexImage2D(GLenum target, GLint level, GLint internalformat,
                  GLsizei width, GLsizei height, GLint border,
                  GLenum format, GLenum type, const GLvoid *data) {
    if (!current_context) return;
    if (isProxyTexture(target)) {
        current_context->proxy_width  = (width << level) > current_context->maxTextureSize ? 0 : width;
        current_context->proxy_height = (height << level) > current_context->maxTextureSize ? 0 : height;
        current_context->proxy_intformat = internalformat;
        return;
    }
    if (data) swizzle_process_upload(target, &format, &type);
    pick_internalformat(&internalformat, &type, &format, &data);
    es3_functions.glTexImage2D(target, level, internalformat, width, height, border, format, type, data);
}

static bool filter_params_float(GLenum target, GLenum pname, GLfloat param) {
    if (pname == GL_TEXTURE_LOD_BIAS && param != 0.0f) {
        static bool warned = false;
        if (!warned) { LXL_WARN("LOD_BIAS non-zero ignored\n"); warned = true; }
        return false;
    }
    return true;
}

static void handle_swizzle_common(GLenum target, GLenum pname, const void *params) {
    if (pname != GL_TEXTURE_SWIZZLE_RGBA) {
        static bool warned = false;
        if (!warned) { LXL_WARN("TexParameterIiv only supports SWIZZLE_RGBA\n"); warned = true; }
        return;
    }
    swizzle_process_swizzle_param(target, pname, params);
}

void glTexParameterf(GLenum target, GLenum pname, GLfloat param) {
    if (!current_context) return;
    if (!filter_params_float(target, pname, param)) return;
    es3_functions.glTexParameterf(target, pname, param);
}

void glTexParameteri(GLenum target, GLenum pname, GLint param) {
    if (!current_context) return;
    if (!filter_params_float(target, pname, (GLfloat)param)) return;
    swizzle_process_swizzle_param(target, pname, &param);
    es3_functions.glTexParameteri(target, pname, param);
}

void glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params) {
    if (!current_context) return;
    if (!filter_params_float(target, pname, *params)) return;
    es3_functions.glTexParameterfv(target, pname, params);
}

void glTexParameteriv(GLenum target, GLenum pname, const GLint *params) {
    if (!current_context) return;
    if (!filter_params_float(target, pname, (GLfloat)*params)) return;
    swizzle_process_swizzle_param(target, pname, params);
    es3_functions.glTexParameteriv(target, pname, params);
}

void glTexParameterIiv(GLenum target, GLenum pname, const GLint *params) {
    if (current_context) handle_swizzle_common(target, pname, params);
}

void glTexParameterIuiv(GLenum target, GLenum pname, const GLuint *params) {
    if (current_context) handle_swizzle_common(target, pname, params);
}

void glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
    if (!current_context) return;
    if (internalformat == GL_DEPTH_COMPONENT) internalformat = GL_DEPTH_COMPONENT16;
    es3_functions.glRenderbufferStorage(target, internalformat, width, height);
}

void glBufferStorage(GLenum target, GLsizeiptr size, const void *data, GLbitfield flags) {
    if (!current_context || !current_context->buffer_storage) return;
    if (never_flush_buffers && (flags & GL_MAP_PERSISTENT_BIT)) flags |= GL_MAP_COHERENT_BIT;
    if (coherent_dynamic_storage && (flags & GL_DYNAMIC_STORAGE_BIT))
        flags |= GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
    es3_functions.glBufferStorageEXT(target, size, data, flags);
}

void *glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access) {
    if (never_flush_buffers) access &= ~GL_MAP_FLUSH_EXPLICIT_BIT;
    return es3_functions.glMapBufferRange(target, offset, length, access);
}

void glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length) {
    if (!never_flush_buffers)
        es3_functions.glFlushMappedBufferRange(target, offset, length);
}

const GLubyte* glGetStringi(GLenum name, GLuint index) {
    if (!current_context || name != GL_EXTENSIONS) return NULL;
    if (index >= current_context->nextras)
        return es3_functions.glGetStringi(name, index - current_context->nextras);
    return (const GLubyte*)current_context->extra_extensions_array[index];
}

const GLubyte* glGetString(GLenum name) {
    if (!current_context) return NULL;
    switch (name) {
        case GL_VERSION:
            return (const GLubyte*)"3.3 OpenLXL (Built on: "__DATE__"/"__TIME__")";
        case GL_SHADING_LANGUAGE_VERSION:
            return (const GLubyte*)"4.60 LXL";
        case GL_VENDOR:
            return (const GLubyte*)"Layer";
        case GL_EXTENSIONS:
            return current_context->extensions_string ?
                (const GLubyte*)current_context->extensions_string :
                es3_functions.glGetString(GL_EXTENSIONS);
        default:
            return es3_functions.glGetString(name);
    }
}

void glEnable(GLenum cap) {
    if (!current_context) return;
    if (cap == GL_DEBUG_OUTPUT && !debug_enabled) return;
    es3_functions.glEnable(cap);
}

GLenum glGetError(void) {
    return noerror ? 0 : es3_functions.glGetError();
}

void glDebugMessageControl(GLenum source, GLenum type, GLenum severity,
                           GLsizei count, const GLuint *ids, GLboolean enabled) {
    // stub
}

void glGetIntegerv(GLenum pname, GLint* data) {
    if (!current_context) return;
    switch (pname) {
        case GL_MAJOR_VERSION: *data = 3; return;
        case GL_MINOR_VERSION: *data = 3; return;
        case GL_NUM_EXTENSIONS:
            es3_functions.glGetIntegerv(pname, data);
            *data += current_context->nextras;
            return;
        case GL_MAX_COLOR_ATTACHMENTS: *data = MAX_FBTARGETS; return;
        case GL_MAX_DRAW_BUFFERS: *data = current_context->max_drawbuffers; return;
        case GL_CONTEXT_FLAGS: *data = GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT; return;
        case GL_CONTEXT_PROFILE_MASK: *data = GL_CONTEXT_CORE_PROFILE_BIT; return;
        default: es3_functions.glGetIntegerv(pname, data);
    }
}

void glDepthRange(GLdouble nearVal, GLdouble farVal) {
    if (current_context)
        es3_functions.glDepthRangef((GLfloat)nearVal, (GLfloat)farVal);
}

void glDeleteTextures(GLsizei n, const GLuint *textures) {
    if (!current_context) return;
    es3_functions.glDeleteTextures(n, textures);
    for (GLsizei i = 0; i < n; i++) {
        void* tracker = unordered_map_remove(
            current_context->texture_swztrack_map, (void*)(uintptr_t)textures[i]);
        free(tracker);
    }
}

static bool buffer_texture_warned = false;
static inline void check_buffer_texture_support(void) {
    if (!buffer_texture_warned && current_context) {
        if (!current_context->es32 && !current_context->buffer_texture_ext) {
            LXL_WARN("Buffer textures not supported\n");
            buffer_texture_warned = true;
        }
    }
}

void glTexBuffer(GLenum target, GLenum internalFormat, GLuint buffer) {
    if (!current_context) return;
    if (current_context->es32)
        es3_functions.glTexBuffer(target, internalFormat, buffer);
    else if (current_context->buffer_texture_ext)
        es3_functions.glTexBufferEXT(target, internalFormat, buffer);
    else
        check_buffer_texture_support();
}

void glTexBufferRange(GLenum target, GLenum internalFormat, GLuint buffer,
                      GLintptr offset, GLsizeiptr size) {
    if (!current_context) return;
    if (current_context->es32)
        es3_functions.glTexBufferRange(target, internalFormat, buffer, offset, size);
    else if (current_context->buffer_texture_ext)
        es3_functions.glTexBufferRangeEXT(target, internalFormat, buffer, offset, size);
    else
        check_buffer_texture_support();
}

void glBindBuffer(GLenum target, GLuint buffer) {
    if (!current_context) return;
    es3_functions.glBindBuffer(target, buffer);
    int idx = get_buffer_index(target);
    if (idx >= 0) current_context->bound_buffers[idx] = buffer;
}

void glBindBufferBase(GLenum target, GLuint index, GLuint buffer) {
    if (!current_context) return;
    es3_functions.glBindBufferBase(target, index, buffer);
    basebuffer_binding_t* binding = set_basebuffer(target, index, buffer);
    if (binding) binding->ranged = false;
}

void glBindBufferRange(GLenum target, GLuint index, GLuint buffer,
                       GLintptr offset, GLsizeiptr size) {
    if (!current_context) return;
    es3_functions.glBindBufferRange(target, index, buffer, offset, size);
    basebuffer_binding_t* binding = set_basebuffer(target, index, buffer);
    if (binding) {
        binding->ranged = true;
        binding->offset = offset;
        binding->size = size;
    }
}

void glUseProgram(GLuint program) {
    if (!current_context) return;
    es3_functions.glUseProgram(program);
    current_context->program = program;
}