/* Test tich vo huong (C). Chay: make test
 * D0 (dot_seq) va cac phuong an song song trong bang VARIANTS (D1-D3), doi chieu voi tong Kahan:
 * sai so tuong doi < 1e-9. Them phuong an moi vao VARIANTS de duoc doi chieu. */
#include "dataset.h"
#include "dot.h"

#include <math.h>
#include <omp.h>
#include <stdio.h>

static int fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) printf("  [OK]   %s\n", msg); \
    else { printf("  [FAIL] %s\n", msg); fail = 1; } \
} while (0)

/* Dap an tham chieu: cong Kahan (bu sai so lam tron) trong double. */
static double dot_kahan(const float *a, const float *b, size_t n) {
    double s = 0.0, c = 0.0;
    for (size_t i = 0; i < n; i++) {
        double y = (double)a[i] * (double)b[i] - c;
        double t = s + y;
        c = (t - s) - y;
        s = t;
    }
    return s;
}

/* Cac ca nho biet truoc dap an */
static void test_known(void) {
    const float a[] = { 1, 2, 3 }, b[] = { 4, 5, 6 };
    CHECK(dot_seq(a, b, 3, NULL) == 32.0, "{1,2,3}.{4,5,6} = 32");

    CHECK(dot_seq(NULL, NULL, 0, NULL) == 0.0, "n = 0 -> 0 (a, b = NULL)");

    const float c[] = { 3 }, d[] = { -2 };
    CHECK(dot_seq(c, d, 1, NULL) == -6.0, "n = 1: 3 * (-2) = -6");

    const float e1[] = { 1, 0 }, e2[] = { 0, 1 };
    CHECK(dot_seq(e1, e2, 2, NULL) == 0.0, "hai vector vuong goc -> 0");

    const float p[] = { 0.5f, -1.5f, 2.25f, -0.25f }, q[] = { 2, 2, 4, -4 };
    CHECK(dot_seq(p, q, 4, NULL) == 8.0, "so am va so le: 1 - 3 + 9 + 1 = 8");
}

/* Bien cong don phai la double: 2^24 + 1 + 1 + ... (1000 lan).
 * Neu cong bang float, moi lan +1 bi lam tron mat (float chi co 24 bit) -> ra 16777216. */
static void test_double_accumulator(void) {
    enum { M = 1001 };
    static float a[M], b[M];
    a[0] = 16777216.0f; /* 2^24 */
    b[0] = 1.0f;
    for (int i = 1; i < M; i++) { a[i] = 1.0f; b[i] = 1.0f; }
    CHECK(dot_seq(a, b, M, NULL) == 16778216.0, "cong don bang double: 2^24 + 1000 = 16778216");
}

/* Tinh chat tren du lieu uniform N = 10^6 */
static void test_uniform(void) {
    const size_t N = 1000000;
    dataset_t A, B, ONE;
    if (dataset_generate(&A, DIST_UNIFORM, N, 1) != 0 ||
        dataset_generate(&B, DIST_UNIFORM, N, 2) != 0 ||
        dataset_generate(&ONE, DIST_UNIFORM, N, 3) != 0) {
        CHECK(0, "sinh du lieu uniform N = 10^6");
        return;
    }
    for (size_t i = 0; i < N; i++) ONE.x[i] = 1.0f;

    timing_t tm;
    double ab = dot_seq(A.x, B.x, N, &tm);

    /* a.1 = tong cac phan tu cua a (cung thu tu cong -> bang tuyet doi) */
    double sum_a = 0.0;
    for (size_t i = 0; i < N; i++) sum_a += A.x[i];
    CHECK(dot_seq(A.x, ONE.x, N, NULL) == sum_a, "a.(1,1,...,1) = tong cac phan tu cua a");

    CHECK(dot_seq(B.x, A.x, N, NULL) == ab, "giao hoan: a.b == b.a");
    CHECK(dot_seq(A.x, A.x, N, NULL) >= 0.0, "a.a >= 0");

    /* Ky vong E[a.b] = N * 0.5 * 0.5 = N/4; lech > 1% la bat thuong */
    CHECK(fabs(ab / (double)N - 0.25) < 0.0025, "uniform: a.b / N xap xi 0.25");

    double ref = dot_kahan(A.x, B.x, N);
    CHECK(fabs(ab - ref) / fabs(ref) < 1e-9, "sai so tuong doi so voi Kahan < 1e-9");

    CHECK(dot_seq(A.x, B.x, N, NULL) == ab, "chay lai cho cung ket qua");

    CHECK(tm.t_compute >= 0 && tm.t_merge == 0 && tm.t_total >= tm.t_compute,
          "timing hop le (t_merge = 0 voi D0)");

    dataset_free(&A);
    dataset_free(&B);
    dataset_free(&ONE);
}

