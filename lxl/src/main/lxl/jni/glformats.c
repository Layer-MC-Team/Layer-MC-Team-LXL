/**
 * Created by: Layer
 * Copyright (c) 2026 Layer.
 * For use under LGPL-3.0
 */

#include <stdbool.h>
#include "egl.h"
#include "glformats.h"
#include "libraryinternal.h"
#include "GL/gl.h"
#include <stdio.h>

// 静态查找表：internalformat -> {format, type}
typedef struct { GLenum format; GLenum type; } fmt_entry;
static const fmt_entry fmt_table[] = {
    [GL_RGB] = {GL_RGB, GL_UNSIGNED_BYTE},
    [GL_RGBA] = {GL_RGBA, GL_UNSIGNED_BYTE},
    [GL_LUMINANCE_ALPHA] = {GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE},
    [GL_LUMINANCE] = {GL_LUMINANCE, GL_UNSIGNED_BYTE},
    [GL_ALPHA] = {GL_ALPHA, GL_UNSIGNED_BYTE},
    [GL_R8] = {GL_RED, GL_UNSIGNED_BYTE},
    [GL_R8_SNORM] = {GL_RED, GL_BYTE},
    [GL_R16F] = {GL_RED, GL_HALF_FLOAT},
    [GL_R32F] = {GL_RED, GL_FLOAT},
    [GL_R8UI] = {GL_RED_INTEGER, GL_UNSIGNED_BYTE},
    [GL_R8I] = {GL_RED_INTEGER, GL_BYTE},
    [GL_R16UI] = {GL_RED_INTEGER, GL_UNSIGNED_SHORT},
    [GL_R16I] = {GL_RED_INTEGER, GL_SHORT},
    [GL_R32UI] = {GL_RED_INTEGER, GL_UNSIGNED_INT},
    [GL_R32I] = {GL_RED_INTEGER, GL_INT},
    [GL_RG8] = {GL_RG, GL_UNSIGNED_BYTE},
    [GL_RG8_SNORM] = {GL_RG, GL_BYTE},
    [GL_RG16F] = {GL_RG, GL_HALF_FLOAT},
    [GL_RG32F] = {GL_RG, GL_FLOAT},
    [GL_RG8UI] = {GL_RG_INTEGER, GL_UNSIGNED_BYTE},
    [GL_RG8I] = {GL_RG_INTEGER, GL_BYTE},
    [GL_RG16UI] = {GL_RG_INTEGER, GL_UNSIGNED_SHORT},
    [GL_RG16I] = {GL_RG_INTEGER, GL_SHORT},
    [GL_RG32UI] = {GL_RG_INTEGER, GL_UNSIGNED_INT},
    [GL_RG32I] = {GL_RG_INTEGER, GL_INT},
    [GL_RGB8] = {GL_RGB, GL_UNSIGNED_BYTE},
    [GL_SRGB8] = {GL_RGB, GL_UNSIGNED_BYTE},
    [GL_RGB565] = {GL_RGB, GL_UNSIGNED_BYTE},
    [GL_RGB8_SNORM] = {GL_RGB, GL_BYTE},
    [GL_R11F_G11F_B10F] = {GL_RGB, GL_FLOAT},
    [GL_RGB9_E5] = {GL_RGB, GL_UNSIGNED_INT_5_9_9_9_REV},
    [GL_RGB16F] = {GL_RGB, GL_HALF_FLOAT},
    [GL_RGB32F] = {GL_RGB, GL_FLOAT},
    [GL_RGB8UI] = {GL_RGB_INTEGER, GL_UNSIGNED_BYTE},
    [GL_RGB8I] = {GL_RGB_INTEGER, GL_BYTE},
    [GL_RGB16UI] = {GL_RGB_INTEGER, GL_UNSIGNED_SHORT},
    [GL_RGB16I] = {GL_RGB_INTEGER, GL_SHORT},
    [GL_RGB32UI] = {GL_RGB_INTEGER, GL_UNSIGNED_INT},
    [GL_RGB32I] = {GL_RGB_INTEGER, GL_INT},
    [GL_RGBA8] = {GL_RGBA, GL_UNSIGNED_BYTE},
    [GL_SRGB8_ALPHA8] = {GL_RGBA, GL_UNSIGNED_BYTE},
    [GL_RGBA8_SNORM] = {GL_RGBA, GL_BYTE},
    [GL_RGB5_A1] = {GL_RGBA, GL_UNSIGNED_BYTE},
    [GL_RGBA4] = {GL_RGBA, GL_UNSIGNED_BYTE},
    [GL_RGB10_A2] = {GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV},
    [GL_RGBA16F] = {GL_RGBA, GL_HALF_FLOAT},
    [GL_RGBA32F] = {GL_RGBA, GL_FLOAT},
    [GL_RGBA8UI] = {GL_RGBA_INTEGER, GL_UNSIGNED_BYTE},
    [GL_RGBA8I] = {GL_RGBA_INTEGER, GL_BYTE},
    [GL_RGB10_A2UI] = {GL_RGBA_INTEGER, GL_UNSIGNED_INT_2_10_10_10_REV},
    [GL_RGBA16UI] = {GL_RGBA_INTEGER, GL_UNSIGNED_SHORT},
    [GL_RGBA16I] = {GL_RGBA_INTEGER, GL_SHORT},
    [GL_RGBA32I] = {GL_RGBA_INTEGER, GL_INT},
    [GL_RGBA32UI] = {GL_RGBA_INTEGER, GL_UNSIGNED_INT},
    [GL_DEPTH_COMPONENT16] = {GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT},
    [GL_DEPTH_COMPONENT24] = {GL_DEPTH_COMPONENT, GL_UNSIGNED_INT},
    [GL_DEPTH_COMPONENT32F] = {GL_DEPTH_COMPONENT, GL_FLOAT},
    [GL_DEPTH24_STENCIL8] = {GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8},
    [GL_DEPTH32F_STENCIL8] = {GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV},
    [GL_STENCIL_INDEX8] = {GL_STENCIL_INDEX, GL_UNSIGNED_BYTE},
};

