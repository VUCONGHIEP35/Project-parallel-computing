# Đặc tả giao diện chung

## Dữ liệu (Thành viên A)

### Tên phân phối

Dùng đúng các tên sau trong CSV (cột `dist`) và dòng lệnh (`gen_data`, `hist_bench`):

| Tên | `dist_t` | Mô tả |
|---|---|---|
| `uniform` | `DIST_UNIFORM` | đều trên [0,1) |
| `normal` | `DIST_NORMAL` | μ = 0,5; σ = 0,05; kẹp về [0,1) |
| `zipf1.1` | `DIST_ZIPF11` | Zipf s = 1,1 |
| `zipf1.5` | `DIST_ZIPF15` | Zipf s = 1,5 |
| `onebin` | `DIST_ONEBIN` | mọi phần tử cùng một giá trị |
| `sorted` | `DIST_SORTED` | uniform đã sắp xếp tăng dần |

Cùng `(dist, n, seed)` cho cùng dữ liệu trên mọi máy: bộ sinh số tự viết (splitmix64 + xorshift64*), không dùng `rand()`/`rand_r()`.

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
