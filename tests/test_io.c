 /* Chay: make test */
#include "dataset.h"
#include "hist.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) printf("  [OK]   %s\n", msg); \
    else { printf("  [FAIL] %s\n", msg); fail = 1; } \
} while (0)

static int all_in_unit(const dataset_t *d) {
    for (size_t i = 0; i < d->n; i++)
        if (!(d->x[i] >= 0.0f && d->x[i] < 1.0f)) return 0;
    return 1;
}

/* Ty le phan tu roi vao bin 0 khi chia [0,1) thanh `bins` bin. */
static double frac_bin0(const dataset_t *d, int bins) {
    size_t c = 0;
    for (size_t i = 0; i < d->n; i++)
        if (d->x[i] < 1.0f / (float)bins) c++;
    return (double)c / (double)d->n;
}

/* Ly thuyet: Zipf tren K = 2^20 hang, hang k vao o bitrev(k-1) (xem docs/interfaces.md).
 * Bin 0 (voi B luy thua cua 2) chua cac hang 1, B+1, 2B+1, ... */
static double zipf_bin0_expected(double s, int bins) {
    const long K = 1L << 20;
    double h = 0.0, b0 = 0.0;
    for (long k = 1; k <= K; k++) {
        double p = pow((double)k, -s);
        h += p;
        if ((k - 1) % bins == 0) b0 += p;
    }
    return b0 / h;
}

/* Kiem tra chung cho moi phan phoi: sinh duoc, trong [0,1), can le, cung seed cung du lieu. */
static void test_each_dist(void) {
    const size_t N = 1000000;
    for (int d = 0; d < DIST_COUNT; d++) {
        dataset_t a, b;
        char msg[96];
        int ok = dataset_generate(&a, (dist_t)d, N, 7) == 0 &&
                 dataset_generate(&b, (dist_t)d, N, 7) == 0;
        snprintf(msg, sizeof msg, "%s: sinh duoc N=10^6", dist_name((dist_t)d));
        CHECK(ok, msg);
        if (!ok) continue;
        snprintf(msg, sizeof msg, "%s: moi phan tu trong [0,1), can le 64 byte",
                 dist_name((dist_t)d));
        CHECK(all_in_unit(&a) && ((uintptr_t)a.x % 64) == 0, msg);
        snprintf(msg, sizeof msg, "%s: cung seed -> cung du lieu", dist_name((dist_t)d));
        CHECK(memcmp(a.x, b.x, N * sizeof(float)) == 0, msg);
        snprintf(msg, sizeof msg, "%s: ten <-> ma", dist_name((dist_t)d));
        CHECK(dist_from_name(dist_name((dist_t)d)) == d, msg);
        dataset_free(&a);
        dataset_free(&b);
    }
}

static void test_normal(void) {
    const size_t N = 1000000;
    dataset_t a;
    dataset_generate(&a, DIST_NORMAL, N, 11);
    double sum = 0.0, sq = 0.0;
    int bounded = 1;
    for (size_t i = 0; i < N; i++) {
        sum += a.x[i];
        sq += (double)a.x[i] * a.x[i];
        if (a.x[i] < 0.2f || a.x[i] > 0.8f) bounded = 0;
    }
    double mean = sum / N, sd = sqrt(sq / N - mean * mean);
    CHECK(fabs(mean - 0.5) < 0.001, "normal: trung binh xap xi 0.5");
    CHECK(fabs(sd - 0.05) < 0.001, "normal: do lech chuan xap xi 0.05");
    CHECK(bounded, "normal: moi gia tri trong [0.2, 0.8]");
    dataset_free(&a);
}

static void test_zipf(void) {
    const size_t N = 1000000;
    const struct { dist_t d; double s; } Z[] = { { DIST_ZIPF11, 1.1 }, { DIST_ZIPF15, 1.5 } };
    const int BINS[] = { 16, 256, 1 << 20 };
    for (int z = 0; z < 2; z++) {
        dataset_t a;
        dataset_generate(&a, Z[z].d, N, 13);
        for (int k = 0; k < 3; k++) {
            double got = frac_bin0(&a, BINS[k]);
            double want = zipf_bin0_expected(Z[z].s, BINS[k]);
            char msg[128];
            snprintf(msg, sizeof msg, "%s: ty le bin 0 voi B=%d = %.4f (ly thuyet %.4f)",
                     dist_name(Z[z].d), BINS[k], got, want);
            CHECK(fabs(got - want) < 0.005, msg);
        }
        dataset_free(&a);
    }
}

