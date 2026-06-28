/**
 * Created by: Layer
 * Copyright (c) 2026 Layer.
 * For use under LGPL-3.0
 */

#include "unordered_map/unordered_map.h"
#include "vgpu_shaderconv/shaderconv.h"
#include "glsl_optimizer/src/code/c_wrapper.h"
#include <GLES3/gl3.h>
#include <string.h>
#include "string_utils.h"
#include "egl.h"
#include "proc.h"

typedef struct {
    GLenum shader_type;
    const GLchar* source;
} shader_info_t;

typedef struct {
    GLuint frag_shader;
    GLchar* colorbindings[MAX_DRAWBUFFERS];
} program_info_t;

static GLchar* str_dup_safe(const char* src) {
    if (!src) return NULL;
    GLchar* dst = malloc(strlen(src) + 1);
    if (dst) strcpy(dst, src);
    return dst;
}

static void free_colorbindings(program_info_t* info) {
    for (int i = 0; i < MAX_DRAWBUFFERS; i++) {
        free(info->colorbindings[i]);
        info->colorbindings[i] = NULL;
    }
}

static void insert_fragout_pos(char* source, int* size, const char* name, GLuint pos) {
    char placeholder[256];
    char replacement[32];
    snprintf(placeholder, sizeof(placeholder), "/* LXL INSERT LOCATION %s LXL */", name);
    snprintf(replacement, sizeof(replacement), "layout(location = %u) ", pos);
    gl4es_inplace_replace_simple(source, size, placeholder, replacement);
}

static GLuint create_patched_fragment_shader(const char* original_source,
                                             program_info_t* prog_info) {
    int src_size = strlen(original_source) + 1;
    char* patched_src = malloc(src_size);
    if (!patched_src) return 0;
    memcpy(patched_src, original_source, src_size);

    bool modified = false;
    for (GLuint i = 0; i < MAX_DRAWBUFFERS; i++) {
        const char* binding = prog_info->colorbindings[i];
        if (binding) {
            insert_fragout_pos(patched_src, &src_size, binding, i);
            modified = true;
        }
    }
    if (!modified) {
        free(patched_src);
        return 0;
    }

    GLuint shader = es3_functions.glCreateShader(GL_FRAGMENT_SHADER);
    if (!shader) {
        free(patched_src);
        return 0;
    }
    const GLchar* src_ptr = patched_src;
    es3_functions.glShaderSource(shader, 1, &src_ptr, NULL);
    es3_functions.glCompileShader(shader);

    GLint status;
    es3_functions.glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        es3_functions.glDeleteShader(shader);
        free(patched_src);
        return 0;
    }
    free(patched_src);
    return shader;
}

GLuint glCreateProgram(void) {
    if (!current_context) return 0;
    GLuint phys = es3_functions.glCreateProgram();
    if (!phys) return 0;
    program_info_t* info = calloc(1, sizeof(program_info_t));
    if (!info) abort();
    unordered_map_put(current_context->program_map, (void*)(uintptr_t)phys, info);
    return phys;
}

void glDeleteProgram(GLuint program) {
    if (!current_context) return;
    es3_functions.glDeleteProgram(program);
    program_info_t* info = unordered_map_remove(
        current_context->program_map, (void*)(uintptr_t)program);
    if (info) { free_colorbindings(info); free(info); }
}

void glAttachShader(GLuint program, GLuint shader) {
    if (!current_context) return;
    es3_functions.glAttachShader(program, shader);
    program_info_t* prog_info = unordered_map_get(
        current_context->program_map, (void*)(uintptr_t)program);
    shader_info_t* shd_info = unordered_map_get(
        current_context->shader_map, (void*)(uintptr_t)shader);
    if (prog_info && shd_info && shd_info->shader_type == GL_FRAGMENT_SHADER)
        prog_info->frag_shader = shader;
}

