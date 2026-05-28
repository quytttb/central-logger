# REST config contract v1 (Central ↔ data-logger)

Base URL: `http://<host>:<api_port>/api/v1`

| Thuộc tính | Giá trị |
|------------|---------|
| API version | **v1** |
| Auth | `Authorization: Bearer <api_token>` |
| Content-Type | `application/json` (config); binary cho report |
| Optimistic locking | `expected_revision` trên `POST /config` |

**Phạm vi v1 (repo C++ mới):**

| REST | Vai trò |
|------|---------|
| `GET/POST /config` | Cấu hình trạm, **catalog sensor** (`sensor_type`, tên, ngưỡng), revision |
| `GET /reports/latest` | Tải báo cáo (on demand) |
| `GET /readings` | **Debug thủ công** — xem JSON gốc từ edge (1 lần / lần bấm); **không** feed bảng sensor live |

**Live values (ANALOG + DI + DO):** chỉ [modbus-map-v1.md](modbus-map-v1.md) — FC03 (analog) + FC02 (DI) + FC01 (DO), cùng nhịp `poll_interval_s`. Xem [`thiet_ke_db.md`](../thiet_ke_db.md).

---

## Central DB (`logger_info`)

| Column | Role |
|--------|------|
| `name`, `host`, `port`, `unit_id`, `poll_interval_s`, `timeout_s` | Modbus TCP client |
| `api_port`, `api_token`, `last_revision` | REST client |
| `enabled`, `note` | Operations |

`port` (Modbus) chỉ sửa ở cột **Central** trong Add/Edit.

---

## `GET/POST /config`

### `GET /config`

| REST key | Device form (Edit, online) |
|----------|----------------------------|
| `station_code` | Station code |
| `station_name` | Station name |
| `poll_interval` | Device poll interval (**seconds**) |
| `modbus_tcp_bind` | Modbus TCP bind |
| `modbus_tcp_enabled` | Modbus TCP server enabled |
| `modbus_tcp_unit_id` | Modbus TCP unit ID (edge) |

Response **phải** đủ để Central cập nhật `logger_sensor`: `sensor_id`, `sensor_type` (`ANALOG`|`DI`|`DO`), `name`, `unit`, ngưỡng (nếu edge trả về trong payload config/catalog).

**Không có trong snapshot:** `rest_api_token` — [provision QR](provision-qr-v1.md).

**Không gửi khi POST:** `logger_serial`, `cloud_enabled`, `cloud_endpoint` — edge trả **422** (`extra=forbid`).

### `POST /config` — optimistic locking

```json
{
  "expected_revision": <int>,
  "config": { ... }
}
```

| Kết quả | HTTP | Central |
|---------|------|---------|
| Thành công | 2xx + `applied_revision` | Cập nhật `last_revision` |
| Xung đột | **409** / **422** | UX: "Connect again, then save" |

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
| POST | `/config` | Bearer | **Bắt buộc** — apply patch |
| GET | `/readings` | Bearer | **Debug** — 1 shot / lần bấm, raw JSON |
| GET | `/reports/latest` | Bearer | On demand |

---

## Lỗi thường gặp (UX)

| HTTP / body | Message gợi ý |
|-------------|----------------|
| 401 + not configured | Device REST token empty — Scan QR on logger |
| 401 + invalid bearer | Token mismatch — Scan QR again on device |
| 409 / revision | Configuration changed on device. Connect again, then save. |

Không log `api_token` — [provision-qr-v1.md](provision-qr-v1.md).

---

## Central implementation checklist (C++)

- [x] `RestConfigService`: `fetchConfig`, `applyConfig`, `probe`, `fetchReadingsDebug` (manual only).
- [x] Config GET/POST + catalog upsert: **Add/Edit form only** (`saveLoggerFromForm`); Detail không fetch/apply config.
- [x] Sau load config (Connect / Edit auto-fetch) → upsert `logger_sensor` on **Save**.
- [ ] `SensorMerger`: Modbus snapshot + catalog DB — **không** nhánh `/readings`.
- [ ] UI debug: nút → hiện JSON; không side-effect lên table model.
- [ ] Unit test: `fetchReadingsDebug` không được gọi từ poll loop / `ModbusBridge`.

---

## Reference (legacy Python)

`central-logger-app`: `rest_coordinator.py` poll `/readings` cho DI/DO — **không** port sang C++ v1.
