/* Cac phuong an tich vo huong song song (OpenMP) (C).
 * D1 - dot_reduce: parallel for + reduction(+:s), schedule(static).
 * D2 - dot_partial: tu chia khoi, tong rieng trong thanh ghi, 1 thread gop.
 * D3 - dot_partial_fs: nhu D2 nhung cong thang vao mang tong rieng SAT NHAU (false sharing). */
#include "compat.h"
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

/* Moi tong rieng chiem tron 1 cache line (64 byte): cac thread khong ghi chung line. */
typedef struct {
    double v;
    char pad[64 - sizeof(double)];
} padded_double;

/* D2 - lam bang tay ca 3 viec ma reduction(+:s) cua D1 lam ngam:
 * chia viec, giu tong rieng cho moi thread, gop cac tong rieng. */
double dot_partial(const float *a, const float *b, size_t n, timing_t *tm) {
    double t0 = omp_get_wtime();

    /* t_init: mang tong rieng, moi thread 1 o */
    int maxT = omp_get_max_threads();
    padded_double *part = xaligned_alloc(64, (size_t)maxT * sizeof *part);
    if (!part) return dot_seq(a, b, n, tm); /* het bo nho: lui ve D0 */
    int used = 1;
    double t1 = omp_get_wtime();

    /* t_compute: moi thread tu tinh khoi lien tuc [lo, hi) cua minh */
    #pragma omp parallel
    {
        int id = omp_get_thread_num(), nt = omp_get_num_threads();
        size_t lo = n * (size_t)id / (size_t)nt;       /* chia deu, phan le rai deu */
        size_t hi = n * (size_t)(id + 1) / (size_t)nt; /* n < nt: co thread nhan khoi rong */
        double s = 0.0;                                /* bien cuc bo -> nam trong thanh ghi */
        for (size_t i = lo; i < hi; i++)
            s += (double)a[i] * (double)b[i];
        part[id].v = s;                                /* ghi ra bo nho dung 1 lan */
        if (id == 0) used = nt;                        /* chi thread 0 ghi, doc sau barrier */
    } /* barrier ngam: cho moi thread xong */
    double t2 = omp_get_wtime();

    /* t_merge: 1 thread cong cac tong rieng */
    double s = 0.0;
    for (int id = 0; id < used; id++)
        s += part[id].v;
    double t3 = omp_get_wtime();

    xaligned_free(part);
    if (tm) {
        tm->t_io      = 0.0;
        tm->t_init    = t1 - t0;
        tm->t_compute = t2 - t1;
        tm->t_merge   = t3 - t2;
        tm->t_total   = t3 - t0;
    }
    return s;
}

/* D3 - phan vi du: lam cham co chu y de thay tac hai cua false sharing.
 * Khac D2 o 2 diem:
 *  (1) cong THANG vao o part[id] trong mang moi vong lap (khong dung bien cuc bo);
 *  (2) cac o part[] nam SAT NHAU (8 byte/o) -> 8 thread chung 1 cache line (64 byte).
 * Moi thread chi ghi o cua minh nen ket qua van dung; nhung moi lan ghi lam cac core khac mat
 * ban sao cache line, cache line bi chuyen qua lai giua cac core -> cham.
 * volatile: bat buoc, neu khong trinh bien dich se tu giu tong trong thanh ghi (thanh D2).
 * Luu y: volatile tu no cung gay cham (doc/ghi bo nho moi vong), nen D3 cham hon D2 la do
 * CA HAI yeu to; tach rieng false sharing can ban doi chung dem 64 byte (D3b). */
double dot_partial_fs(const float *a, const float *b, size_t n, timing_t *tm) {
    double t0 = omp_get_wtime();

    /* t_init: mang tong rieng KHONG dem, can le 64 byte de thread 0..7 luon chung 1 cache line */
    int maxT = omp_get_max_threads();
    double *part = xaligned_alloc(64, (size_t)maxT * sizeof *part);
    if (!part) return dot_seq(a, b, n, tm); /* het bo nho: lui ve D0 */
    int used = 1;
    double t1 = omp_get_wtime();

    /* t_compute: chia khoi giong D2, nhung cong thang vao o chung cache line */
    #pragma omp parallel
    {
        int id = omp_get_thread_num(), nt = omp_get_num_threads();
        size_t lo = n * (size_t)id / (size_t)nt;
        size_t hi = n * (size_t)(id + 1) / (size_t)nt;
        volatile double *p = &part[id];                /* o cua rieng thread nay trong mang chung */
        *p = 0.0;                                      /* xaligned_alloc khong xoa san bo nho */
        for (size_t i = lo; i < hi; i++)
            *p += (double)a[i] * (double)b[i];         /* doc + ghi bo nho moi vong */
        if (id == 0) used = nt;
    } /* barrier ngam: cho moi thread xong */
    double t2 = omp_get_wtime();

    /* t_merge: 1 thread cong cac tong rieng */
    double s = 0.0;
    for (int id = 0; id < used; id++)
        s += part[id];
    double t3 = omp_get_wtime();

    xaligned_free(part);
    if (tm) {
        tm->t_io      = 0.0;
        tm->t_init    = t1 - t0;
        tm->t_compute = t2 - t1;
        tm->t_merge   = t3 - t2;
        tm->t_total   = t3 - t0;
    }
    return s;
}
