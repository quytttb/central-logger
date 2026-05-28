# **Báo Cáo Nghiên Cứu Chiến Lược Áp Dụng Material Design 3 Cho Giao Diện Desktop Central Logger Với Qt 6.11**

## **Tóm tắt Điều hành (Executive Summary)**

Việc chuyển đổi và chuẩn hóa toàn bộ giao diện người dùng (UI) của ứng dụng desktop Central Logger sang hệ thống Material Design 3 (M3) trên nền tảng Qt 6.11 đòi hỏi một chiến lược kiến trúc kết hợp chặt chẽ giữa các tiêu chuẩn thiết kế hiện đại và các ràng buộc kỹ thuật đặc thù của Qt Quick Controls. Thông qua việc phân tích chuyên sâu mã nguồn hiện tại của Central Logger và đối chiếu với các nguyên tắc cốt lõi của Google Material Design 3, báo cáo này xác định rằng Qt 6.11 cung cấp sự hỗ trợ gốc (native support) mạnh mẽ cho M3 thông qua Material Style.1 Các API mới được giới thiệu từ Qt 6.5, đặc biệt là các thuộc tính đính kèm (attached properties) như Material.containerStyle và Material.roundedScale, cho phép các nhà phát triển điều chỉnh hình thái của các thành phần giao diện một cách linh hoạt mà không cần phải viết lại toàn bộ mã nguồn render.1 Tuy nhiên, triển khai mặc định của Qt chỉ giới hạn ở mức độ các thành phần cơ bản (components) và quản lý một tập hợp hẹp các vai trò màu sắc (color roles) như primary, accent, background, và foreground.4 Sự giới hạn này tạo ra một khoảng trống (gap) đáng kể so với hệ thống 26 vai trò màu sắc phức tạp của M3, vốn được thiết kế để đảm bảo tỷ lệ tương phản tối ưu và khả năng thích ứng linh hoạt giữa các chế độ sáng/tối.5  
Do hệ thống Central Logger không sử dụng tính năng màu sắc động (dynamic color / Material You) và bị ràng buộc nghiêm ngặt bởi một bảng màu cố định (Teal cho Primary, Indigo cho Accent), chiến lược tối ưu nhất để đạt được độ chính xác của M3 mà vẫn giữ mức độ thay đổi mã nguồn (diff) ở mức tối thiểu là mở rộng module CentralLogger.Theme hiện có. Báo cáo này đặc biệt nhấn mạnh khuyến nghị **không sử dụng** các thư viện thiết kế của bên thứ ba (như Kirigami của KDE hay các gói M3 QML pack mã nguồn mở bên ngoài). Việc đưa thêm các thư viện này không chỉ đi ngược lại mục tiêu giữ diff tối thiểu mà còn có nguy cơ phá vỡ kiến trúc Model-View-ViewModel (MVVM) thuần túy đang được duy trì, trong đó toàn bộ logic nghiệp vụ và trạng thái giao diện được quản lý tại tầng C++ (ví dụ: SettingsController), và ngăn chặn sự phình to không cần thiết của các phụ thuộc (dependencies) trong dự án.  
Bằng cách xây dựng hệ thống token tĩnh cho màu sắc, kiểu chữ (typography) và hình khối (shape) trực tiếp vào các tệp singleton hiện có là AppTheme và AppColors, hệ thống có thể mô phỏng chính xác hơn 26 vai trò màu sắc của M3 thông qua phương pháp "xấp xỉ hóa Qt" (Qt approximation).5 Hơn nữa, việc giữ nguyên các quyết định về Trải nghiệm Người dùng (UX) và Giao diện Người dùng (UI) đã được phê duyệt trong phạm vi MVP—bao gồm Navigation Rail cố định ở mức rộng 80dp (một sự điều chỉnh có chủ ý từ tiêu chuẩn M3 Expressive 96dp để phù hợp với ngữ cảnh ứng dụng dữ liệu mật độ cao) 7 và Top Bar 80dp—sẽ đảm bảo tính nhất quán của không gian làm việc desktop mà không vi phạm các nguyên tắc về mật độ hiển thị (density) của Material Design dành cho màn hình lớn. Báo cáo nghiên cứu này sẽ trình bày một cách tường minh và chi tiết phân tích khoảng trống, đặc tả hệ thống thiết kế mở rộng, hướng dẫn triển khai từng thành phần giao diện, và một lộ trình chuyển đổi rủi ro thấp nhằm tích hợp hoàn toàn Material Design 3 vào Central Logger.

## **1\. Khả thi Qt 6.11 vs Material Design 3 và Nền tảng Kiến trúc**

### **1.1. Năng lực và Giới hạn của Qt Quick Controls Material Style**

Bắt đầu từ phiên bản 6.5, Qt Quick Controls đã thực hiện một cuộc đại tu lớn đối với Material Style, chuyển đổi hoàn toàn từ các nguyên tắc thiết kế của Material Design 2 sang Material Design 3\.1 Trong bối cảnh kỹ thuật của Qt 6.11, style này được khởi tạo thông qua mã C++ QQuickStyle::setStyle("Material") trong file ThemeSetup.cpp.9 Động thái này đảm bảo rằng tất cả các control tiêu chuẩn của Qt (như Button, TextField, ComboBox, Pane) tự động kế thừa các đặc điểm nhận dạng cốt lõi của M3, bao gồm việc loại bỏ bóng đổ (elevation drop shadows) nặng nề của thế hệ trước và thay thế bằng các container dựa trên tông màu (tone-based surface colors).6  
Khả năng đáp ứng của Qt 6.11 đối với M3 thể hiện rõ nhất qua hai thuộc tính đính kèm mới. Thứ nhất là Material.containerStyle, cho phép chuyển đổi mượt mà giữa các kiểu dáng hiển thị của trường nhập liệu (ví dụ: từ Filled sang Outlined), một yêu cầu quan trọng của M3 đối với các biểu mẫu trên desktop.1 Thứ hai là Material.roundedScale, cung cấp một hệ thống phân cấp các góc bo tròn (từ ExtraSmallScale đến ExtraLargeScale và FullScale), cho phép các thành phần như Dialog và Card tuân thủ nghiêm ngặt các quy tắc hình khối (shape tokens) của M3.1  
Tuy nhiên, giới hạn lớn nhất của Qt 6.11 nằm ở hệ thống màu sắc. API của Qt chỉ map trực tiếp khoảng 30% các vai trò màu M3. Cụ thể, các thuộc tính đính kèm chỉ hỗ trợ cấu hình Material.primary (ánh xạ tới Primary), Material.accent (thường ánh xạ tới Tertiary hoặc Secondary tùy ngữ cảnh), Material.background (ánh xạ tới Surface), và Material.foreground (ánh xạ tới On Surface).4 Sự thiếu hụt này lộ rõ khi ứng dụng cần triển khai các mẫu thiết kế phức tạp hơn. M3 định nghĩa 26 vai trò màu sắc phân biệt 5, bao gồm một hệ thống phân cấp bề mặt từ surfaceContainerLowest đến surfaceContainerHighest, các biến thể đường viền như outline và outlineVariant dùng cho các bảng dữ liệu (Data Tables), cũng như các màu sắc ngữ nghĩa (semantic colors) cho trạng thái lỗi (error, errorContainer, onErrorContainer). Tất cả những vai trò mở rộng này đều **không có sẵn** dưới dạng thuộc tính đính kèm của Qt Quick Controls và do đó, bắt buộc phải được định nghĩa thủ công và bổ sung vào cấu trúc AppColors / AppTheme của dự án.

### **1.2. Chiến lược Vượt qua Giới hạn bằng Phương pháp "Xấp xỉ hóa Qt" (Qt Approximation)**