/* Cac phuong an song song can doi chieu voi D0. Them phuong an moi vao day. */
static const struct { const char *name; dot_fn f; } VARIANTS[] = {
    { "dot_reduce", dot_reduce },
    { "dot_partial", dot_partial },
    { "dot_partial_fs", dot_partial_fs },
    { "dot_partial_pad", dot_partial_pad },
};
#define NVARIANTS (sizeof VARIANTS / sizeof VARIANTS[0])

/* Thu tu cong khac D0 nen khong bang tuyet doi: so voi Kahan bang sai so tuong doi. */
static void test_variants(void) {
    const int threads[] = { 1, 2, 4, 8 };
    char msg[160];

    const size_t N = 1000003; /* khong chia het cho T */
    dataset_t A, B;
    if (dataset_generate(&A, DIST_UNIFORM, N, 1) != 0 ||
        dataset_generate(&B, DIST_UNIFORM, N, 2) != 0) {
        CHECK(0, "sinh du lieu cho test_variants");
        return;
    }
    double ref = dot_kahan(A.x, B.x, N);

    for (size_t v = 0; v < NVARIANTS; v++) {
        printf(" %s\n", VARIANTS[v].name);
        dot_fn f = VARIANTS[v].f;
        for (size_t k = 0; k < sizeof threads / sizeof threads[0]; k++) {
            int T = threads[k];
            omp_set_num_threads(T);

            /* ca biet truoc, bien */
            const float a[] = { 1, 2, 3 }, b[] = { 4, 5, 6 };
            snprintf(msg, sizeof msg, "T=%d: {1,2,3}.{4,5,6} = 32 (n < T khi T > 3)", T);
            CHECK(f(a, b, 3, NULL) == 32.0, msg);
            snprintf(msg, sizeof msg, "T=%d: n = 0 -> 0 (a, b = NULL)", T);
            CHECK(f(NULL, NULL, 0, NULL) == 0.0, msg);
            const float c[] = { 3 }, d[] = { -2 };
            snprintf(msg, sizeof msg, "T=%d: n = 1 -> -6", T);
            CHECK(f(c, d, 1, NULL) == -6.0, msg);

            /* a[i] = i, b[i] = 1 -> n(n-1)/2, so nguyen nho nen chinh xac tuyet doi */
            enum { M = 1000 };
            static float ai[M], bi[M];
            for (int i = 0; i < M; i++) { ai[i] = (float)i; bi[i] = 1.0f; }
            snprintf(msg, sizeof msg, "T=%d: a[i]=i, b[i]=1 -> n(n-1)/2", T);
            CHECK(f(ai, bi, M, NULL) == (double)M * (M - 1) / 2, msg);

            /* so voi Kahan, lap 20 lan de bat race */
            int ok = 1;
            for (int r = 0; r < 20; r++) {
                double got = f(A.x, B.x, N, NULL);
                if (fabs(got - ref) / fabs(ref) >= 1e-9) ok = 0;
            }
            snprintf(msg, sizeof msg, "T=%d: sai so tuong doi so voi Kahan < 1e-9 (20 lan)", T);
            CHECK(ok, msg);

            timing_t tm;
            f(A.x, B.x, N, &tm);
            snprintf(msg, sizeof msg, "T=%d: timing hop le", T);
            CHECK(tm.t_compute >= 0 && tm.t_total >= tm.t_compute, msg);
        }
    }
    omp_set_num_threads(omp_get_num_procs());
    dataset_free(&A);
    dataset_free(&B);
}

/* D2 tach duoc 3 pha: init (cap phat), compute (tinh), merge (gop) */
static void test_partial_timing(void) {
    const size_t N = 1000000;
    dataset_t A, B;
    if (dataset_generate(&A, DIST_UNIFORM, N, 1) != 0 ||
        dataset_generate(&B, DIST_UNIFORM, N, 2) != 0) {
        CHECK(0, "sinh du lieu cho test_partial_timing");
        return;
    }
    timing_t tm;
    dot_partial(A.x, B.x, N, &tm);
    CHECK(tm.t_init >= 0 && tm.t_compute >= 0 && tm.t_merge >= 0,
          "dot_partial: t_init, t_compute, t_merge >= 0");
    CHECK(fabs(tm.t_total - (tm.t_init + tm.t_compute + tm.t_merge)) < 1e-9,
          "dot_partial: t_total = t_init + t_compute + t_merge");
    dataset_free(&A);
    dataset_free(&B);
}

int main(void) {
    printf("test_dot: D0 dot_seq\n");
    test_known();
    test_double_accumulator();
    test_uniform();
    printf("test_dot: cac phuong an song song\n");
    test_variants();
    test_partial_timing();
    printf(fail ? "KET QUA: LOI\n" : "KET QUA: DAT\n");
    return fail;
}
