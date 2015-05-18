#include <QApplication>
#include <QTimer>
#include <QDesktopWidget>
#include <QSplashScreen>

#include "velotrack.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    VeloTrackMainWindow mainWindow;
    QSplashScreen *splash = mainWindow.getSplash();
    splash->show();
    QTimer::singleShot(2500, splash, SLOT(close()));

    QRect desktop = QDesktopWidget().screenGeometry();
    int w = 400;
    int h = 500;
    mainWindow.setGeometry((desktop.width() - w) / 2, (desktop.height() - h) / 2, w, h);
    mainWindow.show();

    return app.exec();
}
