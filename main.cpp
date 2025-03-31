#include <cstdio>
#include <cstdlib>
#include <exception>

#include <QApplication>
#include <QDBusConnection>
#include <QDateTime>
#include <QMessageBox>
#include <QDebug>

#include "mainwindow.h"
#include "yetty.version.h"

static void printUsage()
{
    // NOLINTNEXTLINE(cert-err33-c)
    fputs("Usage: " PROJECT_NAME " PORTNAME BAUDRATE\n"
          "       " PROJECT_NAME " FILENAME",
        stderr);
}

int main(int argc, char* argv[])
{
    if (argc > 3) {
        printUsage();
        // NOLINTNEXTLINE(concurrency-mt-unsafe)
        exit(EXIT_FAILURE);
    }

    const QApplication a(argc, argv);

    if (!QDBusConnection::sessionBus().isConnected()) {
        qWarning() << "DBUS connection error";
    }

    QApplication::setOrganizationDomain(QStringLiteral("yeTTY.aa55.dev"));
    QApplication::setApplicationName(QStringLiteral(PROJECT_NAME));
    // This sets the icon in wayland
    QApplication::setDesktopFileName(QStringLiteral(PROJECT_DOMAIN));
    // This is needed to show the icon in the "About" window
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral(PROJECT_DOMAIN)));

    try {
        MainWindow w;
        w.show();
        return QApplication::exec();
    } catch (std::exception& e) {
        QMessageBox::critical(nullptr, QStringLiteral("Error"), e.what());
        return EXIT_FAILURE;
    }
}
