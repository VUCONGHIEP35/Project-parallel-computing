/* Test moi truong: kiem tra GCC + OpenMP chay song song dung.
 * Chay: make test   (tu dong chay voi 1, 2, 4, 8 thread) */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#if !defined(_OPENMP) || _OPENMP < 201511
#error "Can OpenMP 4.5 tro len (bien dich voi -fopenmp)"
#endif

#define N 10000000L
#define B 16

static int fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) printf("  [OK]   %s\n", msg); \
    else { printf("  [FAIL] %s\n", msg); fail = 1; } \
} while (0)

int main(void) {
    int want = omp_get_max_threads();
    printf("OpenMP %d, so thread: %d\n", _OPENMP, want);

    /* 1. Vung song song tao du so thread */
    int n = 0;
    #pragma omp parallel reduction(+:n)
    n += 1;
    CHECK(n == want, "tao du so thread");

    /* 2. Reduction: tong 1..N */
    long long sum = 0;
    #pragma omp parallel for reduction(+:sum)
    for (long i = 1; i <= N; i++) sum += i;
    CHECK(sum == (long long)N * (N + 1) / 2, "reduction tong 1..N");

    /* 3. Atomic: moi thread cong N lan, khong mat lan nao */
    long cnt = 0;
    #pragma omp parallel for
    for (long i = 0; i < N; i++) {
        #pragma omp atomic
        cnt++;
    }
    CHECK(cnt == N, "atomic khong bi mat cap nhat");

    /* 4. Histogram nho: reduction tren mang (OpenMP 4.5), so voi ban tuan tu */
    long h_seq[B] = {0}, h_par[B] = {0};
    for (long i = 0; i < N; i++) h_seq[(i * 7) % B]++;
    #pragma omp parallel for reduction(+:h_par[:B])
    for (long i = 0; i < N; i++) h_par[(i * 7) % B]++;
    int same = 1;
    for (int b = 0; b < B; b++) same &= (h_seq[b] == h_par[b]);
    CHECK(same, "histogram song song == tuan tu");

    /* 5. Do thoi gian bang omp_get_wtime */
    double t = omp_get_wtime();
    CHECK(t > 0, "omp_get_wtime hoat dong");

    printf(fail ? "KET QUA: LOI\n" : "KET QUA: DAT\n");
    return fail;
}
