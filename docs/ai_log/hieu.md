# Nhật ký dùng AI – Hiếu

### A1
- Thời gian: 19–20/11/2026
- Người sử dụng: Hiếu
- Công cụ/mô hình: Claude Opus 5.5
- Mục đích:  ánh xạ Zipf vào [0,1) để bin 0 ≈ 38% (s = 1,5) với mọi B; sinh 10^8 phần tử đủ nhanh; normal cho cùng dữ liệu trên mọi máy
- Prompt hoặc tóm tắt prompt: Hoàn thiện 6 phân phối và test
- Kết quả: AI viết normal, Zipf (đảo bit), onebin, sorted và test thống kê
- Phần đã sử dụng: dataset.c, dataset.h, test_io.c, interfaces.md
- Kiểm chứng: make test, make debug đạt; tỉ lệ bin 0 của Zipf khớp lý thuyết
- Commit/issue liên quan: <hash, PR #...>