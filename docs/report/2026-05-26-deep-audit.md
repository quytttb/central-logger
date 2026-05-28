# Báo cáo Deep Audit — Central Logger C++ (Qt 6.11)

> **Source of truth** cho audit/bug backlog (2026-05-26). Các report cũ (`independent-audit`, `code-review`, `walkthrough`, prompt fix P0–P2) đã gỡ — chỉ giữ file này trong `docs/report/`.

## 1. Metadata
- **Ngày thực hiện:** 2026-05-26
- **Thư mục Build:** `build/Desktop-Debug`
- **Kết quả CTest:** 13/13 tests PASSED (100% passed, 0 failed)
- **Git Hash:** no git repo
- **Grep Spot-checks:**
  - `createSeries` trong `src/`: Pass (`src/app/qml/views/LoggerDetailView.qml:603` chỉ là comment).
  - `pollInFlight = false` khi reconnect: Pass (`src/network/modbus/ModbusService.cpp:80`, 388).
  - `active = 0` thay vì DELETE: Pass (`src/data/repositories/SensorCatalogRepository.cpp:180`).
  - `PRAGMA user_version` sau commit: Pass (`src/data/db/Database.cpp:192`).
  - `/readings` không auto/timer: Pass (`src/network/rest/RestConfigService.h:22`).
  - Không `QSystemTrayIcon`/`FramelessWindowHint`: Pass (không có kết quả grep).

