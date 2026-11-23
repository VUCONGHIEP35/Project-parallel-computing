/* hist_bench - do thoi gian cac phuong an histogram, xuat CSV ra stdout (moi lan do mot dong).
 * Thuong goi qua scripts/run_hist.sh; goi tay de thu nhanh:
 *
 *   hist_bench --dist uniform,zipf1.5 --n 1e6,1e7 --bins 16,256 --threads 1,2,4 > a.csv
 *   hist_bench --file ~/data/zipf15_1e8.bin --bins 256 --variants hist_atomic,hist_omp_reduce
 *   hist_bench --image ~/data/img4096.pgm --bins 256 --threads 1,8
 *
 * Tuy chon (danh sach cach nhau bang dau phay):
 *   --dist D,..      phan phoi, sinh trong bo nho (t_io = 0)            mac dinh uniform
 *   --n N,..         so phan tu, nhan ca dang 1e8                         mac dinh 1e6
 *   --seed S                                                              mac dinh 1
 *   --file F,..      doc file .bin (t_io = thoi gian doc), thay cho --dist/--n
 *   --image F,..     doc anh PGM, doi x = p/256 (t_io = doc, t_init cong them thoi gian doi)
 *   --bins B,..                                                           mac dinh 256
 *   --threads T,..                                                        mac dinh omp_get_max_threads()
 *   --variants V,..  ten ham, xem --list                                  mac dinh tat ca
 *   --runs R         so lan do ghi lai; them 1 lan khoi dong khong ghi    mac dinh 7
 *   --max-mb M       bo qua cau hinh uoc tinh can > M MiB                 mac dinh 2500
 *   --no-header      khong in dong tieu de (de noi vao file co san)
 *   --list           in ten cac phuong an roi thoat
 *
 * hist_seq chay tuan tu nen chi do mot lan cho moi (du lieu, bins), ghi threads = 1.
 * Moi lan do deu doi chieu tung bin voi hist_seq (cot ok). Co dong ok = 0 -> ma thoat 1.
 * Cot CSV: docs/interfaces.md, muc "CSV cua hist_bench". */
#define _POSIX_C_SOURCE 200809L
#include "dataset.h"
#include "hist.h"

#include <errno.h>
#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef _WIN32
#include <sys/utsname.h>
#include <unistd.h>
#endif

static const struct { const char *name; hist_fn fn; } VARIANTS[] = {
    { "hist_seq",            hist_seq },
    { "hist_atomic",         hist_atomic },
    { "hist_critical",       hist_critical },
    { "hist_local_serial",   hist_local_serial },
    { "hist_local_parmerge", hist_local_parmerge },
    { "hist_omp_reduce",     hist_omp_reduce },
    /* them phuong an moi o day (cung ten ham) */
};
#define NVAR (sizeof VARIANTS / sizeof VARIANTS[0])
#define MAXLIST 64

/* ---- tien ich ---- */

/* Tach "a,b,c" thanh mang; s bi sua tai cho. Tra ve so phan tu, -1 neu qua MAXLIST. */
static int split(char *s, char **items) {
    int k = 0;
    for (char *save = NULL, *t = strtok_r(s, ",", &save); t; t = strtok_r(NULL, ",", &save)) {
        if (k == MAXLIST) return -1;
        items[k++] = t;
    }
    return k;
}

/* So nguyen khong am, nhan ca dang 1e8. Tra ve -1 neu khong hop le. */
static int parse_count(const char *s, double max, size_t *out) {
    char *end;
    errno = 0;
    double v = strtod(s, &end);
    if (errno || end == s || *end != '\0' || v < 0 || v > max || v != floor(v)) return -1;
    *out = (size_t)v;
    return 0;
}

static void clean_field(char *s) {   /* bo dau phay, xuong dong de khong vo CSV */
    for (; *s; s++)
        if (*s == ',' || *s == '\n' || *s == '\r') *s = ' ';
}

