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

### Ảnh

PGM nhị phân P5, maxval 255, đọc/ghi bằng `io_pgm.c` (không dùng thư viện ngoài).

## Histogram (Thành viên B)

### Hàm

Khai báo trong `include/hist.h`, cài đặt trong `src/hist/`. Mọi phương án cùng chữ ký `hist_fn`:

```c
void hist_xxx(const float *x, size_t n, int bins, uint64_t *out, timing_t *tm);
```

- Hàm tự xóa `out[0..bins-1]` rồi đếm; người gọi không cần `memset`.
- `n = 0` cho histogram toàn 0; khi đó `x` được phép là `NULL`. `tm` được phép là `NULL`.
- Điều kiện: `1 ≤ bins ≤ 2^24` (để `bins` biểu diễn đúng bằng `float`).

| Tên | Mô tả | Người làm | Trạng thái |
|---|---|---|---|
| `hist_seq` | V0, tuần tự: mốc speedup, đáp án đối chiếu | B | có |
| `hist_atomic` | V1, `omp atomic` | A | |
| `hist_critical` | V2, `omp critical` | A | |
| `hist_local_serial` | V3a, histogram cục bộ mỗi thread, gộp tuần tự | B | có |
| `hist_local_parmerge` | V3b, histogram cục bộ, gộp song song theo bin | B | |
| `hist_omp_reduce` | V4, `reduction(+:out[:bins])` | B | |

### Quy tắc chia bin

Mọi phương án **phải** dùng `bin_of()` trong `hist.h`, không tự tính lại:

| Giá trị `x` | Bin |
|---|---|
| `0 < x < 1` | `min(floor(x · bins), bins − 1)` |
| `x ≤ 0`, `-0`, **NaN**, `-∞` | 0 |
| `x ≥ 1`, `+∞`, số rất lớn (1e30) | `bins − 1` |

`bin_of` so sánh trước rồi mới ép kiểu, nên không có hành vi không xác định với NaN, ±∞. **Không biên dịch với `-ffast-math`**: cờ này cho phép compiler giả định không có NaN và bỏ phép so sánh.

### Thời gian (`timing_t`)

| Trường | Gồm |
|---|---|
| `t_io` | đọc file (0 nếu dữ liệu đã có trong bộ nhớ) |
| `t_init` | cấp phát và xóa histogram (cả các histogram cục bộ) |
| `t_compute` | vòng lặp đếm |
| `t_merge` | gộp các histogram cục bộ vào `out` (0 với V0) |
| `t_total` | từ đầu đến hết gộp, không tính `free` |

### Đối chiếu kết quả

Đếm số nguyên nên mọi phương án phải cho **đúng từng bin** như V0. Thêm phương án mới vào bảng `VARIANTS` trong `tests/test_hist.c` để được đối chiếu với V0 trên 6 phân phối và nhiều `bins`.

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

### Đối chiếu kết quả

Thứ tự cộng thay đổi theo số thread nên bản song song có thể lệch vài chữ số cuối so với D0. Bản song song đạt nếu `|r - r_D0| / |r_D0| < 1e-6`.

### CSV của `dot_bench`

Cột: `version,n,threads,time_s,gbps,result`.

- `time_s`: thời gian một lần gọi (trung vị 5 lần đo; `n` nhỏ thì lặp nhiều lần rồi chia).
- `gbps`: băng thông bộ nhớ đạt được = `8 · n / time_s / 10^9` (đọc 2 `float` mỗi phần tử).
- Dữ liệu: `a` = uniform seed 1, `b` = uniform seed 2.
