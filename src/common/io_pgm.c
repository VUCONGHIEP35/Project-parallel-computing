/* Doc/ghi anh PGM nhi phan P5 (Thanh vien A), khong dung thu vien ngoai.
 * Dinh dang: "P5" <trang> rong <trang> cao <trang> maxval <1 ky tu trang> <rong*cao byte>.
 * Trong phan header co the co dong chu thich bat dau bang '#'. */
#include "dataset.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int pgm_save(const char *path, const uint8_t *px, int w, int h) {
    if (!path || !px || w <= 0 || h <= 0) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    size_t n = (size_t)w * (size_t)h;
    int ok = fprintf(f, "P5\n%d %d\n255\n", w, h) > 0 && fwrite(px, 1, n, f) == n;
    ok = (fclose(f) == 0) && ok;
    return ok ? 0 : -1;
}

/* Doc mot so nguyen duong trong header, bo qua khoang trang va dong chu thich. */
static int read_header_int(FILE *f, long *out) {
    int c = fgetc(f);
    for (;;) {
        while (c != EOF && isspace(c)) c = fgetc(f);
        if (c != '#') break;
        while (c != EOF && c != '\n') c = fgetc(f);   /* bo qua ca dong chu thich */
    }
    if (c == EOF || !isdigit(c)) return -1;
    long v = 0;
    while (c != EOF && isdigit(c)) {
        v = v * 10 + (c - '0');
        if (v > 1000000) return -1;                    /* chan so qua lon */
        c = fgetc(f);
    }
    if (c == EOF || !isspace(c)) return -1;            /* sau so phai la 1 ky tu trang */
    *out = v;
    return 0;
}

int pgm_load(const char *path, dataset_t *ds) {
    if (!path || !ds) return -1;
    memset(ds, 0, sizeof *ds);

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    long w, h, maxval;
    if (fgetc(f) != 'P' || fgetc(f) != '5' ||
        read_header_int(f, &w) || read_header_int(f, &h) || read_header_int(f, &maxval) ||
        w <= 0 || h <= 0 || maxval != 255) {           /* do an chi dung anh 8-bit day du */
        fclose(f);
        return -1;
    }

    size_t n = (size_t)w * (size_t)h;
    uint8_t *px = malloc(n);
    if (!px || fread(px, 1, n, f) != n) {
        free(px);
        fclose(f);
        return -1;
    }
    fclose(f);

    ds->px = px;
    ds->w = (int)w;
    ds->h = (int)h;
    ds->n = n;
    return 0;
}
