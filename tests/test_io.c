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

    printf(fail ? "KET QUA: LOI\n" : "KET QUA: DAT\n");
    return fail;
}
