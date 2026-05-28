# Bugs Survey — Central Logger C++ (Independent Audit)

**Ngày khảo sát:** 2026-06 (sau deep-audit 2026-05-26)  
**Mục đích:** Tìm bugs liên quan **logic, data storage, UI, reports** theo yêu cầu user. Không fix. Chỉ báo cáo + evidence.  
**Nguồn tham chiếu chính (SoT):**  
- `docs/thiet_ke_db.md` (schema + RAM rules)  
- `docs/contracts/` (Modbus v1, REST v1)  
- `HANDOFF.md` + `AGENTS.md` (MVVM, không logic QML, SQLite qua QSqlDatabase)  
- Previous deep-audit (2026-05-26) chỉ dùng làm tham chiếu lịch sử, **không** thay thế khảo sát độc lập.

---

## 1. Tóm tắt Executive

- **Tổng quan:** Codebase clean, tuân thủ tốt contracts + schema. MVVM rất tốt (không logic nghiệp vụ trong QML). Các fix quan trọng từ previous audit (C-1..C-8, M-*, D-1) **vẫn còn hiệu lực** trong code hiện tại.
- **Bugs nghiêm trọng (Critical/Medium mới):** **0** phát hiện trong quá trình khảo sát độc lập.
- **Vấn đề cấu trúc còn tồn tại (chủ yếu Low, từ previous audit):** 6–7 mục (xác nhận L-1, L-2, L-3, L-4, L-5 và biến thể). Đây là **code quality / maintainability**, không chặn hardware test hay MVP.
- **Điểm mạnh nổi bật:**
  - Transaction đúng chỗ (ModbusBridge applySnapshot, LoggerDetailViewModel catalog upsert).
  - REST `/readings` chỉ debug one-shot (không poll, không merge live).
  - pruneOrphanSensors **đã được gọi** (ModbusBridge), không phải dead code.
  - SensorMerger thuần (không DB, không REST live).
  - Guard in-flight cho REST (fetch/apply độc lập).
- **Khuyến nghị:** Xử lý nhóm "duplicated datetime + string-based retention cutoff" trước khi scale hoặc thêm migration.

---

## 2. Data Storage Bugs (SQLite + Repositories)

### 2.1 Vấn đề còn tồn tại (xác nhận từ previous + khảo sát mới)

| ID | Mô tả | Evidence (file:line) | Spec liên quan | Severity | Ghi chú |
|----|-------|----------------------|----------------|----------|---------|
| DS-1 | Duplicated datetime helpers (`parseUtc`, `isoUtc`, `isoUtcOrNull`) rải rác 4+ repositories. Xử lý timezone không thống nhất (một số force UTC khi timeSpec==Local, một số không). Dễ sai lệch timestamp khi chạy trên locale khác. | `LoggerRepository.cpp:40-63`, `EventRepository.cpp:12-25`, `SensorReadingRepository.cpp:11-14`, `Database.cpp` (gián tiếp) | thiet_ke_db §4 (recorded_at, last_seen là UTC ISO) | Low | L-1 style. Khuyến nghị: một `DateTimeUtils` namespace. |
| DS-2 | Retention purge dùng **lexical string compare** trên cột TEXT ISO: `DELETE ... WHERE recorded_at < :cutoff`. Dễ sai khi string dài ngắn khác nhau hoặc thiếu 'Z'. | `SensorReadingRepository.cpp:75`: `recorded_at < :cutoff` | thiet_ke_db §Retention + §5 (chỉ xóa sensor_reading theo data_retention_days) | Low | L-2. Nên dùng julianday() hoặc lưu unix seconds cho range query. |
| DS-3 | `lastInsertId()` được dùng mà không kiểm tra `isValid()` / kiểu dữ liệu. | `LoggerRepository.cpp:132`, `EventRepository.cpp:67`, `SensorCatalogRepository.cpp:121-127` (upsert path) | — | Low | L-4. Ít gây bug ngay nhưng fragile khi DB lỗi. |
| DS-4 | `SELECT *` thay vì explicit columns trong một số repo. | `EventRepository.cpp:87`, `SensorCatalogRepository.cpp:139,160` | — | Low | L-5. LoggerRepository đã explicit (tốt). |
| DS-5 | Không có `findById(qint64)` trong `SensorCatalogRepository`. Callers phải dùng `findByLoggerAndEdgeId`. | `SensorCatalogRepository.h:28-31` (chỉ có findByLoggerAndEdgeId) | — | Low | L-3. Tiện ích thiếu. |
| DS-6 | `findAllWithSensorCounts` dùng **magic positional index** (`ColCreatedAt + 1`) cho subquery COUNT. Rất dễ vỡ khi thêm cột. | `LoggerRepository.cpp:214`: `q.value(ColCreatedAt + 1)` | — | Low | Biến thể của L-5. Nên dùng alias hoặc QSqlRecord. |

