#include <QApplication>

#include "mainwindow.h"
#include "theme.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Tên hiện trên giao diện (tiêu đề cửa sổ, tiêu đề hộp thoại mặc định).
    // File chạy và file cấu hình mx01.json vẫn giữ nguyên tên cũ.
    QCoreApplication::setOrganizationName(QStringLiteral("MX"));
    QCoreApplication::setApplicationName(QStringLiteral("AR01.01"));

    app.setStyle(QStringLiteral("Fusion"));
    app.setStyleSheet(theme::styleSheet());

    MainWindow window;
    window.showMaximized();

    return app.exec();
}
