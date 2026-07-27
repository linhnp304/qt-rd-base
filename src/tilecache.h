#pragma once

#include <QCache>
#include <QPixmap>
#include <QSet>
#include <QString>

/// Đọc tile bản đồ đã tải sẵn trên đĩa (cấu trúc XYZ: <thư mục>/<z>/<x>/<y>.png)
/// và giữ lại một ít trong bộ nhớ để khỏi đọc đĩa liên tục khi vẽ.
///
/// Bộ tile do tools/download_tiles.py sinh ra, kèm file tileset.json mô tả
/// cỡ ảnh và dải mức phóng — nhờ vậy phần vẽ không phải đoán.
class TileCache
{
public:
    TileCache();

    /// Dò tìm và mở bộ tile. Thứ tự ưu tiên: biến môi trường MX01_TILES_DIR,
    /// rồi các vị trí quen thuộc quanh thư mục chạy và thư mục làm việc.
    /// Trả về false nếu chưa tải tile về.
    bool openAuto();

    /// Mở một thư mục tile cụ thể.
    bool open(const QString &dir);

    bool isValid() const { return m_valid; }
    int  tileSize() const { return m_tileSize; }
    int  minZoom() const { return m_minZoom; }
    int  maxZoom() const { return m_maxZoom; }
    QString attribution() const { return m_attribution; }
    QString directory() const { return m_dir; }

    /// Trả về tile, hoặc QPixmap rỗng nếu không có trên đĩa.
    QPixmap tile(int z, int x, int y);

private:
    static quint64 packKey(int z, int x, int y);

    QString m_dir;
    QString m_format = QStringLiteral("png");
    QString m_attribution;
    int     m_tileSize = 256;
    int     m_minZoom = 0;
    int     m_maxZoom = 0;
    bool    m_valid = false;

    QCache<quint64, QPixmap> m_cache;
    QSet<quint64>            m_missing;   ///< tránh hỏi lại đĩa về tile không có
};
