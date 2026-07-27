#include "appsettings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

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

} // namespace

QString AppSettings::filePath()
{
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return dir + QStringLiteral("/mx01.json");
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

    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    return f.commit();
}
