/* V3a - histogram cuc bo moi thread, merge tuan tu (B).
 * Moi thread dem vao mang rieng (khong atomic, khong khoa), sau do 1 thread cong T mang vao out. */
#include "hist.h"
#include "compat.h"

#include <omp.h>
#include <stdlib.h>
#include <string.h>

#define CACHE_LINE 64
#define PER_LINE (CACHE_LINE / sizeof(uint64_t))   /* 8 bo dem / cache line */

void hist_local_serial(const float *x, size_t n, int bins, uint64_t *out, timing_t *tm) {
    double t0 = omp_get_wtime();

    /* t_init: cap phat + xoa T histogram cuc bo.
     * stride lam tron len boi so cua 8 -> moi mang bat dau o dau cache line, tranh false sharing. */
    const int T = omp_get_max_threads();
    const size_t stride = ((size_t)bins + PER_LINE - 1) / PER_LINE * PER_LINE;
    uint64_t *local = xaligned_alloc(CACHE_LINE, (size_t)T * stride * sizeof *local);
    if (!local) abort();

    /* Xoa song song: moi thread "cham" truoc vao mang cua minh (first-touch). */
    #pragma omp parallel for schedule(static) num_threads(T)
    for (int t = 0; t < T; t++)
        memset(local + (size_t)t * stride, 0, stride * sizeof *local);
    double t1 = omp_get_wtime();

    /* t_compute: moi thread dem vao mang cua minh */
    #pragma omp parallel num_threads(T)
    {
        uint64_t *h = local + (size_t)omp_get_thread_num() * stride;
        #pragma omp for schedule(static)
        for (size_t i = 0; i < n; i++)
            h[bin_of(x[i], bins)]++;
    }
    double t2 = omp_get_wtime();

    /* t_merge: 1 thread cong T mang. Duyet t ngoai, b trong -> doc lien tiep trong bo nho. */
    memcpy(out, local, (size_t)bins * sizeof *out);
    for (int t = 1; t < T; t++) {
        const uint64_t *h = local + (size_t)t * stride;
        for (int b = 0; b < bins; b++)
            out[b] += h[b];
    }
    double t3 = omp_get_wtime();

    xaligned_free(local);

    if (tm) {
        tm->t_io      = 0.0;
        tm->t_init    = t1 - t0;
        tm->t_compute = t2 - t1;
        tm->t_merge   = t3 - t2;
        tm->t_total   = t3 - t0;
    }
}
