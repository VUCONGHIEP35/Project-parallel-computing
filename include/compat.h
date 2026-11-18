#ifndef COMPAT_H
#define COMPAT_H

#include <stddef.h>
#include <stdlib.h>

/* Cap phat bo nho can le `align` byte (vd 64 = mot cache line).
 * C11 aligned_alloc doi hoi size la boi so cua align, nen lam tron len.
 * MinGW khong co aligned_alloc -> dung _aligned_malloc. Giai phong bang xaligned_free. */
#ifdef _WIN32
#include <malloc.h>
static inline void *xaligned_alloc(size_t align, size_t size) {
    return _aligned_malloc(size ? size : 1, align);
}
static inline void xaligned_free(void *p) { _aligned_free(p); }
#else
static inline void *xaligned_alloc(size_t align, size_t size) {
    size_t rounded = (size + align - 1) / align * align;
    return aligned_alloc(align, rounded ? rounded : align);
}
static inline void xaligned_free(void *p) { free(p); }
#endif

#endif
