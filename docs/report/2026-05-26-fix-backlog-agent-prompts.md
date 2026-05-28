# Gửi agent fix backlog — Central Logger C++

**SoT bug:** [`2026-05-26-deep-audit.md`](2026-05-26-deep-audit.md) (§4–§6).

Mỗi lần gửi agent: copy **một** prompt bên dưới + đính kèm:

- `@AGENTS.md`
- `@HANDOFF.md`
- `@docs/thiet_ke_db.md`
- `@docs/contracts/modbus-map-v1.md`
- `@docs/contracts/rest-config-contract-v1.md`
- `@docs/report/2026-05-26-deep-audit.md`

**Không làm:** FE-017 (tray/frameless), FE-014 (REST live merge), D-2 (trừ khi user yêu cầu + ADR).

```bash
cmake --build build/Desktop-Debug --target central_logger -j
cd build/Desktop-Debug && ctest --output-on-failure
```

**Cuối mỗi task:** cập nhật `2026-05-26-deep-audit.md` — đổi dòng bug đã fix thành `✅ Fixed verified` + evidence ngắn; cập nhật §2 Executive (đếm open).

---

## Thứ tự đề xuất

| Bước | Prompt | Khi nào |
|------|--------|---------|
| **1** | Batch 1 — Data & vận hành | Ưu tiên trước hardware test |
| **2** | Batch 2 — REST / Modbus / VM | Sau Batch 1 |
| **3** | Batch 3 — QML / UX | Sau Batch 2 hoặc song song |
| **4** | Batch 4 — Low (tùy chọn) | Polish / maintainability |

---

## Batch 1 — Data & vận hành (D-1, M-10, M-13, L-14, L-23)

```
Sửa backlog Batch 1 — Central Logger C++ (Qt 6.11).

Đọc: AGENTS.md, docs/thiet_ke_db.md, docs/report/2026-05-26-deep-audit.md
(§6 D-1; §4 M-10, M-13; §5 L-14, L-23)

Làm lần lượt:

1. D-1 — SensorCatalogRepository::ensureExists
   Khi sensor đã tồn tại với active=0 (sau prune), poll/reading quay lại phải reactivate.
   Gợi ý: INSERT ... ON CONFLICT(...) DO UPDATE SET active = 1 (và giữ atomic như C-4).
   Thêm test Qt Test: prune → ensureExists → active=1 (tên gợi ý test_sensor_prune_and_recover).

2. M-10 — RestConfigService guard
   fetchConfig và applyConfig không được drop im lặng khi dùng chung bit Endpoint::Config.
   Tách guard per operation HOẶC queue request; emit lỗi rõ nếu busy.

3. M-13 — Shared SensorMonitoringTableModel
   Dashboard vs LoggerDetail không được đè state lẫn nhau.
   Tách instance per view HOẶC reset model khi navigate; giữ MVVM.

4. L-14 — DashboardController::afterMutation
   Gọi refreshIngestionChart() sau CRUD logger (cùng pattern purgeOldData).

5. L-23 — LoggerDetailViewModel REST catalog upsert
   Bọc vòng upsert từ GET /config trong db.transaction()/commit().

Ràng buộc: không đổi contracts edge; MVVM; không FE-017/FE-014.
Build + ctest pass.

Cuối task — sửa docs/report/2026-05-26-deep-audit.md:
- §4/§5/§6: tick Fixed cho ID đã xong
- §2: cập nhật số Confirmed open
- §8 E2E: cập nhật flow Add logger nếu Gap đã hết
```

---

## Batch 2 — REST / Modbus / Core Medium

```
Sửa backlog Batch 2 — Central Logger C++.

Đọc: AGENTS.md, docs/contracts/, docs/report/2026-05-26-deep-audit.md
(M-1, M-2, M-3, M-5, M-7, M-8, M-9, M-14)

1. M-7 — RestConfigService::fetchConfig: bỏ Content-Type trên GET (hoặc chỉnh nhất quán với debug).

2. M-8 — downloadLatestReport: tránh readAll() unbounded; stream/chunk hoặc giới hạn kích thước hợp lý.

3. M-9 — ModbusService destroyState khi ConnectingState: disconnect/timer cleanup, không leak stateChanged holder.

4. M-14 — SensorMerger: phân biệt ALARM_MIN vs ALARM_MAX (hoặc label đúng) khi alarm từ threshold.

5. M-1 — EventRepository::insert: read-back id/createdAt nếu caller cần.

6. M-2 — ensureExists: log hoặc propagate lỗi khi errorOut == nullptr (không nuốt im lặng).

7. M-3 — LoggerRepository::findAllWithSensorCounts: alias cột SQL rõ ràng, tránh ambiguous.

8. M-5 — SensorCatalogRepository::upsert: bỏ dòng no-op sensor_type trong DO UPDATE (hoặc chỉ update khi đổi).

Build + ctest pass. Cập nhật deep-audit.md như Batch 1.
```

