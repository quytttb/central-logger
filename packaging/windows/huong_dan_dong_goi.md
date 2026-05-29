# Hướng dẫn Đóng gói Ứng dụng Central Logger C++ (Windows)

Tài liệu này hướng dẫn chi tiết cách đóng gói ứng dụng **Central Logger** (C++ + Qt 6 + QML) thành một bộ cài đặt duy nhất (`CentralLoggerSetup.exe`) chạy độc lập trên Windows bằng công cụ **Qt Installer Framework (QTIFW)**.

---

## 1. Kết Quả Đóng Gói Hiện Tại

Quá trình đóng gói tự động đã hoàn thành xuất sắc! 
*   **File cài đặt đã tạo:** `packaging\windows\installer_build\CentralLoggerSetup.exe` (trong repo)
*   **Dung lượng bộ cài:** **~114 MB** (Đã nén toàn bộ exe, DLLs, QML modules và compiler runtimes).
*   **Môi trường độc lập:** Bộ cài đặt này chứa đầy đủ các thư viện runtime (MinGW GCC, OpenSSL, SQLite Driver, Qt Quick, Qt Graphs...) nên có thể cài đặt và chạy trên bất kỳ máy Windows sạch nào mà không cần cài đặt Qt hay Qt Creator.

---

## 2. Cấu Trúc Thư Mục Đóng Gói (`installer_build/`)

Bộ khung đóng gói được xây dựng theo chuẩn của **Qt Installer Framework**:

```text
packaging\windows\installer_build\
├── config\
│   ├── config.xml                 # Cấu hình chung của trình cài đặt (Tên, Phiên bản, Thư mục cài mặc định)
│   ├── style.qss                  # Stylesheet giao diện cài đặt (Khắc phục lỗi hiển thị Light/Dark)
│   ├── logo.png                   # Ảnh logo thương hiệu hiển thị trong wizard cài đặt
│   ├── window_icon.png            # Icon hiển thị trên thanh tiêu đề cửa sổ cài đặt
│   └── app_icon.ico               # Icon file chạy của bộ cài đặt CentralLoggerSetup.exe
├── packages\
│   └── com.central_logger.app\    # Định danh gói ứng dụng chính
│       ├── meta\
│       │   ├── package.xml        # Meta-data của gói (Mô tả, Ngày phát hành, Phiên bản)
│       │   └── installscript.qs   # Script JavaScript điều khiển việc tạo shortcut trên Desktop/Start Menu
│       └── data\                  # Toàn bộ file chạy đã deploy (EXE, DLLs, QML, Plugins) - Được nén vào Setup
└── CentralLoggerSetup.exe         # FILE CÀI ĐẶT CUỐI CÙNG (Đầu ra)
```

---

## 3. Chi Tiết Các File Cấu Hình Trình Cài Đặt

### A. File Cấu Hình Trình Cài Đặt: `config/config.xml`
File này quản lý các thông tin hiển thị chính khi người dùng chạy file Setup:
*   `Name` / `Title`: Tên chương trình hiển thị là **Central Logger**.
*   `Publisher`: **4M Technologies**.
*   `TargetDir`: Thư mục cài đặt mặc định trên máy người dùng (thường là `C:\CentralLogger`).
*   `StartMenuDir`: Tên nhóm thư mục trong Start Menu là **Central Logger**.
*   `StyleSheet`: Liên kết đến stylesheet `style.qss` để tạo giao diện tùy chỉnh.
*   `WizardStyle`: Thiết lập thành `Modern` để tạo giao diện cài đặt có thanh sidebar bên trái trực quan.
*   `InstallerApplicationIcon`: Trỏ đến icon file chạy `app_icon` (không kèm đuôi `.ico`).
*   `InstallerWindowIcon`: Đường dẫn đến tệp icon cửa sổ `window_icon.png`.
*   `Logo`: Đường dẫn đến tệp ảnh logo thương hiệu `logo.png` hiển thị góc trên cùng.

> [!NOTE]
> **Nguồn Gốc Logo & Icon 4M Technologies Gốc:**
> Để bảo toàn 100% độ sắc nét và tính nguyên bản của thương hiệu, các tệp ảnh thương hiệu trên (`app_icon.ico`, `logo.png`, `window_icon.png`) được sinh tự động trực tiếp từ tệp thiết kế vector gốc **`resources/icons/brand_4m_technologies_blue.svg`** bằng công cụ biên dịch Qt C++ trong **`packaging/windows/convert_tool/`**.
> *   `app_icon.ico`: Được biên dịch thành định dạng icon Windows đa độ phân giải tối đa (256x256 pixel) giúp hiển thị cực nét ngoài Desktop và trong File Explorer mà không bị vỡ ảnh.
> *   `logo.png` & `window_icon.png`: Định dạng PNG nén chất lượng cao (512x512 pixel) có nền trong suốt (Alpha channel) giúp hòa hợp hoàn hảo với giao diện nền tối của bộ cài đặt.

