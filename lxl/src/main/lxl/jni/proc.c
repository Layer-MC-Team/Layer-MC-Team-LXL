/**
 * Created by: Layer
 * Copyright (c) 2026 Layer.
 * For use under LGPL-3.0
 */

#include <EGL/egl.h>
#include <GLES3/gl31.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <android/log.h>
#include <string.h>
#include "proc.h"
#include "egl.h"
#include "libraryinternal.h"
#define GL_GLEXT_PROTOTYPES
#include "GL/gl.h"
#include "GL/glext.h"

INTERNAL eglMustCastToProperFunctionPointerType (*host_eglGetProcAddress)(const char*);
INTERNAL es3_functions_t es3_functions;

static void log_fatal(const char* msg, const char* detail) {
    __android_log_print(ANDROID_LOG_ERROR, "LXLInit", "%s: %s", msg, detail ? detail : "");
}

static void error_sysegl(void) {
    log_fatal("Failed to load system EGL", dlerror());
    abort();
}

static void error_init_function(const char* funcName) {
    log_fatal("Failed to load function", funcName);
    abort();
}

static void init_es3_proc(void) {
#define GLESFUNC(name, type) \
    do { \
        es3_functions.name = (type)host_eglGetProcAddress(#name); \
        if (es3_functions.name == NULL) error_init_function(#name); \
    } while (0)
#include "es3_functions.h"
#undef GLESFUNC

#define GLESFUNC(name, type) \
    es3_functions.name = (type)host_eglGetProcAddress(#name);
#include "es3_extended.h"
#undef GLESFUNC
}

__attribute__((constructor, used)) static void proc_init(void) {
    const char* eglPath = getenv("LIBGL_EGL");
    if (!eglPath) eglPath = "libEGL.so";
    void* eglHandle = dlopen(eglPath, RTLD_LAZY | RTLD_LOCAL);
    if (!eglHandle) {
        eglHandle = dlopen("libEGL.so", RTLD_LAZY | RTLD_LOCAL);
        if (!eglHandle) error_sysegl();
    }
    host_eglGetProcAddress = (typeof(host_eglGetProcAddress))dlsym(eglHandle, "eglGetProcAddress");
    if (!host_eglGetProcAddress) error_sysegl();
    init_egl();
    init_es3_proc();
}

__attribute__((used))
eglMustCastToProperFunctionPointerType glXGetProcAddress(const char* procname) {
    return eglGetProcAddress(procname);
}

extern void* resolve_stub(const char* procname);

eglMustCastToProperFunctionPointerType eglGetProcAddress(const char* procname) {
    if (procname[0] == 'e' && procname[1] == 'g' && procname[2] == 'l') {
        if (!strcmp(procname, "eglCreateContext"))
            return (eglMustCastToProperFunctionPointerType)eglCreateContext;
        if (!strcmp(procname, "eglDestroyContext"))
            return (eglMustCastToProperFunctionPointerType)eglDestroyContext;
        if (!strcmp(procname, "eglMakeCurrent"))
            return (eglMustCastToProperFunctionPointerType)eglMakeCurrent;
    }
    if (procname[0] != 'g' || procname[1] != 'l')
        goto fallback;

#define GLESOVERRIDE(name) \
    if (!strcmp(procname, #name)) return (eglMustCastToProperFunctionPointerType)name;
#include "es3_overrides.h"
#undef GLESOVERRIDE

fallback:
    eglMustCastToProperFunctionPointerType func = host_eglGetProcAddress(procname);
    return func ? func : (eglMustCastToProperFunctionPointerType)resolve_stub(procname);
}