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
#include <QDir>
#include <QMessageBox>
#include <QStandardPaths>

#include "mainwindow.h"
#include "yetty.version.h"

static void printUsageAndExit()
{
    // NOLINTNEXTLINE(cert-err33-c)
    fputs("Usage: " PROJECT_NAME " PORTNAME BAUDRATE\n", stderr);
    exit(EXIT_FAILURE); // NOLINT(concurrency-mt-unsafe)
}

extern "C" void signal_handler(int);
extern "C" void signal_handler(int)
{
    const auto stackTrace = boost::stacktrace::to_string(boost::stacktrace::stacktrace());
    std::cerr << stackTrace;

    const auto stdLocation = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    if (stdLocation.isEmpty()) {
        return;
    }
    const auto& stackTraceDirPath = stdLocation.at(0);
    QDir stackTraceDir(stackTraceDirPath);

    if (!stackTraceDir.exists()) {
        if (mkdir(stackTraceDirPath.toLocal8Bit(), S_IRWXU)) {
            qCritical() << "Failed to mkdir: " << errno;
            return;
        }
    }

    QFile stackTraceFile(stackTraceDirPath + QStringLiteral("/stacktrace"));

    if (!stackTraceFile.open(QIODevice::WriteOnly)) {
        qCritical() << "Failed to open file for writing: " << errno;
        return;
    }

    stackTraceFile.write(stackTrace.c_str(), static_cast<qint64>(stackTrace.size()));
    stackTraceFile.close();

    exit(-1); // NOLINT(concurrency-mt-unsafe)
}

int main(int argc, char* argv[])
{

    ::signal(SIGSEGV, &signal_handler); // NOLINT(cert-err33-c)
    ::signal(SIGABRT, &signal_handler); // NOLINT(cert-err33-c)

    if (argc != 1 && argc != 3) {
        printUsageAndExit();
    }
    QString portLocation;
    int baud {};
    if (argc == 3) {
        portLocation = argv[1]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        try {
            baud = std::stoi(argv[2]); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        } catch (std::exception& e) {
            qCritical() << "Failed to parse baud: " << e.what();
        }
        if (portLocation.isEmpty() || !baud) {
            printUsageAndExit();
        }
        if (!portLocation.startsWith(QStringLiteral("/dev/"))) {
            qCritical() << "port name is invalid: " << portLocation;
            printUsageAndExit();
        }
    }

    const QApplication a(argc, argv);

    if (!QDBusConnection::sessionBus().isConnected()) {
        qWarning() << "DBUS connection error";
    }

    QApplication::setOrganizationDomain(QStringLiteral("yeTTY.aa55.dev"));
    QApplication::setApplicationName(QStringLiteral(PROJECT_NAME));
    // This will append the application name to the window title
    QApplication::setApplicationDisplayName(QStringLiteral(PROJECT_NAME));
    // This sets the icon in wayland
    QApplication::setDesktopFileName(QStringLiteral(PROJECT_DOMAIN));
    // This is needed to show the icon in the "About" window
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral(PROJECT_DOMAIN)));

    try {
        MainWindow w(argc == 3 ? std::optional<std::pair<QString, int>> { std::in_place, portLocation, baud } : std::nullopt);
        w.show();
        return QApplication::exec();
    } catch (std::exception& e) {
        QMessageBox::critical(nullptr, QStringLiteral("Error"), e.what());
        return EXIT_FAILURE;
    }
}
