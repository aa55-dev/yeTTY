#include "dbus_common.hpp"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDebug>
#include <QProcess>
#include <QRegularExpression>

static void printUsage()
{
    puts("USAGE: yetty_suspend PORT_NAME PROGRAM PROGRAM_ARGS");
}

[[nodiscard]] static int handleReply(const QDBusReply<QString>& reply)
{
    if (!reply.isValid()) {
        qCritical() << "Failed to communicate with yeTTY: " << reply.error();
        return -1;
    }
    const auto v = reply.value();
    if (v == DBUS_RESULT_SUCCESS) {
        return 0;
    }
    qCritical() << "Error: " << v;
    return -1;
}

[[nodiscard]] static int stop(QDBusInterface& dbusIface, const QString& portName)
{
    const QDBusReply<QString> reply = dbusIface.call("control", portName, DBUS_STOP);
    return handleReply(reply);
}

[[nodiscard]] static int start(QDBusInterface& dbusIface, const QString& portName)
{
    const QDBusReply<QString> reply = dbusIface.call("control", portName, DBUS_START);
    return handleReply(reply);
}

[[nodiscard]] static QString getServiceName(QDBusConnection& connection, const QString& portName)
{
    const auto result = connection.interface()->registeredServiceNames();

    if (!result.isValid()) {
        return {};
    }

    const QRegularExpression exp(DBUS_SERVICE_REGEX);
    const auto serviceNameList = result.value().filter(exp);

    for (const auto& serviceName : serviceNameList) {
        QDBusInterface dbusIface(serviceName, "/", DBUS_INTERFACE_NAME);
        const QDBusReply<QString> reply = dbusIface.call("portName");

        if (reply.value() == portName) {
            return serviceName;
        }
    }

    return {};
}

int main(int argc, char* argv[])
{
    const QCoreApplication app(argc, argv);

    if (argc < 3) {
        printUsage();
        return -1;
    }

    QString portName = argv[1];

    static constexpr const std::string_view DEV_PREFIX = "/dev/";

    // Remove the "/dev/" from "/dev/ttyUSB0" since yeTTY only uses the port name ("ttyUSB0")
    if (portName.startsWith(QString::fromLatin1(DEV_PREFIX.data(), DEV_PREFIX.size()))) {
        portName.remove(0, DEV_PREFIX.size());
    }
    if (!portName.startsWith("tty")) {
        printUsage();
        return -1;
    }

    auto connection = QDBusConnection::sessionBus();

    if (!connection.isConnected()) {
        qCritical() << "Could not connect to dbus";
        return -1;
    }

    const auto serviceName = getServiceName(connection, portName);
    if (serviceName.isEmpty()) {
        qCritical() << "Failed to find yeTTY.";
        return -1;
    }

    QDBusInterface dbusIface(serviceName, "/", DBUS_INTERFACE_NAME);
    if (!dbusIface.isValid()) {
        const auto err = connection.lastError();
        qCritical() << (QStringLiteral("connection error: %1")
                .arg(err.type() == QDBusError::ServiceUnknown ? QStringLiteral("Is yeTTY running?") : err.message()));

        return -1;
    }

    if (stop(dbusIface, portName)) {
        return -1;
    }

    const QString program = argv[2];
    QStringList argList;
    for (int i = 3; i < argc; i++) {
        argList << argv[i];
    }

    QProcess process;
    process.setProcessChannelMode(QProcess::ForwardedChannels);
    process.start(program, argList);

    process.waitForFinished();

    return start(dbusIface, portName);
}
