/* V1 - histogram dung chung, moi lan cong la mot phep atomic (A).
 * Cac thread chia nhau mang x, cung cong vao mot mang out duy nhat.
 * "#pragma omp atomic update" bien out[b]++ (doc - cong - ghi) thanh mot lenh khong the */
#include "hist.h"

#include <omp.h>
#include <string.h>

void hist_atomic(const float *x, size_t n, int bins, uint64_t *out, timing_t *tm) {
    double t0 = omp_get_wtime();

    /* t_init: xoa histogram */
    memset(out, 0, (size_t)bins * sizeof *out);
    double t1 = omp_get_wtime();

    /* t_compute: moi thread nhan mot khoi lien tiep cua x (static) */
    #pragma omp parallel for schedule(static)
    for (size_t i = 0; i < n; i++) {
        int b = bin_of(x[i], bins);
        #pragma omp atomic update
        out[b]++;
    }
    double t2 = omp_get_wtime();

    if (tm) {
        tm->t_io      = 0.0;
        tm->t_init    = t1 - t0;
        tm->t_compute = t2 - t1;
        tm->t_merge   = 0.0;   /* cong thang vao out, khong co buoc merge */
        tm->t_total   = t2 - t0;
    }
}
