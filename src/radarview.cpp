#include "radarview.h"

#include "geo.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QSlider>
#include <QWheelEvent>

#include <cmath>

namespace {

constexpr double kMinZoom = 3.0;
constexpr double kMaxZoom = 19.0;
constexpr int    kTileSize = 256;

/// Dưới ngưỡng này các vòng / đường nằm quá sát nhau, vẽ ra chỉ thành mảng đặc.
constexpr double kMinRingSpacingPx = 6.0;
constexpr double kMinAzSpacingPx   = 28.0;

const QColor kGridBright(198, 222, 88);
const QColor kSiteColor(255, 146, 38);
const QColor kTextColor(206, 226, 138);

QString formatLatLng(double lat, double lng)
{
    return QStringLiteral("%1  %2")
        .arg(lat, 0, 'f', 6)
        .arg(lng, 0, 'f', 6);
}

} // namespace

RadarView::RadarView(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::CrossCursor);
    setMinimumSize(320, 240);
    setAutoFillBackground(false);

    m_zoomSlider = new QSlider(Qt::Vertical, this);
    m_zoomSlider->setRange(int(kMinZoom * 100), int(kMaxZoom * 100));
    m_zoomSlider->setSingleStep(25);
    m_zoomSlider->setPageStep(100);
    m_zoomSlider->setToolTip(tr("Phóng to / thu nhỏ"));
    m_zoomSlider->setFixedHeight(160);

    connect(m_zoomSlider, &QSlider::valueChanged, this, [this](int v) {
        if (m_updatingSlider)
            return;
        setZoom(v / 100.0, QPointF(width() / 2.0, height() / 2.0));
    });

    m_center = geo::toWorld(m_settings.siteLat, m_settings.siteLng);
    syncZoomSlider();

    m_tiles.openAuto();   // im lặng nếu chưa tải tile — panel vẫn chạy, chỉ nền đen
}

// ---------------------------------------------------------------- cấu hình --

void RadarView::setSettings(const AppSettings &s)
{
    const bool siteMoved = !qFuzzyCompare(s.siteLat, m_settings.siteLat)
                        || !qFuzzyCompare(s.siteLng, m_settings.siteLng);
    const bool rangeChanged = !qFuzzyCompare(s.maxRangeKm, m_settings.maxRangeKm);

    m_settings = s;

    if (siteMoved || rangeChanged)
        resetView();
    else
        update();
}

void RadarView::resetView()
{
    // Lúc này widget có thể chưa được bố trí xong nên chưa biết kích thước thật;
    // để lần vẽ kế tiếp tính giúp.
    m_needsFit = true;
    update();
}

void RadarView::fitToRange()
{
    m_needsFit = false;
    m_center = geo::toWorld(m_settings.siteLat, m_settings.siteLng);

    // Chọn mức phóng sao cho đường kính cự ly tối đa lọt gọn trong khung nhìn,
    // chừa thêm chút lề để còn thấy nhãn phương vị.
    const double span = qMax(1.0, double(qMin(width(), height())));
    const double wantPxPerKm = span / (2.3 * qMax(0.5, m_settings.maxRangeKm));
    const double mpwu = geo::metersPerWorldUnit(m_settings.siteLat);
    const double scale = wantPxPerKm * mpwu / 1000.0;

    m_zoom = qBound(kMinZoom, std::log2(scale / kTileSize), kMaxZoom);
    syncZoomSlider();
}

// ------------------------------------------------------- chuyển đổi toạ độ --

QPointF RadarView::worldToScreen(const QPointF &w) const
{
    const double scale = kTileSize * std::pow(2.0, m_zoom);
    return QPointF((w.x() - m_center.x()) * scale + width()  / 2.0,
                   (w.y() - m_center.y()) * scale + height() / 2.0);
}

QPointF RadarView::screenToWorld(const QPointF &p) const
{
    const double scale = kTileSize * std::pow(2.0, m_zoom);
    return QPointF((p.x() - width()  / 2.0) / scale + m_center.x(),
                   (p.y() - height() / 2.0) / scale + m_center.y());
}

QPointF RadarView::geoToScreen(double lat, double lng) const
{
    return worldToScreen(geo::toWorld(lat, lng));
}

double RadarView::pixelsPerKm() const
{
    const double scale = kTileSize * std::pow(2.0, m_zoom);
    return 1000.0 / geo::metersPerWorldUnit(m_settings.siteLat) * scale;
}

