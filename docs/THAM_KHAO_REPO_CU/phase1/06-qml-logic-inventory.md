# QML Logic Inventory

Liệt kê tất cả function trong 2 file `.pragma library` hiện tại.
Mỗi function được ghi rõ mục đích, đầu vào/đầu ra, và đề xuất C++ tương ứng trong repo mới.

---

## `src/central_logger/ui/logic/LoggerDetailLogic.js`

Import bởi: `LoggerDetailView.qml`

### `findModelRow(loggersModel, loggerId) → row | null`

**Mục đích:** Tìm `LoggerItem` trong `LoggerListModel` theo ID.

**Input:** `loggersModel` (QML binding tới `LoggerListModel`), `loggerId` (int)

**Output:** object có các trường `loggerId`, `name`, `host`, `port`, `unitId`, `online`, v.v. hoặc `null`

**Logic:** Vòng lặp tuyến tính qua `loggersModel.count()` + `itemAt(i)`.

**C++ tương đương:** `LoggerListModel::findById(int id) → QVariantMap` — `Q_INVOKABLE`; remove JS loop.

---

### `rebuildDetail(detail, row, loggerId) → detail`

**Mục đích:** Dựng lại object `detail` của LoggerDetail từ `LoggerItem` row, giữ nguyên `sensorList`, `configForm`, `cloudForm` hiện tại.

**Input:** `detail` (QML object hiện tại), `row` (kết quả `findModelRow`), `loggerId`

**Output:** Object `detail` mới với các trường: `loggerId`, `loggerName`, `host`, `port`, `unitId`, `pollIntervalS`, `timeoutS`, `enabled`, `note`, `apiPort`, `apiBaseUrl`, `sensorCount`, `online`, `polling`, `rtuConnected`, `anyAlarm`, `currentRevision`, `lastRevision`, `statusText`, `sensorList`, `configForm`, `cloudForm`, `rawConfig`, `catalogError`

**Logic:** Copy-on-write object, fallback nếu `row` null (dùng `loggerId` để tạo placeholder).

**C++ tương đương:** `LoggerDetailViewModel::rebuildDetail(int loggerId)` — đọc trực tiếp từ `LoggerListModel`; expose qua `Q_PROPERTY` hoặc emit signal.

---

### `applySensorsPayload(detail, payload) → detail`

**Mục đích:** Áp payload `sensorsUpdated` (từ `DashboardController.sensorsUpdated` signal) vào `detail.sensorList`.

**Input:** `detail`, `payload` = `{ sensors: [...], iso: "...", polling, rtu_connected, any_alarm }`

**Output:** `detail` với `sensorList` mới, `sensorCount`, `polling`, `rtuConnected`, `anyAlarm` cập nhật.

**Logic:**
- Tính `displayName` từ `s.name` hoặc `s.sensor_type + "#" + s.sensor_id` hoặc `"Sensor " + s.sensor_id`.
- Map mỗi sensor thành object: `sensor_id`, `name`, `type`, `sensor_type`, `unit`, `active`, `timestamp` (HH:MM:SS), `value`, `valid`, `alarm`, `stale`, `display_status`, `alarm_type`, `rest_status`.

**C++ tương đương:** `LoggerDetailViewModel::applySensorsPayload(const QString& payloadJson)` — parse JSON, update internal `QList<SensorEntry>`, emit `sensorListChanged`.

---

### `hydrateDetailFromDb(detail, db) → detail`

**Mục đích:** Merge DB fields (từ `DashboardController.getLoggerFormData`) vào `detail` khi Logger Detail mở.

**Input:** `detail`, `db` = `{ loggerId, timeoutS, note, apiPort, apiBaseUrl, lastRevision, apiToken }`

**Output:** `detail` với `timeoutS`, `note`, `apiPort`, `apiBaseUrl`, `lastRevision`, `cloudForm.apiToken`, `cloudForm.apiPort` từ DB.

**C++ tương đương:** Integrate into `LoggerDetailViewModel::loadFromDb(int loggerId)` — call `LoggerRepository::getById`.

---

### `effectiveConfigRevision(detail) → int`

**Mục đích:** Trả về revision hiện tại để dùng cho `applyConfig(id, revision, json)`.

**Logic:** `currentRevision >= 0` ? `currentRevision` : `lastRevision >= 0` ? `lastRevision` : `-1`.

**C++ tương đương:** `int LoggerDetailViewModel::effectiveConfigRevision() const`

---

### `parseJsonSafe(jsonStr, fallback) → object | fallback`

