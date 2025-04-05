#define BOOST_STACKTRACE_USE_BACKTRACE
#define BOOST_STACKTRACE_USE_ADDR2LINE
#include <boost/stacktrace.hpp>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>

#include <QApplication>
#include <QDBusConnection>
#include <QDateTime>
#include <QDebug>
#include <QMessageBox>

#include "mainwindow.h"
#include "yetty.version.h"

static void printUsage()
{
    // NOLINTNEXTLINE(cert-err33-c)
    fputs("Usage: " PROJECT_NAME " PORTNAME BAUDRATE\n"
          "       " PROJECT_NAME " FILENAME",
        stderr);
}

extern "C" void signal_handler(int);
extern "C" void signal_handler(int)
{
    std::cerr << boost::stacktrace::stacktrace();
    exit(-1); // NOLINT(concurrency-mt-unsafe)
}

int main(int argc, char* argv[])
{
    ::signal(SIGSEGV, &signal_handler); // NOLINT(cert-err33-c)
    ::signal(SIGABRT, &signal_handler); // NOLINT(cert-err33-c)

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
