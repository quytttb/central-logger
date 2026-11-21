# REST config contract v1 (Central ↔ data-logger)

Base URL: `http://<host>:<api_port>/api/v1`

> Từ 2026-11-21, **Central chỉ dùng phần đọc** của contract. Data Logger và
> adapter OSG giữ nguyên hành vi v1 trên wire. `POST /config` bên dưới là
> tham chiếu cho client khác, không phải đường gọi của Central.

| Thuộc tính | Giá trị |
|------------|---------|
| API version | **v1** |
| Auth | `Authorization: Bearer <api_token>` |
| Content-Type | `application/json` (config); binary cho report |
| Optimistic locking | `expected_revision` trên `POST /config` |

**Phạm vi đọc của Central:**

| REST | Vai trò |
|------|---------|
| `GET /config` | Đọc cấu hình trạm, **catalog sensor** (`sensor_type`, tên, ngưỡng), revision |
| `GET /reports/latest` | Endpoint tải báo cáo on demand của edge, không có nút trong UI hiện tại |
| `GET /readings` | **Debug thủ công** — xem JSON gốc từ edge (1 lần / lần bấm); **không** feed bảng sensor live |

**Live values (ANALOG + DI + DO):** chỉ đọc Modbus — FC03 (analog) + FC02 (DI) + FC01 (DO) khi có, cùng nhịp `central_poll_interval_s`. Xem [`thiet_ke_db.md`](../thiet_ke_db.md).

---

## Central DB (`logger_info`)

| Column | Role |
|--------|------|
| `name`, `host`, `modbus_port`, `modbus_unit_id`, `central_poll_interval_s`, `timeout_s` | Tên hiển thị và kết nối của Central |
| `api_port`, `api_token` | Kết nối REST của Central |
| `last_revision` | Revision đọc gần nhất |
| `enabled`, `note` | Giám sát và ghi chú cục bộ |

`Save` trong Settings chỉ lưu dữ liệu ở Central. `Reload config` chỉ cập nhật
catalog/revision sau GET, không ghi đè các cài đặt trên.

---

## `GET/POST /config`

### `GET /config`

| REST key | Central sử dụng |
|----------|----------------------------|
| `station_code` | Gợi ý mã trạm khi Add |
| `station_name` | Gợi ý tên hiển thị khi Add |
| `poll_interval` | Chu kỳ thiết bị (giây), không dùng làm chu kỳ đọc Central |
| `modbus_tcp_bind` | Modbus TCP bind tại edge, không sửa từ Central |
| `modbus_tcp_enabled` | Trạng thái Modbus TCP server tại edge, không sửa từ Central |
| `modbus_tcp_unit_id` | Gợi ý Unit ID cho kết nối Central khi Add |

Response **phải** đủ để Central cập nhật `logger_sensor`: `sensor_id`, `sensor_type` (`ANALOG`|`DI`|`DO`), `name`, `unit`, `active`, và ngưỡng cảnh báo (nếu nguồn dữ liệu có). `revision` và danh sách `sensors[]` phải được đọc trong một snapshot nhất quán.

**`thresholds_writable` (tùy chọn, boolean ở root hoặc trong `config`):**
`true` khi firmware hỗ trợ ghi ngưỡng qua `POST /config` (`config.sensors[]`).
Firmware cũ và adapter read-only có thể không gửi. Central bỏ qua cờ này;
ngưỡng luôn chỉ đọc trong Central.

**`min_threshold` / `max_threshold` (ANALOG):**
- Thiết bị là nguồn sự thật duy nhất của ngưỡng cảnh báo (điều khiển relay DO và báo cáo TT10 tại edge).
- `data-logger` hỗ trợ ghi ngưỡng luôn xuất `min_threshold` và `max_threshold` cho từng cảm biến `ANALOG`: số hữu hạn hoặc `null` khi xác nhận chưa đặt giới hạn.
- Nguồn/adapter không cung cấp ngưỡng **bỏ hẳn key** (Central phân biệt vắng key = chưa biết giới hạn với `null` = đã xác nhận không đặt).