void pick_format(GLint *internalformat, GLenum* type, GLenum* format) {
    switch (*internalformat) {
        case GL_RGBA12:
        case GL_RGBA16:
            *internalformat = GL_RGBA16F; break;
        case GL_DEPTH_COMPONENT:
            *internalformat = GL_DEPTH_COMPONENT32F; break;
        case GL_DEPTH_COMPONENT32:
            *internalformat = GL_DEPTH_COMPONENT32F; break;
        case GL_DEPTH_STENCIL:
            *internalformat = GL_DEPTH24_STENCIL8; break;
        case GL_R8_SNORM:
            *internalformat = GL_R16F; break;
        case GL_RG8_SNORM:
            *internalformat = GL_RG16F; break;
        case GL_RGBA8_SNORM:
            *internalformat = GL_RGBA16F; break;
        case GL_RGB8I:
        case GL_RGB16I:
        case GL_RGB32I:
        case GL_RGB8_SNORM:
        case GL_RGB12:
        case GL_RGB16:
        case GL_RGB16F:
        case GL_RGB32F:
            *internalformat = GL_R11F_G11F_B10F; break;
        case GL_RGB8UI:
            *internalformat = GL_RGB8; break;
        default:
            if (*internalformat >= 0 && *internalformat < sizeof(fmt_table)/sizeof(fmt_table[0])) {
                *format = fmt_table[*internalformat].format;
                *type = fmt_table[*internalformat].type;
                return;
            }
            printf("LXL: unknown format %x\n", *internalformat);
            *format = GL_RGBA;
            *type = GL_UNSIGNED_BYTE;
            return;
    }
    // 重新查表
    if (*internalformat >= 0 && *internalformat < sizeof(fmt_table)/sizeof(fmt_table[0])) {
        *format = fmt_table[*internalformat].format;
        *type = fmt_table[*internalformat].type;
    }
}

