#ifndef HIST_H
#define HIST_H

#include <stddef.h>
#include <stdint.h>

/* Thoi gian tung pha (giay, do bang omp_get_wtime) */
typedef struct {
    double t_io;      /* doc file */
    double t_init;    /* cap phat, sinh du lieu, memset histogram */
    double t_compute; /* vong lap chinh */
    double t_merge;   /* reduction cac histogram cuc bo */
    double t_total;
} timing_t;

/* Moi phuong an: dem n phan tu x[i] trong [0,1) vao `bins` bin, ghi ket qua vao out[0..bins-1]. */
typedef void (*hist_fn)(const float *x, size_t n, int bins, uint64_t *out, timing_t *tm);

/* bin(x) = min(floor(x * bins), bins - 1); x <= 0 va NaN vao bin 0, x >= 1 va +inf vao bin cuoi.
 * So sanh truoc, ep kieu sau: (int) cua NaN, +-inf, 1e30 la hanh vi khong xac dinh trong C.
 * Dieu kien: 1 <= bins <= 2^24 (bins bieu dien dung bang float).
 * Khong bien dich voi -ffast-math: co nay cho phep bo phep so sanh voi NaN. */
static inline int bin_of(float x, int bins) {
    if (!(x > 0.0f)) return 0;          /* x <= 0, -0, NaN (moi so sanh voi NaN deu false) */
    if (x >= 1.0f) return bins - 1;     /* x >= 1, +inf */
    int b = (int)(x * (float)bins);     /* 0 < x < 1 -> 0 <= x*bins <= bins: ep kieu an toan */
    return b < bins ? b : bins - 1;     /* x sat 1 (0.99999994f): x*bins co the lam tron thanh bins */
}

/* Cac phuong an (cai dat trong src/hist/). Ten ham = ten trong CSV. */
void hist_seq(const float *x, size_t n, int bins, uint64_t *out, timing_t *tm);            /* V0  - B */
void hist_atomic(const float *x, size_t n, int bins, uint64_t *out, timing_t *tm);         /* V1  - A */
void hist_critical(const float *x, size_t n, int bins, uint64_t *out, timing_t *tm);       /* V2  - A */
void hist_local_serial(const float *x, size_t n, int bins, uint64_t *out, timing_t *tm);   /* V3a - B */
void hist_local_parmerge(const float *x, size_t n, int bins, uint64_t *out, timing_t *tm); /* V3b - B */
void hist_omp_reduce(const float *x, size_t n, int bins, uint64_t *out, timing_t *tm);     /* V4  - B */
void hist_local_nopad(const float *x, size_t n, int bins, uint64_t *out, timing_t *tm);    /* E7: V3b khong padding - B */

#endif