**Mục đích:** Safe `JSON.parse` với fallback.

**C++ tương đương:** Inline `QJsonDocument::fromJson(...).isNull()` check; no separate function needed.

---

### `mergeConfigFetched(detail, payloadJson) → { detail, error }`

**Mục đích:** Merge kết quả `configFetched` signal vào `detail`.

**Input:** `payloadJson` = response từ `GET /api/v1/config` → `{ revision, config: { station_code, station_name, poll_interval, modbus_tcp_bind, modbus_tcp_enabled, modbus_tcp_unit_id }, api_token, api_port, api_base_url }`

**Output:** `{ detail: mergedDetail, error: "" }` hoặc `{ detail, error: "Invalid config response" }`

**Logic:**
- Update `currentRevision`, `lastRevision`.
- Build `configForm` từ `config` object.
- Update `cloudForm.apiToken`, `cloudForm.apiPort`.
- Update `apiPort`, `apiBaseUrl` nếu server trả về.

**C++ tương đương:** `LoggerDetailViewModel::onConfigFetched(bool ok, const QString& payloadJson)` — parse + emit `configFormChanged`.

---

### `configFetchedErrorMessage(payloadJson, defaultMsg) → string`

**Mục đích:** Humanize REST error cho UI snackbar.

**Logic:**
- HTTP 401 + "not configured" → "Device REST token empty — Scan QR on logger"
- HTTP 401 + "invalid bearer" → "Token mismatch — Scan QR again on device"
- HTTP 401 generic → "REST unauthorized (401)"
- `errors[0].message` nếu có
- `"HTTP {status}: {message}"` hoặc `message`

**C++ tương đương:** `static QString RestConfigService::configFetchErrorMessage(const QJsonObject& payload, const QString& defaultMsg)` — keep same string logic.

---

### `mergeConfigApplied(detail, payloadJson) → detail`

**Mục đích:** Merge kết quả `configApplied` signal — update `currentRevision`.

**Input:** `payloadJson` = `{ ok: true, applied_revision: N }`

**C++ tương đương:** `LoggerDetailViewModel::onConfigApplied(bool ok, const QString& payloadJson)`.

---

## `src/central_logger/ui/logic/LoggerFormLogic.js`

Import bởi: `LoggerFormDialog.qml`

### `humanizeProbeError(raw) → string`

**Mục đích:** Map raw error string từ `edgeConfigProbed` sang user-friendly message.

| Keyword | Message |
|---------|---------|
| "timeout" / "timed out" | "The logger did not respond in time. Check host and API port." |
| "network" / "connection" | "Could not reach the logger. Check host, API port, and network." |
| "401" / "unauthorized" / "token" | "Invalid or missing API token." |
| "404" | "Logger API not available. Update data-logger firmware." |
| "409" / "conflict" / "revision" | "Configuration changed on device. Connect again, then save." |
| other | "Could not load configuration." |

**C++ tương đương:** `static QString LoggerFormViewModel::humanizeProbeError(const QString& raw)`

---

### `diff(current, original) → changedFields`

**Mục đích:** Shallow diff hai JS objects; trả về object chỉ chứa các key có giá trị thay đổi.

**C++ tương đương:** `static QJsonObject diffObjects(const QJsonObject& current, const QJsonObject& original)` — iterate keys, compare values.

---

### `connectionSnapshotFromFields(fields) → snapshot`

**Mục đích:** Tạo snapshot connection từ form fields (Add mode) để dùng làm `snap` cho `buildEditPatch`.

**Input:** `fields` = `{ name, note, host, port, unit, timeout, apiPort, apiBaseUrl, token }`

**Output:** `{ loggerName, note, host, port (int, default 5020), unitId (int, default 1), timeoutS (float, default 2.0), apiPort (int, default 8080), apiBaseUrl, cloudForm: { apiToken, apiPort } }`

**C++ tương đương:** `static LoggerFormData LoggerFormViewModel::connectionSnapshotFromFields(const QVariantMap& fields)` — with safe parseInt/parseFloat with defaults.

---

### `buildEditPatch(mode, detail, configLoaded, fields) → { connection, config, cloud }`

**Mục đích:** Tính diff patch để gửi lên 3 slots: `updateLoggerConnection`, `updateLoggerApi`, `applyConfig`.

**Input:**
- `mode` = `"add"` | `"edit"`
- `detail` = current logger detail object (xem `rebuildDetail`)
- `configLoaded` = bool (device fields unlocked after probe)
- `fields` = tất cả form field values

