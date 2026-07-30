#include "mainwindow.h"

#include "appinfo.h"
#include "radarview.h"
#include "settingstab.h"
#include "tilecache.h"

#include <QApplication>
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QScrollArea>
#include <QShortcut>
#include <QSplitter>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace {

/// Khung rỗng cho các phần sẽ làm ở giai đoạn sau.
QWidget *makePlaceholder(const QString &text)
{
    auto *frame = new QFrame;
    frame->setFrameShape(QFrame::NoFrame);
    auto *lay = new QVBoxLayout(frame);
    auto *label = new QLabel(text, frame);
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);   // để chuỗi dài không đội bề rộng tối thiểu lên
    label->setStyleSheet(QStringLiteral("color: #4a5866;"));
    lay->addWidget(label);
    return frame;
}

QLabel *makeStatusLabel()
{
    auto *l = new QLabel;
    l->setStyleSheet(QStringLiteral("color: #9fb4c8; padding: 0 10px;"));
    return l;
}

QString formatLatLng(double lat, double lng)
{
    return QStringLiteral("%1, %2").arg(lat, 0, 'f', 6).arg(lng, 0, 'f', 6);
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(appinfo::displayName());

    m_settings.load();   // giữ mặc định nếu chưa có file

    m_radar = new RadarView(this);

    // Panel 1 (70%) | Panel 2 (30%)
    auto *hSplit = new QSplitter(Qt::Horizontal, this);
    hSplit->setChildrenCollapsible(false);
    hSplit->setHandleWidth(2);
    hSplit->addWidget(m_radar);
    hSplit->addWidget(buildRightColumn());
    hSplit->setStretchFactor(0, 7);
    hSplit->setStretchFactor(1, 3);
    hSplit->setSizes({700, 300});

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(hSplit, 1);
    root->addWidget(buildStatusBar(), 0);
    setCentralWidget(central);

    connect(m_radar, &RadarView::cursorGeoChanged, this,
            [this](double lat, double lng) {
                m_cursorLabel->setText(tr("Con trỏ  %1").arg(formatLatLng(lat, lng)));
            });

    connect(m_settingsTab, &SettingsTab::settingsChanged, this,
            &MainWindow::applySettings);

    // Đồng hồ hệ thống.
    auto *clock = new QTimer(this);
    connect(clock, &QTimer::timeout, this, [this] {
        m_timeLabel->setText(
            QDateTime::currentDateTime().toString(QStringLiteral("dd/MM/yyyy  HH:mm:ss")));
    });
    clock->start(1000);
    m_timeLabel->setText(
        QDateTime::currentDateTime().toString(QStringLiteral("dd/MM/yyyy  HH:mm:ss")));

    // Ctrl+Q (macOS: Cmd+Q) để thoát, F11 bật/tắt toàn màn hình thật sự.
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q), this,
                  [] { QApplication::quit(); });
    new QShortcut(QKeySequence(Qt::Key_F11), this, [this] {
        isFullScreen() ? showMaximized() : showFullScreen();
    });

    // Danh sách kiểu nền lấy từ chính các thư mục có trên đĩa. Nếu kiểu đang
    // lưu trong cấu hình không còn (đổi máy, xoá bớt bản đồ) thì lùi về kiểu
    // đầu tiên còn dùng được, để ComboBox không trỏ vào chỗ trống.
    QVector<TileSetInfo> styles = TileCache::listStyles(m_settings.resolvedTilesDir());

    // Tiền tố nguồn để phân biệt với lớp bản đồ TC tự dựng. Thêm ở đây chứ
    // không sửa nhãn trong tileset.json, để tải lại tile là không mất tiền tố.
    for (TileSetInfo &s : styles)
        s.label = tr("MT - %1").arg(s.label);

    // Lớp bản đồ TC luôn có trong danh sách, kể cả khi chưa có dữ liệu — chọn
    // vào sẽ thấy dòng nhắc chỉ đúng thư mục còn thiếu.
    styles.push_back({QString::fromLatin1(kTcStyleId), tr("TC")});
    m_settingsTab->setAvailableStyles(styles);

    const bool stillThere = std::any_of(styles.cbegin(), styles.cend(),
        [this](const TileSetInfo &s) { return s.id == m_settings.mapStyle; });
    if (!stillThere)
        m_settings.mapStyle = styles.first().id;

    // Ghi lại ngay lúc khởi động: file luôn tồn tại để người dùng có cái mà
    // sửa (tilesDir, mapStyle), và phản ánh đúng những gì phần mềm đang dùng
    // sau khi đã chuyển đổi bố cục cũ hay lùi về kiểu nền còn dùng được.
    m_settings.save();

    // Trước khi rê chuột, thanh trạng thái hiện luôn toạ độ tâm đài.
    m_settingsTab->setSettings(m_settings);
    m_radar->setSettings(m_settings);
    m_siteLabel->setText(tr("Tâm đài  %1")
                             .arg(formatLatLng(m_settings.siteLat, m_settings.siteLng)));
    m_cursorLabel->setText(tr("Con trỏ  %1")
                               .arg(formatLatLng(m_settings.siteLat, m_settings.siteLng)));

    resize(1920, 1080);
}