// -------------------------------------------------------------------- vẽ ---

void RadarView::paintEvent(QPaintEvent *)
{
    if (m_needsFit)
        fitToRange();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    p.fillRect(rect(), Qt::black);   // tắt bản đồ thì nền đen tuyệt đối
    const int tilesDrawn = m_settings.mapVisible ? drawMap(p) : 0;

    drawRangeRings(p);
    drawAzimuthLines(p);
    drawSiteMarker(p);
    drawCursorReadout(p);

    if (m_settings.mapVisible) {
        QString note;
        if (!m_tiles.isValid())
            note = tr("Chưa tải dữ liệu bản đồ — xem README, mục Nền bản đồ số");
        else if (tilesDrawn == 0)
            note = tr("Vùng này chưa có dữ liệu bản đồ ở mức phóng hiện tại — "
                      "thu nhỏ lại, hoặc tải thêm tile cho khu vực");
        else
            // Giấy phép MapTiler/OpenStreetMap bắt buộc ghi nguồn khi hiển thị.
            note = m_tiles.attribution();

        p.setPen(QColor(110, 125, 140));
        p.drawText(rect().adjusted(10, 0, -10, -8), Qt::AlignLeft | Qt::AlignBottom,
                   note);
    }
}

int RadarView::drawMap(QPainter &p)
{
    if (!m_tiles.isValid())
        return 0;

    // Chọn mức tile theo mật độ điểm ảnh. Tile 512px phủ đúng vùng của tile
    // 256px cùng chỉ số, nên mức tile thấp hơn mức phóng một bậc. Làm tròn lên
    // để tile luôn được thu nhỏ khi vẽ — phóng to tile lên sẽ bị mờ.
    const double scale = kTileSize * std::pow(2.0, m_zoom);
    const double offset = std::log2(m_tiles.tileSize() / double(kTileSize));
    const int z = qBound(m_tiles.minZoom(),
                         int(std::ceil(m_zoom - offset - 1e-6)),
                         m_tiles.maxZoom());

    const double n = std::pow(2.0, z);
    const double tilePx = scale / n;          // bề rộng một tile trên màn hình
    if (tilePx < 1.0)
        return 0;

    const QPointF tl = screenToWorld(QPointF(0, 0));
    const QPointF br = screenToWorld(QPointF(width(), height()));

    const int last = int(n) - 1;
    const int x0 = qBound(0, int(std::floor(tl.x() * n)), last);
    const int x1 = qBound(0, int(std::floor(br.x() * n)), last);
    const int y0 = qBound(0, int(std::floor(tl.y() * n)), last);
    const int y1 = qBound(0, int(std::floor(br.y() * n)), last);

    // Vẽ theo toạ độ nguyên để các tile kề nhau không hở đường chỉ trắng.
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    int drawn = 0;
    for (int x = x0; x <= x1; ++x) {
        for (int y = y0; y <= y1; ++y) {
            const QPixmap pm = m_tiles.tile(z, x, y);
            if (pm.isNull())
                continue;
            const QPointF a = worldToScreen(QPointF(x / n, y / n));
            const QPointF b = worldToScreen(QPointF((x + 1) / n, (y + 1) / n));
            const QRect dst(QPoint(int(std::floor(a.x())), int(std::floor(a.y()))),
                            QPoint(int(std::ceil(b.x())) - 1, int(std::ceil(b.y())) - 1));
            p.drawPixmap(dst, pm);
            ++drawn;
        }
    }

    // Độ sáng: phủ đen mờ lên trên. 100 = nguyên bản, 0 = tối hẳn.
    const int dim = (100 - m_settings.mapBrightness) * 255 / 100;
    if (drawn > 0 && dim > 0)
        p.fillRect(rect(), QColor(0, 0, 0, dim));

    return drawn;
}

