#include <QApplication>
#include <QIcon>
#include "MainWindow.h"

#ifndef AUTOCLICKER_VERSION
#define AUTOCLICKER_VERSION "0.0.0"
#endif

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("AutoClicker");
    app.setApplicationVersion(QStringLiteral(AUTOCLICKER_VERSION));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/autoclicker.svg")));

    MainWindow window;
    window.setWindowTitle(QStringLiteral("AutoClicker v") + app.applicationVersion());
    window.resize(900, 560);
    window.show();

    return app.exec();
}