## 2. Executive Summary
- **Mục tiêu:** Kiểm toán sâu mã nguồn (không sửa), đối chiếu spec, rà soát backlog từ Code Review.
- **Fixed verified (P0–P2):** Đã vá thành công C-1, C-2, C-3, C-4, C-5, C-6, C-7, C-8.
- **Fixed verified (M/L — Batch 1):** M-4, M-6, M-10, M-11, M-12, M-13, M-15, M-16, M-19, M-20, M-21, L-14, L-20, L-21, L-22, L-23.
- **Fixed verified (M — Batch 2):** M-1, M-2, M-3, M-5, M-7, M-8, M-9, M-14.
- **Fixed verified (Deep):** D-1 (sensor reactivate — `ensureExists` ON CONFLICT DO UPDATE SET active=1; test `sensorPruneAndRecover`).
- **Fixed (M — Batch 3):** M-17, M-18, M-22, M-23.
- **Fixed (L — Batch 3):** L-15, L-16, L-17, L-18, L-19.
- **Fixed (L — Batch 4):** L-6, L-10, L-11, L-12, L-13.
- **Won't fix (L — Batch 4):** L-7 (HTTPS schema — cần quyết định product; MVP giữ `http://` theo contract edge LAN).
- **Confirmed open (Critical/Medium):** **0** mục.
- **Confirmed open (Low):** **7** mục — L-1 … L-5, L-8, L-9 (L-6 … L-13 đã fixed hoặc Won't fix; L-14 … L-23 đã fixed trước đó).
- **Enhancement / Backlog:** D-2 (purge `system_event`) — spec hiện tại (`thiet_ke_db.md` §Retention) **chỉ** yêu cầu retention `sensor_reading`; không bắt xóa `system_event`.
- **False positive / Won't fix:** Không có.
- **Scope exclusion:** FE-017 (Tray/Frameless) và FE-014 (REST live) đã chính thức bị loại khỏi MVP theo `HANDOFF.md`.

## 3. Bảng Verification C-1…C-8 (Critical)

| ID | Walkthrough claim | Verified? | File:dòng | Test | Ghi chú |
|---|---|---|---|---|---|
| C-1 | Xóa `createSeries` Qt Charts | ✅ Pass | `LoggerDetailView.qml:617` | Gap | Dùng `Qt.createQmlObject` |
| C-2 | Fix kẹt ConnectingState | ✅ Pass | `ModbusService.cpp:220` | Gap | `QTimer::singleShot` timeoutMs*2 |
| C-3 | Xóa logger mất event | ✅ Pass | `001_initial.sql:67` | `test_database_repositories` | Đã đổi `ON DELETE SET NULL` |
| C-4 | TOCTOU trong `ensureExists` | ✅ Pass | `SensorCatalogRepository.cpp:48` | `test_database_repositories` | `INSERT OR IGNORE` → nâng lên `ON CONFLICT DO UPDATE` (D-1) |
| C-5 | `PRAGMA user_version` | ✅ Pass | `Database.cpp:192` | `test_database_repositories` | `PRAGMA` gọi sau COMMIT |
| C-6 | LineSeries thiếu axis | ✅ Pass | `DashboardView.qml:155` | Gap | Axes được bind vào LineSeries |
| C-7 | Reset `pollInFlight` | ✅ Pass | `ModbusService.cpp:80` | Gap | Đã reset khi ngắt client |
| C-8 | Đổi DELETE thành `active=0` | ✅ Pass | `SensorCatalogRepository.cpp:180` | `test_database_repositories` | Giữ nguyên lịch sử đo |

## 4. Bảng Medium M-1…M-23

| ID | Status | Evidence |
|---|---|---|
| M-1 | ✅ Fixed | `EventRepository::insert` nay đọc lại `created_at` từ DB sau INSERT và điền vào `event.createdAt` (`EventRepository.cpp`). |
| M-2 | ✅ Fixed | `ensureExists` thêm `qWarning` khi `errorOut == nullptr`; lỗi SQL không còn bị nuốt im lặng (`SensorCatalogRepository.cpp`). |
| M-3 | ✅ Fixed | `findAllWithSensorCounts` nay dùng `l.` prefix cho tất cả cột (`LoggerRepository.cpp`); column order giữ nguyên với positional enum. |
| M-4 | ✅ Fixed verified | `applySnapshot` đã được bọc bằng transaction ở `ModbusBridge.cpp`. |
| M-5 | ✅ Fixed | `upsert()` DO UPDATE đã bỏ dòng `sensor_type = excluded.sensor_type` (no-op, vì `sensor_type` là conflict key) (`SensorCatalogRepository.cpp`). |
| M-6 | ✅ Fixed verified | `setTransferTimeout` được gọi rõ ràng trong `RestConfigService.cpp`. |
| M-7 | ✅ Fixed | Đã bỏ `Content-Type` header khỏi GET `/config` (`RestConfigService.cpp`). |
| M-8 | ✅ Fixed | `downloadLatestReport` kiểm tra `Content-Length` + `data.size()` so với giới hạn 50 MB trước `readAll()` (`RestConfigService.cpp`). |
| M-9 | ✅ Fixed | `LoggerState` lưu `connectHolder`; `destroyState` disconnect và giải phóng holder trước `deleteLater`; gọi `client->disconnect(this)` để ngăn signal rò sau khi state bị xóa (`ModbusService.h/.cpp`). |
| M-10 | ✅ Fixed | Tách `Endpoint::Apply` riêng (`kApplyBit`); fetch và apply có guard bit độc lập; emit error signal khi busy thay vì silent drop (`RestConfigService.cpp/h`). |
| M-11 | ✅ Fixed verified | Đã thêm `Qt::DisplayRole` trong emit dataChanged. |
| M-12 | ✅ Fixed verified | Guard `if (!m_busy)` ngừa đụng độ sự kiện `configApplied`. |
| M-13 | ✅ Fixed | `LoggerDetailViewModel` giờ sở hữu `m_ownSensorTable` riêng (value member); không dùng chung với `DashboardController::m_sensorTable`. Test `exposesSensorTable` cập nhật xác nhận hai instance khác nhau. |
| M-14 | ✅ Fixed | Khi alarm flag bật nhưng không có threshold xác định hướng, `computeDisplayStatus` trả `"ALARM"` thay vì `"ALARM_MIN"`. QML cập nhật 3 switch. Test `alarmFlagWithoutThresholdYieldsGenericAlarm` thêm vào `test_sensor_merger` (`SensorMerger.cpp`, `LoggerDetailView.qml`). |
| M-15 | ✅ Fixed verified | QML host input đã dùng `HostValidator` C++ kiểm tra IP/Hostname. |
| M-16 | ✅ Fixed verified | Lỗi ghi đè `0` từ SpinBox đã sửa bằng nullish coalescing. |
| M-17 | ✅ Fixed | Background hover dùng `Material.theme` để chọn `rgba(1,1,1,0.08)` (Dark) / `rgba(0,0,0,0.07)` (Light); base `"transparent"` (`DashboardView.qml`). |
| M-18 | ✅ Fixed | `width: parent ? Math.max(320, Math.min(parent.width - 80, 580)) : 560` — không thể âm; tối thiểu 320 px (`LoggerFormDialog.qml`). |
| M-19 | ✅ Fixed verified | Hủy kích hoạt logger đã gọi `updateAlarmState(id, false)`. |
| M-20 | ✅ Fixed verified | Nút Save ép `forceActiveFocus()` trước khi đóng dialog. |
| M-21 | ✅ Fixed verified | Đường dẫn lưu báo cáo dùng `QUrl::toLocalFile()` tránh `%20`. |
| M-22 | ✅ Fixed | Thêm `clip: true` trên sidebar/rail (lúc đó `AppSidebar.qml`; nay `AppNavigationRail.qml` fixed 80px). |
| M-23 | ✅ Fixed | Section headers opacity animate (đã bỏ section headers khi chuyển rail 80px). |

## 5. Bảng Low L-1…L-23

| ID | Status | Evidence |
|---|---|---|
| L-1 | Open | Parse chuỗi DateTime thành UTC rải rác không đồng nhất. |
| L-2 | Open | Xóa lịch sử dựa trên việc cắt chuỗi string ISO dễ lỗi. |
| L-3 | Open | `SensorCatalogRepository` không có hàm `findById` để tiện dùng. |
| L-4 | Open | Repository lấy `lastInsertId` không kiểm tra giá trị null/hợp lệ. |
| L-5 | Open | Các lệnh SQL dùng `SELECT *` thay vì kê khai cột tường minh. |
| L-6 | ✅ Fixed | Thêm `Database::memoryPath()`; `Database::open` dùng hằng thay vì lặp literal `:memory:` (`Database.h`, `Database.cpp`). |
| L-7 | Won't fix | REST base URL ép `http://` — đúng MVP (edge LAN, contract v1); HTTPS/TLS cần ADR + product decision trước khi đổi (`RestConfigService.cpp`). |
| L-8 | Open | Modbus bắt `isFinished` quá ngặt nghèo, đôi khi hiểu nhầm timeout. |
| L-9 | Open | Bộ chia block register Modbus (Chunk) không ép độ dài block chẵn 8. |
| L-10 | ✅ Fixed | `LoggerRuntimeConfig` có `operator==` / `operator!=` (`ModbusService.h`). |
| L-11 | ✅ Fixed | So sánh `deque.size()` với `static_cast<std::size_t>(kMaxPoints)` (`PollHistoryStore.cpp`). |
| L-12 | ✅ Fixed | `points.reserve(static_cast<QList<QVariant>::size_type>(deque.size()))`; `pointCount` trả `static_cast<int>(sensorIt->size())` (`PollHistoryStore.cpp`). |
| L-13 | ✅ Fixed | `SettingsController::load()` gọi `setError({})` đầu hàm và sau load thành công — không còn `lastError` cũ (`SettingsController.cpp`). |
| L-14 | ✅ Fixed | `afterMutation()` nay gọi `refreshIngestionChart()` sau mỗi CRUD logger (`DashboardController.cpp`). |
| L-15 | ✅ Fixed | Đổi `id => root.selectLogger(id)` thành `loggerId => root.selectLogger(loggerId)` trong cả `DashboardView` và `LoggersView` (`Main.qml`). |
| L-16 | ✅ Fixed | Header `HorizontalHeaderView` thêm `elide: Text.ElideRight`; cột Host đổi sang flex (cùng flex pool với Name); tất cả data cell đã có `elide` (`LoggersView.qml`). |
| L-17 | ✅ Fixed | `elide: Text.ElideRight` áp dụng cho mọi cột text (không chỉ col 1) trong sensor table delegate (`LoggerDetailView.qml`). |
| L-18 | ✅ Fixed | `debugBtn` và `reportBtn` bọc trong `Item` + `HoverHandler`; `ToolTip` gắn vào Item wrapper — tooltip hiển thị kể cả khi button disabled (`LoggerDetailView.qml`). |
| L-19 | ✅ Fixed | `RowLayout` trong `PageHeader` dùng `anchors { left/right/verticalCenter }` thay vì `anchors.fill: parent` — loại bỏ vòng tham chiếu `implicitHeight ↔ anchors.fill` (`PageHeader.qml`). |
| L-20 | ✅ Fixed verified | Timezone Chart truy vấn UTC từ SQLite và dùng `system_timezone`. |
| L-21 | ✅ Fixed verified | Store History ánh xạ đúng tên thay vì `Sensor #`. |
| L-22 | ✅ Fixed verified | Trạng thái Offline đầu tiên đã được mồi từ DB khi khởi động. |
| L-23 | ✅ Fixed | Vòng upsert catalog từ GET /config được bọc trong `conn.transaction()/commit()` (`LoggerDetailViewModel.cpp`). |

## 6. Findings mới (DEEP / D-*)
- **D-1 (SensorCatalogRepository.cpp): Lỗi logic không tự động khôi phục Sensor (active=0)**
  - **Trạng thái:** ✅ **Fixed**. Đổi `INSERT OR IGNORE` thành `INSERT ... ON CONFLICT(logger_id, sensor_type, edge_sensor_id) DO UPDATE SET active = 1`. Sensor bị prune (active=0) được tự động reactivate khi Modbus poll/reading quay lại report nó. Test `sensorPruneAndRecover` đã thêm vào `test_database_repositories`.
- **D-2 (`DashboardController::purgeOldData`): `system_event` không được retention**
  - **Trạng thái:** 💡 **Enhancement / Product Backlog**. `purgeOldData()` chỉ gọi `SensorReadingRepository::purgeOlderThan` — đúng spec retention hiện tại. Nếu cần giới hạn kích thước bảng event, thêm setting + ADR (không phải bug MVP).

## 7. Contract Compliance Matrix

| Tiêu chí | Trạng thái | Ghi chú / Pointer |
|---|---|---|
| **MVVM Architecture** | Pass | Logic giữ ở C++, Data Model bind thuần qua Properties/Signals (`src/core/`). |
| **Modbus Mapping** | Pass | Poll đúng thứ tự FC03 → FC02 → FC01, phân lô ≤ 125 (`ModbusPollPlan.h`). |
| **REST Config** | Pass | `/readings` cô lập đúng; M-7/M-8/M-10 đã vá; transfer timeout M-6 OK. |
| **Database Spec** | Pass | Schema/migration/FK/prune `active=0` khớp `thiet_ke_db.md`. D-1 vá xong. D-2 là enhancement ngoài spec. |

## 8. E2E Flows

| Flow | Status | Ghi chú (Gap) |
|---|---|---|
| **Add logger → probe → persist → Modbus → poll → catalog → live table** | ✅ OK | M-10, M-13, D-1 đã vá — flow thông suốt. |
| **Update host/port → reconnect → poll tiếp (C-7)** | OK | Đã vá thành công (reset `pollInFlight`). |
| **Fetch/apply config → configApplied (M-12) → DB revision** | OK | Khắc phục xong đụng độ double-subscribe. |
| **Disable logger → alarmCount, Modbus stop** | OK | M-19 đã vá, báo thức giả không còn. |
| **Delete logger → system_event FK (C-3)** | OK | CASCADE đã đổi thành SET NULL. |
| **Chart ingestion timezone (L-20), trending (C-1, C-6)** | OK | C-1 (createSeries API), C-6 (Axes), L-20 (Timezone) đã vá sạch sành sanh. |
| **Download report → FileDialog QUrl (M-21)** | OK | `toLocalFile()` dịch đúng định dạng Windows/Linux. |
| **CRUD logger → ingestion chart refresh (L-14)** | OK | `afterMutation()` gọi `refreshIngestionChart()` sau add/update/remove/configApplied. |
| **ModbusService destroy while connecting (M-9)** | ✅ OK | `connectHolder` được lưu trong `LoggerState`; `destroyState` dọn dẹp trước khi xóa state. |
| **Alarm flag set without threshold (M-14)** | ✅ OK | `ALARM` thay vì `ALARM_MIN`; QML hiển thị đúng màu đỏ cho cả 3 alarm variant. |

## 9. Test Coverage Gaps
Danh sách Unit Test còn thiếu (D-1 và M-14 đã được bổ sung):
1. ~~`test_sensor_prune_and_recover`~~ — **Done** (thêm trong `test_database_repositories`).
2. ~~`alarmFlagWithoutThresholdYieldsGenericAlarm`~~ — **Done** (thêm trong `test_sensor_merger`).
3. `test_system_event_retention`: Đảm bảo trigger việc cắt tỉa log (D-2) sau khi đưa tính năng này vào cài đặt.
4. `test_modbus_timeout_recovery`: Giả lập server kẹt `ConnectingState` (C-2) và kiểm chứng callback kết nối bị hủy đúng 2 * timeout.
5. `test_rest_guard_concurrency`: Bắn dồn dập nhiều tín hiệu Fetch/Apply để kiểm tra guard bit (M-10) độc lập.
6. ~~`test_qml_sidebar_animations`~~ — **N/A** (`clip` và `opacity Behavior` cho M-22/M-23 đã được kiểm tra bằng code review; không cần test riêng QML).

## 10. Doc Hygiene

- **Đã gỡ** (nội dung gộp vào báo cáo này): `2026-05-26-independent-audit.md`, `2026-05-26-code-review.md`, `2026-05-26-code-review-walkthrough.md`, `2026-05-26-fix-roadmap-prompts.md`, `2026-05-26-deep-audit-agent-prompt.md`.
- **Giữ:** `docs/report/2026-05-26-deep-audit.md` + `docs/report/README.md`.

## 11. So sánh audit đầu phiên (đã gỡ file gốc)

| Claim audit sơ (2026-05-26) | Thực tế sau deep audit |
|---|---|
| Poll Modbus / MVVM / REST debug "ổn", sẵn hardware | P0–P2 (C-1…C-8) **verified**; Batch 1+2+3 (D-1, M-1…M-3, M-5…M-14, M-17, M-18, M-22, M-23, L-15…L-19) đã vá; còn **13 Low** open |
| Không liệt kê backlog M/L | Bảng §4–§5 là SoT backlog |
| "Ready for real-world testing" (quá lạc quan) | **MVP hardware test OK** với known issues; Batch 1+2+3 đã vá |

## 12. Kết luận

Critical **C-1…C-8** đã vá và verified; **ctest 13/13**. Batch 1–3 (D-1, M-*, L-14…L-19) đã fix. **Batch 4 (Low — 2026-05-26):** L-6, L-10, L-11, L-12, L-13 fixed; L-7 Won't fix (HTTPS). Backlog Low còn **7** (L-1…L-5, L-8, L-9) — code quality, không chặn hardware test. **Sẵn test hardware.**
