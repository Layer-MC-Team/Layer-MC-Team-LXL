/**
 * Created by: Layer
 * Copyright (c) 2026 Layer.
 * For use under LGPL-3.0
 */

#include "egl.h"
#include "unordered_map/int_hash.h"
#include "string_utils.h"
#include "env.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// 使用宏定义的线程局部变量
THREAD_LOCAL context_t *current_context;
unordered_map* context_map;

EGLContext (*host_eglCreateContext)(EGLDisplay, EGLConfig, EGLContext, const EGLint*);
EGLBoolean (*host_eglDestroyContext)(EGLDisplay, EGLContext);
EGLBoolean (*host_eglMakeCurrent)(EGLDisplay, EGLSurface, EGLSurface, EGLContext);

#define LXL_LOG(fmt, ...) printf("LXL: " fmt, ##__VA_ARGS__)
#define LXL_ERROR(fmt, ...) printf("LXL ERROR: " fmt, ##__VA_ARGS__)

void init_egl(void) {
    context_map = alloc_intmap();
    host_eglCreateContext = (EGLContext (*)(EGLDisplay, EGLConfig, EGLContext, const EGLint*))
        host_eglGetProcAddress("eglCreateContext");
    host_eglDestroyContext = (EGLBoolean (*)(EGLDisplay, EGLContext))
        host_eglGetProcAddress("eglDestroyContext");
    host_eglMakeCurrent = (EGLBoolean (*)(EGLDisplay, EGLSurface, EGLSurface, EGLContext))
        host_eglGetProcAddress("eglMakeCurrent");
}

static void free_context_maps(context_t *ctx) {
    if (ctx->shader_map)       unordered_map_free(ctx->shader_map);
    if (ctx->program_map)      unordered_map_free(ctx->program_map);
    if (ctx->framebuffer_map)  unordered_map_free(ctx->framebuffer_map);
    if (ctx->texture_swztrack_map) unordered_map_free(ctx->texture_swztrack_map);
    for (int i = 0; i < MAX_BOUND_BASEBUFFERS; ++i)
        if (ctx->bound_basebuffers[i])
            unordered_map_free(ctx->bound_basebuffers[i]);
}

static void free_extensions(context_t *ctx) {
    if (ctx->extensions_string) {
        free(ctx->extensions_string);
        ctx->extensions_string = NULL;
    }
    if (ctx->extra_extensions_array) {
        for (int i = 0; i < ctx->nextras; ++i)
            free(ctx->extra_extensions_array[i]);
        free(ctx->extra_extensions_array);
        ctx->extra_extensions_array = NULL;
        ctx->nextras = 0;
    }
}

static bool init_context(context_t *ctx) {
    ctx->shader_map = alloc_intmap_safe();
    if (!ctx->shader_map) goto fail;
    ctx->framebuffer_map = alloc_intmap_safe();
    if (!ctx->framebuffer_map) goto fail;
    ctx->program_map = alloc_intmap_safe();
    if (!ctx->program_map) goto fail;
    ctx->texture_swztrack_map = alloc_intmap_safe();
    if (!ctx->texture_swztrack_map) goto fail;
    for (int i = 0; i < MAX_BOUND_BASEBUFFERS; ++i) {
        ctx->bound_basebuffers[i] = alloc_intmap_safe();
        if (!ctx->bound_basebuffers[i]) goto fail;
    }
    return true;

fail:
    free_context_maps(ctx);
    return false;
}

static void free_context(context_t *ctx) {
    if (!ctx) return;
    free_context_maps(ctx);
    free_extensions(ctx);
}

static void add_extra_extension(context_t *ctx, char **str, int *len, const char *ext) {
    size_t ext_len = strlen(ext);
    if (ext_len == 0) return;
    // 分配空间：原有长度 + 前空格 + 扩展名 + 后空格 + 结束符
    char *new_str = realloc(*str, *len + ext_len + 3);
    if (!new_str) return;
    new_str[*len] = ' ';
    memcpy(new_str + *len + 1, ext, ext_len);
    new_str[*len + 1 + ext_len] = ' ';
    new_str[*len + 2 + ext_len] = '\0';
    *str = new_str;
    *len += ext_len + 2;

    // 存储到 extra_extensions_array 用于 glGetStringi
    char **arr = realloc(ctx->extra_extensions_array, (ctx->nextras + 1) * sizeof(char*));
    if (arr) {
        ctx->extra_extensions_array = arr;
        ctx->extra_extensions_array[ctx->nextras] = strdup(ext);
        ctx->nextras++;
    }
}