static void host_info(char *host, size_t hn, char *kernel, size_t kn) {
#ifdef _WIN32
    const char *h = getenv("COMPUTERNAME");
    snprintf(host, hn, "%s", h ? h : "unknown");
    snprintf(kernel, kn, "windows");
#else
    if (gethostname(host, hn) != 0) snprintf(host, hn, "unknown");
    host[hn - 1] = '\0';
    struct utsname u;
    snprintf(kernel, kn, "%s", uname(&u) == 0 ? u.release : "unknown");
#endif
    clean_field(host);
    clean_field(kernel);
}

static void now_iso(char *buf, size_t len) {
    time_t t = time(NULL);
    struct tm tmv;
#ifdef _WIN32
    gmtime_s(&tmv, &t);
#else
    gmtime_r(&t, &tmv);
#endif
    strftime(buf, len, "%Y-%m-%dT%H:%M:%SZ", &tmv);
}

/* Ma phan phoi ghi trong header .bin (offset 24, uint32 little-endian). */
static const char *bin_dist_name(const char *path) {
    unsigned char h[32];
    FILE *f = fopen(path, "rb");
    if (!f) return "file";
    size_t got = fread(h, 1, sizeof h, f);
    fclose(f);
    if (got != sizeof h) return "file";
    unsigned d = h[24] | h[25] << 8 | (unsigned)h[26] << 16 | (unsigned)h[27] << 24;
    const char *name = d < DIST_COUNT ? dist_name((dist_t)d) : NULL;
    return name ? name : "file";
}

/* Checksum co trong so theo bin: hai histogram khac thu tu bin cho checksum khac nhau. */
static unsigned long long checksum(const uint64_t *h, int bins) {
    unsigned long long s = 0;
    for (int b = 0; b < bins; b++) s += (unsigned long long)(b + 1) * h[b];
    return s;
}

static void usage(void) {
    fprintf(stderr, "Cach dung: hist_bench [--dist D,..] [--n N,..] [--seed S] [--file F,..]"
                    " [--image F,..]\n                  [--bins B,..] [--threads T,..]"
                    " [--variants V,..] [--runs R] [--max-mb M] [--no-header] [--list]\n"
                    "Chi tiet o dau file apps/hist_bench.c\n");
}

/* ---- do ---- */

typedef struct {
    int bins[MAXLIST], nb;
    int threads[MAXLIST], nt;
    int var[NVAR], nv;
    int runs;
    double max_mb;
    char host[256], kernel[256];
} config_t;

static int bad = 0;   /* so dong ok = 0 */