### 2.2 Những gì đã tốt (không còn bug)

- Transaction đầy đủ trong `applySnapshot` (ModbusBridge:46-51) và catalog upsert (LoggerDetailViewModel:287).
- `ensureExists` dùng `ON CONFLICT ... DO UPDATE SET active=1` (D-1 fix còn hiệu lực).
- `pruneOrphanSensors` set `active=0` (không DELETE) — C-8 còn tốt.
- FK ON DELETE CASCADE/SET NULL đúng schema.
- `app_settings` luôn id=1 (seed + CHECK constraint).

**Kết luận Data Storage:** Không có data corruption risk mới. Vấn đề chủ yếu là **maintainability + correctness của range query retention**.

---

## 3. Logic Bugs (Modbus, Merge, Events, REST, Charts)

### 3.1 Không phát hiện bug Critical/Medium mới

- **Modbus contract v1:** Thứ tự FC03 header → analog chunks (≤120 reg) → FC02 → FC01 được tuân thủ (`ModbusPollPlan.h`, `ModbusService.cpp`). `HR0 != 1` bị drop đúng. ABCD float đúng.
- **SensorMerger:** Thuần (không DB, không REST). DI/DO chỉ từ snapshot bits + catalog. `computeDisplayStatus` xử lý alarm bit khi không có threshold → "ALARM" (M-14 fix còn tốt).
- **Event edge-trigger (Task 19):** `maybeLogStatusTransition` + seed từ DB khi reload (`DashboardController.cpp:286-328`, L-22 fix) hoạt động đúng. Không spam.
- **REST guards:** Fetch/Apply/Readings/Probe có bit độc lập (M-10). `/readings` chỉ qua button explicit (contract D1–D7 respected).
- **Probe + Apply on Save (Task 22/23):** RAM probe → Save upsert catalog + last_revision + optional applyConfig. Transaction cho catalog. Không race lớn.
- **Report download:** Size guard 50MB + `toLocalFile()` (M-8, M-21 còn tốt).
- **Retention purge:** Chỉ gọi `SensorReadingRepository::purgeOlderThan` (đúng spec, không đụng system_event/logger_sensor).

### 3.2 Vấn đề nhỏ / Efficiency (không phải correctness bug)

- Mỗi poll thành công gọi `ensureExists` + re-query id cho **mọi** analog + DI + DO (chatty DB). Với Na=30 @ 2s → ~60+ queries/poll. Hoạt động ổn nhưng có thể tối ưu (RETURNING hoặc cache).
- `PollHistoryStore` ring buffer 24 points/analog — đúng spec FE-010.
- `ChartQueryService` bucket SQL phức tạp nhưng đã fix timezone (L-20) và dùng unix seconds + tz convert.

**Kết luận Logic:** Không có logic bug mới gây sai dữ liệu hoặc vi phạm contract. Code khá chắc.

---

## 4. UI / QML / MVVM Bugs

### 4.1 Rất sạch — gần như không có violation

- **Không business logic trong QML:** Toàn bộ CRUD, Modbus state, REST, merge, chart query, event logging đều ở C++ ViewModel/Controller. QML chỉ bind property + gọi `Q_INVOKABLE` đơn giản.
- Các hàm `function` trong QML chỉ là:
  - Presentation helpers (levelAccent, badgeColor, formatTimestamp).
  - Imperative QtGraphs series rebuild (bắt buộc vì QtGraphs API hiện tại chưa declarative tốt cho dynamic LineSeries).
- **Search:** `LoggerSearchProxyModel` đúng (substring case-insensitive trên stationCode/name/host).
- **Responsive:** GridLayout 2 cột >950px, stack dọc — OK (FE-023).
- **Form draft buffer:** SettingsView + LoggerFormDialog dùng local draft + dirty flag. Cancel = re-sync từ controller. Chuẩn và an toàn.
- **Navigation rail 80px cố định:** Không hamburger/frameless (đúng quyết định dropped FE-017).

