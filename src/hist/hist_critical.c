/* V2 - histogram dung chung, moi lan cong nam trong vung critical (A).
 * "#pragma omp critical" chi cho mot thread vao doan ma tai mot thoi diem, bat ke bin nao. */
#include "hist.h"

#include <omp.h>
#include <string.h>

void hist_critical(const float *x, size_t n, int bins, uint64_t *out, timing_t *tm) {
    double t0 = omp_get_wtime();

    /* t_init: xoa histogram */
    memset(out, 0, (size_t)bins * sizeof *out);
    double t1 = omp_get_wtime();

    /* t_compute: tinh bin ngoai vung critical (song song duoc), chi phep cong nam trong */
    #pragma omp parallel for schedule(static)
    for (size_t i = 0; i < n; i++) {
        int b = bin_of(x[i], bins);
        #pragma omp critical(hist_critical_update)
        out[b]++;
    }
    double t2 = omp_get_wtime();

    if (tm) {
        tm->t_io      = 0.0;
        tm->t_init    = t1 - t0;
        tm->t_compute = t2 - t1;
        tm->t_merge   = 0.0;
        tm->t_total   = t2 - t0;
    }
}
