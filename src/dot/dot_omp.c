/* Cac phuong an tich vo huong song song (OpenMP).
 * D1 - dot_reduce: parallel for + reduction(+:s), schedule(static). */
#include "dot.h"

#include <omp.h>

double dot_reduce(const float *a, const float *b, size_t n, timing_t *tm) {
    double t0 = omp_get_wtime();

    /* Moi thread giu mot ban s rieng (khoi lien tuc ~ n/T phan tu), cuoi vung
     * song song OpenMP cong cac ban rieng lai. Cong don bang double nhu D0. */
    double s = 0.0;
#pragma omp parallel for schedule(static) reduction(+ : s)
    for (size_t i = 0; i < n; i++)
        s += (double)a[i] * (double)b[i];
    double t1 = omp_get_wtime();

    if (tm) {
        tm->t_io      = 0.0;
        tm->t_init    = 0.0;
        tm->t_compute = t1 - t0; /* gom ca buoc gop do runtime lo, khong tach rieng duoc */
        tm->t_merge   = 0.0;
        tm->t_total   = t1 - t0;
    }
    return s;
}
