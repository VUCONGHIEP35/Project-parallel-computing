/* D0 - tich vo huong tuan tu (C). Moc de tinh speedup va dap an de doi chieu D1-D5. */
#include "dot.h"

#include <omp.h>

double dot_seq(const float *a, const float *b, size_t n, timing_t *tm) {
    double t0 = omp_get_wtime();

    /* t_compute: vong lap chinh, nhan trong double (tich 2 float la chinh xac trong double) */
    double s = 0.0;
    for (size_t i = 0; i < n; i++)
        s += (double)a[i] * (double)b[i];
    double t1 = omp_get_wtime();

    if (tm) {
        tm->t_io      = 0.0;
        tm->t_init    = 0.0;   /* khong cap phat gi them */
        tm->t_compute = t1 - t0;
        tm->t_merge   = 0.0;   /* tuan tu, khong co buoc gop */
        tm->t_total   = t1 - t0;
    }
    return s;
}