static void build_extension_string(context_t *ctx) {
    const char *es_ext = (const char*)es3_functions.glGetString(GL_EXTENSIONS);
    int base_len = strlen(es_ext);
    char *str = malloc(base_len + 128);
    if (!str) return;
    memcpy(str, es_ext, base_len);
    str[base_len] = '\0';
    int len = base_len;

    if (ctx->buffer_storage && !env_istrue("LXL_HIDE_BUFFER_STORAGE"))
        add_extra_extension(ctx, &str, &len, "GL_ARB_buffer_storage");
    if (ctx->buffer_texture_ext || ctx->es32)
        add_extra_extension(ctx, &str, &len, "GL_ARB_texture_buffer_object");
    add_extra_extension(ctx, &str, &len, "GL_ARB_draw_elements_base_vertex");
    if (ctx->blending.available)
        add_extra_extension(ctx, &str, &len, "GL_ARB_draw_buffers_blend");
    add_extra_extension(ctx, &str, &len, "GL_ARB_timer_query");

    // 去除末尾多余空格
    if (len > 0 && str[len-1] == ' ') str[len-1] = '\0';
    ctx->extensions_string = str;
}

static void detect_es_version(context_t *ctx) {
    const char *version = (const char*)es3_functions.glGetString(GL_VERSION);
    const char *shader_ver = (const char*)es3_functions.glGetString(GL_SHADING_LANGUAGE_VERSION);
    int es_major = 3, es_minor = 0, shader_major = 3, shader_minor = 0;

    // 安全解析
    if (strncmp(version, "OpenGL ES ", 10) == 0) {
        const char *p = version + 10;
        es_major = atoi(p);
        p = strchr(p, '.');
        if (p) es_minor = atoi(p + 1);
    }
    if (strncmp(shader_ver, "OpenGL ES GLSL ES ", 19) == 0) {
        const char *p = shader_ver + 19;
        shader_major = atoi(p);
        p = strchr(p, '.');
        if (p) shader_minor = atoi(p + 1);
    }
    ctx->shader_version = shader_major * 100 + shader_minor;
    LXL_LOG("Running on OpenGL ES %d.%d, ESSL %d\n", es_major, es_minor, ctx->shader_version);

    ctx->es31 = (es_major > 3 || (es_major == 3 && es_minor >= 1));
    ctx->es32 = (es_major > 3 || (es_major == 3 && es_minor >= 2));

    const char *exts = (const char*)es3_functions.glGetString(GL_EXTENSIONS);
    ctx->buffer_storage = (strstr(exts, "GL_EXT_buffer_storage") != NULL);
    ctx->buffer_texture_ext = (strstr(exts, "GL_EXT_texture_buffer") != NULL);
    ctx->multidraw_indirect = (strstr(exts, "GL_EXT_multi_draw_indirect") != NULL);
    ctx->timer_query = (strstr(exts, "GL_EXT_disjoint_timer_query") != NULL) ||
                       env_istrue_d("LXL_ENABLE_TIMER_QUERY", false);

    bool basevertex_oes = strstr(exts, "GL_OES_draw_elements_base_vertex") != NULL;
    bool basevertex_ext = strstr(exts, "GL_EXT_draw_elements_base_vertex") != NULL;
    if (ctx->es32)
        ctx->drawelementsbasevertex = es3_functions.glDrawElementsBaseVertex;
    else if (basevertex_oes)
        ctx->drawelementsbasevertex = es3_functions.glDrawElementsBaseVertexOES;
    else if (basevertex_ext)
        ctx->drawelementsbasevertex = es3_functions.glDrawElementsBaseVertexEXT;
    else
        ctx->drawelementsbasevertex = NULL;

    bool drawbuffersi_oes = strstr(exts, "GL_OES_draw_buffers_indexed") != NULL;
    bool drawbuffersi_ext = strstr(exts, "GL_EXT_draw_buffers_indexed") != NULL;
    blending_functions_t *blend = &ctx->blending;

    if (ctx->es32) {
        blend->blendequationi = es3_functions.glBlendEquationi;
        blend->blendequationseparatei = es3_functions.glBlendEquationSeparatei;
        blend->blendfunci = es3_functions.glBlendFunci;
        blend->blendfuncseparatei = es3_functions.glBlendFuncSeparatei;
        blend->colormaski = es3_functions.glColorMaski;
        blend->available = 1;
    } else if (drawbuffersi_oes) {
        blend->blendequationi = es3_functions.glBlendEquationiOES;
        blend->blendequationseparatei = es3_functions.glBlendEquationSeparateiOES;
        blend->blendfunci = es3_functions.glBlendFunciOES;
        blend->blendfuncseparatei = es3_functions.glBlendFuncSeparateiOES;
        blend->colormaski = es3_functions.glColorMaskiOES;
        blend->available = 1;
    } else if (drawbuffersi_ext) {
        blend->blendequationi = es3_functions.glBlendEquationiEXT;
        blend->blendequationseparatei = es3_functions.glBlendEquationSeparateiEXT;
        blend->blendfunci = es3_functions.glBlendFunciEXT;
        blend->blendfuncseparatei = es3_functions.glBlendFuncSeparateiEXT;
        blend->colormaski = es3_functions.glColorMaskiEXT;
        blend->available = 1;
    } else {
        blend->available = 0;
    }

    build_extension_string(ctx);
} // <-- 这里补上了缺失的闭合花括号

