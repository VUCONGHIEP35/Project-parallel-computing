 /* Chay: make test */
#include "dataset.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) printf("  [OK]   %s\n", msg); \
    else { printf("  [FAIL] %s\n", msg); fail = 1; } \
} while (0)

static int all_in_unit(const dataset_t *d) {
    for (size_t i = 0; i < d->n; i++)
        if (!(d->x[i] >= 0.0f && d->x[i] < 1.0f)) return 0;
    return 1;
}

/* Ty le phan tu roi vao bin 0 khi chia [0,1) thanh `bins` bin. */
static double frac_bin0(const dataset_t *d, int bins) {
    size_t c = 0;
    for (size_t i = 0; i < d->n; i++)
        if (d->x[i] < 1.0f / (float)bins) c++;
    return (double)c / (double)d->n;
}

/* Ly thuyet: Zipf tren K = 2^20 hang, hang k vao o bitrev(k-1) (xem docs/interfaces.md).
 * Bin 0 (voi B luy thua cua 2) chua cac hang 1, B+1, 2B+1, ... */
static double zipf_bin0_expected(double s, int bins) {
    const long K = 1L << 20;
    double h = 0.0, b0 = 0.0;
    for (long k = 1; k <= K; k++) {
        double p = pow((double)k, -s);
        h += p;
        if ((k - 1) % bins == 0) b0 += p;
    }
    return b0 / h;
}

/* Kiem tra chung cho moi phan phoi: sinh duoc, trong [0,1), can le, cung seed cung du lieu. */
static void test_each_dist(void) {
    const size_t N = 1000000;
    for (int d = 0; d < DIST_COUNT; d++) {
        dataset_t a, b;
        char msg[96];
        int ok = dataset_generate(&a, (dist_t)d, N, 7) == 0 &&
                 dataset_generate(&b, (dist_t)d, N, 7) == 0;
        snprintf(msg, sizeof msg, "%s: sinh duoc N=10^6", dist_name((dist_t)d));
        CHECK(ok, msg);
        if (!ok) continue;
        snprintf(msg, sizeof msg, "%s: moi phan tu trong [0,1), can le 64 byte",
                 dist_name((dist_t)d));
        CHECK(all_in_unit(&a) && ((uintptr_t)a.x % 64) == 0, msg);
        snprintf(msg, sizeof msg, "%s: cung seed -> cung du lieu", dist_name((dist_t)d));
        CHECK(memcmp(a.x, b.x, N * sizeof(float)) == 0, msg);
        snprintf(msg, sizeof msg, "%s: ten <-> ma", dist_name((dist_t)d));
        CHECK(dist_from_name(dist_name((dist_t)d)) == d, msg);
        dataset_free(&a);
        dataset_free(&b);
    }
}

static void test_normal(void) {
    const size_t N = 1000000;
    dataset_t a;
    dataset_generate(&a, DIST_NORMAL, N, 11);
    double sum = 0.0, sq = 0.0;
    int bounded = 1;
    for (size_t i = 0; i < N; i++) {
        sum += a.x[i];
        sq += (double)a.x[i] * a.x[i];
        if (a.x[i] < 0.2f || a.x[i] > 0.8f) bounded = 0;
    }
    double mean = sum / N, sd = sqrt(sq / N - mean * mean);
    CHECK(fabs(mean - 0.5) < 0.001, "normal: trung binh xap xi 0.5");
    CHECK(fabs(sd - 0.05) < 0.001, "normal: do lech chuan xap xi 0.05");
    CHECK(bounded, "normal: moi gia tri trong [0.2, 0.8]");
    dataset_free(&a);
}

static void test_zipf(void) {
    const size_t N = 1000000;
    const struct { dist_t d; double s; } Z[] = { { DIST_ZIPF11, 1.1 }, { DIST_ZIPF15, 1.5 } };
    const int BINS[] = { 16, 256, 1 << 20 };
    for (int z = 0; z < 2; z++) {
        dataset_t a;
        dataset_generate(&a, Z[z].d, N, 13);
        for (int k = 0; k < 3; k++) {
            double got = frac_bin0(&a, BINS[k]);
            double want = zipf_bin0_expected(Z[z].s, BINS[k]);
            char msg[128];
            snprintf(msg, sizeof msg, "%s: ty le bin 0 voi B=%d = %.4f (ly thuyet %.4f)",
                     dist_name(Z[z].d), BINS[k], got, want);
            CHECK(fabs(got - want) < 0.005, msg);
        }
        dataset_free(&a);
    }
}

static void test_onebin_sorted(void) {
    const size_t N = 1000000;
    dataset_t a, u;

    dataset_generate(&a, DIST_ONEBIN, N, 5);
    int same = 1;
    for (size_t i = 1; i < N; i++) same &= (a.x[i] == a.x[0]);
    CHECK(same, "onebin: moi phan tu bang nhau");
    dataset_free(&a);

    dataset_generate(&a, DIST_SORTED, N, 5);
    dataset_generate(&u, DIST_UNIFORM, N, 5);
    int sorted = 1;
    for (size_t i = 1; i < N; i++) sorted &= (a.x[i - 1] <= a.x[i]);
    CHECK(sorted, "sorted: tang dan");
    /* Moi gia tri la k/2^24 nen tong trong double la chinh xac, khong phu thuoc thu tu cong. */
    double s1 = 0.0, s2 = 0.0;
    for (size_t i = 0; i < N; i++) { s1 += a.x[i]; s2 += u.x[i]; }
    CHECK(s1 == s2, "sorted: cung cac gia tri voi uniform cung seed");
    dataset_free(&a);
    dataset_free(&u);
}

int main(void) {
    const size_t N = 1000000;
    dataset_t a, b, c;

    CHECK(dataset_generate(&a, DIST_UNIFORM, N, 42) == 0, "sinh uniform N=10^6");
    CHECK(a.n == N && a.x != NULL, "dung so phan tu");

    int in_range = 1;
    double sum = 0.0;
    for (size_t i = 0; i < a.n; i++) {
        if (!(a.x[i] >= 0.0f && a.x[i] < 1.0f)) in_range = 0;
        sum += a.x[i];
    }
    CHECK(in_range, "moi phan tu nam trong [0,1)");
    CHECK(fabs(sum / (double)N - 0.5) < 0.005, "trung binh xap xi 0.5");

    CHECK(((uintptr_t)a.x % 64) == 0, "mang can le 64 byte");

    dataset_generate(&b, DIST_UNIFORM, N, 42);
    CHECK(memcmp(a.x, b.x, N * sizeof(float)) == 0, "cung seed -> cung du lieu");

    dataset_generate(&c, DIST_UNIFORM, N, 43);
    CHECK(memcmp(a.x, c.x, N * sizeof(float)) != 0, "khac seed -> khac du lieu");

    dataset_t z;
    CHECK(dataset_generate(&z, DIST_UNIFORM, 0, 1) == 0 && z.n == 0, "N = 0 khong loi");
    dataset_free(&z);

    CHECK(dist_from_name("zipf1.5") == DIST_ZIPF15 && dist_from_name("abc") == -1,
          "doi ten <-> phan phoi");

    dataset_free(&a);
    dataset_free(&b);
    dataset_free(&c);

    test_each_dist();
    test_normal();
    test_zipf();
    test_onebin_sorted();

    printf(fail ? "KET QUA: LOI\n" : "KET QUA: DAT\n");
    return fail;
}
