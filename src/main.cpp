#include <QApplication>
#include "MainWindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("AutoClicker");
    app.setApplicationVersion("1.0.0");

    MainWindow window;
    window.setWindowTitle("AutoClicker");
    window.resize(600, 400);
    window.show();

    return app.exec();
}