#pragma once

#include "appsettings.h"

#include <QMainWindow>

class QLabel;
class RadarView;
class SettingsTab;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    QWidget *buildRightColumn();
    QWidget *buildStatusBar();

    void applySettings(const AppSettings &s);

    AppSettings  m_settings;
    RadarView   *m_radar       = nullptr;
    SettingsTab *m_settingsTab = nullptr;

    QLabel *m_timeLabel   = nullptr;
    QLabel *m_cursorLabel = nullptr;
    QLabel *m_siteLabel   = nullptr;
};
