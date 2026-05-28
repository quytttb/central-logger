# Modbus TCP Map v1 (frozen)

Hợp đồng đọc giữa **Central Logger** và **Data Logger** edge firmware.

| Thuộc tính | Giá trị |
|------------|---------|
| Map version | **1** (`HR0` phải = 1) |
| Addressing | PDU **0-based** (địa chỉ trong bảng dưới) |
| Byte order float (FC03) | **ABCD** — 32-bit IEEE 754, big-endian |

## Function codes (v1 — chuẩn công nghiệp)

| FC | Tên | Vai trò v1 |
|----|-----|------------|
| **03** | Read Holding Registers | Header trạm + snapshot **ANALOG** |
| **02** | Read Discrete Inputs | Trạng thái **DI** (1 bit / điểm) |
| **01** | Read Coils | Trạng thái **DO** (1 bit / điểm) |

**Phạm vi v1:** Toàn bộ live (ANALOG + DI + DO) qua **Modbus** trong **một chu kỳ poll** (`poll_interval_s`). Metadata từ `GET /config`. `GET /readings` chỉ dùng **debug thủ công** (raw JSON) — [rest-config-contract-v1.md](rest-config-contract-v1.md) §`GET /readings`.

**Không** nhét DI/DO vào float holding block — digital dùng đúng FC02/FC01.

Layout map version **1** frozen. Bump layout → `HR0` = 2 + tài liệu map mới.

---

## 1. Header (FC03 — holding)

Luôn đọc **10 register** đầu tiên mỗi chu kỳ poll: start = **0**, quantity = **10**.

| Register | Field |
|----------|--------|
| HR0 | Map version (= **1**) |
| HR1 | Status flags: bit0=polling, bit1=RTU connected, bit2=any alarm |
| HR2–HR3 | Unix timestamp uint32, big-endian (HR2=high, HR3=low) |
| HR4 | **Na** — số sensor **ANALOG** (uint16) |
| HR5 | **Ndi** — số bit **DI** (FC02), quantity tối đa cho một lệnh đọc |
| HR6 | **Ndo** — số bit **DO** (FC01), quantity tối đa cho một lệnh đọc |
| HR7–HR9 | Reserved (0) |

### Status flags (HR1)

| Bit | Meaning |
|-----|---------|
| 0 | Polling active |
| 1 | RTU connected |
| 2 | Any sensor alarm |

Central đọc HR1 trước khi parse analog / digital — đủ cho badge fleet mà chưa cần duyệt hết sensor.

**Validation:**

- `HR0 ≠ 1` → bỏ snapshot, không đọc tiếp.
- `Na = 0` → bỏ qua pha analog FC03 (vẫn có thể đọc FC02/FC01 nếu `Ndi`/`Ndo` > 0).

---

## 2. Analog snapshot (FC03 — holding)

Chỉ sensor **ANALOG** nằm trong holding blocks. Bắt đầu tại **HR10**.

| Vị trí | Nội dung |
|--------|----------|
| HR10 + i×8 | Block analog **i** (i = 0 … **Na−1**) |

### Analog block (8 registers)

| Offset | Field |
|--------|--------|
| +0 | `sensor_id` (uint16) — ID trên edge |
| +1 | Flags: bit0=valid, bit1=alarm, bit2=stale |
| +2..+3 | `value` — float32 **ABCD** |
| +4..+7 | Reserved |

`sensor_type = ANALOG` lấy từ catalog; không gửi trên wire.

### Giới hạn FC03 (125 registers / PDU)

| Ký hiệu | Giá trị |
|---------|---------|
| `H` | 10 (header) |
| `B` | 8 (block analog) |
| `Na` | HR4 |

**Cấm** một PDU FC03 từ HR0 đọc `10 + 8×Na` khi `Na > 14`.

**Multi-PDU bắt buộc (chỉ analog):**

1. FC03: start **0**, quantity **10** (header).
2. FC03: start **10 + 8×k**, quantity **min(8×(Na−k), 120)**; chunk ≤ **15** analog (120 reg); lặp đến hết `Na`.

| Na | Số PDU FC03 (sau header) | Ghi chú |
|----|--------------------------|---------|
| 14 | 1 × **112** reg | `14×8` |
| 15 | 1 × **120** reg | `15×8` |
| 16 | **120** + **8** reg | 2 PDU |
| 30 | **120** + **120** reg | 2 PDU |