---

## Batch 3 — QML / UX

```
Sửa backlog Batch 3 — QML/UI Central Logger.

Đọc: AGENTS.md, docs/report/2026-05-26-deep-audit.md
(M-17, M-18, M-22, M-23, L-15, L-16, L-17, L-18, L-19)

1. M-17 — DashboardView.qml: hover event list theme-aware (giống LoggerDetailView — rgba theo Material theme).

2. M-18 — LoggerFormDialog.qml: width = Math.max(320, Math.min(...)) tránh width âm.

3. M-22 — Sidebar/rail: clip (historical `AppSidebar`; superseded by `AppNavigationRail.qml`).

4. M-23 — SectionLabel opacity animate (removed with fixed 80px rail).

5. L-15 — Main.qml: đổi tham số arrow `id` → `loggerId` (hoặc tên khác không shadow keyword).

6. L-18 — LoggerDetailView: tooltip khi nút disabled (dùng MouseArea wrapper hoặc ToolTip on parent).

7. L-19 — PageHeader.qml: sửa binding loop implicitHeight / anchors.fill.

8. L-16, L-17 — LoggersView / LoggerDetailView: elide hoặc flexible column width cho text dài.

Không thêm logic nghiệp vụ vào .js. Build + ctest pass. Cập nhật deep-audit.md.
```

---

## Batch 4 — Low / maintainability (tùy chọn)

```
Sửa backlog Batch 4 (Low) — chỉ các mục an toàn, không đổi hành vi product ngoài ý muốn.

Đọc: docs/report/2026-05-26-deep-audit.md §5 (L-1 … L-13, L-23 nếu chưa xong ở Batch 1).

Ưu tiên: L-13 (SettingsController clear lastError), L-6 (:memory: constant), L-10 (operator== LoggerRuntimeConfig), L-11/L-12 (PollHistoryStore warnings).

Bỏ qua nếu risk cao: L-7 (https) — cần product decision, ghi Won't fix trong deep-audit.

Mỗi fix nhỏ, một commit logic. ctest pass. Cập nhật deep-audit.md — Low có thể ghi Won't fix + lý do.
```

---

## Prompt — Chỉ sửa báo cáo audit (không code)

```
Task: Cập nhật docs/report/2026-05-26-deep-audit.md sau khi user/agent đã fix code (không sửa C++ trong task này).

Đọc repo + deep-audit hiện tại. Với từng ID trong §4–§6:
- Verified Fixed: đọc code, ghi file:dòng + test name
- Vẫn Open: giữ hoặc sửa evidence
- False positive / Won't fix: ghi lý do + trích spec

Chạy ctest, ghi kết quả §1 Metadata.
Sửa §2 đếm open cho khớp bảng.
Không mâu thuẫn §12 với số open.

Không commit trừ khi user yêu cầu.
```

---

## Prompt — Deep audit lại (read-only)

```
Deep audit READ-ONLY — Central Logger C++.

Đọc: AGENTS.md, HANDOFF.md, thiet_ke_db.md, docs/contracts/, docs/report/2026-05-26-deep-audit.md

KHÔNG sửa source. Chạy build + ctest. Spot-check grep (createSeries, pollInFlight, active=0, user_version, /readings, tray).

EDIT IN PLACE docs/report/2026-05-26-deep-audit.md:
- Xác minh lại C-1…C-8 và mọi dòng "Fixed verified"
- Cập nhật bảng M/L open vs fixed
- §7 contract matrix, §8 E2E, §9 test gaps
- Không kết luận "all green" nếu còn open

Không commit trừ khi user yêu cầu.
```

---

## Checklist reviewer

- [ ] deep-audit.md §2 số open khớp bảng §4–§5
- [ ] ID fixed có evidence + test (nếu có)
- [ ] ctest pass sau mỗi batch
- [ ] Không FE-017 / FE-014 / D-2 (trừ khi user đổi scope)
