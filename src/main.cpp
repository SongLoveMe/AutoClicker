#include <QApplication>
#include <QScreen>
#include <QStyleFactory>
#include <QPalette>
#include "MainWindow.h"

#ifndef AUTOCLICKER_VERSION
#define AUTOCLICKER_VERSION "0.0.0"
#endif

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("AutoClicker");
    app.setApplicationVersion(QStringLiteral(AUTOCLICKER_VERSION));

    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#f3f5f9"));
    palette.setColor(QPalette::WindowText, QColor("#233047"));
    palette.setColor(QPalette::Base, Qt::white);
    palette.setColor(QPalette::AlternateBase, QColor("#f8fafc"));
    palette.setColor(QPalette::Text, QColor("#233047"));
    palette.setColor(QPalette::Button, Qt::white);
    palette.setColor(QPalette::ButtonText, QColor("#233047"));
    palette.setColor(QPalette::Highlight, QColor("#dbeafe"));
    palette.setColor(QPalette::HighlightedText, QColor("#17253c"));
    app.setPalette(palette);

    MainWindow window;
    window.setWindowTitle(QStringLiteral("AutoClicker v") + app.applicationVersion());
    const QSize available = window.screen()->availableGeometry().size();
    window.setMinimumSize(QSize(780, 460).boundedTo(available - QSize(40, 60)));
    window.resize(QSize(960, 680).boundedTo(available - QSize(40, 60)));
    window.show();

    return app.exec();
}
