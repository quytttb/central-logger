# Central Logger (Qt 6 C++)

Desktop app quản lý Data Logger qua Modbus TCP + REST.

## Bắt đầu

- **Thiết kế DB:** [`docs/thiet_ke_db.md`](docs/thiet_ke_db.md)
- **Tổng quan:** [HANDOFF.md](HANDOFF.md)
- **Agent / AI:** [AGENTS.md](AGENTS.md)

## Build

Yêu cầu: **Qt 6.11**, CMake 3.16+. Cài qua [Qt Online Installer](https://doc.qt.io/qt-6/get-and-install-qt.html) + **Maintenance Tool** (không dùng Qt Charts — deprecated; dùng **Qt Graphs**).

### Qt components (Maintenance Tool)

| Cần cho repo | Tên trong Maintenance Tool | Ghi chú |
|--------------|----------------------------|---------|
| Bắt buộc | **Qt 6.11.1** → *Desktop* → **GCC 64-bit** | Quick, Qml, Sql, Network, Test, … |
| Modbus | **Qt Serial Bus** | |
| (phụ thuộc) | **Qt Serial Port** | Bắt buộc khi cài Serial Bus |
| Biểu đồ | **Qt Graphs** | Thay **Qt Charts** (deprecated 6.11) |
| Dev | **Qt Creator** (tuỳ chọn) | Đã có trong `~/Qt/Tools` |

**Đã cài nhưng repo không dùng** (có thể gỡ nếu muốn gọn): Qt Multimedia, Qt Quick 3D, Qt Quick Timeline, Qt Shader Tools, Qt Task Tree.

**Không cài / gỡ nếu còn:** **Qt Charts** — deprecated; dùng Qt Graphs.

ID gói (CLI `MaintenanceTool search`): `qt.qt6.6111.linux_gcc_64`, `qt.qt6.6111.addons.qtserialbus.linux_gcc_64`, `qt.qt6.6111.addons.qtserialport.linux_gcc_64`, `qt.qt6.6111.addons.qtgraphs.linux_gcc_64`.

Cài Qt (ví dụ `~/Qt/6.11.1/gcc_64`), rồi:

```bash
export CMAKE_PREFIX_PATH=~/Qt/6.11.1/gcc_64${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}

cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/bin/central_logger
```

Trên máy chỉ có Qt **6.10** từ apt: vẫn có thể thử build nếu hạ `REQUIRES` tạm thời — toolchain chuẩn của repo là **6.11**.

Cấu trúc CMake theo [Qt CMake Get Started](https://doc.qt.io/qt-6/cmake-get-started.html): `src/app` (QML + executable), `src/data` (database layer).

### Qt Creator

1. Mở `CMakeLists.txt` → kit **Desktop** → build dir `build/Desktop-Debug` (hoặc tương đương).
2. **Run configuration:** chọn target **`app`** (executable `central_logger`), không phải test.
3. **Deploy:** dùng **Default** (không deploy) — **không** chọn *Application Manager* / *Install Application Manager package*.  
   Nếu Run báo `appman-controller` does not exist: **Projects → Run → Deploy** → chọn cấu hình **Default** (index 0), bỏ *Application Manager*.
4. Sau khi đổi `CMakeLists.txt` (vd. `QT_QML_OUTPUT_DIRECTORY`): **Run CMake** rồi **Rebuild**.

Chạy ngoài IDE: `./build/Desktop-Debug/bin/central_logger` (hoặc `./build/bin/central_logger` tùy build dir).

### Test

```bash
cd build && ctest --output-on-failure
```

### CI / đóng gói

Chi tiết Linux + Windows + phát hành: [`packaging/README.md`](packaging/README.md).

| Workflow | Khi chạy | Kết quả |
|----------|----------|---------|
| `ci.yml` | push / PR `main` | `cmake` Debug + `ctest` |
| `dev-build.yml` | push `main` | artifact `.deb` + `CentralLoggerSetup.exe` |
| `build-release.yml` | tag `v*.*.*` | GitHub Release |

**Release (bump → tag → push):** `./packaging/linux/deploy.sh` hoặc `.\packaging\windows\deploy.ps1`

## Tài liệu

| Path | Nội dung |
|------|----------|
| [`docs/thiet_ke_db.md`](docs/thiet_ke_db.md) | Schema SQLite + RAM (spec chính thức) |
| [`docs/adr/0001-db.md`](docs/adr/0001-db.md) | ADR: `QSqlDatabase` / `QSQLITE` |
| [`docs/contracts/`](docs/contracts/) | REST / Modbus / QR |
| [`docs/THAM_KHAO_REPO_CU/`](docs/THAM_KHAO_REPO_CU/) | Khảo sát app cũ — **chỉ tham khảo**, không port Phase 1/2 cũ |