**`decimals` (tùy chọn, ANALOG):** số chữ số thập phân hiển thị cho giá trị analog. Integer, range **0–6**, **default 4** khi vắng mặt. Central clamp vào [0,6], lưu `logger_sensor.decimals`, và dùng cho bảng/thẻ sensor live, History, CSV export, và tooltip biểu đồ trending. DI/DO bỏ qua (hiển thị ON/OFF). Field này **additive**, backward-compatible: firmware cũ không gửi → Central dùng 4.

**Không có trong snapshot:** `rest_api_token` — [provision QR](provision-qr-v1.md).

**Không gửi khi POST:** `logger_serial`, `cloud_enabled`, `cloud_endpoint` — edge trả **422** (`extra=forbid`).

### `POST /config` — optimistic locking & patch ngưỡng

**Edge API giữ nguyên; Central không gọi endpoint này.**

```json
{
  "api_version": 1,
  "request_id": "central-<uuid>",
  "expected_revision": <int>,
  "config": {
    "station_name": "...",
    "poll_interval": 10,
    "sensors": [
      { "sensor_id": 3, "min_threshold": 0, "max_threshold": 30 },
      { "sensor_id": 4, "min_threshold": null, "max_threshold": 320 }
    ]
  }
}
```

| Field | Bắt buộc | Ghi chú |
|-------|----------|---------|
| `api_version` | Có | Phải `1` (khớp `/api/v1`) |
| `request_id` | Có | Trace id từ client (string, 1..128 ký tự) |
| `expected_revision` | Có | Optimistic lock (số nguyên `>= 0`) |
| `config` | Có | Patch partial app-config và/hoặc `sensors[]` (**patch ngưỡng**, không replace toàn bộ catalog) |

**Quy tắc `config.sensors[]` (chốt 2026-11-18):**
1. Mỗi phần tử chỉ cho phép các khóa `sensor_id`, `min_threshold`, `max_threshold`; bắt buộc có `sensor_id` và ít nhất một trong hai khóa ngưỡng. Trùng `sensor_id` hoặc dư khóa khác (vd. `name`, `decimals`) → **422**.
2. Giá trị ngưỡng chỉ nhận số hữu hạn hoặc `null` (`null` = xóa ngưỡng; vắng khóa = giữ nguyên giá trị hiện có trên thiết bị).
3. `sensor_id` phải là cảm biến `ANALOG` đang `active` và tồn tại trên thiết bị; không tạo/xóa cảm biến hay sửa liên kết DO qua patch này. ID không hợp lệ → **422**, rollback toàn bộ transaction.
4. Sau khi ghép với giá trị hiện tại trên thiết bị, nếu cả `min_threshold` và `max_threshold` đều có giá trị và `min_threshold >= max_threshold` → **422**, rollback.
5. So khớp `expected_revision` bên trong transaction trước khi ghi (lệch → **409**). Ghi ngưỡng, tăng `config_revision` (+1) và ghi `audit_log` theo whitelist `{sensor_id, from, to}` trong **cùng một transaction**.
6. Sau khi commit, edge tự nạp lại ngưỡng cho worker polling mà không cần khởi động lại ứng dụng.

| Kết quả | HTTP | Ý nghĩa |
|---------|------|---------|
| Thành công | 2xx + `applied_revision` | Edge đã áp dụng patch |
| Xung đột revision | **409** | `expected_revision` không khớp |
| Sai dữ liệu / read-only | **422** | `detail`/`error` từ thiết bị hoặc adapter ("Adapter is read-only…") |

---

## `GET /readings` — debug thủ công (không live UI)

Endpoint do edge expose; Central **không** dùng làm nguồn realtime cho bảng sensor. **Vai trò v1: công cụ chẩn đoán** khi UI/Modbus có vẻ sai — so sánh JSON gốc từ thiết bị với snapshot Modbus để phân biệt lỗi **code/parser** vs **phần cứng / firmware**.

