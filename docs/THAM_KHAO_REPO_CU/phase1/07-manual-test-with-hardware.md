# Manual Test with Hardware — Central Logger App

Checklist test thủ công với thiết bị Data Logger thật.
Điền cột `Actual` và `Pass` khi chạy; ghi failures vào `02-pain-points.md`.

## Chuẩn bị

- 1+ Data Logger với Modbus TCP (default port **5020**), REST API (default port **8080**), token hợp lệ.
- Nếu logger chỉ listen IPv4, dùng host `127.0.0.1` (không dùng `localhost`).
- App đang chạy: `python -m central_logger` (hoặc `uv run python -m central_logger`).
- `CENTRAL_LOGGER_DEBUG=1` để xem log chi tiết nếu cần.

## Bảng test


| #    | Scenario                   | Steps                                                                                                   | Expected                                                                                            | Actual                                                                      | Pass |
| ---- | -------------------------- | ------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------- | ---- |
| H-01 | Modbus online              | 1. Add logger (đúng host:5020, unit_id). 2. Chờ 2–3 giây.                                               | Status badge chuyển Online; sensor count > 0 trong table.                                           | Add new logger -> status badge change to Online                             | pass |
| H-02 | Modbus offline             | 1. Stop device **hoặc** nhập sai host. 2. Chờ backoff.                                                  | Badge chuyển Offline; "Offline" xuất hiện trong Recent Events; UI không treo.                       | status badge change to offfline when logger offline or edit wrong host/port | pass |
| H-03 | REST config fetch          | 1. Logger online, có API token. 2. Mở Logger Detail.                                                    | `configFetched` signal; revision hiển thị; device fields hiển thị (station_code, poll_interval...). | expect                                                                      |      |
| H-04 | REST apply config          | 1. Logger online. 2. Edit → thay đổi station_name hoặc poll_interval. 3. Save.                          | Snackbar "Config applied"; `currentRevision` tăng.                                                  | expect                                                                      |      |
| H-05 | REST 401                   | 1. Logger online. 2. Đặt api_token sai. 3. Fetch config.                                                | Snackbar hiển thị lỗi human-readable ("Token mismatch…" hoặc tương tự); không crash.                | expect                                                                      |      |
| H-06 | Readings stale             | 1. Logger online, có DI/DO sensors. 2. Mở Logger Detail. 3. Chờ `fetchReadingsIfStale`.                 | Sensor table cập nhật DI/DO values; không block UI.                                                 |                                                                             |      |
| H-07 | Report download            | 1. Logger online, token hợp lệ. 2. Click download icon. 3. Chọn path save.                              | File được lưu xuống; snackbar "Report downloaded" (hoặc tương tự).                                  |                                                                             |      |
| H-08 | QR provision scan          | 1. Có file PNG QR hợp lệ (schema `central-logger-provision/v1`). 2. Add Logger → Scan QR. 3. Chọn file. | Các field: host, token, apiPort được điền tự động.                                                  |                                                                             |      |
| H-09 | Retention purge            | 1. Có dữ liệu sensor > N ngày. 2. Settings → `data_retention_days` = 1. 3. Save.                        | Old sensor_reading rows bị xóa; ingestion chart cập nhật.                                           |                                                                             |      |
| H-10 | Stress — 3+ loggers 30 min | 1. Add 3+ loggers, mỗi cái poll_interval = 2s. 2. Để chạy 30 phút. 3. Quan sát CPU/RAM.                 | CPU ổn định; RAM không tăng không ngừng; không crash hoặc UI freeze.                                |                                                                             |      |


---

## Ghi chú sau khi test

> *(Điền khi chạy)*

**Ngày test:** _______________

**Người test:** _______________

**Thiết bị:** _______________

**Failures → link pain point:**


| Test ID | Failure description | Pain Point ref |
| ------- | ------------------- | -------------- |
|         |                     |                |


**Open questions từ hardware test (cho Phase 2):**

- ---
- ---

