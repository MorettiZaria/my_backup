#include <QApplication>
#include "MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("my_backup");
    app.setApplicationVersion("3.0");

    MainWindow window;
    window.show();

    return app.exec();
}
