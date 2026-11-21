/* gen_data - sinh file du lieu cho thi nghiem (Thanh vien A).
 *
 *   gen_data <phan_phoi> <n> <seed> <file.bin>     vd: gen_data zipf1.5 1e8 1 ~/data/zipf15_1e8.bin
 *   gen_data image <rong> <cao> <seed> <file.pgm>  vd: gen_data image 4096 4096 1 ~/data/img4096.pgm
 *
 * phan_phoi: uniform normal zipf1.1 zipf1.5 onebin sorted. n nhan ca dang 1e6, 1e8.
 * File lon de ngoai repo (vd ~/data), khong commit. */
#include "dataset.h"

#include <errno.h>
#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void) {
    fprintf(stderr,
            "Cach dung:\n"
            "  gen_data <phan_phoi> <n> <seed> <file.bin>\n"
            "  gen_data image <rong> <cao> <seed> <file.pgm>\n"
            "phan_phoi:");
    for (int d = 0; d < DIST_COUNT; d++) fprintf(stderr, " %s", dist_name((dist_t)d));
    fprintf(stderr, "\n");
}

/* So nguyen khong am, nhan ca dang khoa hoc (1e8). Tra ve -1 neu khong hop le. */
static int parse_count(const char *s, uint64_t *out) {
    char *end;
    errno = 0;
    double v = strtod(s, &end);
    if (errno || *end != '\0' || v < 0 || v > 1e12 || v != floor(v)) return -1;
    *out = (uint64_t)v;
    return 0;
}

static int run_image(char **argv) {
    uint64_t w, h, seed;
    if (parse_count(argv[2], &w) || parse_count(argv[3], &h) || parse_count(argv[4], &seed) ||
        w == 0 || h == 0 || w > 65536 || h > 65536) {
        usage();
        return 2;
    }
    dataset_t ds;
    double t0 = omp_get_wtime();
    if (dataset_generate_image(&ds, (int)w, (int)h, seed) != 0) {
        fprintf(stderr, "Loi: khong du bo nho cho anh %llux%llu\n",
                (unsigned long long)w, (unsigned long long)h);
        return 1;
    }
    double t1 = omp_get_wtime();
    if (pgm_save(argv[5], ds.px, ds.w, ds.h) != 0) {
        fprintf(stderr, "Loi: khong ghi duoc %s\n", argv[5]);
        dataset_free(&ds);
        return 1;
    }
    double t2 = omp_get_wtime();
    printf("image %dx%d seed=%llu -> %s (sinh %.2f s, ghi %.2f s)\n", ds.w, ds.h,
           (unsigned long long)seed, argv[5], t1 - t0, t2 - t1);
    dataset_free(&ds);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 5 && !(argc == 6 && strcmp(argv[1], "image") == 0)) {
        usage();
        return 2;
    }
    if (argc == 6) return run_image(argv);

    int d = dist_from_name(argv[1]);
    uint64_t n, seed;
    if (d < 0 || parse_count(argv[2], &n) || parse_count(argv[3], &seed)) {
        usage();
        return 2;
    }

    dataset_t ds;
    double t0 = omp_get_wtime();
    if (dataset_generate(&ds, (dist_t)d, (size_t)n, seed) != 0) {
        fprintf(stderr, "Loi: khong sinh duoc du lieu (thieu bo nho?)\n");
        return 1;
    }
    double t1 = omp_get_wtime();
    if (dataset_save(argv[4], &ds, (dist_t)d, seed) != 0) {
        fprintf(stderr, "Loi: khong ghi duoc %s\n", argv[4]);
        dataset_free(&ds);
        return 1;
    }
    double t2 = omp_get_wtime();

    double sum = 0.0;
    for (size_t i = 0; i < ds.n; i++) sum += ds.x[i];
    printf("%s n=%llu seed=%llu -> %s (sinh %.2f s, ghi %.2f s, trung binh %.4f)\n",
           dist_name((dist_t)d), (unsigned long long)n, (unsigned long long)seed, argv[4],
           t1 - t0, t2 - t1, ds.n ? sum / (double)ds.n : 0.0);
    dataset_free(&ds);
    return 0;
}