void glBindFragDataLocation(GLuint program, GLuint colorNumber, const char* name) {
    if (!current_context || colorNumber >= MAX_DRAWBUFFERS) return;
    program_info_t* info = unordered_map_get(
        current_context->program_map, (void*)(uintptr_t)program);
    if (!info) return;
    free(info->colorbindings[colorNumber]);
    info->colorbindings[colorNumber] = str_dup_safe(name);
}

void glGetShaderiv(GLuint shader, GLenum pname, GLint* params) {
    if (!current_context) return;
    shader_info_t* info = unordered_map_get(
        current_context->shader_map, (void*)(uintptr_t)shader);
    if (info && info->shader_type == GL_FRAGMENT_SHADER && pname == GL_COMPILE_STATUS) {
        *params = GL_TRUE;
        return;
    }
    es3_functions.glGetShaderiv(shader, pname, params);
}

void glLinkProgram(GLuint program) {
    if (!current_context) {
        es3_functions.glLinkProgram(program);
        return;
    }
    program_info_t* prog_info = unordered_map_get(
        current_context->program_map, (void*)(uintptr_t)program);
    if (!prog_info || !prog_info->frag_shader)
        goto fallback;

    shader_info_t* shd_info = unordered_map_get(
        current_context->shader_map, (void*)(uintptr_t)prog_info->frag_shader);
    if (!shd_info || !shd_info->source)
        goto fallback;

    GLuint patched = create_patched_fragment_shader(shd_info->source, prog_info);
    if (patched) {
        es3_functions.glDetachShader(program, prog_info->frag_shader);
        es3_functions.glAttachShader(program, patched);
        es3_functions.glLinkProgram(program);
        es3_functions.glDeleteShader(patched);
        return;
    }
fallback:
    es3_functions.glLinkProgram(program);
}

GLuint glCreateShader(GLenum shaderType) {
    if (!current_context) return 0;
    GLuint phys = es3_functions.glCreateShader(shaderType);
    if (!phys) return 0;
    shader_info_t* info = calloc(1, sizeof(shader_info_t));
    if (!info) abort();
    info->shader_type = shaderType;
    unordered_map_put(current_context->shader_map, (void*)(uintptr_t)phys, info);
    return phys;
}

void glDeleteShader(GLuint shader) {
    if (!current_context) return;
    es3_functions.glDeleteShader(shader);
    shader_info_t* info = unordered_map_remove(
        current_context->shader_map, (void*)(uintptr_t)shader);
    if (info) {
        free((void*)info->source);
        free(info);
    }
}

void glShaderSource(GLuint shader, GLsizei count, const GLchar* const* strings,
                    const GLint* lengths) {
    if (!current_context) {
        es3_functions.glShaderSource(shader, count, strings, lengths);
        return;
    }
    shader_info_t* info = unordered_map_get(
        current_context->shader_map, (void*)(uintptr_t)shader);
    if (!info) {
        es3_functions.glShaderSource(shader, count, strings, lengths);
        return;
    }

    size_t total = 0;
    for (GLsizei i = 0; i < count; i++)
        total += (lengths && lengths[i] >= 0) ? lengths[i] : strlen(strings[i]);
    char* combined = malloc(total + 1);
    if (!combined) {
        es3_functions.glShaderSource(shader, count, strings, lengths);
        return;
    }
    char* ptr = combined;
    for (GLsizei i = 0; i < count; i++) {
        size_t len = (lengths && lengths[i] >= 0) ? lengths[i] : strlen(strings[i]);
        memcpy(ptr, strings[i], len);
        ptr += len;
    }
    *ptr = '\0';

    char* optimized = optimize_shader(combined, info->shader_type, 460,
                                      current_context->shader_version);
    free(combined);
    if (optimized) {
        free((void*)info->source);
        info->source = optimized;
        es3_functions.glShaderSource(shader, 1, &info->source, NULL);
    } else {
        es3_functions.glShaderSource(shader, count, strings, lengths);
    }
}