extern void basevertex_init(context_t*);
extern void buffer_copier_init(context_t*);

static void init_incontext(context_t *ctx) {
    es3_functions.glGetIntegerv(GL_MAX_TEXTURE_SIZE, &ctx->maxTextureSize);
    es3_functions.glGetIntegerv(GL_MAX_DRAW_BUFFERS, &ctx->max_drawbuffers);
    es3_functions.glGetIntegerv(GL_NUM_EXTENSIONS, &ctx->nextensions_es);
    if (ctx->max_drawbuffers > MAX_DRAWBUFFERS)
        ctx->max_drawbuffers = MAX_DRAWBUFFERS;
    detect_es_version(ctx);
    basevertex_init(ctx);
    buffer_copier_init(ctx);
    es3_functions.glGenBuffers(1, &ctx->multidraw_element_buffer);
}

EGLContext eglCreateContext(EGLDisplay dpy, EGLConfig config,
                            EGLContext share_context, const EGLint *attrib_list) {
    EGLContext phys = host_eglCreateContext(dpy, config, share_context, attrib_list);
    if (phys == EGL_NO_CONTEXT)
        return EGL_NO_CONTEXT;

    context_t *ctx = calloc(1, sizeof(context_t));
    if (!ctx || !init_context(ctx)) {
        free(ctx);
        host_eglDestroyContext(dpy, phys);
        return EGL_NO_CONTEXT;
    }
    unordered_map_put(context_map, (void*)(uintptr_t)phys, ctx);
    return phys;
}

EGLBoolean eglDestroyContext(EGLDisplay dpy, EGLContext ctx) {
    if (!host_eglDestroyContext(dpy, ctx))
        return EGL_FALSE;

    context_t *old = (context_t*)unordered_map_remove(context_map, (void*)(uintptr_t)ctx);
    if (old) {
        free_context(old);
        free(old);
    }
    return EGL_TRUE;
}

EGLBoolean eglMakeCurrent(EGLDisplay dpy, EGLSurface draw,
                          EGLSurface read, EGLContext ctx) {
    if (!host_eglMakeCurrent(dpy, draw, read, ctx))
        return EGL_FALSE;

    if (ctx == EGL_NO_CONTEXT) {
        current_context = NULL;
        return EGL_TRUE;
    }

    context_t *tw_ctx = (context_t*)unordered_map_get(context_map, (void*)(uintptr_t)ctx);
    if (!tw_ctx) {
        LXL_ERROR("Failed to find context %p\n", ctx);
        abort();
    }
    if (!tw_ctx->context_rdy) {
        init_incontext(tw_ctx);
        tw_ctx->context_rdy = 1;
    }
    current_context = tw_ctx;
    return EGL_TRUE;
}