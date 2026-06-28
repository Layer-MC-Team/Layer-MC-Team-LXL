#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "string_utils.h"

#pragma GCC visibility push(hidden)

const char* AllSeparators = " \t\n\r.,;()[]{}-<>+*/%&\\\"'^$=!:?";

static inline char* resize_if_needed(char* buf, int *size, int add) {
    int need = strlen(buf) + add + 1;
    if (need > *size) {
        buf = realloc(buf, need);
        *size = need;
    }
    return buf;
}

char* gl4es_inplace_replace(char* buf, int* size, const char* S, const char* D) {
    int lS = strlen(S), lD = strlen(D);
    int count = gl4es_count_string(buf, S);
    buf = resize_if_needed(buf, size, (lD - lS) * count);
    char* p = buf;
    while ((p = strstr(p, S))) {
        if (strchr(AllSeparators, p[lS]) && (p == buf || strchr(AllSeparators, p[-1]))) {
            memmove(p + lD, p + lS, strlen(p) - lS + 1);
            memcpy(p, D, lD);
            p += lD;
        } else {
            p += lS;
        }
    }
    return buf;
}

char* gl4es_inplace_replace_simple(char* buf, int* size, const char* S, const char* D) {
    int lS = strlen(S), lD = strlen(D);
    int count = gl4es_countstring_simple(buf, S);
    buf = resize_if_needed(buf, size, (lD - lS) * count);
    char* p = buf;
    while ((p = strstr(p, S))) {
        memmove(p + lD, p + lS, strlen(p) - lS + 1);
        memcpy(p, D, lD);
        p += lD;
    }
    return buf;
}

int gl4es_count_string(const char* buf, const char* S) {
    int lS = strlen(S), n = 0;
    const char* p = buf;
    while ((p = strstr(p, S))) {
        if (strchr(AllSeparators, p[lS]) && (p == buf || strchr(AllSeparators, p[-1])))
            n++;
        p += lS;
    }
    return n;
}

int gl4es_countstring_simple(char* buf, const char* S) {
    int lS = strlen(S), n = 0;
    char* p = buf;
    while ((p = strstr(p, S))) { n++; p += lS; }
    return n;
}

const char* gl4es_find_string(const char* buf, const char* S) {
    int lS = strlen(S);
    const char* p = buf;
    while ((p = strstr(p, S))) {
        if (strchr(AllSeparators, p[lS]) && (p == buf || strchr(AllSeparators, p[-1])))
            return p;
        p += lS;
    }
    return NULL;
}

char* gl4es_append(char* buf, int* size, const char* S) {
    buf = resize_if_needed(buf, size, strlen(S));
    strcat(buf, S);
    return buf;
}

// 以下函数保留但可能未使用，为保持兼容性保留空实现或简化
char* gl4es_getline(char* buf, int num) {
    char* p = buf;
    while (num-- && (p = strstr(p, "\n"))) p++;
    return p ? p : buf;
}

int gl4es_countline(const char* buf) {
    int n = 0;
    const char* p = buf;
    while ((p = strstr(p, "\n"))) { n++; p++; }
    return n;
}

// 其他函数保持不变，但为了简洁，此处仅列出主要优化函数。
// 若需要使用其他函数，可保留原实现。
#pragma GCC visibility pop(hidden)