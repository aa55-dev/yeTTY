#include <QApplication>
#include <QDateTime>
#include <QMessageBox>

#include "mainwindow.h"
#include "yetty.version.h"

static void printUsage()
{
    fputs("Usage: " PROJECT_NAME " PORTNAME BAUDRATE\n"
          "       " PROJECT_NAME " FILENAME",
        stderr);
}

int main(int argc, char* argv[])
{
    if (argc > 3) {
        printUsage();
        exit(EXIT_FAILURE);
    }

    QApplication a(argc, argv);

    QApplication::setOrganizationDomain("aa55.dev");
    QApplication::setApplicationName(PROJECT_NAME);
    // This sets the icon in wayland
    QApplication::setDesktopFileName(PROJECT_DOMAIN);
    // This is needed to show the icon in the "About" window
    a.setWindowIcon(QIcon::fromTheme(PROJECT_DOMAIN));

    try {
        MainWindow w;
        w.show();
        return a.exec();
    } catch (std::exception& e) {
        QMessageBox::critical(nullptr, "Error", e.what());
        return EXIT_FAILURE;
    }
}
