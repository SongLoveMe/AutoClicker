#include <QApplication>
#include "MainWindow.h"

#ifndef AUTOCLICKER_VERSION
#define AUTOCLICKER_VERSION "0.0.0"
#endif

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("AutoClicker");
    app.setApplicationVersion(QStringLiteral(AUTOCLICKER_VERSION));

    MainWindow window;
    window.setWindowTitle(QStringLiteral("AutoClicker v") + app.applicationVersion());
    window.resize(600, 400);
    window.show();

    return app.exec();
}