void RadarView::drawRangeRings(QPainter &p) const
{
    if (m_settings.ringMode == RingMode::Off)
        return;

    QPen thin(QColor(kGridBright.red(), kGridBright.green(), kGridBright.blue(), 48));
    thin.setWidthF(1.0);
    QPen mid(QColor(kGridBright.red(), kGridBright.green(), kGridBright.blue(), 90));
    mid.setWidthF(1.0);
    QPen bold(QColor(kGridBright.red(), kGridBright.green(), kGridBright.blue(), 165));
    bold.setWidthF(1.6);

    // Vẽ từ lớp dày nhất tới lớp thưa nhất để nét đậm luôn nằm trên.
    if (m_settings.ringMode == RingMode::R05)
        drawRingLayer(p, 1, 2, thin, false);      // mỗi 0.5 km, bỏ vòng trùng 1 km
    if (m_settings.ringMode == RingMode::R05 || m_settings.ringMode == RingMode::R1)
        drawRingLayer(p, 2, 10, mid, false);      // mỗi 1 km, bỏ vòng trùng 5 km
    drawRingLayer(p, 10, 0, bold, true);          // mỗi 5 km, có nhãn
}

void RadarView::drawRingLayer(QPainter &p, int stepHalfKm, int skipHalfKm,
                              const QPen &pen, bool withLabels) const
{
    const double stepKm = stepHalfKm * 0.5;
    if (stepKm * pixelsPerKm() < kMinRingSpacingPx)
        return;

    const int maxHalfKm = int(std::floor(m_settings.maxRangeKm * 2.0 + 1e-6));
    if (maxHalfKm < stepHalfKm)
        return;

    p.setPen(pen);
    constexpr int kSegments = 180;

    for (int u = stepHalfKm; u <= maxHalfKm; u += stepHalfKm) {
        if (skipHalfKm > 0 && u % skipHalfKm == 0)
            continue;   // bán kính này đã có ở lớp thưa hơn

        const double r = u * 0.5;
        QPolygonF poly;
        poly.reserve(kSegments + 1);
        for (int i = 0; i <= kSegments; ++i) {
            double lat = 0.0, lng = 0.0;
            geo::destination(m_settings.siteLat, m_settings.siteLng,
                             i * 360.0 / kSegments, r, lat, lng);
            poly << geoToScreen(lat, lng);
        }
        p.drawPolyline(poly);

        if (withLabels) {
            double lat = 0.0, lng = 0.0;
            geo::destination(m_settings.siteLat, m_settings.siteLng, 0.0, r, lat, lng);
            const QPointF at = geoToScreen(lat, lng);
            // Đặt bên trái tia bắc để không đụng nhãn phương vị 0 độ ở vành ngoài.
            p.setPen(kTextColor);
            p.drawText(QRectF(at.x() - 68, at.y() - 16, 60, 15),
                       Qt::AlignRight | Qt::AlignVCenter,
                       QStringLiteral("%1 km").arg(r, 0, 'g', 4));
            p.setPen(pen);
        }
    }
}

void RadarView::drawAzimuthLines(QPainter &p) const
{
    if (m_settings.azimuthMode == AzimuthMode::Off)
        return;

    QPen mid(QColor(kGridBright.red(), kGridBright.green(), kGridBright.blue(), 90));
    mid.setWidthF(1.0);
    QPen bold(QColor(kGridBright.red(), kGridBright.green(), kGridBright.blue(), 165));
    bold.setWidthF(1.6);

    if (m_settings.azimuthMode == AzimuthMode::A10)
        drawAzimuthLayer(p, 10, 30, mid, false);  // mỗi 10 độ, bỏ đường trùng 30 độ
    drawAzimuthLayer(p, 30, 0, bold, true);       // mỗi 30 độ, có nhãn
}

void RadarView::drawAzimuthLayer(QPainter &p, int stepDeg, int skipDeg,
                                 const QPen &pen, bool withLabels) const
{
    const double r = m_settings.maxRangeKm;

    // Khoảng cách giữa hai đường kề nhau, đo ở vành ngoài.
    const double arcPx = 2.0 * geo::kPi * r * pixelsPerKm() * stepDeg / 360.0;
    if (arcPx < kMinAzSpacingPx)
        return;

    const QPointF centre = geoToScreen(m_settings.siteLat, m_settings.siteLng);

    for (int deg = 0; deg < 360; deg += stepDeg) {
        if (skipDeg > 0 && deg % skipDeg == 0)
            continue;   // phương vị này đã có ở lớp thưa hơn

        double lat = 0.0, lng = 0.0;
        geo::destination(m_settings.siteLat, m_settings.siteLng, deg, r, lat, lng);
        const QPointF outer = geoToScreen(lat, lng);

        p.setPen(pen);
        p.drawLine(centre, outer);

        if (withLabels) {
            // Đẩy nhãn ra ngoài vành một chút, theo đúng hướng của tia.
            QPointF dir = outer - centre;
            const double len = std::hypot(dir.x(), dir.y());
            if (len > 1.0) {
                dir /= len;
                const QPointF at = outer + dir * 14.0;
                p.setPen(kTextColor);
                p.drawText(QRectF(at.x() - 22, at.y() - 9, 44, 18),
                           Qt::AlignCenter, QString::number(deg));
            }
        }
    }
}

