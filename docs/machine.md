# Cấu hình máy đo (C)

## Phần cứng

| Mục | Giá trị |
|---|---|
| CPU | Intel Core Ultra 7 155H (Meteor Lake) |
| Core / thread (WSL thấy) | 11 core, 22 thread (2 thread/core) |
| Core thật của CPU | 6 P-core + 8 E-core + 2 LP E-core = 16 core, 22 thread (WSL ảo hoá topology nên báo 11×2) |
| Xung nhịp tối đa (Windows báo) | 3.8 GHz |
| L1d | 528 KiB (11 × 48 KiB) |
| L2 | 22 MiB (11 instance) |
| L3 | 24 MiB (chung) |
| RAM máy | 15.5 GiB |
| RAM cấp cho WSL | 7.5 GiB (swap 2 GiB), mặc định 50% RAM, chưa có `.wslconfig` |

## Phần mềm

| Mục | Giá trị |
|---|---|
| Host | Windows 11 Home 10.0.26200 |
| WSL | WSL2 3.0.5.0, kernel 6.18.58.1-microsoft-standard-WSL2 |
| Distro | Ubuntu 24.04.5 LTS (glibc 2.39) |
| GCC | 13.3.0 (Ubuntu 13.3.0-6ubuntu2~24.04.1) |
| GNU Make | 4.3 |
| OpenMP | `_OPENMP 201511` (OpenMP 4.5) |
| Cờ biên dịch | `-std=c11 -O3 -march=native -fopenmp -Wall -Wextra` (xem `make flags`) |

## Lưu ý khi đo

- `nproc` = 22, nhưng chỉ 6 P-core có hyper-threading (12 thread nhanh). 8 E-core chậm hơn, còn 2 LP E-core (nằm trên SoC tile, không dùng chung L3) chậm nhất. Speedup có thể gãy khúc ở khoảng 6–12 thread và gần như không tăng ở 21–22 thread; đó là đặc điểm CPU lai, không phải lỗi code.
- Topology WSL báo (11×2) là ảo, không khớp core thật, nên `OMP_PROC_BIND`/`OMP_PLACES` không ghim chính xác được vào P-core.
- Khác máy B (i7-12700H, 20 thread, WSL 3.6 GiB): khi so sánh số đo giữa hai máy, so speedup tương đối thay vì thời gian tuyệt đối.
- RAM WSL 7.5 GiB: dataset tới ~2–3 GiB vẫn an toàn; để so sánh được với máy B, giữ bộ dataset chung dưới ~1 GiB.
- Cắm sạc, chọn power mode "Best performance", đóng app nặng, chạy mỗi phép đo 3–5 lần lấy trung vị.

## Lệnh đã dùng

```bash
lsb_release -ds
uname -r
gcc --version | head -1
make --version | head -1
ldd --version | head -1
echo | gcc -fopenmp -dM -E - | grep _OPENMP
nproc
lscpu | grep -E "Model name|^CPU\(s\)|Thread|Core|Socket|L1d|L1i|L2|L3"
free -h
```

```powershell
Get-CimInstance Win32_Processor | Select-Object Name,NumberOfCores,NumberOfLogicalProcessors,MaxClockSpeed
wsl --version
```
