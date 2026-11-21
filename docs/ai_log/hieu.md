# Nhật ký dùng AI – Hiếu

### A1
- Thời gian: 20/11/2026
- Người sử dụng: Hiếu
- Công cụ/mô hình: Claude Opus 5.5
- Mục đích:  ánh xạ Zipf vào [0,1) để bin 0 ≈ 38% (s = 1,5) với mọi B; sinh 10^8 phần tử đủ nhanh; normal cho cùng dữ liệu trên mọi máy
- Tóm tắt prompt: Hoàn thiện phân phối đảo bit, onebin, sorted và test


### A2
- Thời gian: 21/11/2026
- Người sử dụng: Hiếu
- Công cụ/mô hình: Claude Opus 5.5
- Mục đích: viết V1 (atomic), V2 (critical) đúng và không có race; cách chứng minh không race khi repo chưa có CI chạy TSan; hiểu vì sao atomic có thể chậm hơn cả bản tuần tự (cache line bị giành qua lại giữa các lõi, false sharing khi 8 bộ đếm chung một cache line)
- Tóm tắt prompt: AI giải thích cách dùng omp atomic, omp critical, vì sao atomic có thể chậm hơn tuần tự và cách kiểm chứng không có race và cách kiểm chứng
