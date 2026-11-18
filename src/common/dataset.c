
#include "dataset.h"
#include "compat.h"

#include <string.h>

/* ---- Bo sinh so ngau nhien tu viet (khong dung rand_r vi MinGW khong co) ---- */

/* splitmix64: bien seed bat ky (ke ca 0) thanh trang thai khoi dau tot. */
static uint64_t splitmix64(uint64_t *s) {
    uint64_t z = (*s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

/* xorshift64*: nhanh, du tot cho sinh du lieu thi nghiem. Trang thai khong duoc bang 0. */
static uint64_t xorshift64s(uint64_t *s) {
    uint64_t x = *s;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    *s = x;
    return x * 0x2545F4914F6CDD1DULL;
}

/* So thuc deu trong [0,1): lay 24 bit cao -> k / 2^24, luon < 1 va chinh xac trong float. */
static float next_unit_float(uint64_t *s) {
    return (float)(xorshift64s(s) >> 40) * (1.0f / 16777216.0f);
}

static const char *const DIST_NAMES[DIST_COUNT] = {
    "uniform", "normal", "zipf1.1", "zipf1.5", "onebin", "sorted"
};

const char *dist_name(dist_t dist) {
    return (dist >= 0 && dist < DIST_COUNT) ? DIST_NAMES[dist] : NULL;
}

int dist_from_name(const char *name) {
    if (!name) return -1;
    for (int d = 0; d < DIST_COUNT; d++)
        if (strcmp(name, DIST_NAMES[d]) == 0) return d;
    return -1;
}

int dataset_generate(dataset_t *ds, dist_t dist, size_t n, uint64_t seed) {
    if (!ds) return -1;
    memset(ds, 0, sizeof *ds);
    if (dist != DIST_UNIFORM) return -1; 

    ds->x = xaligned_alloc(64, n * sizeof(float));
    if (!ds->x) return -1;
    ds->n = n;

    uint64_t sm = seed;
    uint64_t state = splitmix64(&sm);
    if (state == 0) state = 1;
    for (size_t i = 0; i < n; i++) ds->x[i] = next_unit_float(&state);
    return 0;
}

void dataset_free(dataset_t *ds) {
    if (!ds) return;
    xaligned_free(ds->x);
    free(ds->px);
    memset(ds, 0, sizeof *ds);
}
