/* V4 - V0 + reduction tren mang.
 * OpenMP tu tao ban sao out rieng cho moi thread, dem, roi cong vao out.
 * Mac dinh GCC dat ban sao tren stack (bins = 2^20 -> 8 MiB -> segfault). Menh de allocate
 * (OpenMP 5.0) voi omp_large_cap_mem_alloc dua ban sao len heap. omp_default_mem_alloc
 * KHONG du: GCC 15 van dung stack (da thu 22/11).
 * Buoc cong nam trong runtime nen t_merge = 0, t_compute gom ca cap phat va cong ban sao. */
#include "hist.h"

#include <omp.h>
#include <string.h>

void hist_omp_reduce(const float *x, size_t n, int bins, uint64_t *out, timing_t *tm) {
    double t0 = omp_get_wtime();

    /* reduction cong vao gia tri ban dau cua out -> phai xoa truoc */
    memset(out, 0, (size_t)bins * sizeof *out);
    double t1 = omp_get_wtime();

    #pragma omp parallel for schedule(static) reduction(+:out[:bins]) \
            allocate(omp_large_cap_mem_alloc: out)
    for (size_t i = 0; i < n; i++)
        out[bin_of(x[i], bins)]++;
    double t2 = omp_get_wtime();

    if (tm) {
        tm->t_io      = 0.0;
        tm->t_init    = t1 - t0;
        tm->t_compute = t2 - t1;
        tm->t_merge   = 0.0;
        tm->t_total   = t2 - t0;
    }
}
