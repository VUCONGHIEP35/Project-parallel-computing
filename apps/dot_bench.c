/* Do thoi gian tich vo huong, xuat CSV ra stdout.
 * Chay: ./build/release/bin/dot_bench [n1 n2 ...] > dot.csv
 * Mac dinh n = 10^4 (cache L1/L2), 10^6 (L3), 10^8 (RAM, ~800 MB cho 2 vector). */
#include "dataset.h"
#include "dot.h"

#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#define TRIALS 5
#define MIN_WORK 100000000.0 /* moi lan do lap lai den khi xu ly >= 10^8 phan tu */

static int cmp_double(const void *p, const void *q) {
    double x = *(const double *)p, y = *(const double *)q;
    return (x > y) - (x < y);
}

/* Thoi gian 1 lan goi f (giay): trung vi cua TRIALS lan do.
 * Voi n nho, moi lan do goi f nhieu lan roi chia, de dong ho do du chinh xac. */
static double bench(dot_fn f, const float *a, const float *b, size_t n, double *result) {
    long reps = (long)(MIN_WORK / (double)n);
    if (reps < 1) reps = 1;

    double t[TRIALS];
    volatile double sink = 0.0; /* chan trinh bien dich bo vong lap */
    for (int k = 0; k < TRIALS; k++) {
        double t0 = omp_get_wtime();
        for (long r = 0; r < reps; r++) sink += f(a, b, n, NULL);
        t[k] = (omp_get_wtime() - t0) / (double)reps;
    }
    *result = f(a, b, n, NULL);
    (void)sink;
    qsort(t, TRIALS, sizeof t[0], cmp_double);
    return t[TRIALS / 2];
}

int main(int argc, char **argv) {
    size_t def[] = { 10000, 1000000, 100000000 };
    size_t count = argc > 1 ? (size_t)(argc - 1) : sizeof def / sizeof def[0];

    struct { const char *name; dot_fn f; int parallel; } V[] = {
        { "dot_seq", dot_seq, 0 },
        /* them D1..D5 o day */
    };
    size_t nv = sizeof V / sizeof V[0];

    printf("version,n,threads,time_s,gbps,result\n");
    for (size_t k = 0; k < count; k++) {
        size_t n = argc > 1 ? (size_t)strtoull(argv[k + 1], NULL, 10) : def[k];
        if (n == 0) { fprintf(stderr, "bo qua n = 0\n"); continue; }

        dataset_t A, B;
        if (dataset_generate(&A, DIST_UNIFORM, n, 1) != 0 ||
            dataset_generate(&B, DIST_UNIFORM, n, 2) != 0) {
            fprintf(stderr, "khong cap phat duoc n = %zu\n", n);
            return 1;
        }

        for (size_t v = 0; v < nv; v++) {
            double res, t = bench(V[v].f, A.x, B.x, n, &res);
            int threads = V[v].parallel ? omp_get_max_threads() : 1;
            double gbps = 2.0 * sizeof(float) * (double)n / t / 1e9; /* doc 2 float / phan tu */
            printf("%s,%zu,%d,%.9f,%.3f,%.6f\n", V[v].name, n, threads, t, gbps, res);
            fflush(stdout);
        }
        dataset_free(&A);
        dataset_free(&B);
    }
    return 0;
}
