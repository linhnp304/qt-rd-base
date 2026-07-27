#pragma once

#include <QString>

/// Mật độ vòng tròn cự ly. Mỗi mức giữ nguyên các vòng của mức thưa hơn.
enum class RingMode {
    Off,
    R5,     // vòng mỗi 5 km, nét đậm
    R1,     // thêm vòng mỗi 1 km
    R05     // thêm vòng mỗi 0.5 km
};

/// Mật độ đường chia phương vị.
enum class AzimuthMode {
    Off,
    A30,    // đường mỗi 30 độ, nét đậm
    A10     // thêm đường mỗi 10 độ
};

/// Toàn bộ cấu hình người dùng, nạp/lưu ở dạng JSON.
struct AppSettings {
    bool        mapVisible    = true;
    int         mapBrightness = 60;         // 0..100
    double      siteLat       = 21.028;
    double      siteLng       = 105.852;
    double      maxRangeKm    = 20.0;
    RingMode    ringMode      = RingMode::R1;
    AzimuthMode azimuthMode   = AzimuthMode::A30;

    /// Thư mục chứa tile bản đồ. Đường dẫn tuyệt đối thì dùng nguyên; tương
    /// đối thì tính từ thư mục chứa file chạy. Sửa trực tiếp trong file JSON
    /// khi chuyển máy hoặc để tile ở ổ khác.
    QString     tilesDir      = QStringLiteral("maps/mt/tiles");

    /// File cấu hình nằm ngay cạnh file chạy, để cả bộ mang đi máy khác được.
    static QString filePath();

    /// Đường dẫn tuyệt đối tới thư mục tile, đã áp dụng thứ tự ưu tiên:
    /// biến môi trường MX01_TILES_DIR > trường tilesDir. Thư mục trả về có
    /// thể chưa tồn tại — nơi gọi tự xử lý.
    QString resolvedTilesDir() const;

    /// Nạp từ đĩa. Trả về false nếu chưa có file / file hỏng — khi đó
    /// các trường giữ nguyên giá trị mặc định.
    bool load();

    bool save() const;
};
