/* V0 - histogram tuan tu (B). Moc de tinh speedup va dap an de doi chieu V1-V4. */
#include "hist.h"

#include <omp.h>
#include <string.h>

void hist_seq(const float *x, size_t n, int bins, uint64_t *out, timing_t *tm) {
    double t0 = omp_get_wtime();

    /* t_init: xoa histogram */
    memset(out, 0, (size_t)bins * sizeof *out);
    double t1 = omp_get_wtime();

    /* t_compute: vong lap chinh */
    for (size_t i = 0; i < n; i++)
        out[bin_of(x[i], bins)]++;
    double t2 = omp_get_wtime();

    if (tm) {
        tm->t_io      = 0.0;
        tm->t_init    = t1 - t0;
        tm->t_compute = t2 - t1;
        tm->t_merge   = 0.0;   /* tuan tu, khong co buoc merge */
        tm->t_total   = t2 - t0;
    }
}
