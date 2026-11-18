#ifndef DATASET_H
#define DATASET_H

#include <stddef.h>
#include <stdint.h>

/* 6 phan phoi cua de bai. Ten trong CSV va dong lenh lay tu dist_name(). */
typedef enum {
    DIST_UNIFORM = 0, /* "uniform"  deu tren [0,1)                       */
    DIST_NORMAL,      /* "normal"   mu = 0.5, sigma = 0.05, kep ve [0,1) */
    DIST_ZIPF11,      /* "zipf1.1"  Zipf s = 1.1                          */
    DIST_ZIPF15,      /* "zipf1.5"  Zipf s = 1.5                          */
    DIST_ONEBIN,      /* "onebin"   moi phan tu cung mot gia tri          */
    DIST_SORTED,      /* "sorted"   uniform da sap xep tang dan           */
    DIST_COUNT
} dist_t;

/* Du lieu dau vao. x can le 64 byte (xaligned_alloc trong compat.h). */
typedef struct {
    size_t   n;
    float   *x;    /* mang so trong [0,1), NULL neu la anh */
    uint8_t *px;   /* anh xam 8-bit, NULL neu khong dung   */
    int      w, h; /* kich thuoc anh                        */
} dataset_t;

/* Sinh n phan tu theo phan phoi `dist` voi `seed` co dinh.
 * Cung (dist, n, seed) -> cung du lieu tren moi may. Tra ve 0 neu thanh cong. */
int  dataset_generate(dataset_t *ds, dist_t dist, size_t n, uint64_t seed);
void dataset_free(dataset_t *ds);

/* "uniform", "normal", ... ; NULL neu dist khong hop le. */
const char *dist_name(dist_t dist);
/* Nguoc lai cua dist_name; tra ve -1 neu ten khong hop le. */
int dist_from_name(const char *name);

/* Doc/ghi file .bin (dinh dang o docs/interfaces.md). Cai dat trong io_bin.c. */
int dataset_save(const char *path, const dataset_t *ds, dist_t dist, uint64_t seed);
int dataset_load(const char *path, dataset_t *ds);

#endif
