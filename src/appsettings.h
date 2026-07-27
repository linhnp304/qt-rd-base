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

    /// Đường dẫn file cấu hình (theo chuẩn từng hệ điều hành).
    static QString filePath();

    /// Nạp từ đĩa. Trả về false nếu chưa có file / file hỏng — khi đó
    /// các trường giữ nguyên giá trị mặc định.
    bool load();

    bool save() const;
};
