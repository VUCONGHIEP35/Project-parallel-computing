/* V3a, V3b - histogram cuc bo moi thread (B).
 * Moi thread dem vao mang rieng (khong atomic, khong khoa), sau do gop T mang vao out:
 *   V3a hist_local_serial   : 1 thread gop (tuan tu)
 *   V3b hist_local_parmerge : chia cac bin cho cac thread cung gop (song song)
 * Hai ban dung chung buoc cap phat + dem (local_count), chi khac buoc gop. */
#include "hist.h"
#include "compat.h"

#include <omp.h>
#include <stdlib.h>
#include <string.h>

#define CACHE_LINE 64
#define PER_LINE (CACHE_LINE / sizeof(uint64_t))   /* 8 bo dem / cache line */

/* Cap phat + xoa T histogram cuc bo (t_init), roi moi thread dem vao mang cua minh (t_compute).
 * Tra ve mang local gom T mang, mang t bat dau tai local + t * stride. Nguoi goi giai phong. */
static uint64_t *local_count(const float *x, size_t n, int bins,
                             int *T_out, size_t *stride_out, double *t1, double *t2) {
    /* stride lam tron len boi so cua 8 -> moi mang bat dau o dau cache line, tranh false sharing. */
    const int T = omp_get_max_threads();
    const size_t stride = ((size_t)bins + PER_LINE - 1) / PER_LINE * PER_LINE;
    uint64_t *local = xaligned_alloc(CACHE_LINE, (size_t)T * stride * sizeof *local);
    if (!local) abort();

    /* Xoa song song: moi thread "cham" truoc vao mang cua minh (first-touch). */
    #pragma omp parallel for schedule(static) num_threads(T)
    for (int t = 0; t < T; t++)
        memset(local + (size_t)t * stride, 0, stride * sizeof *local);
    *t1 = omp_get_wtime();

    /* Neu runtime cap it thread hon T, cac mang thua van toan 0 -> gop van dung. */
    #pragma omp parallel num_threads(T)
    {
        uint64_t *h = local + (size_t)omp_get_thread_num() * stride;
        #pragma omp for schedule(static)
        for (size_t i = 0; i < n; i++)
            h[bin_of(x[i], bins)]++;
    }
    *t2 = omp_get_wtime();

    *T_out = T;
    *stride_out = stride;
    return local;
}

static void fill_timing(timing_t *tm, double t0, double t1, double t2, double t3) {
    if (!tm) return;
    tm->t_io      = 0.0;
    tm->t_init    = t1 - t0;
    tm->t_compute = t2 - t1;
    tm->t_merge   = t3 - t2;
    tm->t_total   = t3 - t0;
}

void hist_local_serial(const float *x, size_t n, int bins, uint64_t *out, timing_t *tm) {
    double t0 = omp_get_wtime(), t1, t2;
    int T;
    size_t stride;
    uint64_t *local = local_count(x, n, bins, &T, &stride, &t1, &t2);

    /* t_merge: 1 thread cong T mang. Duyet t ngoai, b trong -> doc lien tiep trong bo nho. */
    memcpy(out, local, (size_t)bins * sizeof *out);
    for (int t = 1; t < T; t++) {
        const uint64_t *h = local + (size_t)t * stride;
        for (int b = 0; b < bins; b++)
            out[b] += h[b];
    }
    double t3 = omp_get_wtime();

    xaligned_free(local);
    fill_timing(tm, t0, t1, t2, t3);
}

void hist_local_parmerge(const float *x, size_t n, int bins, uint64_t *out, timing_t *tm) {
    double t0 = omp_get_wtime(), t1, t2;
    int T;
    size_t stride;
    uint64_t *local = local_count(x, n, bins, &T, &stride, &t1, &t2);

    /* t_merge: chia cac bin cho cac thread. Thread giu bin b cong T mang tai bin b
     * -> moi out[b] chi do mot thread ghi, mot lan, khong can atomic hay memset truoc. */
    #pragma omp parallel for schedule(static) num_threads(T)
    for (int b = 0; b < bins; b++) {
        uint64_t s = 0;
        for (int t = 0; t < T; t++)
            s += local[(size_t)t * stride + b];
        out[b] = s;
    }
    double t3 = omp_get_wtime();

    xaligned_free(local);
    fill_timing(tm, t0, t1, t2, t3);
}