### B. File Descriptor Gói: `packages/com.central_logger.app/meta/package.xml`
File mô tả thông tin gói phần mềm để trình cài đặt nhận diện và hiển thị trong danh sách thành phần cài đặt:
*   Mặc định là gói bắt buộc cài đặt (`Default: true`).
*   Phiên bản cài đặt hiện tại là `1.0.0`.
*   Liên kết tới script bổ trợ `installscript.qs`.

### C. Script Tạo Shortcut: `packages/com.central_logger.app/meta/installscript.qs`
Script bổ sung hoạt động đăng ký hệ thống sau khi giải nén hoàn tất:
*   **Tạo Start Menu Shortcut:** Tạo lối tắt `Central Logger.lnk` trỏ đến `central_logger.exe` trong Start Menu.
*   **Tạo Desktop Shortcut:** Tạo lối tắt ngoài màn hình chính để người dùng mở ứng dụng nhanh chóng.
*   **Cấu hình workingDirectory:** Thiết lập thư mục làm việc chính xác giúp ứng dụng tìm thấy SQLite database local và các tài nguyên đi kèm mà không gặp lỗi đường dẫn.

### D. Stylesheet Giao Diện: `config/style.qss`
Đây là bộ stylesheet dạng CSS dành cho Qt Widgets để kiểm soát toàn diện màu sắc giao diện cài đặt:
*   **Khắc phục triệt để lỗi Light/Dark mode lai:** Bằng cách thiết lập màu nền tối cụ thể (`#121212`), màu chữ sáng rõ ràng (`#e3e3e3`), và màu nhấn Teal (`#008080`) đồng bộ với giao diện ứng dụng.
*   **Tùy biến thanh Sidebar:** Đổi màu nền của thanh bên trái thành màu xám tối đậm (`#1c1c1c`), làm nổi bật bước hiện tại bằng dải màu Teal ở mép trái.
*   **Làm đẹp các nút bấm và thanh tiến trình:** Các nút bấm `Next`, `Back`, `Cancel` và thanh chạy phần trăm (`QProgressBar`) được bo góc mềm mại và đổi màu mượt mà khi hover.

---

## 4. Tự Động Hóa Trong Tương Lai: `build_installer.ps1`

Script PowerShell tự động hóa toàn bộ quy trình: [`packaging/windows/build_installer.ps1`](build_installer.ps1).

### Quy trình tự động hóa của script:
1.  **Kiểm tra môi trường:** Đảm bảo có sẵn file thực thi Release `central_logger.exe`, các công cụ `windeployqt.exe` và `binarycreator.exe`.
2.  **Khởi tạo & Dọn dẹp:** Dọn sạch các tệp thừa cũ trong thư mục tạm `build/deploy` và `installer_build/packages/com.central_logger.app/data`.
3.  **Thu thập thư viện (`windeployqt`):** Chạy công cụ deploy của Qt quét mã nguồn QML trong `src/` để tự động kéo tất cả các module Qt Quick, Qt Graphs và DLL phụ thuộc cần thiết, đồng thời đính kèm bộ thư viện compiler runtime của MinGW (`libstdc++-6.dll`, `libgcc_s_seh-1.dll`...) vào gói.
4.  **Chuyển dữ liệu:** Sao chép toàn bộ thư mục deploy vào thư mục `data/` của gói cài đặt.
5.  **Biên dịch Installer:** Gọi `binarycreator.exe` đóng gói và xuất ra file cài đặt `CentralLoggerSetup.exe` mới.

### Cách chạy script đóng gói:

1.  **Mở Qt Creator**, chọn chế độ **Release** và nhấn **Build** (hoặc Ctrl+B) để biên dịch ứng dụng mới nhất.
2.  Mở PowerShell tại thư mục gốc repo.
3.  Chạy lệnh sau:
    ```powershell
    .\packaging\windows\build_installer.ps1
    ```
4.  Đợi khoảng 15–20 giây, bộ cài `CentralLoggerSetup.exe` sẽ nằm trong `packaging\windows\installer_build\`.

---

## 5. Hướng Dẫn Cài Đặt Trực Quan Cho Người Dùng Cuối

Khi bạn gửi file `CentralLoggerSetup.exe` cho khách hàng hoặc chạy thử nghiệm:

1.  **Khởi chạy Setup:** Click đúp vào `CentralLoggerSetup.exe`. Trình cài đặt mang thương hiệu **4M Technologies** sẽ hiện ra.
2.  **Chọn thư mục cài đặt:** Mặc định trình cài đặt sẽ đề xuất `C:\CentralLogger`. Người dùng có thể nhấn Browse để đổi nếu muốn.
3.  **Xác nhận cài đặt:** Nhấn **Next** và **Install**. Chương trình sẽ tự động giải nén tất cả thư viện Qt, QML plugins và tệp chạy.
4.  **Hoàn tất:** Trình cài đặt sẽ tự động tạo Shortcut ngoài màn hình chính (Desktop) và trong Start Menu. Người dùng chỉ cần click đúp vào shortcut là có thể chạy ứng dụng ngay lập tức mà không cần bất kỳ thao tác cấu hình phức tạp nào khác.
