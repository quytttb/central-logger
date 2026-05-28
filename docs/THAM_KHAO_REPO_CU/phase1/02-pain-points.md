# Pain Points & Improvement Ideas

Mỗi mục có `Evidence` (file + dòng code hoặc test ID) và `Hướng cải thiện` trong repo C++ mới.
Điều kiện: **≥8 mục với evidence cụ thể.**

---

## P-01 — GIL + asyncio threading complexity / Phức tạp luồng Python

**Evidence:** `src/central_logger/controllers/modbus_bridge.py` — `threading.Thread` chạy `asyncio.run()` riêng; `_snapshotForUi = Signal(object, object)` với `Qt.ConnectionType.QueuedConnection` để đưa kết quả về main thread. `dashboard_controller.py:381` dùng `asyncio.run_coroutine_threadsafe(coro, loop)` cho REST probe.

**Triệu chứng:** Python GIL + asyncio event loop chạy trên thread riêng → khó debug race condition; GIL không loại bỏ hoàn toàn overhead context-switch.

**Hướng cải thiện (C++):** `QThread` worker + `QModbusTcpClient` (hoặc `QTcpSocket` raw); single Qt event loop per thread; `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` thay thế `_snapshotForUi`. Không cần asyncio.

---

## P-02 — Native dependency: ZBar DLL Windows / Phụ thuộc native ZBar trên Windows

**Evidence:** `resources/native/windows/README.md` — bundle ZBar DLL cạnh `.exe`; `scripts/stage_zbar_windows.ps1` tự tải DLL; `utils/native_libs.py` — `ctypes` load + version check. `tests/test_native_libs.py` test path discovery logic. `services/qr_provision.py` — `pyzbar.decode()`.

**Triệu chứng:** Deploy Windows phức tạp; DLL version phải khớp; pyzbar Python binding có thể fail nếu DLL path sai; tăng kích thước Nuitka bundle.

**Hướng cải thiện (C++):** `QZXing` (LGPL Qt wrapper) hoặc `ZBar` Qt integration; không cần DLL Windows riêng; biên dịch cùng CMake.

---

## P-03 — Business logic split across JS + 3 Python slots / Logic nghiệp vụ phân tán

**Evidence:** Edit logger flow cần gọi 3 slots riêng: `updateLoggerConnection`, `updateLoggerApi`, `applyConfig` — quyết định gọi slot nào và diff thế nào nằm trong `LoggerFormLogic.js:buildEditPatch` (110 dòng). Probe unlock device fields nằm trong `parseProbeSuccess` (JS). Error humanization nằm trong `humanizeProbeError` (JS).

**Triệu chứng:** Logic nghiệp vụ leak vào QML logic file; khó unit test (cần QML test harness); thay đổi contract REST phải sửa cả Python và JS.

**Hướng cải thiện (C++):** `LoggerFormViewModel` C++ class chứa `buildEditPatch`, `parseProbeSuccess`, `humanizeProbeError`; QML chỉ bind properties và gọi `Q_INVOKABLE`; full unit test với Qt Test.

---

## P-04 — Python↔QML data crossing on every Modbus poll / Overhead dữ liệu Python-QML

**Evidence:** `viewmodels/sensor_monitoring_table_model.py:updateFromJson` — nhận `QVariantList`, parse JSON string mỗi lần poll. `viewmodels/logger_list_model.py:update_from_snapshot` — cập nhật `LoggerItem`, emit `dataChanged`. Poll interval mặc định 2s → ~30 QML data crossings/phút.

**Triệu chứng:** PySide6 marshaling overhead khi truyền `QVariantList` qua bridge; JSON decode trong Python rồi re-encode để gửi QML là redundant.

**Hướng cải thiện (C++):** `SensorMonitoringTableModel` C++ `QAbstractTableModel`; struct `SensorRow` native; `dataChanged` signal targeted (specific rows); không có Python bridge overhead.

---

## P-05 — Revision conflict UX complexity / UX xung đột revision phức tạp

**Evidence:** `LoggerDetailLogic.js:mergeConfigFetched` — merge `currentRevision` + `lastRevision` (2 fields); `LoggerDetailLogic.js:effectiveConfigRevision` — fallback logic; `rest_coordinator.py` — 409 handling returns specific error JSON; `LoggerFormLogic.js:humanizeProbeError` — "409 conflict" → "Configuration changed on device. Connect again, then save."

**Triệu chứng:** `currentRevision` vs `lastRevision` (DB field) là 2 khái niệm cần phân biệt rõ; logic phân tán qua cả JS, Python, và DB; dễ desync.