static void test_onebin_sorted(void) {
    const size_t N = 1000000;
    dataset_t a, u;

    dataset_generate(&a, DIST_ONEBIN, N, 5);
    int same = 1;
    for (size_t i = 1; i < N; i++) same &= (a.x[i] == a.x[0]);
    CHECK(same, "onebin: moi phan tu bang nhau");
    dataset_free(&a);

    dataset_generate(&a, DIST_SORTED, N, 5);
    dataset_generate(&u, DIST_UNIFORM, N, 5);
    int sorted = 1;
    for (size_t i = 1; i < N; i++) sorted &= (a.x[i - 1] <= a.x[i]);
    CHECK(sorted, "sorted: tang dan");
    /* Moi gia tri la k/2^24 nen tong trong double la chinh xac, khong phu thuoc thu tu cong. */
    double s1 = 0.0, s2 = 0.0;
    for (size_t i = 0; i < N; i++) { s1 += a.x[i]; s2 += u.x[i]; }
    CHECK(s1 == s2, "sorted: cung cac gia tri voi uniform cung seed");
    dataset_free(&a);
    dataset_free(&u);
}

/* ---- Doc/ghi file (21/11). File tam ghi o thu muc hien tai roi xoa. ---- */
#define TMP_BIN "test_io_tmp.bin"
#define TMP_PGM "test_io_tmp.pgm"

static long file_size(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long s = ftell(f);
    fclose(f);
    return s;
}

static void write_raw(const char *path, const void *data, size_t len) {
    FILE *f = fopen(path, "wb");
    if (f) { fwrite(data, 1, len, f); fclose(f); }
}

static void test_bin(void) {
    const size_t N = 100000;
    dataset_t a, b;
    dataset_generate(&a, DIST_ZIPF15, N, 3);

    CHECK(dataset_save(TMP_BIN, &a, DIST_ZIPF15, 3) == 0, ".bin: ghi duoc");
    CHECK(file_size(TMP_BIN) == 32 + (long)(N * sizeof(float)), ".bin: kich thuoc = 32 + 4n byte");

    unsigned char hdr[32];
    FILE *f = fopen(TMP_BIN, "rb");
    size_t got = f ? fread(hdr, 1, 32, f) : 0;
    if (f) fclose(f);
    CHECK(got == 32 && memcmp(hdr, "HIST", 4) == 0 && hdr[4] == 1 && hdr[6] == 1 && hdr[24] == DIST_ZIPF15,
          ".bin: header co magic HIST, version 1, kieu float32, ma phan phoi");

    CHECK(dataset_load(TMP_BIN, &b) == 0 && b.n == N &&
          memcmp(a.x, b.x, N * sizeof(float)) == 0, ".bin: doc lai dung tung phan tu");
    CHECK(((uintptr_t)b.x % 64) == 0, ".bin: mang doc vao can le 64 byte");
    dataset_free(&b);

    /* File hong phai bao loi, khong duoc doc rac */
    CHECK(dataset_load("khong_ton_tai.bin", &b) != 0, ".bin: file khong ton tai -> loi");
    write_raw(TMP_BIN, "ABCD", 4);
    CHECK(dataset_load(TMP_BIN, &b) != 0, ".bin: file qua ngan -> loi");
    hdr[0] = 'X';
    write_raw(TMP_BIN, hdr, 32);
    CHECK(dataset_load(TMP_BIN, &b) != 0, ".bin: sai magic -> loi");
    hdr[0] = 'H';
    write_raw(TMP_BIN, hdr, 32);                /* header noi co N phan tu nhung khong co du lieu */
    CHECK(dataset_load(TMP_BIN, &b) != 0, ".bin: thieu du lieu so voi n trong header -> loi");

    dataset_t z = { 0 }, z2;
    CHECK(dataset_save(TMP_BIN, &z, DIST_UNIFORM, 0) == 0 && dataset_load(TMP_BIN, &z2) == 0 &&
          z2.n == 0, ".bin: n = 0 ghi va doc duoc");
    dataset_free(&z2);

    remove(TMP_BIN);
    dataset_free(&a);
}

