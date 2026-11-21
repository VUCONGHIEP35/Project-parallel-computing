# Đặc tả giao diện chung

## Dữ liệu (Thành viên A)

### Tên phân phối

Dùng đúng các tên sau trong CSV (cột `dist`) và dòng lệnh (`gen_data`, `hist_bench`):

| Tên | `dist_t` | Mô tả |
|---|---|---|
| `uniform` | `DIST_UNIFORM` | đều trên [0,1): x = k / 2^24, k ngẫu nhiên 24 bit |
| `normal` | `DIST_NORMAL` | μ = 0,5; σ = 0,05, xấp xỉ bằng tổng 12 biến đều; nằm trong [0,2; 0,8] |
| `zipf1.1` | `DIST_ZIPF11` | Zipf s = 1,1 trên 2^20 hạng; bin 0 ≈ 12–17% |
| `zipf1.5` | `DIST_ZIPF15` | Zipf s = 1,5 trên 2^20 hạng; bin 0 ≈ 38–40% |
| `onebin` | `DIST_ONEBIN` | mọi phần tử bằng 0,3 |
| `sorted` | `DIST_SORTED` | đúng dữ liệu `uniform` cùng seed, đã sắp xếp tăng dần |

Cùng `(dist, n, seed)` cho cùng dữ liệu trên mọi máy: bộ sinh số tự viết (splitmix64 + xorshift64*), không dùng `rand()`/`rand_r()`. Riêng Zipf có dùng `pow()` khi lập bảng xác suất nên có thể lệch ở mức làm tròn cuối giữa các thư viện C khác nhau.

### Chi tiết Zipf (đề xuất, chờ nhóm xác nhận)

- Hạng k = 1..K với K = 2^20, P(k) ∝ k^(-s). Lấy mẫu bằng bảng phân phối tích lũy + bảng dẫn đường (guide table).
- Hạng k được đặt vào ô `bitrev20(k - 1)` (đảo thứ tự 20 bit), mỗi ô rộng 2^-20, rồi chọn ngẫu nhiên một trong 16 vị trí trong ô.
- Nhờ đảo bit, các hạng nóng nằm ở các bin khác nhau: hạng 1 luôn ở bin 0, hạng 2 ở bin B/2, hạng 3 ở bin B/4, ... Với mọi B là lũy thừa của 2, bin 0 chứa các hạng 1, B+1, 2B+1, ... nên tỉ lệ bin 0 gần bằng P(hạng 1), không phụ thuộc nhiều vào B:

| | B = 16 | B = 256 | B = 65 536 | B = 2^20 |
|---|---|---|---|---|
| zipf1.1 | 16,6% | 12,5% | 12,4% | 12,4% |
| zipf1.5 | 39,8% | 38,3% | 38,3% | 38,3% |

(Lý thuyết; `tests/test_io.c` kiểm tra lại bằng dữ liệu sinh ra.)

### File `.bin`

Header 32 byte, little-endian, rồi đến mảng thô `n` phần tử.

| Offset | Kích thước | Kiểu | Trường |
|---|---|---|---|
| 0 | 4 | char[4] | magic `"HIST"` |
| 4 | 2 | uint16 | version = 1 |
| 6 | 2 | uint16 | kiểu phần tử: 1 = float32, 2 = uint8 |
| 8 | 8 | uint64 | n (số phần tử) |
| 16 | 8 | uint64 | seed |
| 24 | 4 | uint32 | mã phân phối (`dist_t`) |
| 28 | 4 | uint32 | dự trữ, ghi 0 |
| 32 | n × 4 (hoặc n × 1) | | dữ liệu |

`dataset_load` báo lỗi nếu sai magic, sai version hoặc file ngắn hơn `32 + n × kích thước phần tử`.

Phiên bản 1 chỉ ghi `float32` (`dataset_save` ghi `ds->x`); ảnh dùng PGM. Mảng số được ghi nguyên khối nên máy phải là little-endian (x86, ARM thông dụng); `dataset_save`/`dataset_load` tự báo lỗi trên máy big-endian.

### Ảnh

PGM nhị phân P5, maxval 255, đọc/ghi bằng `io_pgm.c` (không dùng thư viện ngoài). Header được phép có dòng chú thích `#`.

| Hàm | Việc |
|---|---|
| `pgm_save(path, px, w, h)` | ghi ảnh |
| `pgm_load(path, &ds)` | đọc vào `ds.px`, `ds.w`, `ds.h`, `ds.n = w·h` |
| `dataset_generate_image(&ds, w, h, seed)` | sinh ảnh tổng hợp cho E5: sáng dần theo hàng + nhiễu, cùng seed cùng ảnh |
| `dataset_pixels_to_unit(&ds)` | tạo `ds.x[i] = px[i] / 256` để đưa ảnh vào `hist_fn` |

**Đưa ảnh vào `hist_fn` (đề xuất, chờ nhóm xác nhận):** đổi điểm ảnh p thành x = p / 256. Phép chia này chính xác trong `float`, nên với B là lũy thừa của 2 và B ≤ 256 thì `bin_of(x, B) = p·B/256`, đúng bin của ảnh gốc. Mảng `x` tốn gấp 4 lần bộ nhớ ảnh; thời gian đổi tính vào `t_init`.

### Chương trình `gen_data`

```bash
gen_data <phan_phoi> <n> <seed> <file.bin>      # vd: gen_data zipf1.5 1e8 1 ~/data/zipf15_1e8.bin
gen_data image <rong> <cao> <seed> <file.pgm>   # vd: gen_data image 4096 4096 1 ~/data/img4096.pgm
```

File dữ liệu lớn để ngoài repo (ví dụ `~/data`), không commit.

## Tích vô hướng

### Hàm

Khai báo trong `include/dot.h`, cài đặt trong `src/dot/`. Mọi phương án cùng chữ ký `dot_fn`:

```c
double dot_xxx(const float *a, const float *b, size_t n, timing_t *tm);
```

- Đầu vào `float` (sinh bằng `dataset_generate`), cộng dồn và trả về `double`.
- `n = 0` trả về 0; khi đó `a`, `b` được phép là `NULL`. `tm` được phép là `NULL`.

| Tên | Mô tả | Trạng thái |
|---|---|---|
| `dot_seq` | D0, tuần tự: mốc speedup, đáp án đối chiếu | có |
| `dot_reduce` | D1, `parallel for reduction(+:s)`: OpenMP tự chia, giữ tổng riêng và gộp | có |
| `dot_partial` | D2, tự chia khối `[lo, hi)` cho từng thread, tổng riêng trong biến cục bộ, ghi 1 lần vào mảng đệm 64 byte, 1 thread gộp. Tách được `t_init` / `t_compute` / `t_merge` | có |

### Đối chiếu kết quả

Thứ tự cộng thay đổi theo số thread nên bản song song có thể lệch vài chữ số cuối so với D0. Bản song song đạt nếu `|r - r_D0| / |r_D0| < 1e-6`.

### CSV của `dot_bench`

Cột: `version,n,threads,time_s,gbps,result`.

- `time_s`: thời gian một lần gọi (trung vị 5 lần đo; `n` nhỏ thì lặp nhiều lần rồi chia).
- `gbps`: băng thông bộ nhớ đạt được = `8 · n / time_s / 10^9` (đọc 2 `float` mỗi phần tử).
- Dữ liệu: `a` = uniform seed 1, `b` = uniform seed 2.