INTERNAL void pick_internalformat(GLint *internalformat, GLenum* type, GLenum* format,
                                  GLvoid const** data) {
    if (*data == NULL) {
        pick_format(internalformat, type, format);
        return;
    }
    // 处理深度和特殊格式
    switch (*internalformat) {
        case GL_DEPTH_COMPONENT32:
            if (*type == GL_FLOAT) {
                *internalformat = GL_DEPTH_COMPONENT32F;
            } else {
                *internalformat = GL_DEPTH_COMPONENT24;
                *type = GL_UNSIGNED_INT;
            }
            break;
        case GL_DEPTH_COMPONENT:
            if (*type == GL_FLOAT) *internalformat = GL_DEPTH_COMPONENT32F;
            else *internalformat = GL_DEPTH_COMPONENT16;
            break;
        case GL_DEPTH_STENCIL:
            if (*type == GL_FLOAT_32_UNSIGNED_INT_24_8_REV)
                *internalformat = GL_DEPTH32F_STENCIL8;
            else
                *internalformat = GL_DEPTH24_STENCIL8;
            break;
        case GL_RED:
            if (*type == GL_FLOAT) *internalformat = GL_R32F;
            else if (*type == GL_HALF_FLOAT) *internalformat = GL_R16F;
            else *internalformat = GL_R8;
            break;
        case GL_RG:
            if (*type == GL_FLOAT) *internalformat = GL_RG32F;
            else if (*type == GL_HALF_FLOAT) *internalformat = GL_RG16F;
            else *internalformat = GL_RG8;
            break;
        case GL_R8I: case GL_R8UI: case GL_R16I: case GL_R16UI:
        case GL_R32I: case GL_R32UI:
            *format = GL_RED_INTEGER;
            break;
        case GL_RG8I: case GL_RG8UI: case GL_RG16I: case GL_RG16UI:
        case GL_RG32I: case GL_RG32UI:
            *format = GL_RG_INTEGER;
            break;
        case GL_RGB8I: case GL_RGB8UI: case GL_RGB16I: case GL_RGB16UI:
        case GL_RGB32I: case GL_RGB32UI:
            *format = GL_RGB_INTEGER;
            break;
        case GL_RGBA8I: case GL_RGBA8UI: case GL_RGBA16I: case GL_RGBA16UI:
        case GL_RGBA32I: case GL_RGBA32UI:
            *format = GL_RGBA_INTEGER;
            break;
        default: {
            // 尝试查表
            if (*internalformat >= 0 && *internalformat < sizeof(fmt_table)/sizeof(fmt_table[0])) {
                *format = fmt_table[*internalformat].format;
                *type = fmt_table[*internalformat].type;
                return;
            }
            // 兜底：RGB浮点映射到R11F_G11F_B10F
            if (*format == GL_RGB && (*type == GL_FLOAT || *type == GL_HALF_FLOAT)) {
                *internalformat = GL_R11F_G11F_B10F;
                *type = GL_FLOAT;
            } else if (*format == GL_RGBA && *type == GL_FLOAT) {
                *internalformat = GL_RGBA32F;
            } else if (*format == GL_RGBA && *type == GL_HALF_FLOAT) {
                *internalformat = GL_RGBA16F;
            } else {
                // 未知格式，使用默认
                *format = GL_RGBA;
                *type = GL_UNSIGNED_BYTE;
            }
        }
    }
    // 二次查表确保完整
    if (*internalformat >= 0 && *internalformat < sizeof(fmt_table)/sizeof(fmt_table[0])) {
        *format = fmt_table[*internalformat].format;
        *type = fmt_table[*internalformat].type;
    }
}