/* Anh A5 trong tai lieu vi du: B = 4 -> [5, 4, 4, 3] */
static const uint8_t A5[16] = {
     12, 200,  45, 130,
    250,  64,  63, 190,
    128, 127,   5, 255,
     70,  33, 180,  99,
};

static void test_pgm(void) {
    dataset_t img;
    CHECK(pgm_save(TMP_PGM, A5, 4, 4) == 0, "pgm: ghi anh A5 4x4");
    CHECK(pgm_load(TMP_PGM, &img) == 0 && img.w == 4 && img.h == 4 && img.n == 16 &&
          memcmp(img.px, A5, 16) == 0, "pgm: doc lai dung kich thuoc va tung diem anh");

    /* Doi sang so thuc roi dem bang bin_of cua nhom, nhu cach hist_fn se dung */
    CHECK(dataset_pixels_to_unit(&img) == 0 && img.x != NULL, "pgm: doi diem anh sang [0,1)");
    uint64_t h[4] = { 0 };
    for (size_t i = 0; i < img.n; i++) h[bin_of(img.x[i], 4)]++;
    CHECK(h[0] == 5 && h[1] == 4 && h[2] == 4 && h[3] == 3, "pgm: anh A5, B = 4 -> [5, 4, 4, 3]");
    dataset_free(&img);

    /* Gia tri bien: 63 -> 0, 64 -> 1, 127 -> 1, 128 -> 2, 255 -> 3 (B = 4); 255 -> 255 (B = 256) */
    const uint8_t edge[5] = { 63, 64, 127, 128, 255 };
    const int want[5] = { 0, 1, 1, 2, 3 };
    dataset_t e = { 0 };
    e.px = (uint8_t *)edge;
    e.n = 5;
    int ok = dataset_pixels_to_unit(&e) == 0;
    for (int i = 0; ok && i < 5; i++) ok = bin_of(e.x[i], 4) == want[i];
    ok = ok && bin_of(e.x[4], 256) == 255 && bin_of(e.x[0], 256) == 63;
    CHECK(ok, "pgm: bien 63/64/127/128/255 vao dung bin");
    e.px = NULL;                                  /* edge la mang tinh, khong free */
    dataset_free(&e);

    /* Header co dong chu thich va xuong dong kieu khac van doc duoc */
    const char hdr[] = "P5\n# anh thu\n4  4\n255\n";
    unsigned char buf[sizeof hdr - 1 + 16];
    memcpy(buf, hdr, sizeof hdr - 1);
    memcpy(buf + sizeof hdr - 1, A5, 16);
    write_raw(TMP_PGM, buf, sizeof buf);
    CHECK(pgm_load(TMP_PGM, &img) == 0 && memcmp(img.px, A5, 16) == 0, "pgm: header co chu thich #");
    dataset_free(&img);

    write_raw(TMP_PGM, "P2\n4 4\n255\n", 11);
    CHECK(pgm_load(TMP_PGM, &img) != 0, "pgm: khong phai P5 -> loi");
    write_raw(TMP_PGM, buf, sizeof buf - 3);
    CHECK(pgm_load(TMP_PGM, &img) != 0, "pgm: thieu diem anh -> loi");
    remove(TMP_PGM);

    /* Anh tong hop cho E5: cung seed -> cung anh, gia tri trai rong */
    dataset_t g1, g2;
    dataset_generate_image(&g1, 256, 128, 9);
    dataset_generate_image(&g2, 256, 128, 9);
    int lo = 255, hi = 0;
    for (size_t i = 0; i < g1.n; i++) {
        if (g1.px[i] < lo) lo = g1.px[i];
        if (g1.px[i] > hi) hi = g1.px[i];
    }
    CHECK(g1.n == 256 * 128 && memcmp(g1.px, g2.px, g1.n) == 0, "anh tong hop: cung seed -> cung anh");
    CHECK(lo < 64 && hi > 192, "anh tong hop: do sang trai tu toi den sang");
    dataset_free(&g1);
    dataset_free(&g2);
}

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

    test_each_dist();
    test_normal();
    test_zipf();
    test_onebin_sorted();
    test_bin();
    test_pgm();

    printf(fail ? "KET QUA: LOI\n" : "KET QUA: DAT\n");
    return fail;
}
