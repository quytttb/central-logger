# Non-Functional Requirements (NFR)

Baseline từ app PySide6 hiện tại (đo 2026-05-23) và mục tiêu cho repo C++ mới.

---

## Performance

| Metric | Baseline (Python app) | Target (C++ rewrite) |
|--------|----------------------|----------------------|
| Ingestion chart query @100k rows | avg **2.3 ms** (min 1.9, max 3.6) | ≤ 5 ms (giữ hoặc cải thiện) |
| Ingestion chart query @empty DB | ~11 ms (cold start overhead) | ≤ 5 ms |
| Test suite (104 tests) | **1.47 s** headless | ≤ 5 s (Qt Test suite) |
| UI frame rate | Không đo; Material QML | 60 fps target (QML `targetRenderTime`) |
| Modbus poll latency | ≤ `poll_interval_s` (2s default) under 3 loggers | ≤ `poll_interval_s` under 10 loggers |
| App startup | Không đo chính xác | ≤ 3 s đến QML visible |
| Memory (idle, 4 loggers) | Không đo | ≤ 150 MB RSS |

**Method baseline:** `scripts/bench_ingestion_chart.py --seed 100000 --runs 5` trên SQLite `dist/bench_ingestion.db`.

---

## Reliability

| Requirement | Hiện trạng | Target C++ |
|-------------|------------|------------|
| UI không treo khi 1 logger mất kết nối | ✓ Backoff + QueuedConnection | Giữ: `QThread` worker, main thread không block |
| Modbus reconnect tự động | ✓ Exponential backoff 1s → 2s → ... | Giữ logic backoff |
| REST 401 → user message, không crash | ✓ `configFetchedErrorMessage` | Giữ |
| Graceful shutdown | ✓ `stop()` + thread join | `QThread::quit()` + `wait()` |
| DB schema migration an toàn | ✓ ALTER TABLE + `CREATE INDEX IF NOT EXISTS` | Version-gated SQL migrations |
| Test coverage (unit) | 104 tests, 20 test files | ≥ 80% line coverage cho core C++ classes |

---

## Security

| Requirement | Hiện trạng | Target C++ |
|-------------|------------|------------|
| API token storage | Plaintext trong SQLite `logger_info.api_token` | v1: giữ SQLite; Future: OS keychain (`QKeychain`) |
| Token trong logs | `rest_auth.py` — không log token (explicit comment trong `provision-qr-v1.md`) | Giữ: không log token; `Q_LOGGING_CATEGORY` filter |
| Network | HTTP cleartext REST trên LAN | v1: HTTP (LAN only); Future: HTTPS option |
| QR secret | QR chứa token; chỉ dùng trên LAN during install | Giữ: không thay đổi scheme |

---

## Platform support

| Platform | Hiện trạng | Target C++ v1 |
|----------|------------|---------------|
| Linux (Ubuntu/Debian) | ✓ Dev + deploy (`.deb`) | ✓ Required |
| Windows 10/11 x64 | ✓ Deploy (`.msi` via Nuitka + WiX) | ✓ Required (Qt IFW) |
| macOS | ✗ | Future (FE-033) |
| Qt version | PySide6 6.11.1 (Qt 6.11) | Qt 6.7+ LTS **hoặc** Qt 6.8+ |
| Minimum screen | 1024×768 (frameless, min size set) | Giữ |

---

## Internationalization (i18n)

| Requirement | Hiện trạng | Target C++ |
|-------------|------------|------------|
| Ngôn ngữ UI | Tiếng Anh (phần lớn) + Tiếng Việt (mixed trong labels) | Qt Linguist: `en` (primary), `vi` (secondary) |
| Timezone | `system_timezone` configurable, default `Asia/Ho_Chi_Minh` | `QTimeZone` + `QDateTime`; giữ IANA timezone config |
| Date/time format | ISO 8601 trong DB; local tz trong chart labels | Giữ: `QDateTime::toLocalTime()` với configured timezone |

---

## Themeing

| Requirement | Hiện trạng | Target C++ |
|-------------|------------|------------|
| Dark / Light | ✓ `Colors.qml` (zinc/shadcn palette); `window.isDark` + `Material.theme` | Giữ palette và cơ chế toggle; `Colors.qml` port sang QML singleton |
| Primary color | `#000666` (deep navy); accent `#4C56AF` | Giữ |
| Font | Roboto + Roboto Mono (bundled); Material Symbols Outlined | Bundle font trong qrc |

---

## Logging & observability

| Requirement | Hiện trạng | Target C++ |
|-------------|------------|------------|
| Debug mode | `CENTRAL_LOGGER_DEBUG=1` env → `logging.DEBUG` | `QT_LOGGING_RULES` env hoặc `QLoggingCategory` |
| Log categories | `central_logger.services`, `central_logger.controllers` | `CentralLogger.Modbus`, `CentralLogger.Rest`, `CentralLogger.DB` categories |
| Log rotation | Không có | `QLoggingCategory` + optional file sink (`QFile`) |

---

## Offline operation

| Requirement | Giá trị |
|-------------|---------|
| Local DB required | ✓ SQLite tại `~/.central-logger/central-logger.db` |
| Modbus không cần cloud | ✓ Core functionality offline |
| REST optional per-logger | ✓ `api_token` empty → REST features disabled |
| Multi-site sync | ✗ Out of scope v1 |

---

## Accessibility

| Requirement | Hiện trạng | Target C++ |
|-------------|------------|------------|
| Keyboard navigation | Chưa đánh giá | Gap — ghi nhận; Future |
| Screen reader | Chưa đánh giá | Gap — ghi nhận; Future |
| Color contrast | Dark/light themes có contrast | Verify WCAG AA cho critical status badges |

---

## Build & CI

| Requirement | Hiện trạng | Target C++ |
|-------------|------------|------------|
| Build tool | Hatchling (Python wheel) + Nuitka | CMake 3.22+; `qt_add_qml_module` |
| CI | GitHub Actions: ci.yml (pytest), dev-build.yml, build-release.yml | GitHub Actions: cmake build + Qt Test + CPack |
| Code quality | ruff + black | clang-format + clang-tidy |
| Test runner | pytest + pytest-qt | Qt Test (`QTEST_MAIN`) + CTest |

---

## Data retention

| Requirement | Hiện trạng | Target C++ |
|-------------|------------|------------|
| Configurable days | ✓ `app_settings.data_retention_days` default 30 | Giữ |
| Purge mechanism | `DELETE WHERE recorded_at < cutoff` | `RetentionService::purge()` on QThread |
| Purge trigger | On app start + `QTimer` 1h + Settings save | Giữ |