Để giải quyết sự thiếu hụt các token màu sắc mà không phá vỡ kiến trúc cốt lõi của Qt Quick Controls, chiến lược được đề xuất là áp dụng phương pháp nội suy màu (color derivation) dựa trên độ trong suốt (opacity). Phương pháp này sẽ được cài đặt trực tiếp trong tệp AppColors.qml. Trong codebase hiện tại, việc sử dụng hàm AppColors.secondaryContainer(Material.accent, opacity) là một bước đi đúng hướng, thể hiện một sự "xấp xỉ hóa của Qt" hợp lý. Tuy nhiên, logic này cần được tiêu chuẩn hóa để đảm bảo tỷ lệ tương phản tuân thủ Tiêu chuẩn Tiếp cận Nội dung Web (WCAG).  
Nguyên lý cốt lõi của M3 là tạo ra các container nhạt màu cho các trạng thái kích hoạt (active) hoặc lướt chuột (hover) bằng cách hòa trộn màu nhấn (accent) hoặc màu chính (primary) với màu nền (surface) theo các tỷ lệ nhất định.5 Trong môi trường QML, việc này có thể được mô phỏng chính xác bằng cách áp dụng kênh alpha (ví dụ: opacity ở mức 12% hoặc 16%) lên mã màu cơ sở, sau đó để bộ máy kết xuất đồ họa (render engine) của Qt tự động hòa trộn nó với nền Surface bên dưới. Đối với màu văn bản và biểu tượng nằm trên các container này, M3 yêu cầu sử dụng các màu "On" tương ứng (như onSecondaryContainer), vốn có độ tương phản rất cao. Vì bảng màu của dự án là cố định (không dùng tính năng trích xuất động HCT), chúng ta có thể thiết lập hàm nội suy tĩnh: tạo ra màu onSecondaryContainer bằng cách làm tối đi (đối với giao diện sáng) hoặc làm sáng lên (đối với giao diện tối) màu Material.accent ban đầu thông qua các hàm tiện ích của Qt như Qt.lighter() và Qt.darker(). Cách tiếp cận này đáp ứng các yêu cầu kiểm tra WCAG AA (tỷ lệ tương phản 4.5:1 đối với văn bản) mà không yêu cầu phải hardcode hàng loạt mã Hex tĩnh gây khó khăn cho việc bảo trì.

### **1.3. Khuyến nghị Phân định Biên giới Kiến trúc và Loại trừ Thư viện Bên thứ ba**

Dự án Central Logger đang duy trì một kiến trúc MVVM nghiêm ngặt, trong đó toàn bộ logic nghiệp vụ (business logic) được khóa chặt tại tầng C++, và QML chỉ đóng vai trò là tầng hiển thị (View layer) phản ứng với các thay đổi thông qua cơ chế property bindings. Việc giữ gìn tính toàn vẹn của biên giới kiến trúc này là tối quan trọng. Trong quá trình nghiên cứu, câu hỏi về việc có nên sử dụng các thư viện bên thứ ba như Kirigami hay các gói M3 QML pack (như QmlMaterial 10) đã được đặt ra.  
Báo cáo này khuyến nghị **từ chối hoàn toàn** việc đưa bất kỳ thư viện UI bên thứ ba nào vào dự án. Nguyên nhân thứ nhất là vấn đề về dung lượng phần mềm (binary bloat) và quản lý phụ thuộc (dependency management); việc chèn thêm một framework UI cồng kềnh chỉ để giải quyết vài vấn đề về màu sắc là không hiệu quả. Nguyên nhân thứ hai và quan trọng nhất là các thư viện này thường áp đặt mô hình khởi tạo theme (theme bootstrapping) riêng của chúng, điều này sẽ xung đột trực tiếp với phương pháp CentralLogger::Theme::applyQuickControlsStyle() đang vận hành ổn định trong file ThemeSetup.cpp. Thêm vào đó, việc sử dụng các thành phần (components) từ thư viện ngoài thường kéo theo các đoạn mã logic Javascript nhúng lặp lại (inline JS logic) gây ô nhiễm tầng View, vi phạm trực tiếp ràng buộc "không logic nghiệp vụ trong QML". Việc mở rộng module CentralLogger.Theme nội bộ cung cấp quyền kiểm soát 100% đối với các độ đo (metrics), màu sắc, và logic render, bảo vệ cấu trúc MVVM và giữ nguyên phương thức tương tác tĩnh (REST/Modbus) đã được thiết lập.

## **2\. Phân tích Khoảng trống (Gap Analysis)**

Quá trình kiểm tra (audit) chuyên sâu mã nguồn hiện tại của Central Logger—dựa trên các tệp trong module Theme, Components và App—đã bộc lộ những khoảng cách cụ thể giữa trạng thái UI hiện có, thông số kỹ thuật M3 tiêu chuẩn do Google ban hành, và các giải pháp đề xuất. Bảng phân tích dưới đây lập bản đồ chi tiết các điểm mù thiết kế và đề xuất lộ trình khắc phục cho từng tệp cụ thể.

| Thành phần & Tệp tin | Trạng thái Hiện tại (As-is) | Thông số Kỹ thuật M3 (To-be) | Đề xuất & Xấp xỉ hóa Qt (Qt Approximation) |
| :---- | :---- | :---- | :---- |
| **Hệ thống Màu sắc** AppTheme.qml, AppColors.qml | Dựa vào hàm Material.color() rải rác trong mã; các hàm opacity helpers cơ bản (như onSurfaceVariant, hover) đã tồn tại nhưng chưa có cấu trúc phân cấp. | Yêu cầu 26+ vai trò màu (roles) phân biệt hỗ trợ kiểm soát độ tương phản; phân cấp hệ thống nền Surface thành 5 mức độ rõ rệt.5 | Cập nhật AppColors với các hằng số màu ngữ nghĩa (error, warning) tĩnh cho hai chế độ Light/Dark. Giữ nguyên các hàm opacity helpers nhưng chuẩn hóa hệ số alpha theo M3 state layers. |
| **Hệ thống Kiểu chữ** Phân tán trong các \*View.qml | Hardcode các thuộc tính font.pixelSize và font.weight rải rác khắp các màn hình, gây thiếu tính đồng bộ và khó bảo trì. | Hệ thống Typography yêu cầu 15 tỷ lệ kích thước chia theo 5 nhóm (Display, Headline, Title, Body, Label) ở mức L/M/S.11 | Tạo mới tệp AppTypography.qml dưới dạng singleton. Ánh xạ các tỷ lệ M3 (vd: Title Large \= 22sp, Body Medium \= 14sp) sang các giá trị font.pixelSize tương ứng của Qt.13 |
| **Navigation Rail** AppNavigationRail.qml, NavItem.qml | Chiều rộng tổng 80dp, pill trạng thái active kích thước 56x32, sử dụng AppColors.secondaryContainer, label không in đậm (đúng chuẩn). | Thiết kế M3 tiêu chuẩn cũ: 80dp. Thiết kế M3 Expressive mới: mở rộng lên 96dp.7 Icon ở trạng thái active bắt buộc dùng màu onSecondaryContainer. | Cương quyết giữ nguyên chiều rộng **80dp** (bỏ qua bản cập nhật 96dp để bảo vệ không gian làm việc desktop). Cập nhật màu của icon khi active thành màu Indigo đậm hơn (onSecondaryContainer). |
| **Thanh Công cụ (Top Bar)** AppTopBar.qml, inline \*TopBar | Chiều cao hardcode **80dp**, padding ngang **24dp**, hiện thiếu hệ thống quản lý tiêu đề trang đồng nhất toàn cục. | Cấu trúc Medium top app bar quy định chiều cao 64dp 14, với padding ngang thường dao động từ 16-24dp. | Giữ nguyên chiều cao **80dp** (lý do: tạo vùng touch target lớn hơn và cân bằng đối xứng với logo header của Rail). Đẩy logic cấu trúc Toolbar dùng chung vào module Components. |
| **Thẻ Thống kê (Stat Cards)** DashboardView.qml | Viết dưới dạng mã inline dùng Pane với thuộc tính elevation: 1 mang phong cách bóng đổ của MD2. | Hỗ trợ 3 kiểu Card: Filled, Outlined, hoặc Elevated. Elevated card sử dụng vùng đổ bóng (shadow) tiêu chuẩn M3 và nền surfaceContainerLow.5 | Đưa logic StatCard ra thành một component riêng biệt kế thừa từ Pane. Đặt Material.elevation: 1 và thiết lập màu nền thành surfaceContainerLow thay vì nền mặc định. |
| **Bảng Dữ liệu (Data Tables)** LoggersView.qml, LoggerDetailView.qml | Sử dụng Custom grid tự xây dựng, logic cho đường phân cách (divider) và trạng thái hover bị lặp lại nhiều nơi; hành động chính dùng FAB màu nhấn đậm của MD2. | Yêu cầu mật độ dữ liệu có thể tinh chỉnh, sử dụng đường phân cách màu outlineVariant, phần header nổi bật. FAB không nên lấn át không gian dữ liệu. | Rút trích (Extract) Table Header và Table Row thành các shared components. Bỏ kiểu accent-fill cho nút FAB, chuyển sang kiểu Tonal hoặc Outlined Button. Cập nhật divider sử dụng màu outlineVariant. |
| **Biểu mẫu (Forms / Inputs)** SettingsView.qml, LoggerFormDialog.qml | Các control như TextField và ComboBox nằm trực tiếp trong Pane với giao diện Filled mặc định của hệ thống. | TextField yêu cầu phân biệt rõ kiểu Filled hoặc Outlined container.1 M3 khuyến nghị Outlined cho desktop form để dễ nhận diện. | Khai báo toàn cục Material.containerStyle: Material.Outlined 2 cho các inputs trong Form. Các góc bo tròn cần được tiêu chuẩn hóa bằng Material.SmallScale.1 |
| **Chuyển đổi Giao diện (Theme Toggle)** ThemeToggle.qml | Có tới 2 điểm truy cập: trên Navigation Rail và trong Settings form. Toggle trên Rail là một outlined circle 40dp. | Nút chuyển đổi (toggle) nên là Icon Button chuẩn trên rail hoặc dạng segmented button nằm trong form. Không nên phân tán điểm thao tác. | Hợp nhất hành vi thao tác. Khuyến nghị duy trì duy nhất ThemeToggle trên Rail dưới dạng IconButton (kích thước 40dp hoặc 48dp) với trạng thái tương tác hover chuẩn mực. |
| **Trực quan hóa (Charts)** DashboardView.qml | Đang sử dụng thư viện Qt Graphs với bộ màu sắc (chrome) mặc định, chưa liên kết với cấu trúc theme hiện tại. | Không có thông số (spec) thiết kế biểu đồ M3 chuẩn hóa, nhưng màu sắc (chrome) bao quanh biểu đồ bắt buộc phải tuân thủ bảng màu theme.6 | Map thông số của AppTheme sang cấu hình GraphsTheme.15 Gán colorScheme (Light/Dark) dựa trên trạng thái của SettingsController.theme. |