| Rule | Mô tả |
|------|--------|
| **D1 — Không live table** | **Cấm** merge `/readings` vào `SensorMerger`, `SensorMonitoringTableModel`, hoặc poll theo `poll_interval_s`. |
| **D2 — Nguồn live** | ANALOG / DI / DO trên UI: chỉ Modbus FC03 / FC02 / FC01 — [modbus-map-v1.md](modbus-map-v1.md). |
| **D3 — Catalog** | Metadata từ **`GET /config`**, không từ `/readings`. |
| **D4 — Gọi thủ công** | Chỉ khi user chủ động (vd. nút **“Fetch readings (debug)”** trên Logger Detail hoặc menu dev). **Một request** mỗi lần bấm — **không** scheduler, **không** tự gọi khi mở màn hình. |
| **D5 — Hiển thị kết quả** | Hiện **raw JSON** (dialog scrollable, copy clipboard, hoặc panel debug) — **không** cập nhật ô giá trị trên bảng chính. |
| **D6 — Token** | `api_token` rỗng → nút debug disabled + tooltip. |
| **D7 — So sánh** | Khuyến nghị log cùng timestamp: payload `/readings` + snapshot Modbus vừa poll (để đối chiếu từng `sensor_id`). |

### Khi nào dùng

| Triệu chứng | Hành động debug |
|-------------|-----------------|
| UI Modbus hiển thị sai giá trị | Gọi `/readings` 1 lần → nếu JSON edge **đúng** → lỗi parser/merge C++; nếu JSON **sai** → edge/RTU/hardware |
| DI/DO lệch so với thực tế | So sánh bit FC02/FC01 với mục `sensor_type` DI/DO trong JSON |
| Sau đổi firmware | Xác nhận schema response vẫn khớp contract dưới đây |

### C++ (gợi ý)

- `RestConfigService::fetchReadingsDebug(int loggerId)` → signal `readingsDebugFetched(loggerId, ok, rawJson)`.
- **Không** gọi từ `ModbusBridge` hay timer retention.

### Response schema (tham chiếu edge)

```json
{
  "ok": true,
  "polling": true,
  "rtu_connected": true,
  "sensors": [
    {
      "sensor_id": 1,
      "sensor_type": "ANALOG",
      "value": 25.5,
      "status": "OK",
      "is_alarm": false,
      "alarm_type": "",
      "valid": true,
      "recorded_at": "2026-05-19T10:00:00"
    }
  ]
}
```

Payload **chỉ** phục vụ màn hình/log debug — không ghi `sensor_reading`, không đổi `current_value` trên RAM merge.

---

## `GET /reports/latest`

| Thuộc tính | Giá trị |
|------------|---------|
| Auth | Bearer |
| Response | File TXT; `Content-Disposition: attachment` |
| Poll | **Không** — chỉ khi user bấm tải |

---

## Endpoint summary

| Method | Path | Auth | Central v1 |
|--------|------|------|------------|
| GET | `/config` | Bearer | **Bắt buộc** — form + catalog |
| POST | `/config` | Bearer | **Không dùng** — giữ nguyên contract thiết bị |
| GET | `/readings` | Bearer | **Debug** — 1 shot / lần bấm, raw JSON |
| GET | `/reports/latest` | Bearer | On demand |

---

## Lỗi thường gặp (UX)

| HTTP / body | Message gợi ý |
|-------------|----------------|
| 401 + not configured | Device REST token empty — Scan QR on logger |
| 401 + invalid bearer | Token mismatch — Scan QR again on device |

Không log `api_token` — [provision-qr-v1.md](provision-qr-v1.md).

---

## Central implementation checklist (C++)

- [x] `RestConfigService`: GET-only (`fetchConfig`, `probeConfig`, `fetchReadingsDebug`).
- [x] Add nhập bản sao catalog/revision khi Save; Edit chỉ lưu cài đặt Central.
- [x] Reload config trong Settings cập nhật bản sao bằng GET, không ghi thiết bị.
- [x] `SensorMerger`: Modbus snapshot + catalog DB — **không** nhánh `/readings`.

---

## Reference (legacy Python)

`central-logger-app`: `rest_coordinator.py` poll `/readings` cho DI/DO — **không** port sang C++ v1.
