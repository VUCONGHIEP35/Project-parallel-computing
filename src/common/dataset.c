
#include "dataset.h"
#include "compat.h"

#include <math.h>
#include <stdlib.h>
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

static uint64_t rng_init(uint64_t seed) {
    uint64_t sm = seed;
    uint64_t state = splitmix64(&sm);
    return state ? state : 1;
}

/* So nguyen 24 bit k trong [0, 2^24). Gia tri float tuong ung la k / 2^24. */
static uint32_t next_u24(uint64_t *s) {
    return (uint32_t)(xorshift64s(s) >> 40);
}

#define U24_SCALE (1.0f / 16777216.0f) /* 2^-24 */

/* ---- 6 phan phoi ---- */

/* uniform: x = k / 2^24, luon < 1 va chinh xac trong float. */
static void gen_uniform(float *x, size_t n, uint64_t seed) {
    uint64_t s = rng_init(seed);
    for (size_t i = 0; i < n; i++) x[i] = (float)next_u24(&s) * U24_SCALE;
}

/* normal(0.5; 0.05): xap xi chuan bang tong 12 bien deu (Irwin-Hall).
 * Tong 12 bien deu [0,1) co trung binh 6, phuong sai 1 -> (tong - 6) ~ N(0,1).
 * Chon cach nay vi chi dung phep cong so nguyen (khong dung log/sin cua libm)
 * nen cung seed cho cung du lieu tren moi nen tang. Gia tri bi chan trong
 * [0.5 - 6*0.05, 0.5 + 6*0.05] = [0.2, 0.8], khong can kep ve [0,1). */
static void gen_normal(float *x, size_t n, uint64_t seed) {
    uint64_t s = rng_init(seed);
    for (size_t i = 0; i < n; i++) {
        uint64_t sum = 0;
        for (int j = 0; j < 12; j++) sum += next_u24(&s);
        double z = (double)sum / 16777216.0 - 6.0;
        x[i] = (float)(0.5 + 0.05 * z);
    }
}

/* Zipf tren K = 2^20 hang: P(hang k) ~ k^(-s), k = 1..K.
 * Hang k dat vao o (slot) bitrev20(k-1) roi chon ngau nhien 1 trong 16 vi tri trong o:
 *   x = (slot * 16 + r) / 2^24,  r trong [0,16)
 * Dao bit lam cac hang nong roi vao cac bin khac nhau: hang 1 luon o bin 0, hang 2 o
 * bin B/2, ... Voi moi B la luy thua cua 2, bin 0 chiem xap xi P(hang 1):
 * ~38% voi s = 1.5, ~12% voi s = 1.1. Xem docs/interfaces.md. */
#define ZIPF_BITS 20
#define ZIPF_K (1u << ZIPF_BITS)

static uint32_t bitrev20(uint32_t v) {
    uint32_t r = 0;
    for (int i = 0; i < ZIPF_BITS; i++) {
        r = (r << 1) | (v & 1u);
        v >>= 1;
    }
    return r;
}

static int gen_zipf(float *x, size_t n, uint64_t seed, double s_exp) {
    /* Bang phan phoi tich luy cdf[k-1] = P(hang <= k). */
    double *cdf = malloc(ZIPF_K * sizeof *cdf);
    if (!cdf) return -1;
    double acc = 0.0;
    for (uint32_t k = 1; k <= ZIPF_K; k++) {
        acc += pow((double)k, -s_exp);
        cdf[k - 1] = acc;
    }
    for (uint32_t k = 0; k < ZIPF_K; k++) cdf[k] /= acc;
    cdf[ZIPF_K - 1] = 1.0;

    /* Bang dan duong (guide table): guide[j] = hang nho nhat co cdf > j/K.
     * Voi u trong [j/K, (j+1)/K) chi can do tiep vai buoc tu guide[j], thay vi
     * tim kiem nhi phan tren ca bang 8 MB (nhanh hon nhieu voi N = 10^8). */
    uint32_t *guide = malloc(ZIPF_K * sizeof *guide);
    if (!guide) { free(cdf); return -1; }
    uint32_t k = 0;
    for (uint32_t j = 0; j < ZIPF_K; j++) {
        while (cdf[k] <= (double)j / ZIPF_K) k++;
        guide[j] = k;
    }

    uint64_t s = rng_init(seed);
    for (size_t i = 0; i < n; i++) {
        /* u trong [0,1) voi 53 bit; tim hang nho nhat co cdf > u. */
        double u = (double)(xorshift64s(&s) >> 11) * (1.0 / 9007199254740992.0);
        uint32_t r0 = guide[(uint32_t)(u * ZIPF_K)];
        while (cdf[r0] <= u) r0++;
        uint32_t slot = bitrev20(r0);           /* r0 = hang - 1 */
        uint32_t r = (uint32_t)(xorshift64s(&s) >> 60);
        x[i] = (float)((slot << 4) | r) * U24_SCALE;
    }
    free(guide);
    free(cdf);
    return 0;
}

