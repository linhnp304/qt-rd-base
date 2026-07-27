#pragma once

// Các phép chiếu / tính toán trắc địa dùng chung cho màn hình ra đa.
//
// Hệ toạ độ "world" ở đây là Web Mercator đã chuẩn hoá về [0,1] x [0,1]
// (0,0 = góc tây bắc). Chọn hệ này để khi ghép nền bản đồ số (tile XYZ)
// ở giai đoạn sau thì không phải sửa lại phần vẽ.

#include <QPointF>

#include <cmath>

namespace geo {

inline constexpr double kPi          = 3.14159265358979323846;
inline constexpr double kDeg2Rad     = kPi / 180.0;
inline constexpr double kRad2Deg     = 180.0 / kPi;
inline constexpr double kEarthRadKm  = 6371.0088;   // bán kính trung bình
inline constexpr double kEquatorM    = 40075016.686; // chu vi xích đạo (m)

// Mercator chỉ hợp lệ tới ~85.05 độ.
inline constexpr double kMaxLat = 85.05112878;

inline double clampLat(double lat)
{
    return std::fmax(-kMaxLat, std::fmin(kMaxLat, lat));
}

/// lat/lng (độ) -> world [0,1]
inline QPointF toWorld(double lat, double lng)
{
    const double s = std::sin(clampLat(lat) * kDeg2Rad);
    return QPointF((lng + 180.0) / 360.0,
                   0.5 - std::log((1.0 + s) / (1.0 - s)) / (4.0 * kPi));
}

/// world [0,1] -> lat/lng (độ)
inline void fromWorld(const QPointF &w, double &lat, double &lng)
{
    lng = w.x() * 360.0 - 180.0;
    const double n = kPi * (1.0 - 2.0 * w.y());
    lat = kRad2Deg * std::atan(std::sinh(n));
}

/// Số mét trên mặt đất ứng với 1 đơn vị world, tại vĩ độ lat.
inline double metersPerWorldUnit(double lat)
{
    return kEquatorM * std::cos(clampLat(lat) * kDeg2Rad);
}

/// Điểm đến khi đi từ (lat,lng) theo phương vị bearingDeg, cự ly distKm.
/// Công thức cầu — đủ chính xác ở cự ly vài trăm km.
inline void destination(double lat, double lng, double bearingDeg, double distKm,
                        double &outLat, double &outLng)
{
    const double lat1 = lat * kDeg2Rad;
    const double lng1 = lng * kDeg2Rad;
    const double brg  = bearingDeg * kDeg2Rad;
    const double dr   = distKm / kEarthRadKm;

    const double sinLat1 = std::sin(lat1);
    const double cosLat1 = std::cos(lat1);
    const double sinDr   = std::sin(dr);
    const double cosDr   = std::cos(dr);

    const double sinLat2 = sinLat1 * cosDr + cosLat1 * sinDr * std::cos(brg);
    const double lat2    = std::asin(sinLat2);
    const double lng2    = lng1 + std::atan2(std::sin(brg) * sinDr * cosLat1,
                                             cosDr - sinLat1 * sinLat2);

    outLat = lat2 * kRad2Deg;
    outLng = lng2 * kRad2Deg;
}

} // namespace geo
