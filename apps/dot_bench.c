/* dot_bench - do thoi gian cac phuong an tich vo huong, xuat CSV ra stdout (moi lan do mot dong).
 * Thuong goi qua scripts/run_dot.sh (thi nghiem E6); goi tay de thu nhanh:
 *
 *   dot_bench --n 1e4,1e6,1e8 --threads 1,2,4,8 > e6.csv
 *   dot_bench --n 1e8 --threads 16 --variants dot_seq,dot_partial,dot_partial_fs --runs 3
 *
 * Tuy chon (danh sach cach nhau bang dau phay):
 *   --n N,..         so phan tu, nhan ca dang 1e8                         mac dinh 1e6
 *   --seed S         a = uniform seed S, b = uniform seed S+1             mac dinh 1
 *   --threads T,..                                                        mac dinh omp_get_max_threads()
 *   --variants V,..  ten ham, xem --list                                  mac dinh tat ca
 *   --runs R         so lan do ghi lai; them 1 lan khoi dong khong ghi    mac dinh 7
 *   --min-time S     moi lan do goi ham lap lai den khi du S giay roi chia mac dinh 0.05
 *   --max-mb M       bo qua n can > M MiB cho 2 vector                    mac dinh 2500
 *   --no-header      khong in dong tieu de (de noi vao file co san)
 *   --list           in ten cac phuong an roi thoat
 *
 * Cot CSV giong het hist_bench (docs/interfaces.md) de dung chung cong cu phan tich:
 *   bins = 0 (khong ap dung), dist = uniform, schedule = static,
 *   t_* = trung binh mot lan goi trong lan do (lay tu timing_t cua phuong an),
 *   checksum = ket qua tich vo huong, ok = 1 neu sai so tuong doi so voi dot_seq < 1e-6.
 * So lan lap trong moi lan do tinh theo THOI GIAN (--min-time), khong theo n: n nho thi lap nhieu
 * lan cho dong ho du chinh xac; khi mot lan goi da cham (vd >= 18 thread tren may C ~10 ms) thi
 * chi lap it lan, khong bi treo.
 * dot_seq chay tuan tu nen chi do mot lan cho moi n, ghi threads = 1.
 * Co dong ok = 0 -> ma thoat 1. */
#define _POSIX_C_SOURCE 200809L
#include "dataset.h"
#include "dot.h"

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

static const struct { const char *name; dot_fn fn; } VARIANTS[] = {
    { "dot_seq",        dot_seq },
    { "dot_reduce",     dot_reduce },
    { "dot_partial",    dot_partial },
    { "dot_partial_fs", dot_partial_fs },
    /* them phuong an moi o day (cung ten ham) */
};
#define NVAR (sizeof VARIANTS / sizeof VARIANTS[0])
#define MAXLIST 64
#define TOL 1e-6        /* sai so tuong doi cho phep so voi dot_seq (docs/interfaces.md) */
#define MAX_REPS 10000000L

/* ---- tien ich (giong hist_bench) ---- */

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

