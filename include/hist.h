#ifndef HIST_H
#define HIST_H

#include <stddef.h>
#include <stdint.h>

/* Thoi gian tung pha (giay, do bang omp_get_wtime)
typedef struct {
    double t_io;      /* doc file */
    double t_init;    /* cap phat, sinh du lieu, memset histogram */
    double t_compute; /* vong lap chinh */
    double t_merge;   /* reduction cac histogram cuc bo */
    double t_total;
} timing_t;

/* Moi phuong an: dem n phan tu x[i] trong [0,1) vao `bins` bin, ghi ket qua vao out[0..bins-1]. */
typedef void (*hist_fn)(const float *x, size_t n, int bins, uint64_t *out, timing_t *tm);

/* bin(x) = min(floor(x * bins), bins - 1); x < 0 kep ve 0. */
static inline int bin_of(float x, int bins) {
    int b = (int)(x * (float)bins);
    if (b < 0) return 0;
    if (b >= bins) return bins - 1;
    return b;
}

/* Cac phuong an (cai dat trong src/hist/). Ten ham = ten trong CSV. */
void hist_seq(const float *x, size_t n, int bins, uint64_t *out, timing_t *tm);            /* V0  - B */
void hist_atomic(const float *x, size_t n, int bins, uint64_t *out, timing_t *tm);         /* V1  - A */
void hist_critical(const float *x, size_t n, int bins, uint64_t *out, timing_t *tm);       /* V2  - A */
void hist_local_serial(const float *x, size_t n, int bins, uint64_t *out, timing_t *tm);   /* V3a - B */
void hist_local_parmerge(const float *x, size_t n, int bins, uint64_t *out, timing_t *tm); /* V3b - B */
void hist_omp_reduce(const float *x, size_t n, int bins, uint64_t *out, timing_t *tm);     /* V4  - B */

#endif