QWidget *MainWindow::buildRightColumn()
{
    auto *tabs = new QTabWidget;
    tabs->setDocumentMode(true);

    tabs->addTab(makePlaceholder(tr("Danh sách quỹ đạo — làm ở giai đoạn sau")),
                 tr("Danh sách"));
    tabs->addTab(makePlaceholder(tr("Kết nối — làm ở giai đoạn sau")),
                 tr("Kết nối"));

    // Bọc trong vùng cuộn: panel hẹp vẫn dùng được, và bề rộng tối thiểu của
    // form không ép splitter phá vỡ tỉ lệ 70/30.
    m_settingsTab = new SettingsTab;
    auto *scroll = new QScrollArea;
    scroll->setWidget(m_settingsTab);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    tabs->addTab(scroll, tr("Cài đặt"));
    tabs->setCurrentWidget(scroll);

    // Panel 2.1 (70% chiều dọc) | Panel 2.2 (30%)
    auto *vSplit = new QSplitter(Qt::Vertical);
    vSplit->setChildrenCollapsible(false);
    vSplit->setHandleWidth(2);
    vSplit->addWidget(tabs);
    vSplit->addWidget(makePlaceholder(tr("Cửa sổ biên độ — làm ở giai đoạn sau")));
    vSplit->setStretchFactor(0, 7);
    vSplit->setStretchFactor(1, 3);
    vSplit->setSizes({700, 300});
    return vSplit;
}

QWidget *MainWindow::buildStatusBar()
{
    auto *bar = new QFrame;
    bar->setFixedHeight(26);
    bar->setStyleSheet(QStringLiteral(
        "QFrame { background: #131b24; border-top: 1px solid #23303d; }"));

    m_timeLabel   = makeStatusLabel();
    m_cursorLabel = makeStatusLabel();
    m_siteLabel   = makeStatusLabel();

    // Nhóm giữa nằm chính giữa: hai bên dùng cùng hệ số giãn nên rộng bằng nhau.
    auto *left = new QWidget(bar);

    auto *centre = new QWidget(bar);
    auto *centreLay = new QHBoxLayout(centre);
    centreLay->setContentsMargins(0, 0, 0, 0);
    centreLay->setSpacing(0);
    centreLay->addWidget(m_timeLabel);
    centreLay->addWidget(m_cursorLabel);

    auto *right = new QWidget(bar);
    auto *rightLay = new QHBoxLayout(right);
    rightLay->setContentsMargins(0, 0, 0, 0);
    rightLay->setSpacing(0);
    rightLay->addStretch(1);
    rightLay->addWidget(m_siteLabel);

    auto *lay = new QHBoxLayout(bar);
    lay->setContentsMargins(6, 0, 6, 0);
    lay->setSpacing(0);
    lay->addWidget(left, 1);
    lay->addWidget(centre, 0);
    lay->addWidget(right, 1);
    return bar;
}

void MainWindow::applySettings(const AppSettings &s)
{
    m_settings = s;
    m_radar->setSettings(s);
    m_siteLabel->setText(tr("Tâm đài  %1").arg(formatLatLng(s.siteLat, s.siteLng)));
    m_settings.save();
}
