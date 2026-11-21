/* Doc/ghi file .bin (Thanh vien A). Dinh dang: docs/interfaces.md.
 * Header 32 byte little-endian + mang float32 tho. */
#include "dataset.h"
#include "compat.h"

#include <stdio.h>
#include <string.h>

#define BIN_MAGIC   "HIST"
#define BIN_VERSION 1
#define BIN_F32     1
#define HEADER_SIZE 32

/* Ghi/doc so nguyen theo thu tu little-endian tung byte, khong phu thuoc may. */
static void put_le(uint8_t *p, uint64_t v, int nbytes) {
    for (int i = 0; i < nbytes; i++) p[i] = (uint8_t)(v >> (8 * i));
}

static uint64_t get_le(const uint8_t *p, int nbytes) {
    uint64_t v = 0;
    for (int i = 0; i < nbytes; i++) v |= (uint64_t)p[i] << (8 * i);
    return v;
}

/* Mang float duoc ghi nguyen khoi bo nho, nen may phai la little-endian (x86, ARM thuong gap). */
static int host_is_little_endian(void) {
    const uint16_t one = 1;
    return *(const uint8_t *)&one == 1;
}

int dataset_save(const char *path, const dataset_t *ds, dist_t dist, uint64_t seed) {
    if (!path || !ds || (!ds->x && ds->n > 0) || !host_is_little_endian()) return -1;

    uint8_t hdr[HEADER_SIZE] = { 0 };
    memcpy(hdr, BIN_MAGIC, 4);
    put_le(hdr + 4, BIN_VERSION, 2);
    put_le(hdr + 6, BIN_F32, 2);
    put_le(hdr + 8, ds->n, 8);
    put_le(hdr + 16, seed, 8);
    put_le(hdr + 24, (uint32_t)dist, 4);
    /* hdr[28..31] du tru, giu 0 */

    FILE *f = fopen(path, "wb");   /* "b": quan trong tren Windows, tranh doi \n thanh \r\n */
    if (!f) return -1;
    int ok = fwrite(hdr, 1, HEADER_SIZE, f) == HEADER_SIZE &&
             (ds->n == 0 || fwrite(ds->x, sizeof(float), ds->n, f) == ds->n);
    ok = (fclose(f) == 0) && ok;
    return ok ? 0 : -1;
}

int dataset_load(const char *path, dataset_t *ds) {
    if (!path || !ds || !host_is_little_endian()) return -1;
    memset(ds, 0, sizeof *ds);

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    uint8_t hdr[HEADER_SIZE];
    if (fread(hdr, 1, HEADER_SIZE, f) != HEADER_SIZE ||
        memcmp(hdr, BIN_MAGIC, 4) != 0 ||
        get_le(hdr + 4, 2) != BIN_VERSION ||
        get_le(hdr + 6, 2) != BIN_F32) {
        fclose(f);
        return -1;
    }

    size_t n = (size_t)get_le(hdr + 8, 8);
    float *x = xaligned_alloc(64, n * sizeof(float));
    if (!x || (n > 0 && fread(x, sizeof(float), n, f) != n)) {   /* file ngan hon khai bao -> loi */
        xaligned_free(x);
        fclose(f);
        return -1;
    }
    fclose(f);

    ds->x = x;
    ds->n = n;
    return 0;
}