/* Do moi (bins, threads, variant) tren mot bo du lieu da nap. */
static void bench_dataset(const config_t *c, const dataset_t *ds, const char *dname,
                          double t_io, double t_conv) {
    for (int ib = 0; ib < c->nb; ib++) {
        const int bins = c->bins[ib];
        uint64_t *ref = malloc((size_t)bins * sizeof *ref);
        uint64_t *out = malloc((size_t)bins * sizeof *out);
        if (!ref || !out) {
            fprintf(stderr, "bo qua bins = %d: khong du bo nho\n", bins);
            free(ref); free(out);
            continue;
        }
        hist_seq(ds->x, ds->n, bins, ref, NULL);   /* dap an doi chieu, khong tinh gio */

        for (int it = 0; it < c->nt; it++) {
            const int T = c->threads[it];
            /* du lieu + ref + out + T histogram cuc bo (V3) hoac T ban sao (V4) */
            double mb = ((double)ds->n * sizeof(float) +
                         (double)(T + 3) * ((bins + 7) / 8 * 8) * sizeof(uint64_t)) / (1 << 20);
            if (mb > c->max_mb) {
                fprintf(stderr, "bo qua %s n=%zu bins=%d T=%d: can ~%.0f MiB > %.0f\n",
                        dname, ds->n, bins, T, mb, c->max_mb);
                continue;
            }
            omp_set_num_threads(T);

            for (int iv = 0; iv < c->nv; iv++) {
                const int v = c->var[iv];
                const int serial = VARIANTS[v].fn == hist_seq;
                if (serial && it > 0) continue;   /* V0 khong phu thuoc T: do mot lan */
                const int tlabel = serial ? 1 : T;
                fprintf(stderr, "# %s n=%zu bins=%d T=%d %s\n", dname, ds->n, bins, tlabel,
                        VARIANTS[v].name);

                timing_t tm;
                VARIANTS[v].fn(ds->x, ds->n, bins, out, &tm);   /* khoi dong, bo */
                for (int r = 1; r <= c->runs; r++) {
                    VARIANTS[v].fn(ds->x, ds->n, bins, out, &tm);
                    int ok = memcmp(out, ref, (size_t)bins * sizeof *out) == 0;
                    if (!ok) {
                        bad++;
                        fprintf(stderr, "SAI: %s khac hist_seq (%s n=%zu bins=%d T=%d run %d)\n",
                                VARIANTS[v].name, dname, ds->n, bins, tlabel, r);
                    }
                    char ts[32];
                    now_iso(ts, sizeof ts);
                    printf("%s,%s,%s,%s,%zu,%d,%s,%d,%s,%d,%.9f,%.9f,%.9f,%.9f,%.9f,%llu,%d\n",
                           ts, c->host, c->kernel, VARIANTS[v].name, ds->n, bins, dname, tlabel,
                           "static", r, t_io, tm.t_init + t_conv, tm.t_compute, tm.t_merge,
                           tm.t_total + t_io + t_conv, checksum(out, bins), ok);
                    fflush(stdout);
                }
            }
        }
        free(ref);
        free(out);
    }
}