### 4.2 Vấn đề nhỏ (không bug)

- Một số `Connections` + `on*Changed` lặp lại (ví dụ LoggerDetailView + DashboardController). Dễ maintenance nhưng không gây leak (disconnect khi destroy).
- QtGraphs series management thủ công (remove + create lại) — chấp nhận được cho MVP.

**Kết luận UI:** Tuân thủ MVVM xuất sắc. Không cần refactor lớn.

---

## 5. Reports & Charts Bugs

- **Ingestion chart (24h):** SQL + bucket + timezone conversion đã ổn (L-20 fix verified).
- **Trending chart (detail view):** `PollHistoryStore` (RAM ring) + `updateSensorNames` (dùng catalog name thay "Sensor #N") — L-21 fix còn tốt.
- **Download report:** `GET /reports/latest` → binary write, guard 50MB, `toLocalFile()` — không còn %20 bug.
- **Không có** merge `/readings` vào live table / chart (contract D1–D7).

Không phát hiện bug mới trong luồng báo cáo.

---

## 6. So sánh với Previous Deep Audit (2026-05-26)

| Loại | Previous claim | Khảo sát hiện tại | Kết luận |
|------|----------------|-------------------|----------|
| Critical (C-1..C-8) | Đã fix + verified | Vẫn đúng (transaction, prune active=0, REST guard, no /readings live, no tray) | Good |
| Medium (M-*) | Đa số fixed | Các fix còn hiệu lực (guard bit, own sensor table, alarm generic, toLocalFile, etc.) | Good |
| Low open (L-1..L-5, L-8, L-9) | 7 mục open | DS-1..DS-6 xác nhận **vẫn tồn tại** (dupe datetime, string cutoff, SELECT *, lastInsertId, positional, thiếu findById). L-8/L-9 (Modbus isFinished / chunk) không thấy bug mới trong code hiện tại. | Vấn đề cũ chưa được xử lý |
| D-1 (reactivate sensor) | Fixed + test | Vẫn đúng (`ON CONFLICT ... active=1`) | Good |
| D-2 (purge system_event) | Enhancement (không phải bug) | Vẫn đúng — spec chỉ yêu cầu purge sensor_reading | Không phải bug |

**Kết luận:** Không có regression lớn. Các Low issue cũ vẫn là backlog code-quality.

---

## 7. Các Vùng Khác (không phải bug nhưng đáng lưu ý)

- **Test coverage:** 13/13 tests pass. Có test cho prune + reactivate, retention, Modbus chunking 14/15/16/30, REST error mapping. Thiếu test cho edge Na=0, probe concurrent, large report.
- **Performance:** Chatty DB trên poll path (ensureExists × sensor). Không ảnh hưởng MVP nhưng cần quan tâm khi Na lớn.
- **Error UX:** Nhiều chỗ chỉ `qWarning` + return false. User cuối cùng thấy generic message. Có thể cải thiện sau.
- **DateTime:** Toàn bộ dùng TEXT ISO8601. Không có cột unix seconds. Range query (retention, chart 24h) đều phải parse string → dễ lỗi (DS-2).

---

## 8. Khuyến nghị (không phải fix ngay)

1. **Ưu tiên cao (trước scale):** Tạo `DateTimeUtils` + sửa retention cutoff (dùng julianday hoặc unix seconds).
2. **Medium:** Loại SELECT *, thêm `findById` cho SensorCatalogRepository, thay magic positional bằng alias.
3. **Nice-to-have:** Cache sensor id trong ModbusBridge/SensorSnapshotCache để giảm query ensureExists.
4. **Docs:** Bổ sung ADR nhỏ cho "Retention cutoff strategy" nếu đổi sang numeric.

---

## 9. Kết luận cho User

Codebase **sẵn sàng cho hardware test** (Modbus + REST + live table + chart + report download + CRUD). Không có bug logic/data/UI/reports nào chặn MVP hoặc vi phạm frozen contracts.

Các vấn đề còn lại chủ yếu là **technical debt / maintainability** (duplicated code, fragile string operations) — không gây sai dữ liệu hay crash.

**File này là output chính** để bạn review. Tôi **không** sửa gì. Khi bạn muốn, hãy cho tôi biết ưu tiên fix cái nào (hoặc "fix all Low" / "chỉ DS-1 + DS-2").

Chờ bạn kiểm tra.
