#include <QApplication>

#include "mainwindow.h"
#include "theme.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Quyết định nơi đặt file cấu hình JSON (QStandardPaths dựa vào các tên này).
    QCoreApplication::setOrganizationName(QStringLiteral("MX"));
    QCoreApplication::setApplicationName(QStringLiteral("MX01"));

    app.setStyle(QStringLiteral("Fusion"));
    app.setStyleSheet(theme::styleSheet());

    MainWindow window;
    window.showMaximized();

    return app.exec();
}
