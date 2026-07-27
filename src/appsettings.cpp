#include "appsettings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace {

QString ringToString(RingMode m)
{
    switch (m) {
    case RingMode::R5:  return QStringLiteral("5km");
    case RingMode::R1:  return QStringLiteral("1km");
    case RingMode::R05: return QStringLiteral("0.5km");
    case RingMode::Off: break;
    }
    return QStringLiteral("off");
}

RingMode ringFromString(const QString &s, RingMode fallback)
{
    if (s == QLatin1String("5km"))   return RingMode::R5;
    if (s == QLatin1String("1km"))   return RingMode::R1;
    if (s == QLatin1String("0.5km")) return RingMode::R05;
    if (s == QLatin1String("off"))   return RingMode::Off;
    return fallback;
}

QString azimuthToString(AzimuthMode m)
{
    switch (m) {
    case AzimuthMode::A30: return QStringLiteral("30");
    case AzimuthMode::A10: return QStringLiteral("10");
    case AzimuthMode::Off: break;
    }
    return QStringLiteral("off");
}

AzimuthMode azimuthFromString(const QString &s, AzimuthMode fallback)
{
    if (s == QLatin1String("30"))  return AzimuthMode::A30;
    if (s == QLatin1String("10"))  return AzimuthMode::A10;
    if (s == QLatin1String("off")) return AzimuthMode::Off;
    return fallback;
}

/// Thư mục gốc hợp lệ là thư mục có ít nhất một thư mục con chứa tileset.json.
/// Kiểm tra chặt như vậy để lúc dò ngược lên thư mục cha không vớ phải một
/// thư mục "maps/mt" rỗng nằm sẵn đâu đó.
bool looksLikeTileBase(const QString &dir)
{
    const QDir d(dir);
    if (!d.exists())
        return false;
    const QStringList subs = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &s : subs) {
        if (QFileInfo::exists(d.filePath(s) + QStringLiteral("/tileset.json")))
            return true;
    }
    return false;
}

} // namespace

QString AppSettings::filePath()
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/mx01.json");
}

QString AppSettings::resolvedTilesDir() const
{
    const QByteArray env = qgetenv("MX01_TILES_DIR");
    if (!env.isEmpty())
        return QDir::cleanPath(QString::fromLocal8Bit(env));

    const QString want = tilesDir.isEmpty() ? QStringLiteral("maps/mt") : tilesDir;
    if (QDir::isAbsolutePath(want))
        return QDir::cleanPath(want);

    // Đường dẫn tương đối tính từ thư mục chứa file chạy. Lúc phát triển, file
    // chạy nằm trong build/ còn tile để ở gốc repo, nên dò thêm vài cấp cha.
    const QString appDir = QCoreApplication::applicationDirPath();
    QString firstGuess;
    for (const char *up : {"", "/..", "/../..", "/../../.."}) {
        const QString p = QDir::cleanPath(appDir + QLatin1String(up)
                                          + QLatin1Char('/') + want);
        if (firstGuess.isEmpty())
            firstGuess = p;
        if (looksLikeTileBase(p))
            return p;
    }
    return firstGuess;   // không thấy — trả vị trí chuẩn để báo lỗi cho đúng chỗ
}

QString AppSettings::resolvedStyleDir() const
{
    return QDir::cleanPath(resolvedTilesDir() + QLatin1Char('/') + mapStyle);
}

bool AppSettings::load()
{
    QFile f(filePath());
    if (!f.open(QIODevice::ReadOnly))
        return false;

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return false;

    const QJsonObject o = doc.object();
    mapVisible    = o.value(QStringLiteral("mapVisible")).toBool(mapVisible);
    mapBrightness = o.value(QStringLiteral("mapBrightness")).toInt(mapBrightness);
    siteLat       = o.value(QStringLiteral("siteLat")).toDouble(siteLat);
    siteLng       = o.value(QStringLiteral("siteLng")).toDouble(siteLng);
    maxRangeKm    = o.value(QStringLiteral("maxRangeKm")).toDouble(maxRangeKm);
    ringMode      = ringFromString(o.value(QStringLiteral("ringMode")).toString(),
                                   ringMode);
    azimuthMode   = azimuthFromString(o.value(QStringLiteral("azimuthMode")).toString(),
                                      azimuthMode);
    tilesDir      = o.value(QStringLiteral("tilesDir")).toString(tilesDir);
    mapStyle      = o.value(QStringLiteral("mapStyle")).toString(mapStyle);

    // Chuyển đổi từ bố cục cũ (một bộ tile duy nhất ở maps/mt/tiles) sang bố
    // cục nhiều kiểu nền (maps/mt/<style>/). Chỉ đụng đúng giá trị mặc định cũ
    // mà phần mềm từng ghi ra, không động vào đường dẫn người dùng tự đặt.
    if (tilesDir == QLatin1String("maps/mt/tiles"))
        tilesDir = QStringLiteral("maps/mt");

    // Chặn giá trị vô lý từ file bị sửa tay.
    mapBrightness = qBound(0, mapBrightness, 100);
    siteLat       = qBound(-85.0, siteLat, 85.0);
    siteLng       = qBound(-180.0, siteLng, 180.0);
    maxRangeKm    = qBound(0.5, maxRangeKm, 2000.0);
    return true;
}

bool AppSettings::save() const
{
    const QString path = filePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonObject o;
    o[QStringLiteral("mapVisible")]    = mapVisible;
    o[QStringLiteral("mapBrightness")] = mapBrightness;
    o[QStringLiteral("siteLat")]       = siteLat;
    o[QStringLiteral("siteLng")]       = siteLng;
    o[QStringLiteral("maxRangeKm")]    = maxRangeKm;
    o[QStringLiteral("ringMode")]      = ringToString(ringMode);
    o[QStringLiteral("azimuthMode")]   = azimuthToString(azimuthMode);
    o[QStringLiteral("tilesDir")]      = tilesDir;
    o[QStringLiteral("mapStyle")]      = mapStyle;

    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    return f.commit();
}
