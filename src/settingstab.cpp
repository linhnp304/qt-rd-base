#include "settingstab.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QVBoxLayout>

SettingsTab::SettingsTab(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(12);

    // --- Nền bản đồ số ---------------------------------------------------
    auto *mapBox = new QGroupBox(tr("Nền bản đồ số"), this);
    auto *mapForm = new QFormLayout(mapBox);
    mapForm->setLabelAlignment(Qt::AlignLeft);

    m_mapVisible = new QCheckBox(tr("Hiển thị nền bản đồ"), mapBox);
    mapForm->addRow(m_mapVisible);

    m_brightness = new QSlider(Qt::Horizontal, mapBox);
    m_brightness->setRange(0, 100);
    auto *brightValue = new QLabel(mapBox);
    brightValue->setMinimumWidth(36);
    brightValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto *brightRow = new QHBoxLayout;
    brightRow->addWidget(m_brightness, 1);
    brightRow->addWidget(brightValue, 0);
    mapForm->addRow(tr("Độ sáng"), brightRow);

    // --- Tâm đài ---------------------------------------------------------
    auto *siteBox = new QGroupBox(tr("Tâm đài"), this);
    auto *siteForm = new QFormLayout(siteBox);

    m_siteLat = new QDoubleSpinBox(siteBox);
    m_siteLat->setDecimals(6);
    m_siteLat->setRange(-85.0, 85.0);
    m_siteLat->setSingleStep(0.001);
    siteForm->addRow(tr("Vĩ độ (lat)"), m_siteLat);

    m_siteLng = new QDoubleSpinBox(siteBox);
    m_siteLng->setDecimals(6);
    m_siteLng->setRange(-180.0, 180.0);
    m_siteLng->setSingleStep(0.001);
    siteForm->addRow(tr("Kinh độ (lng)"), m_siteLng);

    m_applySite = new QPushButton(tr("Áp dụng"), siteBox);
    siteForm->addRow(QString(), m_applySite);

    // --- Lưới cự ly / phương vị ------------------------------------------
    auto *gridBox = new QGroupBox(tr("Vòng cự ly và phương vị"), this);
    auto *gridForm = new QFormLayout(gridBox);

    m_maxRange = new QDoubleSpinBox(gridBox);
    m_maxRange->setDecimals(1);
    m_maxRange->setRange(0.5, 2000.0);
    m_maxRange->setSingleStep(1.0);
    m_maxRange->setSuffix(tr(" km"));
    gridForm->addRow(tr("Cự ly tối đa"), m_maxRange);

    // Hai dãy radio nằm chung một widget cha, nên phải tách nhóm loại trừ bằng
    // QButtonGroup — nếu không, chọn bên này sẽ bỏ chọn bên kia.
    m_ring5   = new QRadioButton(tr("5 km"), gridBox);
    m_ring1   = new QRadioButton(tr("1 km"), gridBox);
    m_ring05  = new QRadioButton(tr("0.5 km"), gridBox);
    m_ringOff = new QRadioButton(tr("Tắt"), gridBox);
    auto *ringGroup = new QButtonGroup(this);
    auto *ringRow = new QHBoxLayout;
    for (auto *b : {m_ring5, m_ring1, m_ring05, m_ringOff}) {
        ringGroup->addButton(b);
        ringRow->addWidget(b);
    }
    ringRow->addStretch(1);
    gridForm->addRow(tr("Vòng tròn cự ly"), ringRow);

    m_az30  = new QRadioButton(tr("30°"), gridBox);
    m_az10  = new QRadioButton(tr("10°"), gridBox);
    m_azOff = new QRadioButton(tr("Tắt"), gridBox);
    auto *azGroup = new QButtonGroup(this);
    auto *azRow = new QHBoxLayout;
    for (auto *b : {m_az30, m_az10, m_azOff}) {
        azGroup->addButton(b);
        azRow->addWidget(b);
    }
    azRow->addStretch(1);
    gridForm->addRow(tr("Đường chia độ"), azRow);

    root->addWidget(mapBox);
    root->addWidget(siteBox);
    root->addWidget(gridBox);
    root->addStretch(1);

    // --- Nối tín hiệu ----------------------------------------------------
    connect(m_brightness, &QSlider::valueChanged, brightValue,
            [brightValue](int v) { brightValue->setText(QString::number(v)); });

    connect(m_mapVisible, &QCheckBox::toggled, this, &SettingsTab::emitChange);
    connect(m_brightness, &QSlider::valueChanged, this, &SettingsTab::emitChange);
    connect(m_maxRange, &QDoubleSpinBox::valueChanged, this, &SettingsTab::emitChange);

    for (auto *b : {m_ring5, m_ring1, m_ring05, m_ringOff,
                    m_az30, m_az10, m_azOff})
        connect(b, &QRadioButton::toggled, this, &SettingsTab::emitChange);

    // Toạ độ tâm đài chỉ có hiệu lực khi bấm "Áp dụng".
    connect(m_applySite, &QPushButton::clicked, this, &SettingsTab::emitChange);

    setSettings(m_settings);
}

void SettingsTab::setSettings(const AppSettings &s)
{
    m_loading = true;
    m_settings = s;

    m_mapVisible->setChecked(s.mapVisible);
    m_brightness->setValue(s.mapBrightness);
    m_siteLat->setValue(s.siteLat);
    m_siteLng->setValue(s.siteLng);
    m_maxRange->setValue(s.maxRangeKm);

    switch (s.ringMode) {
    case RingMode::R5:  m_ring5->setChecked(true);   break;
    case RingMode::R1:  m_ring1->setChecked(true);   break;
    case RingMode::R05: m_ring05->setChecked(true);  break;
    case RingMode::Off: m_ringOff->setChecked(true); break;
    }

    switch (s.azimuthMode) {
    case AzimuthMode::A30: m_az30->setChecked(true);  break;
    case AzimuthMode::A10: m_az10->setChecked(true);  break;
    case AzimuthMode::Off: m_azOff->setChecked(true); break;
    }

    m_loading = false;
}

void SettingsTab::emitChange()
{
    if (m_loading)
        return;

    m_settings.mapVisible    = m_mapVisible->isChecked();
    m_settings.mapBrightness = m_brightness->value();
    m_settings.maxRangeKm    = m_maxRange->value();

    if (m_ring5->isChecked())        m_settings.ringMode = RingMode::R5;
    else if (m_ring1->isChecked())   m_settings.ringMode = RingMode::R1;
    else if (m_ring05->isChecked())  m_settings.ringMode = RingMode::R05;
    else                             m_settings.ringMode = RingMode::Off;

    if (m_az30->isChecked())         m_settings.azimuthMode = AzimuthMode::A30;
    else if (m_az10->isChecked())    m_settings.azimuthMode = AzimuthMode::A10;
    else                             m_settings.azimuthMode = AzimuthMode::Off;

    // Chỉ nhận toạ độ mới khi người dùng bấm "Áp dụng".
    if (sender() == m_applySite) {
        m_settings.siteLat = m_siteLat->value();
        m_settings.siteLng = m_siteLng->value();
    }

    emit settingsChanged(m_settings);
}
