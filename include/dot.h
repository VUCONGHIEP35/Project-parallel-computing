#ifndef DOT_H
#define DOT_H

#include <stddef.h>

#include "hist.h" /* dung lai timing_t cua nhom */

/* Tich vo huong: tra ve sum(a[i] * b[i]), i = 0..n-1.
 * - Dau vao float (dung chung dataset_generate), cong don bang double de giam sai so.
 * - n = 0 tra ve 0; khi do a, b duoc phep la NULL.
 * - tm co the la NULL (khong can do thoi gian).
 * Moi phuong an co cung chu ky de test/bench duyet qua bang dot_fn. */
typedef double (*dot_fn)(const float *a, const float *b, size_t n, timing_t *tm);

/* Cac phuong an (cai dat trong src/dot/). Ten ham = ten trong CSV. */
double dot_seq(const float *a, const float *b, size_t n, timing_t *tm); /* D0 - C: tuan tu */
double dot_reduce(const float *a, const float *b, size_t n, timing_t *tm); /* D1 - C: reduction(+:s) */
double dot_partial(const float *a, const float *b, size_t n, timing_t *tm); /* D2 - C: tong rieng, chia va gop thu cong */
double dot_partial_fs(const float *a, const float *b, size_t n, timing_t *tm); /* D3 - C: tong rieng sat nhau: false sharing */
/* D4.. (atomic, simd) se khai bao them o day */

#endif