/* So thuc duong trong (0, max]. Tra ve -1 neu khong hop le. */
static int parse_seconds(const char *s, double max, double *out) {
    char *end;
    errno = 0;
    double v = strtod(s, &end);
    if (errno || end == s || *end != '\0' || !(v > 0) || v > max) return -1;
    *out = v;
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

static void usage(void) {
    fprintf(stderr, "Cach dung: dot_bench [--n N,..] [--seed S] [--threads T,..] [--variants V,..]\n"
                    "                 [--runs R] [--min-time S] [--max-mb M] [--no-header] [--list]\n"
                    "Chi tiet o dau file apps/dot_bench.c\n");
}

/* ---- do ---- */

typedef struct {
    int threads[MAXLIST], nt;
    int var[NVAR], nv;
    int runs;
    double min_time, max_mb;
    char host[256], kernel[256];
} config_t;

static int bad = 0;   /* so dong ok = 0 */

/* Mot lan do: goi f lien tiep reps lan, tra ve trung binh tung pha cua mot lan goi. */
static double measure(dot_fn f, const float *a, const float *b, size_t n, long reps, timing_t *avg) {
    timing_t sum = { 0 }, tm;
    double res = 0.0;
    for (long k = 0; k < reps; k++) {
        res = f(a, b, n, &tm);
        sum.t_io += tm.t_io;
        sum.t_init += tm.t_init;
        sum.t_compute += tm.t_compute;
        sum.t_merge += tm.t_merge;
        sum.t_total += tm.t_total;
    }
    avg->t_io = sum.t_io / (double)reps;
    avg->t_init = sum.t_init / (double)reps;
    avg->t_compute = sum.t_compute / (double)reps;
    avg->t_merge = sum.t_merge / (double)reps;
    avg->t_total = sum.t_total / (double)reps;
    return res;
}

/* So lan goi moi lan do de tong thoi gian >= min_time (it nhat 1).
 * Nhan doi so lan goi cho den khi ca loat du min_time: mot lan goi cham bat thuong (vd lan dau
 * sau khi doi so thread) chi anh huong loat nho dau tien, khong quyet dinh so lan lap. */
static long reps_for(dot_fn f, const float *a, const float *b, size_t n, double min_time) {
    timing_t tm;
    for (long reps = 1;; reps *= 2) {
        double t0 = omp_get_wtime();
        for (long k = 0; k < reps; k++) f(a, b, n, &tm);
        if (omp_get_wtime() - t0 >= min_time || reps >= MAX_REPS) return reps;
    }
}

/* Do moi (threads, variant) tren mot cap vector da sinh. */
static void bench_n(const config_t *c, const float *a, const float *b, size_t n) {
    const double ref = dot_seq(a, b, n, NULL);   /* dap an doi chieu, khong tinh gio */

    for (int it = 0; it < c->nt; it++) {
        const int T = c->threads[it];
        omp_set_num_threads(T);

        for (int iv = 0; iv < c->nv; iv++) {
            const int v = c->var[iv];
            const int serial = VARIANTS[v].fn == dot_seq;
            if (serial && it > 0) continue;   /* D0 khong phu thuoc T: do mot lan */
            const int tlabel = serial ? 1 : T;

            VARIANTS[v].fn(a, b, n, NULL);    /* khoi dong, bo */
            long reps = reps_for(VARIANTS[v].fn, a, b, n, c->min_time);
            fprintf(stderr, "# n=%zu T=%d %s reps=%ld\n", n, tlabel, VARIANTS[v].name, reps);

            for (int r = 1; r <= c->runs; r++) {
                timing_t tm;
                double res = measure(VARIANTS[v].fn, a, b, n, reps, &tm);
                int ok = fabs(res - ref) <= TOL * fabs(ref);
                if (!ok) {
                    bad++;
                    fprintf(stderr, "SAI: %s = %.17g khac dot_seq = %.17g (n=%zu T=%d run %d)\n",
                            VARIANTS[v].name, res, ref, n, tlabel, r);
                }
                char ts[32];
                now_iso(ts, sizeof ts);
                printf("%s,%s,%s,%s,%zu,%d,%s,%d,%s,%d,%.9f,%.9f,%.9f,%.9f,%.9f,%.17g,%d\n",
                       ts, c->host, c->kernel, VARIANTS[v].name, n, 0, "uniform", tlabel,
                       "static", r, tm.t_io, tm.t_init, tm.t_compute, tm.t_merge, tm.t_total,
                       res, ok);
                fflush(stdout);
            }
        }
    }
}

int main(int argc, char **argv) {
    char *ns[MAXLIST], *items[MAXLIST];
    int nn = 0, header = 1;
    char def_n[] = "1e6";
    size_t seed = 1;
    config_t c = { .nt = 0, .nv = 0, .runs = 7, .min_time = 0.05, .max_mb = 2500 };

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
        if (!strcmp(a, "--n")) {
            if ((nn = split(val, ns)) < 1) { usage(); return 2; }
        } else if (!strcmp(a, "--seed")) {
            if (parse_count(val, 1e18, &seed)) { usage(); return 2; }
        } else if (!strcmp(a, "--runs")) {
            if (parse_count(val, 1000, &x) || x < 1) { usage(); return 2; }
            c.runs = (int)x;
        } else if (!strcmp(a, "--min-time")) {
            if (parse_seconds(val, 10.0, &c.min_time)) { usage(); return 2; }
        } else if (!strcmp(a, "--max-mb")) {
            if (parse_count(val, 1e9, &x)) { usage(); return 2; }
            c.max_mb = (double)x;
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

    if (!nn) { ns[0] = def_n; nn = 1; }
    if (!c.nt) { c.threads[0] = omp_get_max_threads(); c.nt = 1; }
    if (!c.nv) for (size_t v = 0; v < NVAR; v++) c.var[c.nv++] = (int)v;
    host_info(c.host, sizeof c.host, c.kernel, sizeof c.kernel);

    if (header)
        printf("timestamp,host,kernel,variant,n,bins,dist,threads,schedule,run,"
               "t_io,t_init,t_compute,t_merge,t_total,checksum,ok\n");

    /* du lieu sinh trong bo nho: t_io = 0, thoi gian sinh khong tinh */
    for (int k = 0; k < nn; k++) {
        size_t n;
        if (parse_count(ns[k], 1e12, &n) || n < 1) { fprintf(stderr, "n khong hop le: %s\n", ns[k]); return 2; }
        if (2.0 * (double)n * sizeof(float) / (1 << 20) > c.max_mb) {
            fprintf(stderr, "bo qua n=%zu: 2 vector vuot %.0f MiB\n", n, c.max_mb);
            continue;
        }
        dataset_t A, B;
        if (dataset_generate(&A, DIST_UNIFORM, n, seed) != 0) {
            fprintf(stderr, "khong sinh duoc a, n=%zu\n", n);
            return 1;
        }
        if (dataset_generate(&B, DIST_UNIFORM, n, seed + 1) != 0) {
            fprintf(stderr, "khong sinh duoc b, n=%zu\n", n);
            dataset_free(&A);
            return 1;
        }
        bench_n(&c, A.x, B.x, n);
        dataset_free(&A);
        dataset_free(&B);
    }
    if (bad) fprintf(stderr, "CO %d LAN DO SAI KET QUA\n", bad);
    return bad ? 1 : 0;
}