**Hướng cải thiện (C++):** `LoggerDetailViewModel` lưu `currentRevision` làm Q_PROPERTY; `lastRevision` chỉ đọc từ DB khi hydrate; clear separation; unit test revision conflict path.

---

## P-06 — Custom Canvas chart paint verbosity / Chart Canvas tự vẽ phức tạp

**Evidence:** `BaseChart.qml` — 130+ dòng Canvas with `schedulePaint()` debounce timer, `hoverCrosshairOpacity`, `pointCount`, `effectiveYAxisLabelsCount`, manual paint on `onPaint`. `NetworkTrafficChart.qml` + `SensorTrendingChart.qml` implement custom series JSON parsing.

**Triệu chứng:** Canvas custom paint khó maintain; tooltip logic thủ công; không dùng QtCharts; cần test thủ công cho mỗi edge case chart layout.

**Hướng cải thiện (C++):** Evaluate `QtCharts::QChart` (declarative `ChartView` + `LineSeries`) vs giữ Canvas; QtCharts có sẵn legends, tooltips, zoom — giảm code đáng kể. Quyết định ở Phase 2 ADR.

---

## P-07 — REST readings + catalog merge intricacy / Logic merge REST readings + catalog phức tạp

**Evidence:** `controllers/rest_coordinator.py:60–391` — `format_rest_error_message`, catalog request scheduling, readings cache, `extract_sensors_from_config_raw`, `extract_sensors_from_readings_raw` (từ `services/sensor_catalog.py`). `SensorState.cache_sensors` — merge Modbus raw + REST readings + catalog; `test_sensor_catalog.py` — 15 test cases.

**Triệu chứng:** Merge logic nhiều bước; DI/DO sensors chỉ xuất hiện qua REST; catalog fetch lần đầu rồi cache; REST readings chỉ gọi khi có catalog; ordering dependency giữa catalog và readings.

**Hướng cải thiện (C++):** `SensorMerger` class với rõ ràng state machine: `NoCatalog → CatalogFetched → ReadingsMerged`; unit test với Qt Test.

---

## P-08 — Poll interval drift under Python load / Drift poll interval dưới tải Python

**Evidence:** `modbus_manager.py` — asyncio `asyncio.sleep(logger_config.poll_interval_s)` sau mỗi read. Python asyncio single-threaded; GIL + slow DB write có thể delay task scheduling.

**Triệu chứng:** Dưới tải cao (nhiều loggers + DB writes), poll interval thực tế có thể lớn hơn configured. Chưa đo được chính xác vì không có hardware stress test kết quả (H-10).

**Hướng cải thiện (C++):** `QTimer` với `Qt::PreciseTimer` (millisecond accuracy); DB write trên worker thread riêng không block poll loop.

---

## P-09 — Docs drift: AGENT.md vs actual pattern / Tài liệu cũ không khớp code thực

**Evidence:** Root `AGENT.md` mô tả: "Controllers expose to QML via `setContextProperty`" — nhưng code thực (`main.py`) chỉ dùng context property cho `TrayCtl` và `logoUrl`; tất cả controllers dùng `@QmlElement` + `import CentralLogger.Core 1.0`. `.agent/SKILL_MVVM_INTEGRATION.md` cũng dùng context property pattern cũ trong example.

**Triệu chứng:** Onboarding confusion; agent hoặc developer mới có thể implement sai pattern.

**Hướng cải thiện:** Phase 1 deliverable này (`docs/phase1/`) là single source of truth cho repo mới. Viết ADR trong Phase 2 xác nhận `qt_add_qml_module` + `QML_ELEMENT` là pattern chính thức.

---

## P-10 — Nuitka + Python deploy size / Kích thước bundle deploy lớn

**Evidence:** `pysidedeploy.spec`, `scripts/build_deploy_linux.sh`, `scripts/build_deploy_windows.ps1` — Nuitka one-file mode với PySide6 full bundle. `resources/native/windows/` — ZBar DLLs riêng. Windows deploy cần Python 64-bit từ python.org (không dùng Store version — ghi trong README).

**Triệu chứng:** Bundle PySide6 đầy đủ (~200–400MB); Python Store version làm Nuitka fail (`unable to find dynamic system library 'python313'`); deploy phức tạp.

**Hướng cải thiện (C++):** Qt Installer Framework (IFW) với shared Qt libs; CMake `windeployqt` / `macdeployqt`; không cần Nuitka; bundle nhỏ hơn đáng kể.
