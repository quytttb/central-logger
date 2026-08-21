# ADR 0002 — Store-on-change cho `sensor_reading`

**Trạng thái:** Accepted (audit H-C) — 2026-08-17

## Bối cảnh

Central poll mỗi logger mỗi ~2s. Với cách ghi "mọi sample mọi sensor" (gồm cả DI/DO),
10 logger × 30 sensor tạo ~13 triệu dòng/ngày dù phần lớn giá trị không đổi.
Bảng `sensor_reading` phình nhanh, chart query và purge phải quét nhiều hơn, WAL lớn.

## Quyết định

`ModbusBridge::buildReadings` (chạy trên writer thread) chỉ ghi một reading khi:

1. **value thay đổi** so với lần ghi gần nhất của sensor đó, hoặc
2. cờ **valid/alarm/stale thay đổi**, hoặc
3. **Heartbeat**: đã ≥ 15 phút (`kHeartbeatMs`) kể từ lần ghi gần nhất của sensor đó
   (đảm bảo chart liên tục có điểm, purge/cut-off hoạt động).

Trạng thái "lần viết cuối" giữ trong RAM (`m_lastValue`/`m_lastFlags`/`m_lastWrittenMs`),
chỉ tồn tại trên history-writer thread nên không cần lock. Sau khởi động lại app,
sample đầu tiên luôn được ghi (coi là thay đổi).

## Hệ quả

- Write volume giảm 1–2 bậc độ lớn ở trạng thái ổn định.
- Chart "số reading theo giờ" vẫn phản ánh traffic; chart giá trị ít điểm hơn
  nhưng đúng nghĩa (giá trị không đổi ⇒ 1 điểm + điểm heartbeat).
- History search không còn dòng cho mỗi chu kỳ poll liên tục — đây là hành vi
  mong muốn (dedup). Nếu cần audit trail từng sample, cân nhắc lưu ở edge.
- Không cần đổi schema — chỉ đổi cách ghi.

## Từ chối các phương án khác

- **Giữ nguyên write-all:** đơn giản nhưng không scale (13M dòng/ngày).
- **Downsampling theo thời gian (ghi mỗi N giây):** mất chi tiết khi giá trị
  dao động nhanh trong cửa sổ N giây; store-on-change tự thích ứng tần suất
  thay đổi thực tế.