**Output:**
```json
{
  "connection": {
    "name", "host", "port", "unitId", "pollIntervalS", "timeoutS", "note"
  },
  "config": { /* diff of device config fields — only changed */ },
  "cloud": { /* diff of apiToken, apiPort, apiBaseUrl — only changed */ }
}
```

**Logic:**
- `connection` luôn trả về (no diff; overwrite always).
- `config` chỉ tính nếu `mode === "edit" && configLoaded`; dùng `diff()` với configOriginal từ `detail.configForm` / `detail.rawConfig`.
- `cloud` dùng `diff()` với `cloudOriginal` từ `detail.cloudForm`.

**C++ tương đương:** `LoggerFormViewModel::buildEditPatch(const QString& mode, ...)` — returns `struct EditPatch { LoggerConnectionData connection; QJsonObject config; QJsonObject cloud; }`.

---

### `parseProbeSuccess(jsonStr, snap) → { ok, error } | { ok, detail, revision }`

**Mục đích:** Parse kết quả `edgeConfigProbed` signal; merge config vào `snap` để unlock device fields.

**Input:** `jsonStr` = `{ ok, config: {...}, revision, errors: [...], message }`, `snap` = connection snapshot

**Output success:** `{ ok: true, detail: { ...snap, configForm, rawConfig, currentRevision }, revision }`

**Output failure:** `{ ok: false, error: "..." }` — message từ `errors[0].message` hoặc `p.message`

**C++ tương đương:** `LoggerFormViewModel::parseProbeSuccess(const QString& jsonStr, const LoggerFormData& snap)` — returns `ProbeResult { bool ok; QString error; LoggerFormData detail; int revision; }`.

---

## `src/central_logger/ui/components/common/StatusBadges.js`

Import bởi: `SensorStatusBadge.qml`

### `statusColor(displayStatus, isDark) → color`
### `statusText(displayStatus) → string`

**Mục đích:** Map `display_status` string → badge text + color.

| `display_status` | Text | Color note |
|-----------------|------|-----------|
| "OK" | "OK" | green |
| "ALARM_MIN" / "ALARM_MAX" | "ALARM" | red |
| "STALE" | "STALE" | orange |
| "ERR" | "ERR" | red |
| "INACTIVE" | "—" | gray |
| "" / unknown | "—" | gray |

**C++ tương đương:** `SensorStatusHelper::statusText(const QString& displayStatus)` + `QColor SensorStatusHelper::statusColor(...)` — or pure QML enum binding.

---

## Summary: Functions → C++ ViewModel methods

| File | Function | C++ destination |
|------|----------|-----------------|
| LoggerDetailLogic.js | `findModelRow` | `LoggerListModel::findById` |
| LoggerDetailLogic.js | `rebuildDetail` | `LoggerDetailViewModel::rebuildDetail` |
| LoggerDetailLogic.js | `applySensorsPayload` | `LoggerDetailViewModel::applySensorsPayload` |
| LoggerDetailLogic.js | `hydrateDetailFromDb` | `LoggerDetailViewModel::loadFromDb` |
| LoggerDetailLogic.js | `effectiveConfigRevision` | `LoggerDetailViewModel::effectiveConfigRevision` |
| LoggerDetailLogic.js | `parseJsonSafe` | inline QJsonDocument |
| LoggerDetailLogic.js | `mergeConfigFetched` | `LoggerDetailViewModel::onConfigFetched` |
| LoggerDetailLogic.js | `configFetchedErrorMessage` | `RestConfigService::configFetchErrorMessage` |
| LoggerDetailLogic.js | `mergeConfigApplied` | `LoggerDetailViewModel::onConfigApplied` |
| LoggerFormLogic.js | `humanizeProbeError` | `LoggerFormViewModel::humanizeProbeError` |
| LoggerFormLogic.js | `diff` | `LoggerFormViewModel::diffObjects` |
| LoggerFormLogic.js | `connectionSnapshotFromFields` | `LoggerFormViewModel::connectionSnapshotFromFields` |
| LoggerFormLogic.js | `buildEditPatch` | `LoggerFormViewModel::buildEditPatch` |
| LoggerFormLogic.js | `parseProbeSuccess` | `LoggerFormViewModel::parseProbeSuccess` |
| StatusBadges.js | `statusColor` / `statusText` | `SensorStatusHelper` or QML enum |

**QML View sau khi migrate:** Chỉ còn binding tới ViewModel properties và signal handlers — không còn `.pragma library` `.js` files.
