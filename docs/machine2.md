# Cấu hình máy đo (B)

Máy tham chiếu để thiết kế chương trình.

## Phần cứng

| Mục | Giá trị |
|---|---|
| CPU | 12th Gen Intel Core i7-12700H |
| Core / thread (WSL thấy) | 10 core, 20 thread (2 thread/core) |
| Core thật của CPU | 6 P-core + 8 E-core = 14 core, 20 thread (WSL ảo hoá topology nên báo 10×2) |
| L1d | 480 KiB (10 × 48 KiB) |
| L2 | 12.5 MiB (10 instance) |
| L3 | 24 MiB (chung) |
| RAM cấp cho WSL | 3.6 GiB (swap 1 GiB) |

## Phần mềm

| Mục | Giá trị |
|---|---|
| Host | Windows 11 Home 10.0.26200 |
| WSL | WSL2, Ubuntu 26.04 LTS (glibc 2.43) |
| GCC | 15.2.0 (Ubuntu 15.2.0-16ubuntu1) |
| OpenMP | `_OPENMP 201511` (OpenMP 4.5) |
| Cờ biên dịch | `-std=c11 -O3 -march=native -fopenmp -Wall -Wextra` (xem `make flags`) |

## Lưu ý khi đo

- `nproc` = 20, nhưng chỉ 6 P-core có hyper-threading; E-core chậm hơn P-core. Speedup có thể gãy khúc ở khoảng 6–8 thread rồi tăng chậm, đó là đặc điểm CPU lai chứ không phải lỗi code.
- RAM WSL chỉ 3.6 GiB. Dataset nên dưới ~1 GiB; nếu cần lớn hơn, tăng qua `C:\Users\<tên>\.wslconfig` (`memory=8GB`) rồi `wsl --shutdown`.
- Cắm sạc, chọn power mode "Best performance", đóng app nặng, chạy mỗi phép đo 3–5 lần lấy trung vị.

## Lệnh đã dùng

```bash
lsb_release -ds
gcc --version | head -1
echo | gcc -fopenmp -dM -E - | grep _OPENMP
nproc
lscpu | grep -E "Model name|^CPU\(s\)|Thread|Core|L1d|L2|L3"
free -h
```