DI/DO **không** tính vào giới hạn 125 register — đọc bằng FC02/FC01.

---

## 3. Digital inputs — DI (FC02)

| Thuộc tính | Giá trị |
|------------|---------|
| Function | **02** — Read Discrete Inputs |
| Start address | **0** (PDU 0-based) |
| Quantity | **Ndi** = HR5 (sau khi đọc header) |

### Ánh xạ bit ↔ sensor

| Quy tắc | Mô tả |
|---------|--------|
| **Địa chỉ bit** | `edge_sensor_id` trong catalog (`logger_sensor`) **là** địa chỉ bit FC02 (0 … Ndi−1) |
| **Giá trị** | Bit = **0** → Tắt, bit = **1** → Bật |
| **Catalog** | `sensor_type` = `DI` (từ `GET /config`) |

Firmware **phải** đảm bảo mọi DI active có `edge_sensor_id < Ndi` và bit phản ánh đúng trạng thái vật lý.

**Bỏ qua:** `Ndi = 0` → không gửi FC02 trong chu kỳ đó.

**Giới hạn chuẩn Modbus:** tối đa **2000** discrete / PDU — đủ cho hàng trăm DI; Central đọc **một** PDU `quantity = Ndi` mỗi poll.

---

## 4. Digital outputs — DO (FC01)

| Thuộc tính | Giá trị |
|------------|---------|
| Function | **01** — Read Coils |
| Start address | **0** (PDU 0-based) |
| Quantity | **Ndo** = HR6 |

### Ánh xạ bit ↔ sensor

| Quy tắc | Mô tả |
|---------|--------|
| **Địa chỉ bit** | `edge_sensor_id` = địa chỉ coil FC01 (0 … Ndo−1) |
| **Giá trị** | Coil OFF → Tắt, ON → Bật |
| **Catalog** | `sensor_type` = `DO` |

**Bỏ qua:** `Ndo = 0` → không gửi FC01.

---

## 5. Chu kỳ poll Central (một logger)

Thứ tự **bắt buộc** trong cùng `poll_interval_s`:

```text
1. FC03  header (10 reg)
2. FC03  analog chunks (nếu Na > 0) — multi-PDU theo bảng trên
3. FC02  DI (nếu Ndi > 0) — quantity = Ndi, start = 0
4. FC01  DO (nếu Ndo > 0) — quantity = Ndo, start = 0
5. Merge → SensorMerger → UI + INSERT sensor_reading
```

| Giới hạn | Ghi chú |
|----------|---------|
| `timeout_s` | Tổng thời gian bước 1–4 ≤ timeout |
| Chồng poll | **Cấm** — chu kỳ trước xong mới bắt chu kỳ sau |
| Nhịp UI | ANALOG, DI, DO cùng chu kỳ (~2 s mặc định) |

### Lưu SQLite ([`thiet_ke_db.md`](../thiet_ke_db.md))

| Loại | `sensor_reading.value` |
|------|-------------------------|
| ANALOG | Float đo từ FC03 |
| DI / DO | **0.0** hoặc **1.0** (chuẩn hóa từ bit FC02/FC01) |

---

## 6. Connection defaults

| Parameter | Default |
|-----------|---------|
| TCP port | **5020** |
| Unit ID | **1** |
| Float endian (FC03) | **ABCD** |

---

## 7. Central implementation checklist (C++)

- [ ] `ModbusMapParser`: header; analog blocks; unpack FC02/FC01 bit array.
- [ ] `ModbusService::planPoll(Na, Ndi, Ndo)` → danh sách PDU (FC03 chunks + optional FC02 + FC01).
- [ ] `QModbusTcpClient` (hoặc raw codec) hỗ trợ FC **01, 02, 03**.
- [ ] `SensorMerger`: catalog + analog[] + diBits[] + doBits[] — **không** REST live.
- [ ] Unit test: `Na ∈ {14,15,16}` chunking; DI bit 3 = ON → UI + DB `1.0`.
- [ ] Worker thread; queued signal về main thread.

---

## Reference (legacy Python)

`central-logger-app`: holding-only + REST cho DI/DO — **không** port sang repo C++ v1; tuân map v1 ở trên.