/* onebin: moi phan tu bang 0.3 (khong nam o bien bin nao voi B la luy thua cua 2). */
#define ONEBIN_VALUE 0.3f

static void gen_onebin(float *x, size_t n) {
    for (size_t i = 0; i < n; i++) x[i] = ONEBIN_VALUE;
}

/* sorted: dung lai du lieu uniform cung seed roi sap xep tang dan.
 * Moi gia tri la k / 2^24 nen dung sap xep dem (counting sort) tren khoa k: O(n + 2^24). */
static int gen_sorted(float *x, size_t n, uint64_t seed) {
    uint32_t *count = calloc((size_t)1 << 24, sizeof *count);
    if (!count) return -1;
    uint64_t s = rng_init(seed);
    for (size_t i = 0; i < n; i++) count[next_u24(&s)]++;
    size_t i = 0;
    for (uint32_t k = 0; k < (1u << 24); k++)
        for (uint32_t c = 0; c < count[k]; c++) x[i++] = (float)k * U24_SCALE;
    free(count);
    return 0;
}

/* ---- Giao dien ---- */

static const char *const DIST_NAMES[DIST_COUNT] = {
    "uniform", "normal", "zipf1.1", "zipf1.5", "onebin", "sorted"
};

const char *dist_name(dist_t dist) {
    return ((int)dist >= 0 && dist < DIST_COUNT) ? DIST_NAMES[dist] : NULL;
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
    if ((int)dist < 0 || dist >= DIST_COUNT) return -1;

    ds->x = xaligned_alloc(64, n * sizeof(float));
    if (!ds->x) return -1;
    ds->n = n;

    int rc = 0;
    switch (dist) {
    case DIST_UNIFORM: gen_uniform(ds->x, n, seed); break;
    case DIST_NORMAL:  gen_normal(ds->x, n, seed); break;
    case DIST_ZIPF11:  rc = gen_zipf(ds->x, n, seed, 1.1); break;
    case DIST_ZIPF15:  rc = gen_zipf(ds->x, n, seed, 1.5); break;
    case DIST_ONEBIN:  gen_onebin(ds->x, n); break;
    case DIST_SORTED:  rc = gen_sorted(ds->x, n, seed); break;
    default:           rc = -1; break;
    }
    if (rc != 0) dataset_free(ds);
    return rc;
}

/* ---- Anh xam ---- */

/* Anh tong hop: nen sang dan tu tren xuong (32 -> 224) cong nhieu ~ chuan (tong 4 byte
 * ngau nhien, do lech khoang 18 muc xam), kep ve [0, 255]. Chi dung so nguyen -> tat dinh. */
int dataset_generate_image(dataset_t *ds, int w, int h, uint64_t seed) {
    if (!ds) return -1;
    memset(ds, 0, sizeof *ds);
    if (w <= 0 || h <= 0) return -1;

    size_t n = (size_t)w * (size_t)h;
    uint8_t *px = malloc(n);
    if (!px) return -1;

    uint64_t s = rng_init(seed);
    for (int y = 0; y < h; y++) {
        int base = 32 + (int)((192LL * y) / h);
        for (int c = 0; c < w; c++) {
            uint64_t r = xorshift64s(&s);
            int noise = (int)((r & 0xFF) + ((r >> 8) & 0xFF) + ((r >> 16) & 0xFF)
                              + ((r >> 24) & 0xFF)) - 510;      /* trong [-510, 510] */
            int v = base + noise / 8;
            px[(size_t)y * w + c] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
        }
    }
    ds->px = px;
    ds->w = w;
    ds->h = h;
    ds->n = n;
    return 0;
}

int dataset_pixels_to_unit(dataset_t *ds) {
    if (!ds || (!ds->px && ds->n > 0)) return -1;
    float *x = xaligned_alloc(64, ds->n * sizeof(float));
    if (!x) return -1;
    for (size_t i = 0; i < ds->n; i++) x[i] = (float)ds->px[i] * (1.0f / 256.0f);
    xaligned_free(ds->x);
    ds->x = x;
    return 0;
}

void dataset_free(dataset_t *ds) {
    if (!ds) return;
    xaligned_free(ds->x);
    free(ds->px);
    memset(ds, 0, sizeof *ds);
}
