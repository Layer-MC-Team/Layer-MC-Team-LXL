/**
 * Created by: Layer
 * Copyright (c) 2026 Layer.
 * For use under LGPL-3.0
 */

#include "proc.h"
#include "egl.h"
#include <string.h>
#include "libraryinternal.h"

#define GL_TEXTURE_SWIZZLE_RGBA 0x8E46

static inline void swizzle_process_bgra(GLenum* swizzle) {
    GLenum tmp = swizzle[0];
    swizzle[0] = swizzle[2];
    swizzle[2] = tmp;
}

static inline void swizzle_process_endianness(GLenum* swizzle) {
    GLenum orig[4];
    memcpy(orig, swizzle, 4 * sizeof(GLenum));
    swizzle[0] = orig[3];
    swizzle[1] = orig[2];
    swizzle[2] = orig[1];
    swizzle[3] = orig[0];
}

static texture_swizzle_track_t* get_swizzle_track(GLenum target) {
    GLenum getter = get_textarget_query_param(target);
    if (!getter) return NULL;
    GLint texture;
    es3_functions.glGetIntegerv(getter, &texture);
    if (!texture) return NULL;
    texture_swizzle_track_t* track = unordered_map_get(
        current_context->texture_swztrack_map, (void*)(uintptr_t)texture);
    if (track) return track;
    track = malloc(sizeof(texture_swizzle_track_t));
    if (!track) return NULL;
    es3_functions.glGetTexParameteriv(target, GL_TEXTURE_SWIZZLE_R,
                                      (GLint*)&track->original_swizzle[0]);
    es3_functions.glGetTexParameteriv(target, GL_TEXTURE_SWIZZLE_G,
                                      (GLint*)&track->original_swizzle[1]);
    es3_functions.glGetTexParameteriv(target, GL_TEXTURE_SWIZZLE_B,
                                      (GLint*)&track->original_swizzle[2]);
    es3_functions.glGetTexParameteriv(target, GL_TEXTURE_SWIZZLE_A,
                                      (GLint*)&track->original_swizzle[3]);
    track->goofy_byte_order = false;
    track->upload_bgra = false;
    unordered_map_put(current_context->texture_swztrack_map,
                      (void*)(uintptr_t)texture, track);
    return track;
}

static void apply_swizzles(GLenum target, texture_swizzle_track_t* track) {
    GLenum new_swizzle[4];
    memcpy(new_swizzle, track->original_swizzle, 4 * sizeof(GLenum));
    if (track->goofy_byte_order) swizzle_process_endianness(new_swizzle);
    if (track->upload_bgra) swizzle_process_bgra(new_swizzle);
    es3_functions.glTexParameteri(target, GL_TEXTURE_SWIZZLE_R, new_swizzle[0]);
    es3_functions.glTexParameteri(target, GL_TEXTURE_SWIZZLE_G, new_swizzle[1]);
    es3_functions.glTexParameteri(target, GL_TEXTURE_SWIZZLE_B, new_swizzle[2]);
    es3_functions.glTexParameteri(target, GL_TEXTURE_SWIZZLE_A, new_swizzle[3]);
}

INTERNAL void swizzle_process_upload(GLenum target, GLenum* format, GLenum* type) {
    texture_swizzle_track_t* track = get_swizzle_track(target);
    if (!track) return;
    bool apply_bgra = false, apply_goofy = false;
    if (*format == GL_BGRA_EXT) {
        apply_bgra = true;
        *format = GL_RGBA;
    }
    if (*type == 0x8035) { // GL_UNSIGNED_BYTE_3_3_2
        apply_goofy = true;
        *type = GL_UNSIGNED_BYTE;
    }
    if (*type == 0x8367) { // GL_UNSIGNED_BYTE_2_3_3_REV
        *type = GL_UNSIGNED_BYTE;
    }
    if (apply_goofy != track->goofy_byte_order || apply_bgra != track->upload_bgra) {
        track->goofy_byte_order = apply_goofy;
        track->upload_bgra = apply_bgra;
        apply_swizzles(target, track);
    }
}

INTERNAL void swizzle_process_swizzle_param(GLenum target, GLenum swizzle_param,
                                            const GLenum* swizzle) {
    if (swizzle_param < GL_TEXTURE_SWIZZLE_R || swizzle_param > GL_TEXTURE_SWIZZLE_RGBA)
        return;
    texture_swizzle_track_t* track = get_swizzle_track(target);
    if (!track) return;
    if (swizzle_param == GL_TEXTURE_SWIZZLE_RGBA) {
        memcpy(track->original_swizzle, swizzle, 4 * sizeof(GLenum));
    } else {
        track->original_swizzle[swizzle_param - GL_TEXTURE_SWIZZLE_R] = *swizzle;
    }
    apply_swizzles(target, track);
}