void RadarView::drawSiteMarker(QPainter &p) const
{
    const QPointF c = geoToScreen(m_settings.siteLat, m_settings.siteLng);

    p.setPen(QPen(kSiteColor, 1.4));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(c, 5.0, 5.0);
    p.drawLine(c + QPointF(-9, 0), c + QPointF(-6, 0));
    p.drawLine(c + QPointF(6, 0),  c + QPointF(9, 0));
    p.drawLine(c + QPointF(0, -9), c + QPointF(0, -6));
    p.drawLine(c + QPointF(0, 6),  c + QPointF(0, 9));

    p.setBrush(kSiteColor);
    p.drawEllipse(c, 1.6, 1.6);
}

void RadarView::drawCursorReadout(QPainter &p) const
{
    if (!m_hasCursor)
        return;

    double lat = 0.0, lng = 0.0;
    geo::fromWorld(screenToWorld(m_cursorPos), lat, lng);

    const QString text = formatLatLng(lat, lng);
    const QRect box(10, 10, 210, 24);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 150));
    p.drawRect(box);
    p.setPen(kTextColor);
    p.drawText(box.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
}

// ------------------------------------------------------------ tương tác ----

void RadarView::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    layoutZoomSlider();
}

void RadarView::layoutZoomSlider()
{
    if (!m_zoomSlider)
        return;
    const int w = m_zoomSlider->sizeHint().width();
    m_zoomSlider->setGeometry(width() - w - 14,
                              height() - m_zoomSlider->height() - 34,
                              w, m_zoomSlider->height());
}

void RadarView::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragLastPos = e->position();
        setCursor(Qt::ClosedHandCursor);
    }
    QWidget::mousePressEvent(e);
}

void RadarView::mouseMoveEvent(QMouseEvent *e)
{
    m_hasCursor = true;
    m_cursorPos = e->position();

    if (m_dragging) {
        const double scale = kTileSize * std::pow(2.0, m_zoom);
        const QPointF delta = e->position() - m_dragLastPos;
        m_dragLastPos = e->position();
        m_center -= QPointF(delta.x() / scale, delta.y() / scale);
        m_center.setY(qBound(0.0, m_center.y(), 1.0));
    }

    double lat = 0.0, lng = 0.0;
    geo::fromWorld(screenToWorld(m_cursorPos), lat, lng);
    emit cursorGeoChanged(lat, lng);

    update();
    QWidget::mouseMoveEvent(e);
}

void RadarView::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        setCursor(Qt::CrossCursor);
    }
    QWidget::mouseReleaseEvent(e);
}

void RadarView::wheelEvent(QWheelEvent *e)
{
    const double steps = e->angleDelta().y() / 120.0;
    if (qFuzzyIsNull(steps)) {
        e->ignore();
        return;
    }
    setZoom(m_zoom + steps * 0.25, e->position());
    e->accept();
}

void RadarView::setZoom(double z, const QPointF &anchorScreen)
{
    const double newZoom = qBound(kMinZoom, z, kMaxZoom);
    if (qFuzzyCompare(newZoom, m_zoom))
        return;

    // Giữ nguyên điểm địa lý nằm dưới con trỏ khi phóng to / thu nhỏ.
    const QPointF anchorWorld = screenToWorld(anchorScreen);
    m_zoom = newZoom;
    const QPointF afterWorld = screenToWorld(anchorScreen);
    m_center += anchorWorld - afterWorld;
    m_center.setY(qBound(0.0, m_center.y(), 1.0));

    syncZoomSlider();
    update();
}

void RadarView::syncZoomSlider()
{
    if (!m_zoomSlider)
        return;
    m_updatingSlider = true;
    m_zoomSlider->setValue(int(std::lround(m_zoom * 100.0)));
    m_updatingSlider = false;
}