int main(int argc, char **argv) {
    char *dists[MAXLIST], *ns[MAXLIST], *files[MAXLIST], *images[MAXLIST], *items[MAXLIST];
    int nd = 0, nn = 0, nf = 0, ni = 0, header = 1;
    char def_dist[] = "uniform", def_n[] = "1e6";
    size_t seed = 1;
    config_t c = { .nb = 0, .nt = 0, .nv = 0, .runs = 7, .max_mb = 2500 };

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "--list")) {
            for (size_t v = 0; v < NVAR; v++) puts(VARIANTS[v].name);
            return 0;
        }
        if (!strcmp(a, "--no-header")) { header = 0; continue; }
        if (i + 1 >= argc) { usage(); return 2; }
        char *val = argv[++i];
        int k;
        size_t x;
        if (!strcmp(a, "--dist")) {
            if ((nd = split(val, dists)) < 1) { usage(); return 2; }
        } else if (!strcmp(a, "--n")) {
            if ((nn = split(val, ns)) < 1) { usage(); return 2; }
        } else if (!strcmp(a, "--file")) {
            if ((nf = split(val, files)) < 1) { usage(); return 2; }
        } else if (!strcmp(a, "--image")) {
            if ((ni = split(val, images)) < 1) { usage(); return 2; }
        } else if (!strcmp(a, "--seed")) {
            if (parse_count(val, 1e18, &seed)) { usage(); return 2; }
        } else if (!strcmp(a, "--runs")) {
            if (parse_count(val, 1000, &x) || x < 1) { usage(); return 2; }
            c.runs = (int)x;
        } else if (!strcmp(a, "--max-mb")) {
            if (parse_count(val, 1e9, &x)) { usage(); return 2; }
            c.max_mb = (double)x;
        } else if (!strcmp(a, "--bins")) {
            if ((k = split(val, items)) < 1) { usage(); return 2; }
            for (c.nb = 0; c.nb < k; c.nb++) {
                if (parse_count(items[c.nb], 1 << 24, &x) || x < 1) {
                    fprintf(stderr, "bins phai trong 1..2^24: %s\n", items[c.nb]);
                    return 2;
                }
                c.bins[c.nb] = (int)x;
            }
        } else if (!strcmp(a, "--threads")) {
            if ((k = split(val, items)) < 1) { usage(); return 2; }
            for (c.nt = 0; c.nt < k; c.nt++) {
                if (parse_count(items[c.nt], 4096, &x) || x < 1) { usage(); return 2; }
                c.threads[c.nt] = (int)x;
            }
        } else if (!strcmp(a, "--variants")) {
            if ((k = split(val, items)) < 1) { usage(); return 2; }
            for (int j = 0; j < k; j++) {
                size_t v = 0;
                while (v < NVAR && strcmp(items[j], VARIANTS[v].name)) v++;
                if (v == NVAR) { fprintf(stderr, "khong co phuong an %s (xem --list)\n", items[j]); return 2; }
                if (c.nv < (int)NVAR) c.var[c.nv++] = (int)v;
            }
        } else {
            usage();
            return 2;
        }
    }

    if (!nd) { dists[0] = def_dist; nd = 1; }
    if (!nn) { ns[0] = def_n; nn = 1; }
    if (!c.nb) { c.bins[0] = 256; c.nb = 1; }
    if (!c.nt) { c.threads[0] = omp_get_max_threads(); c.nt = 1; }
    if (!c.nv) for (size_t v = 0; v < NVAR; v++) c.var[c.nv++] = (int)v;
    host_info(c.host, sizeof c.host, c.kernel, sizeof c.kernel);

    if (header)
        printf("timestamp,host,kernel,variant,n,bins,dist,threads,schedule,run,"
               "t_io,t_init,t_compute,t_merge,t_total,checksum,ok\n");

    if (nf || ni) {
        /* du lieu tu file: t_io = thoi gian doc */
        for (int j = 0; j < nf; j++) {
            dataset_t ds;
            double t0 = omp_get_wtime();
            if (dataset_load(files[j], &ds) != 0) { fprintf(stderr, "khong doc duoc %s\n", files[j]); return 1; }
            double t_io = omp_get_wtime() - t0;
            bench_dataset(&c, &ds, bin_dist_name(files[j]), t_io, 0.0);
            dataset_free(&ds);
        }
        for (int j = 0; j < ni; j++) {
            dataset_t ds;
            double t0 = omp_get_wtime();
            if (pgm_load(images[j], &ds) != 0) { fprintf(stderr, "khong doc duoc %s\n", images[j]); return 1; }
            double t1 = omp_get_wtime();
            if (dataset_pixels_to_unit(&ds) != 0) { fprintf(stderr, "khong du bo nho doi anh\n"); return 1; }
            double t2 = omp_get_wtime();
            free(ds.px);   /* tu day chi dung ds.x */
            ds.px = NULL;
            bench_dataset(&c, &ds, "image", t1 - t0, t2 - t1);
            dataset_free(&ds);
        }
    } else {
        /* du lieu sinh trong bo nho: t_io = 0, thoi gian sinh khong tinh */
        for (int j = 0; j < nd; j++) {
            int d = dist_from_name(dists[j]);
            if (d < 0) { fprintf(stderr, "khong co phan phoi %s\n", dists[j]); return 2; }
            for (int k = 0; k < nn; k++) {
                size_t n;
                if (parse_count(ns[k], 1e12, &n)) { fprintf(stderr, "n khong hop le: %s\n", ns[k]); return 2; }
                if ((double)n * sizeof(float) / (1 << 20) > c.max_mb) {
                    fprintf(stderr, "bo qua %s n=%zu: du lieu vuot %.0f MiB\n", dists[j], n, c.max_mb);
                    continue;
                }
                dataset_t ds;
                if (dataset_generate(&ds, (dist_t)d, n, seed) != 0) {
                    fprintf(stderr, "khong sinh duoc %s n=%zu\n", dists[j], n);
                    return 1;
                }
                bench_dataset(&c, &ds, dists[j], 0.0, 0.0);
                dataset_free(&ds);
            }
        }
    }
    if (bad) fprintf(stderr, "CO %d LAN DO SAI KET QUA\n", bad);
    return bad ? 1 : 0;
}
