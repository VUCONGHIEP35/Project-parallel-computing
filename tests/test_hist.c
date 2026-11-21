/* Test histogram (B). Chay: make test
 * Hien chi kiem V0 (hist_seq). Khi co V1-V4, them vao bang VARIANTS de doi chieu voi V0. */
#include "dataset.h"
#include "hist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) printf("  [OK]   %s\n", msg); \
    else { printf("  [FAIL] %s\n", msg); fail = 1; } \
} while (0)

static uint64_t sum_bins(const uint64_t *h, int bins) {
    uint64_t s = 0;
    for (int b = 0; b < bins; b++) s += h[b];
    return s;
}

/* Ca nho biet truoc dap an, bins = 4: [0,.25) [.25,.5) [.5,.75) [.75,1) */
static void test_known(void) {
    const float x[] = { 0.0f, 0.1f, 0.25f, 0.49f, 0.5f, 0.74f, 0.75f, 0.99f, 0.999f };
    const uint64_t want[4] = { 2, 2, 2, 3 };
    uint64_t h[4] = { 99, 99, 99, 99 };   /* rac san, ham phai tu xoa */
    hist_seq(x, sizeof x / sizeof x[0], 4, h, NULL);
    CHECK(memcmp(h, want, sizeof want) == 0, "ca biet truoc, bins = 4, ke ca bien 0.25/0.5/0.75");
}

/* Gia tri o bien: sat 1, bang 1, am -> phai kep vao bin dau/cuoi */
static void test_edges(void) {
    const float x[] = { 0.99999994f, 1.0f, 1.5f, -0.0f, -0.5f };
    uint64_t h[8];
    hist_seq(x, 5, 8, h, NULL);
    CHECK(h[7] == 3, "x sat 1, x == 1.0, x > 1 roi vao bin cuoi");
    CHECK(h[0] == 2, "x = -0.0 va x < 0 roi vao bin 0");
    CHECK(sum_bins(h, 8) == 5, "tong bin == n o ca bien");

    uint64_t h1[1];
    hist_seq(x, 5, 1, h1, NULL);
    CHECK(h1[0] == 5, "bins = 1: moi phan tu vao bin 0");
}

static void test_empty_and_onebin(void) {
    uint64_t h[16];
    memset(h, 0xFF, sizeof h);
    hist_seq(NULL, 0, 16, h, NULL);
    CHECK(sum_bins(h, 16) == 0, "n = 0: histogram toan 0");

    enum { N = 100000 };
    float *x = malloc(N * sizeof *x);
    for (int i = 0; i < N; i++) x[i] = 0.3f;   /* moi phan tu cung mot bin */
    hist_seq(x, N, 16, h, NULL);
    CHECK(h[4] == N && sum_bins(h, 16) == N, "moi phan tu 0.3 -> tat ca vao bin 4 (bins = 16)");
    free(x);
}

/* Uniform N = 10^6 voi nhieu so bin: tong bin == n, timing hop le */
static void test_uniform(void) {
    const size_t N = 1000000;
    dataset_t ds;
    if (dataset_generate(&ds, DIST_UNIFORM, N, 42) != 0) {
        CHECK(0, "sinh uniform N = 10^6");
        return;
    }

    const int BINS[] = { 16, 256, 4096, 65536, 1 << 20 };
    for (size_t k = 0; k < sizeof BINS / sizeof BINS[0]; k++) {
        int bins = BINS[k];
        uint64_t *h = malloc((size_t)bins * sizeof *h);
        timing_t tm;
        hist_seq(ds.x, ds.n, bins, h, &tm);

        char msg[96];
        snprintf(msg, sizeof msg, "uniform N = 10^6, bins = %d: tong bin == n", bins);
        CHECK(sum_bins(h, bins) == N, msg);

        if (bins == 16) {
            /* moi bin ky vong N/16 = 62500; lech > 2% la bat thuong */
            int flat = 1;
            for (int b = 0; b < bins; b++)
                if (h[b] < 61250 || h[b] > 63750) flat = 0;
            CHECK(flat, "uniform, bins = 16: moi bin lech < 2% so voi N/16");
            CHECK(tm.t_compute >= 0 && tm.t_init >= 0 && tm.t_merge == 0
                  && tm.t_total >= tm.t_compute, "timing hop le (t_merge = 0 voi V0)");
        }
        free(h);
    }

    /* cung du lieu -> cung ket qua */
    uint64_t a[256], b[256];
    hist_seq(ds.x, ds.n, 256, a, NULL);
    hist_seq(ds.x, ds.n, 256, b, NULL);
    CHECK(memcmp(a, b, sizeof a) == 0, "chay lai cho cung ket qua");

    dataset_free(&ds);
}

int main(void) {
    printf("test_hist: V0 hist_seq\n");
    test_known();
    test_edges();
    test_empty_and_onebin();
    test_uniform();
    printf(fail ? "KET QUA: LOI\n" : "KET QUA: DAT\n");
    return fail;
}