## **3\. Đặc tả Hệ thống Thiết kế (Design System Specification)**

Bản lề của chiến lược áp dụng M3 cho Central Logger chính là việc cấu trúc lại và mở rộng module CentralLogger.Theme. Ràng buộc từ yêu cầu hệ thống chỉ ra rằng tuyệt đối không được tạo thêm một tập hợp theme song song nào mang tên M3Theme để tránh phân mảnh; do đó, mọi token mới phải được nhúng trực tiếp vào các singleton AppTheme và AppColors. Cần đặc biệt lưu ý rằng, trong ngữ cảnh phát triển cho Desktop (hỗ trợ Linux và Windows) trên nền Qt, 1 dp (density-independent pixel) trong đặc tả thiết kế của Google được diễn dịch tương đương với 1 pixel logic của Qt, giả định rằng cơ chế High-DPI scaling mặc định của Qt đã được kích hoạt trong main.cpp.

### **3.1. Cấu trúc Khối AppTheme và Hệ thống Màu sắc Cố định**

Triết lý của M3 yêu cầu một sự phân tách cực kỳ rõ ràng giữa các tông màu (tonal palettes) để phục vụ cho các chế độ ánh sáng khác nhau.6 Vì bảng màu gốc của ứng dụng đã được chốt (sử dụng Teal và Indigo làm chủ đạo), các màu này sẽ được định nghĩa là hằng số tuyệt đối. Thay vì trích xuất màu sắc động từ hình nền (dynamic color extraction) phức tạp, hệ thống sẽ sử dụng các phép toán tính toán màu sắc thông minh để sinh ra các biến thể bề mặt (surface variants).  
**Định nghĩa Phân bổ Màu cơ sở (Base Colors Allocation)**:

* **Màu Chính (Primary \- Teal)**: Vai trò này sẽ được chỉ định độc quyền cho các hành động quan trọng nhất (Primary actions) trên màn hình, ví dụ như các nút lưu dữ liệu (Save buttons), nút xác nhận trong Dialog, hoặc các FAB chính (nếu có). Việc sử dụng Material.Teal (xấp xỉ mã Hex \#006D77 hoặc biến thể tương đương theo bảng màu nội tại của Qt 17) mang lại cảm giác công nghiệp, đáng tin cậy.  
* **Màu Nhấn/Phụ (Accent/Secondary \- Indigo)**: Vai trò này được sử dụng để làm màu đánh dấu (highlight), biểu thị các trạng thái đang hoạt động (active state) như nền Pill trên Navigation Rail, hoặc các tab đang được chọn. Việc sử dụng Material.Indigo (xấp xỉ \#3F51B5 4) tạo ra một sự tương phản lạnh, sắc nét so với nền Teal, giúp phân luồng thị giác rõ ràng.

Dưới đây là đặc tả kỹ thuật dưới dạng pseudo-QML, được thiết kế để có thể sao chép trực tiếp vào tệp AppColors.qml. Cấu trúc này tuân thủ tuyệt đối triết lý MVVM (không chứa hàm JS mang tính nghiệp vụ) và tận dụng các helper của Qt:

QML  
pragma Singleton  
import QtQuick  
import QtQuick.Controls.Material

QtObject {  
    id: appColors

    // \=====================================================================  
    // HỆ SỐ TRONG SUỐT CHO TRẠNG THÁI TƯƠNG TÁC (INTERACTION STATE LAYERS)  
    // Chuẩn hóa nghiêm ngặt theo thông số kỹ thuật của M3 Interaction States  
    // \=====================================================================  
    readonly property real hoverOpacity: 0.08  
    readonly property real focusOpacity: 0.12  
    readonly property real pressedOpacity: 0.12  
    readonly property real draggedOpacity: 0.16  
    readonly property real disabledOpacity: 0.38

    // \=====================================================================  
    // HÀM TÍNH TOÁN MÀU TRẠNG THÁI (COMPUTED STATE COLORS \- QT APPROXIMATION)  
    // Sử dụng rộng rãi cho Navigation Rail Pill và các nút Tonal (Tonal Buttons)  
    // \=====================================================================  
    function secondaryContainer(baseColor, isDark) {  
        // Nguyên tắc M3: hòa trộn màu nhấn với nền Surface.  
        // Xấp xỉ hóa trong Qt: áp dụng alpha blending (16%) trực tiếp lên mã màu cơ sở.  
        return Qt.rgba(baseColor.r, baseColor.g, baseColor.b, 0.16)  
    }

    function onSecondaryContainer(baseColor, isDark) {  
        // Yêu cầu M3: Đảm bảo độ tương phản cao cho text/icon nằm trên secondaryContainer.  
        // Giải pháp Qt: Trích xuất màu bằng cách làm tối đi (Light mode) hoặc làm sáng lên (Dark mode) màu nhấn gốc.  
        return isDark? Qt.lighter(baseColor, 1.4) : Qt.darker(baseColor, 1.4)  
    }

    // \=====================================================================  
    // TOKEN BỀ MẶT VÀ ĐƯỜNG VIỀN M3 (SURFACE & OUTLINE TOKENS)  
    // Giải quyết triệt để sự thiếu hụt các properties này trong module Material gốc  
    // \=====================================================================  
    property color surfaceContainerLowest: Material.theme \=== Material.Dark? "\#0F0F0F" : "\#FFFFFF"  
    property color surfaceContainerLow: Material.theme \=== Material.Dark? "\#1C1C1C" : "\#F7F8F8"  
    property color surfaceContainer: Material.theme \=== Material.Dark? "\#212121" : "\#F3F4F4"  
    property color surfaceContainerHigh: Material.theme \=== Material.Dark? "\#2B2B2B" : "\#EDEDED"  
    property color surfaceContainerHighest: Material.theme \=== Material.Dark? "\#363636" : "\#E5E6E6"

    property color outline: Material.theme \=== Material.Dark? "\#938F99" : "\#79747E"  
    // outlineVariant đặc biệt quan trọng để render các đường phân cách (dividers) mỏng trong Data Table  
    property color outlineVariant: Material.theme \=== Material.Dark? "\#49454F" : "\#CAC4D0" 

    // \=====================================================================  
    // MÀU SẮC NGỮ NGHĨA VÀ TRẠNG THÁI HỆ THỐNG (SEMANTIC / STATUS COLORS)  
    // Dùng để biểu thị trạng thái kết nối Modbus (OK/ALARM/STALE) và cấp độ Sự kiện  
    // \=====================================================================  
    property color error: Material.theme \=== Material.Dark? "\#FFB4AB" : "\#BA1A1A"  
    property color errorContainer: Material.theme \=== Material.Dark? "\#93000A" : "\#FFDAD6"  
    property color onErrorContainer: Material.theme \=== Material.Dark? "\#FFDAD6" : "\#410002"

    property color warning: Material.theme \=== Material.Dark? "\#FFD270" : "\#B38C00"  
    property color warningContainer: Material.theme \=== Material.Dark? "\#8A6A00" : "\#FFEA79"

    // Sử dụng màu Success cho các trạng thái kết nối logger "OK"  
    property color success: Material.theme \=== Material.Dark? "\#81C784" : "\#2E7D32"   
    property color successContainer: Material.theme \=== Material.Dark? "\#1B5E20" : "\#C8E6C9"  
}

### **3.2. Hệ thống Kiểu chữ và Hình khối (Typography & Shape Tokens)**

Sự tùy tiện trong việc gán cứng (hardcode) kích thước chữ là một trong những rào cản lớn nhất đối với một thiết kế mạch lạc. Báo cáo này đặc biệt đề xuất việc tạo ra tệp AppTypography.qml dưới dạng một singleton toàn cục để loại bỏ vĩnh viễn thói quen này. Bằng cách nghiên cứu các hướng dẫn kiểu chữ của M3, hệ thống sẽ sử dụng 5 vai trò chính với các mức độ kích thước (L/M/S).11 Bảng dưới đây cung cấp phép ánh xạ chính xác từ M3 sang bộ hằng số Qt, giúp các developer dễ dàng áp dụng:

| Tên Token (Vai trò M3) | Kích thước (dp/px) tương đương Qt | Độ đậm (Weight) Qt QML | Vị trí Khuyến nghị Ứng dụng trong Central Logger |
| :---- | :---- | :---- | :---- |
| **Display Small** | 36px | Font.Normal (400) | Sử dụng cho các số liệu thống kê lớn, mang tính trình diễn tại màn hình DashboardView. |
| **Headline Small** | 24px | Font.Normal (400) | Định dạng tiêu đề chính của các cửa sổ pop-up hoặc Dialog (LoggerFormDialog). |
| **Title Large** | 22px | Font.Medium (500) | Được sử dụng duy nhất cho các dòng tiêu đề trang nằm trên thanh \*TopBar. |
| **Title Medium** | 16px | Font.Medium (500) | Cấu trúc tiêu đề cột cho các bảng Data Table, hoặc tên của các mục lớn trong danh sách. |
| **Body Large** | 16px | Font.Normal (400) | Cấu trúc văn bản dài, đoạn nội dung giải thích chi tiết nằm trong nội dung Dialog. |
| **Body Medium** | 14px | Font.Normal (400) | Chuẩn hiển thị Dữ liệu tiêu chuẩn trong các ô (cells) của LoggersView (Data Table). |
| **Label Large** | 14px | Font.Medium (500) | Định dạng văn bản hiển thị trên các nút bấm (Button), và trên Navigation Rail khi ở trạng thái Active. |
| **Label Medium** | 12px | Font.Medium (500) | Hiển thị nhãn trên Navigation Rail khi ở trạng thái Inactive, hoặc định dạng mốc thời gian (Timestamps). |

**Cấu hình Hình khối (Shape Specifications)**: Từ Qt 6.5 trở đi, thuộc tính Material.roundedScale đã được tích hợp sâu vào hệ thống render.1 Để mô phỏng độ cong hoàn hảo của M3, báo cáo chỉ định các giá trị sau cho từng trường hợp sử dụng:

* **Card / Pane**: Áp dụng Material.MediumScale để tạo góc bo tròn 12dp, tạo cảm giác bề mặt nổi nhẹ nhàng.  
* **Dialog**: Bắt buộc áp dụng Material.ExtraLargeScale để kích hoạt góc bo 28dp, tuân thủ nghiêm ngặt định nghĩa thành phần Dialog của M3.  
* **Button**: Duy trì giá trị mặc định của hệ thống Qt (Material.FullScale), tạo ra nút bấm hình viên thuốc (pill-shaped) cho các tương tác bấm.

## **4\. Hướng dẫn Triển khai Thành phần Cụ thể (Component Guidelines)**

Bằng cách phân tích chuyên sâu các khu vực chức năng chính của Central Logger, phần này kết nối trực tiếp thông số kỹ thuật thiết kế của M3 với các biện pháp thực thi kỹ thuật trên Qt 6.11 QML, đảm bảo rằng mã nguồn sinh ra vừa chính xác về mặt hình ảnh, vừa tối ưu về hiệu suất render.

### **4.1. Hệ thống Shell & Thanh Điều hướng (Navigation Rail)**

Khối kiến trúc phân chia giao diện thông qua SplitView cần thiết lập một hệ thống điều hướng bền vững và không chiếm quá nhiều diện tích hiển thị của bảng dữ liệu. Quá trình kiểm tra file AppNavigationRail.qml và NavItem.qml cho thấy cần thực hiện các tùy chỉnh sau:

* **Ràng buộc về Kích thước Tổng thể**: Khuyến nghị giữ nguyên chiều rộng của rail ở mức **80dp**. Mặc dù bản cập nhật M3 Expressive gần đây của Google đề xuất mở rộng Navigation Rail lên 96dp cho các màn hình có cửa sổ lớn (large window size classes) 7, việc giữ lại mốc 80dp là một quyết định kiến trúc có chủ ý. Trong bối cảnh ứng dụng Central Logger hoạt động như một hệ thống quản lý công nghiệp chuyên dụng, không gian theo chiều ngang là tài nguyên cực kỳ quý giá dành cho các Data Table phức tạp chứa hàng chục cột thông số từ giao thức Modbus. Chiều rộng 80dp hoàn toàn đáp ứng được yêu cầu về kích thước mục tiêu chạm (touch target size) của M3 baseline 19 mà không gây lãng phí không gian.  
* **Cấu trúc Khối Chỉ thị (Pill Indicator) trong NavItem.qml**: Kích thước vùng đánh dấu trạng thái hoạt động (Active Indicator Pill) bắt buộc phải tuân theo số đo **56dp rộng x 32dp cao**, với hình dạng bo tròn hoàn toàn 100% (cigar shape).7 Khoảng cách (Spacing) tính toán cho padding trên/dưới đối với mỗi mục điều hướng là 4dp. Ngoài ra, khoảng cách từ biểu tượng logo trên cùng đến điểm chạm của item điều hướng đầu tiên phải duy trì tối thiểu là 40dp để đảm bảo không gian thở (breathing room).  
* **Chiến lược Viết Mã QML (QML Implementation Delta)**:  
  * Bên trong tệp NavItem.qml, màu sắc của biểu tượng (icon) khi ở trạng thái hoạt động (active) *phải* được chuyển từ màu accent cơ bản sang màu có độ tương phản sâu hơn để người dùng dễ nhận diện. Mã logic cụ thể nên là: color: isActive? AppColors.onSecondaryContainer(Material.accent, isDark) : Material.foreground.  
  * Nhãn văn bản (Label) dưới biểu tượng khi active sẽ sử dụng token font Label Medium. Đáng chú ý, M3 không còn khuyến khích việc sử dụng văn bản in đậm (bold text) cho trạng thái active trên Rail 7; thay vào đó, màu sắc chữ sẽ giữ nguyên Material.foreground với độ đậm thông thường.

### **4.2. Khung chứa Thanh Công cụ Trên (Top Bar Host)**

Các phân tích đối với AppTopBar.qml bộc lộ một sự cân nhắc thú vị về mặt thiết kế giao diện cho môi trường desktop.

* **Phân tích Chiều cao (Height Justification)**: Hiện tại, mã nguồn đang gán cứng (hardcode) chiều cao thanh này ở mức **80dp**. Trong khi thông số kỹ thuật M3 cho loại Medium Top App Bar chỉ quy định chiều cao 64dp 14, việc áp dụng 80dp trên Desktop trong dự án này được báo cáo chấp nhận như một ngoại lệ UX hoàn toàn hợp lệ. Sự biện minh ở đây là việc giữ thanh ngang cao 80dp sẽ khớp chính xác với kích thước vùng chứa logo 80x80dp trên Navigation Rail bên trái, từ đó tạo ra một đường ranh giới header tuyến tính, liền mạch chạy xuyên suốt toàn màn hình. Do đó, báo cáo đề xuất **duy trì 80dp** nhưng cần tái cấu trúc lại các tham số đệm (padding).  
* **Mật độ và Căn lề Nội dung**: Khoảng cách lề ngang (content padding) bên trong Top Bar nên được thiết lập nghiêm ngặt ở mức **24dp**. Đây là tiêu chuẩn bất di bất dịch của M3 cho khung lưới (grid) trên giao diện desktop mức medium/expanded.20 Các thành phần hành động (actions) được nhúng trong topBarToolbar cần duy trì khoảng cách (gap) chính xác 8dp giữa các icon kế cận nhau.  
* **Định tuyến Phụ (Sub-route) tại Màn hình Chi tiết**: Mẫu điều hướng dạng phân cấp (sub-route) khi truy cập vào màn hình chi tiết (ví dụ LoggerDetailView) đòi hỏi một khả năng quay lại (Back affordance) rõ ràng. Nhà phát triển nên chèn một thành phần ToolButton chứa biểu tượng "Arrow Back" vào lề trái cùng của tiêu đề trong LoggerDetailTopBar. Nút này sẽ kích hoạt hàm StackView.pop() (hoặc cơ chế tương đương đang vận hành trong App shell) để đẩy người dùng trở về danh sách Loggers tổng. Điểm trọng yếu là Navigation Rail vẫn phải duy trì trạng thái highlight tại mục "Loggers" để giữ định hướng trong không gian hệ thống cho người dùng, tránh hiện tượng mất dấu ngữ cảnh (context loss).

### **4.3. Các Thẻ Thống kê (Stat Cards)**

Khu vực màn hình DashboardView.qml phụ thuộc nhiều vào các thẻ thống kê.

* **Sự tiến hóa của Thiết kế M3**: M3 đã loại bỏ phần lớn khái niệm về độ cao (elevation) dựa trên vùng bóng đổ (drop shadows) nặng nề vốn là đặc trưng của M2, và chuyển dịch sang các container phẳng được phân biệt thông qua biến thể của tông màu (tone-based surfaces).6  
* **Xấp xỉ hóa trong Qt (Qt Approximation)**: Cần tách biệt StatCard ra thành một component độc lập và lưu trữ trong module CentralLogger.Components.  
  * Bên trong component, sử dụng đối tượng Pane nhưng ghi đè thuộc tính bằng cách đặt Material.elevation: 0 để xóa hoàn toàn bóng đổ.  
  * Gán thuộc tính nền thông qua một thẻ Rectangle cụ thể: background: Rectangle { color: AppColors.surfaceContainerLow; radius: 12 }.  
  * Để xử lý trạng thái tương tác (Hover state), tích hợp thẻ MouseArea vào component để phủ thêm một lớp Rectangle màu Material.foreground với độ trong suốt được điều khiển bởi tham số AppColors.hoverOpacity khi hệ thống nhận diện con trỏ chuột lướt qua khu vực thẻ.

### **4.4. Bảng Dữ liệu (Data Tables) \- Thành phần Cốt lõi**

Hai tệp LoggersView.qml và LoggerDetailView.qml mang trọng trách hiển thị thông tin dạng lưới. Tuy nhiên, do framework Qt 6.11 không cung cấp sẵn một component TableView mang trọn vẹn Material Style M3 chuẩn chỉnh, việc giả lập (approximate) giao diện thông qua cấu trúc lặp (Repeater/ListView) kết hợp với Custom Grid là một thủ thuật bắt buộc.

* **Vùng Tiêu đề (Header)**: Vùng này cần có chiều cao tối thiểu 56dp, đủ không gian cho việc hiển thị và sắp xếp. Kiểu chữ phải được ép kiểu về Title Medium. Đường gạch ngang dưới header (Divider) phân định với phần dữ liệu sẽ sử dụng một đường viền dày 1dp với màu sắc lấy từ token AppColors.outline.  
* **Các Dòng Dữ liệu (Rows)**: Chiều cao hàng tiêu chuẩn cho một ứng dụng desktop xử lý dữ liệu nên được cấu hình cố định ở mức 48dp hoặc 52dp để tạo sự cân bằng giữa mật độ thông tin và khả năng đọc (readability). Các dòng phân cách giữa các hàng (dividers) sẽ áp dụng token AppColors.outlineVariant (một dải màu nhạt hơn so với outline thông thường, tránh gây nhiễu thị giác).5  
* **Xử lý State Layers trên Rows**: Khi người dùng lướt chuột qua một hàng, toàn bộ nền của hàng đó phải phản hồi bằng cách chuyển sang trạng thái hover. Kỹ thuật này được thực hiện bằng cách áp dụng hệ số AppColors.hoverOpacity phủ lên màu nền chính (primary hoặc foreground tùy vào định dạng hiển thị đang dùng).5  
* **Cấu trúc Màu Trạng thái (Status Colors)**: Codebase hiện tại đang mắc lỗi hardcode các màu sắc như Material.color(). Cần lập tức thay thế logic này, chuyển sang tham chiếu trực tiếp đến các token ngữ nghĩa: sử dụng AppColors.success, AppColors.warning, AppColors.error gắn trực tiếp vào các ô (cells) hoặc cột hiển thị trạng thái của thiết bị (OK/STALE/ALARM).

### **4.5. Biểu mẫu và Điều khiển Đầu vào (Forms & Controls)**

Màn hình cài đặt (SettingsView) và các hộp thoại chức năng (LoggerFormDialog) là nơi mật độ thông tin dày đặc nhất.

* **TextField & ComboBox**: Các trường nhập liệu bắt buộc phải chuyển từ phong cách Filled (mặc định của Qt) sang phong cách Outlined. Điều này đặc biệt phù hợp với ngữ cảnh quản trị desktop mật độ cao (high-density desktop forms). Việc này được thực thi đơn giản bằng cách khai báo thuộc tính đính kèm Material.containerStyle: Material.Outlined 1 cho toàn bộ phạm vi của Form, hoặc áp đặt lên từng component riêng lẻ. Ngoài ra, khoảng cách dọc (vertical spacing) giữa các trường input nên được canh chuẩn ở mức 16dp hoặc 24dp để tạo cấu trúc form rõ ràng.20  
* **Triết lý sử dụng Nút Hành động (FAB vs Buttons)**: Hệ thống MD2 thường lạm dụng nút nổi FAB (Floating Action Button) với màu nền nhấn (accent fill) ở hầu khắp mọi nơi. Báo cáo đề xuất **loại bỏ hoàn toàn** các FAB lơ lửng nếu vị trí của chúng chồng lấn, che khuất lên Data Table. Thay vào đó, chức năng Primary Action (Ví dụ: "Add Logger") phải được di chuyển lên thanh công cụ (Top Bar) dưới dạng một Button chuẩn. Nút này sẽ được cấu hình thành Primary Button thông qua mã: Material.background: Material.primary, và Material.foreground: Material.primaryHighlightedTextColor.  
* **Cấu trúc Hộp thoại (Dialogs)**: Hộp thoại LoggerFormDialog phải khai báo thuộc tính Material.roundedScale: Material.ExtraLargeScale (để bộ máy Qt tự động kích hoạt tính năng bo góc 28dp, khớp chính xác với M3 Dialog spec).1 Padding của phần nội dung (content) bên trong hộp thoại nên thiết lập đạt chuẩn 24dp xung quanh các mép.

### **4.6. Chuyển đổi Giao diện (Theme Toggle) và Tích hợp Biểu đồ**

* **Thống nhất Theme Toggle**: Sự tồn tại của Theme Toggle tại 2 điểm truy cập (entry points) khác nhau gây ra sự bối rối về mặt thiết kế. Để tuân thủ M3 về sự nhất quán của mẫu giao diện (pattern consistency), báo cáo khuyến nghị giữ công tắc bật/tắt ở dưới cùng của Navigation Rail (ThemeToggle.qml) dưới dạng một nút biểu tượng (Icon Button). Khi người dùng nhấn vào nút này, icon sẽ thực hiện hiệu ứng chuyển đổi giữa biểu tượng mặt trời (chế độ Light) và mặt trăng (chế độ Dark). Đồng thời, nhà phát triển cần xóa bỏ phần tùy chọn combobox nằm trong SettingsView để tránh sự dư thừa UI và khó khăn trong việc đồng bộ trạng thái bindings bên trong ViewModel.  
* **Tích hợp Qt Graphs**: Thư viện Qt Graphs phục vụ vẽ biểu đồ trong DashboardView không liên kết tự động (auto-binding) với AppTheme của Qt Quick Controls.15 Do đó, việc xây dựng giao diện phải gán màu thủ công thông qua một đối tượng GraphsTheme.  
  * Cài đặt biến colorScheme thành giá trị GraphsTheme.ColorScheme.Dark hoặc Light đồng bộ trực tiếp với trạng thái của SettingsController.theme.16  
  * Màu nền biểu đồ (thuộc tính backgroundColor) cần gán bằng token AppColors.surfaceContainerLowest hoặc cài đặt cho trong suốt (transparent) nếu bản thân biểu đồ đã nằm sẵn trong một Stat Card Pane có nền.  
  * Cần cập nhật danh sách mảng seriesColors bằng cách đưa các màu Primary (Teal) và Accent (Indigo) từ cấu trúc AppTheme vào làm màu đường dẫn mặc định cho biểu đồ.

## **5\. Chuyển động và Khả năng Truy cập trên Desktop (Motion & Accessibility)**

* **Triết lý Chuyển động (Motion Philosophy)**: Việc bổ sung các hoạt ảnh Morph (định dạng thông qua thẻ Behavior) vào Rail Indicator (Pill) từ item này sang item khác trên thanh điều hướng desktop có thể mang lại trải nghiệm rất mượt mà và tinh tế. Tuy nhiên, dựa trên đánh giá về **chi phí/lợi ích (cost/benefit)**, nỗ lực lập trình này không mang lại giá trị cốt lõi đáng kể đối với một ứng dụng phần mềm MVP quản lý công nghiệp. Vì vậy, báo cáo khuyến nghị giới hạn việc sử dụng hoạt ảnh chỉ ở mức độ làm mờ (fade); cụ thể là dùng thẻ Behavior on opacity (với thời lượng khoảng 150ms) cho các thay đổi giữa các trạng thái lớp tương tác (state layers) như hover và pressed.23  
* **Bảo vệ Độ Tương phản (WCAG Contrast)**: Việc sử dụng tông màu Teal và Indigo làm khung chuẩn đã được thẩm định là an toàn. Mặc dù vậy, việc áp dụng toán tử opacity lên các màu sắc cần một sự cẩn trọng tuyệt đối để không vi phạm chuẩn khả năng truy cập (accessibility). Hàm tiện ích secondaryContainer được đề xuất bên trên (dựa trên độ trong suốt alpha 0.16) khi kết hợp với màu nền Surface (sáng hoặc tối) sẽ đảm bảo màu nền của Pill luôn tạo ra độ tương phản trên tỷ lệ 3:1 (đạt tiêu chuẩn WCAG AA cho các thành phần UI) so với background. Đồng thời, cấu hình chữ và icon (lấy từ biến onSurface hoặc onSecondaryContainer) sẽ đáp ứng mức độ tương phản tối thiểu là 4.5:1, cho phép người dùng đọc thông tin dễ dàng ở mọi góc độ màn hình.  
* **Định tuyến Focus Bàn phím (Keyboard Focus Order)**: Để đảm bảo việc điều hướng hệ thống bằng bàn phím trên desktop diễn ra logic và mượt mà, kỹ sư phần mềm phải thiết lập chuỗi KeyNavigation.tab một cách tường minh. Chu trình focus nên xuất phát từ thanh AppNavigationRail, di chuyển sang AppTopBar, sau đó mới hướng vào vùng nội dung chính (ví dụ DashboardView hoặc LoggersView). Trạng thái focus (Focus state) trên các component nên sử dụng lớp phủ focusOpacity (mức 12%) để báo hiệu trực quan cho người dùng về thành phần UI đang được kích hoạt hiện tại.

## **6\. Lộ trình Chuyển đổi và Tái cấu trúc (Migration Roadmap)**

Nhằm kiểm soát rủi ro đứt gãy (regression risk) trong mã nguồn đang hoạt động ổn định, lộ trình chuyển đổi giao diện này được thiết kế và chia thành 4 giai đoạn nối tiếp nhau, trong đó Giai đoạn 0 đóng vai trò là nền tảng cốt lõi định hình toàn bộ cấu trúc sau này.

### **Giai đoạn 0: Thiết lập Nền tảng Theme & Tinh chỉnh Navigation Rail**

* **Mức độ nỗ lực**: Thấp. **Tác động thị giác**: Cao.  
* **File liên đới**: AppTheme.qml, AppColors.qml, AppNavigationRail.qml, NavItem.qml, ThemeToggle.qml.  
* **Hành động chi tiết**:  
  1. Cập nhật mã nguồn AppColors.qml bằng cách chèn toàn bộ các roles màu sắc M3 (bao gồm nhóm surface containers, outline, và nhóm màu semantic) như đã đặc tả ở phần trên. Tạo mới file AppTypography.qml định nghĩa các thông số tĩnh.  
  2. Tiến hành tinh chỉnh trực tiếp bên trong NavItem.qml: Viết mã đảm bảo thành phần pill hiển thị chính xác ở kích thước 56x32dp, đồng thời cập nhật logic màu cho thuộc tính icon và label khi ở trạng thái active.  
  3. Chuyển rời chức năng ThemeToggle hoàn toàn về vị trí trên thanh Rail, đồng thời xóa bỏ thành phần tương tự bị trùng lặp trong phần form của trang Settings.  
* **Tiêu chí nghiệm thu (Acceptance Criteria \- AC)**: Hành động chuyển đổi giữa Dark/Light mode hoạt động trơn tru; bảng terminal không xuất hiện bất kỳ cảnh báo lỗi bindings nào; Rail pill hiển thị đúng chuẩn M3 về cả hình thái kích thước lẫn trạng thái màu sắc trên nền desktop.

### **Giai đoạn 1: Khai phá và Chuẩn hóa Shared Components**

* **Mức độ nỗ lực**: Trung bình. **Tác động thị giác**: Trung bình.  
* **File liên đới**: AppTopBar.qml, tạo mới các file StatCard.qml và DataTableChrome.qml.  
* **Hành động chi tiết**:  
  1. Cấu trúc lại thanh Top Bar bằng cách thêm khoảng padding ngang 24dp, gán các giá trị Typography chuẩn cho phần hiển thị tiêu đề trang.  
  2. Trích xuất logic vùng chứa thẻ tĩnh của màn hình Dashboard thành một component độc lập tên là StatCard.qml, sử dụng màu nền surfaceContainerLow để phân lớp.  
  3. Đóng gói toàn bộ logic liên quan đến việc hiển thị Header và Dòng phân cách (Divider) của bảng Data Table thành một component dùng chung, tích hợp màu AppColors.outlineVariant.  
* **Tiêu chí nghiệm thu (AC)**: Không còn cảnh báo (warnings) liên quan đến phân giải component (component resolution) khi biên dịch; thanh Top Bar align (căn chỉnh thẳng hàng) hoàn hảo với cấu trúc của thanh Rail bên trái màn hình.

### **Giai đoạn 2: Tái cấu trúc Màn hình Hiển thị (Views)**

* **Mức độ nỗ lực**: Cao. **Tác động thị giác**: Cao.  
* **File liên đới**: SettingsView.qml, LoggerFormDialog.qml, DashboardView.qml, LoggersView.qml.  
* **Hành động chi tiết**:  
  1. *Đối với Settings & Form*: Gán toàn cục thuộc tính Material.containerStyle: Material.Outlined cho tất cả các thành phần Inputs (TextField, ComboBox).2 Kích hoạt chế độ bo góc tự động ExtraLargeScale cho khung Dialog.1  
  2. *Đối với Màn hình Loggers*: Tìm kiếm và thay thế toàn bộ màu cứng đang sử dụng trong các thẻ báo trạng thái (Status Chips) bằng bộ semantic tokens mới được tạo (AppColors.success/warning/error).  
  3. Loại bỏ triệt để các nút FAB nằm lơ lửng, cản trở tầm nhìn người dùng, thay thế bằng một nút hành động chính (Primary action) tích hợp gọn gàng trên Toolbar.  
* **Tiêu chí nghiệm thu (AC)**: Các ô input phải hiển thị rõ ràng đường viền (outline), khoảng cách dọc giữa các dòng phải đồng nhất; Bảng màu trạng thái hiển thị chuẩn xác với mức độ tương phản tối ưu ở cả hai chế độ Light và Dark mode.

### **Giai đoạn 3: Hoàn thiện Bảng dữ liệu Chi tiết & Tích hợp Biểu đồ**

* **Mức độ nỗ lực**: Trung bình. **Tác động thị giác**: Cao.  
* **File liên đới**: LoggerDetailView.qml, các cấu trúc component inline của phần biểu đồ.  
* **Hành động chi tiết**:  
  1. Tiến hành tinh chỉnh lại hệ thống padding và kích thước hiển thị của bảng Data Table chi tiết tại màn hình LoggerDetailView.  
  2. Cài đặt các thông số cho GraphsTheme trong cấu hình của thư viện Qt Graphs, thực hiện property binding trực tiếp với hệ thống AppTheme để bảo đảm màu sắc tự động đồng bộ khi có lệnh đổi chế độ từ người dùng.  
* **Tiêu chí nghiệm thu (AC)**: Giao diện theo dõi chi tiết thiết bị logger (với tính năng Back button tại top bar) hoạt động trơn tru; Biểu đồ tự động chuyển tông màu (ví dụ từ sáng sang chế độ dark mode) đồng bộ và mượt mà cùng tông với màu Surface container chung mà không cần thực hiện thao tác khởi động lại ứng dụng.

## **7\. Các Cải tiến Nhanh Ưu tiên (Prioritized Quick Wins)**

Đây là 5 hạng mục tinh chỉnh mang tính chiến thuật, có thể ngay lập tức ủy quyền cho các AI Agent (như Cursor) hoặc các kỹ sư trong dự án triển khai. Những hạng mục này đòi hỏi nỗ lực chỉnh sửa mã tối thiểu (low effort) nhưng lại mang lại sự lột xác lớn về mặt hình ảnh (high visual impact):

1. **Cập nhật Token Hoạt động của Rail**: Thực hiện thay thế ngay lập tức màu sắc icon trên Navigation Rail từ màu primary thuần (đang dùng sai) sang giá trị hàm AppColors.onSecondaryContainer (hoặc có thể tính toán nhanh bằng Qt.lighter/darker), và đổi màu của văn bản (text) thành Material.foreground với weight (độ đậm) được reset về mức Normal. (Hạng mục này ước tính tiêu tốn dưới 10 phút, tạo tác động thị giác mạnh mẽ).  
2. **Kích hoạt Outlined Input Containers**: Viết một dòng lệnh duy nhất Material.containerStyle: Material.Outlined tại thuộc tính root (lớp gốc) của màn hình SettingsView và tệp LoggerFormDialog. Khung engine của Qt 6.11 sẽ ngay lập tức tự động thay đổi giao diện toàn bộ các trường TextField thành phong cách có viền (outline) M3 hiện đại.1  
3. **Tự động Bo Góc Dialog**: Đặt thuộc tính Material.roundedScale: Material.ExtraLargeScale vào cấu hình của LoggerFormDialog.1 Thay đổi nhỏ gọn này sẽ ngay lập tức xóa bỏ thiết kế hình khối hộp cứng nhắc và lỗi thời của thế hệ MD2, thay thế bằng dáng vẻ bo tròn 28dp cực kỳ mượt mà và quyến rũ của chuẩn M3.  
4. **Chuẩn hóa Đường phân cách trong Bảng (Table Divider)**: Truy vấn tìm và thay thế tất cả các thẻ Rectangle đang đóng vai trò làm vạch kẻ (divider) trong danh sách hiển thị Loggers thành tham chiếu mã màu AppColors.outlineVariant với việc ấn định cứng chiều cao cố định 1dp.  
5. **Dọn dẹp triệt để FAB Accent-Fill**: Rà soát để loại bỏ các nút Floating Action Button đang dùng màu nền đắp đầy (fill) bằng Material.accent. Chuyển dời các chức năng tạo/đọc/cập nhật/xóa (CRUD) cơ bản lên một thanh công cụ (Toolbar) nằm phẳng ở phía trên màn hình dưới dạng Text/Icon Button, tuân thủ đúng luồng thao tác của người dùng trên môi trường nền tảng Desktop.

## **8\. Các Anti-patterns Cần Tránh Trực Diện**

Để bảo vệ cấu trúc kiến trúc MVVM mạnh mẽ và chất lượng mã nguồn bền vững sau khi thực hiện di chuyển giao diện, đội ngũ phát triển cần đặc biệt ghi nhớ các điều cấm kỵ sau:

* **KHÔNG thực hiện port mã 1:1 từ môi trường Android/Compose**: Cần phải liên tục cảnh báo rủi ro về việc sao chép mù quáng cách sử dụng lề (margins) tiêu chuẩn 16dp của thiết bị Mobile lên môi trường Desktop. Bắt buộc phải sử dụng các lề an toàn 24dp cho việc dàn trang nội dung.  
* **KHÔNG được phép lạm dụng Màu Nhấn (Accent Color)**: Giao diện hệ MD2 trước đây có xu hướng tô đậm mọi khu vực tương tác bằng các mảng màu nhấn lớn. M3, ngược lại, sử dụng nghệ thuật các bảng màu Tonal (Tonal palettes). Phải thường xuyên kiểm tra và thay thế các khối màu đậm bằng sự kết hợp giữa nền màu nhạt secondaryContainer và chữ đậm onSecondaryContainer.  
* **KHÔNG sử dụng kỹ thuật Hardcode Material.color(Material.Grey, Shade800)**: Việc đóng đinh một mã màu cụ thể như thế này sẽ làm vỡ hoàn toàn khả năng thích ứng giao diện khi ứng dụng chuyển đổi sang chế độ Dark Mode. Bắt buộc phải thông qua các biến trung gian như AppColors.surfaceContainerHigh.  
* **KHÔNG nhúng Logic Javascript phức tạp để xử lý styling**: Hệ thống cấm tuyệt đối việc sử dụng các hàm JS tính toán phức tạp nằm ngay trong các tệp UI để suy diễn Theme. Mọi trạng thái cấu hình (chẳng hạn như việc xác định đang ở Light hay Dark) phải được điều khiển duy nhất thông qua cơ chế bindings nhận tín hiệu từ C++ ViewModel (SettingsController.theme).  
* **KHÔNG tạo mới module M3Theme riêng biệt**: Tuyệt đối bảo vệ cấu trúc hiện tại, chỉ được phép mở rộng các Singleton AppTheme và AppColors đang cư ngụ trong module CentralLogger.Theme nhằm giữ vững và không phá hỏng điểm khởi chạy (entry point) thông qua ThemeSetup.cpp.

## **9\. Các Vấn đề Mở (Dành cho Product Owner Quyết định)**

Trước khi tiến hành thực thi dự án chuyển đổi một cách toàn diện, có một số điểm thắt nút (choke points) cần Product Owner (PO) tham gia đưa ra quyết định cuối cùng:

1. **Sự chênh lệch giữa Top Bar 80dp và tiêu chuẩn M3 64dp**: Báo cáo đang đưa ra đề xuất ưu tiên việc giữ nguyên thông số 80dp để đảm bảo sự đồng bộ liền mạch với thanh Rail Header 80dp (tạo thành một cấu trúc grid ngang thống nhất tuyệt đối). PO cần đánh giá và xác nhận liệu sự hy sinh không gian hiển thị theo chiều dọc này (chấp nhận mất mát 16dp) có hoàn toàn phù hợp với tầm nhìn UX hay không.  
2. **Định vị Kiến trúc của Shared Component**: PO cần chốt phương án xác nhận việc di chuyển logic của StatCard và các bộ phận hiển thị cấu trúc của Data Table (DataTableChrome) vào khu vực module chức năng CentralLogger.Components, hay nên được cấu trúc tách biệt đưa vào một namespace phụ thuộc riêng biệt thuộc quyền quản lý của CentralLogger.Theme.  
3. **Hành vi Định tuyến Phụ (Sub-route) tại Chi tiết Logger**: Khi một người dùng tương tác đi sâu vào trang chi tiết của một thiết bị logger nhất định, thanh Rail bên trái sẽ tiếp tục duy trì trạng thái sáng (active) tại mục "Loggers" để người dùng không bị mất phương hướng ngữ cảnh. Đồng thời, thanh Top Bar của trang detail sẽ có nhiệm vụ cung cấp một nút bấm Back vật lý (hoặc ảo). PO có đồng thuận với mẫu định hướng này thay thế hoàn toàn cho hệ thống breadcrumbs đường dẫn truyền thống hay không?

#### **Nguồn trích dẫn**

1. Material 3 Changes in Qt Quick Controls, truy cập vào tháng 5 27, 2026, [https://www.qt.io/blog/material-3-changes-in-qt-quick-controls](https://www.qt.io/blog/material-3-changes-in-qt-quick-controls)  
2. Material Style \- Developpez.com, truy cập vào tháng 5 27, 2026, [https://qt.developpez.com/doc/6.5/qtquickcontrols-material/](https://qt.developpez.com/doc/6.5/qtquickcontrols-material/)  
3. What's New in Qt 6.5 \- Qt Documentation, truy cập vào tháng 5 27, 2026, [https://doc.qt.io/qt-6/whatsnew65.html](https://doc.qt.io/qt-6/whatsnew65.html)  
4. Material Style | Qt Quick Controls | Qt 6.11.1, truy cập vào tháng 5 27, 2026, [https://doc.qt.io/qt-6/qtquickcontrols-material.html](https://doc.qt.io/qt-6/qtquickcontrols-material.html)  
5. Color roles \- Material Design 3, truy cập vào tháng 5 27, 2026, [https://m3.material.io/styles/color/roles](https://m3.material.io/styles/color/roles)  
6. Color \- Material Design 3 \- Create personal color schemes, truy cập vào tháng 5 27, 2026, [https://m3.material.io/styles/color/overview](https://m3.material.io/styles/color/overview)  
7. Navigation Rail \- material-components-android \- GitHub, truy cập vào tháng 5 27, 2026, [https://github.com/material-components/material-components-android/blob/master/docs/components/NavigationRail.md](https://github.com/material-components/material-components-android/blob/master/docs/components/NavigationRail.md)  
8. src/quickcontrols/material/TextField.qml · c70530078e0672b28cfede9290b5e66ac23d7724 · Arnout Vandecappelle / qtdeclarative · GitLab \- KDE Invent, truy cập vào tháng 5 27, 2026, [https://invent.kde.org/arnout/qtdeclarative/-/blob/c70530078e0672b28cfede9290b5e66ac23d7724/src/quickcontrols/material/TextField.qml](https://invent.kde.org/arnout/qtdeclarative/-/blob/c70530078e0672b28cfede9290b5e66ac23d7724/src/quickcontrols/material/TextField.qml)  
9. Styling Qt Quick Controls \- Qt Documentation, truy cập vào tháng 5 27, 2026, [https://doc.qt.io/qt-6/qtquickcontrols-styles.html](https://doc.qt.io/qt-6/qtquickcontrols-styles.html)  
10. hypengw/QmlMaterial: Material Design 3 for Qml \- GitHub, truy cập vào tháng 5 27, 2026, [https://github.com/hypengw/QmlMaterial](https://github.com/hypengw/QmlMaterial)  
11. Typography – Material Design 3, truy cập vào tháng 5 27, 2026, [https://m3.material.io/styles/typography/overview](https://m3.material.io/styles/typography/overview)  
12. Typography – Material Design 3, truy cập vào tháng 5 27, 2026, [https://m3.material.io/styles/typography/applying-type](https://m3.material.io/styles/typography/applying-type)  
13. Material 3 (You) Typography Cheatsheet​ | by Egor Tarasov | Medium, truy cập vào tháng 5 27, 2026, [https://medium.com/@vosarat1995/material-3-you-typography-cheatsheet-ffc58c540181](https://medium.com/@vosarat1995/material-3-you-typography-cheatsheet-ffc58c540181)  
14. Layout – Material Design 3, truy cập vào tháng 5 27, 2026, [https://m3.material.io/foundations/layout/layout-overview/parts-of-layout](https://m3.material.io/foundations/layout/layout-overview/parts-of-layout)  
15. GraphsTheme QML Type | Qt Graphs | Qt 6.11.1, truy cập vào tháng 5 27, 2026, [https://doc.qt.io/qt-6/qml-qtgraphs-graphstheme.html](https://doc.qt.io/qt-6/qml-qtgraphs-graphstheme.html)  
16. QGraphsTheme Class | Qt Graphs | Qt 6.11.0 \- Qt Documentation, truy cập vào tháng 5 27, 2026, [https://doc.qt.io/qt-6/qgraphstheme.html](https://doc.qt.io/qt-6/qgraphstheme.html)  
17. Material Style \- Developpez.com, truy cập vào tháng 5 27, 2026, [https://qt.developpez.com/doc/6.1/qtquickcontrols2-material/](https://qt.developpez.com/doc/6.1/qtquickcontrols2-material/)  
18. weprex/ChartWindow.qml at master · oniksan/weprex · GitHub, truy cập vào tháng 5 27, 2026, [https://github.com/oniksan/weprex/blob/master/ChartWindow.qml](https://github.com/oniksan/weprex/blob/master/ChartWindow.qml)  
19. Navigation rail – Material Design 3, truy cập vào tháng 5 27, 2026, [https://m3.material.io/components/navigation-rail/overview](https://m3.material.io/components/navigation-rail/overview)  
20. Spacing \- Material Design 3, truy cập vào tháng 5 27, 2026, [https://m3.material.io/styles/spacing/overview](https://m3.material.io/styles/spacing/overview)  
21. Breakpoints– Material Design 3, truy cập vào tháng 5 27, 2026, [https://m3.material.io/foundations/layout/applying-layout](https://m3.material.io/foundations/layout/applying-layout)  
22. Grids & Spacing – Material Design 3, truy cập vào tháng 5 27, 2026, [https://m3.material.io/foundations/layout/grids-spacing/density](https://m3.material.io/foundations/layout/grids-spacing/density)  
23. Google's Material 3 Tokens System Is About to Change Theming Forever | seenode blog, truy cập vào tháng 5 27, 2026, [https://seenode.com/blog/what-is-material-3-and-why-it-matters-in-2025](https://seenode.com/blog/what-is-material-3-and-why-it-matters-in-2025)