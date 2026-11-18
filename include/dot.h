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
double dot_seq(const float *a, const float *b, size_t n, timing_t *tm); /* D0 - tuan tu */
/* D1..D5 (critical, atomic, partial, partial_fs, reduce, simd) se khai bao them o day */

#endif
