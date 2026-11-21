# Histogram song song cho dữ liệu lớn (OpenMP)

**Tính toán song song**, chủ đề 4.

## Môi trường

Windows 10/11 + WSL2 Ubuntu 24.04 (hoặc mới hơn), GCC + OpenMP, GNU Make.

## Cài đặt

Trong **PowerShell (Run as Administrator)**:

```powershell
wsl --install -d Ubuntu-24.04
```

Khởi động lại nếu được yêu cầu, mở Ubuntu, tạo username/password, rồi:

```bash
sudo apt update && sudo apt install -y build-essential git
```

## Kiểm tra

Chạy trong Ubuntu:

```bash
gcc --version | head -1                         # gcc 13.x trở lên
make --version | head -1                        # GNU Make 4.x
echo | gcc -fopenmp -dM -E - | grep _OPENMP     # phải in: #define _OPENMP 2015xx
nproc                                           # số thread WSL được dùng
lscpu | grep -E "Model name|Core|Thread|L2|L3"  # CPU, core, cache
free -h                                         # RAM cấp cho WSL
```

Thiếu dòng `_OPENMP` thì GCC chưa có OpenMP: `sudo apt install libgomp1`.

Lưu kết quả vào `docs/machine(1,2,3).md` (mẫu: [docs/machine2.md], máy tham chiếu của người thứ 2) để nhóm đối chiếu khi so sánh số đo.

## Build và kiểm thử

```bash
make          # build bản release -> build/release/
make test     # chạy test với 1, 2, 4, 8 thread
make debug    # chạy test với AddressSanitizer/UBSan
make flags    # in cờ biên dịch (ghi vào báo cáo)
make clean
```

## Cấu trúc thư mục

```
include/   header dùng chung
src/       mã nguồn
apps/      chương trình đo
tests/     kiểm thử
docs/      biên bản họp, nhật ký AI, cấu hình máy
```

## Tài liệu
[Dùng AI](AI_USAGE.md) ·
