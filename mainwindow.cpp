#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "dbus_common.hpp"
#include "longtermrunmodedialog.h"
#include "portselectiondialog.h"
#include "triggersetupdialog.h"
#include "yetty.version.h"

#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/Message>
#include <KTextEditor/View>

#include <zstd.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <grp.h>
#include <malloc.h>
#include <pwd.h>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

#include <QApplication>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDateTime>
#include <QDebug>
#include <QEvent>
#include <QFile>
#include <QFileSystemWatcher>
#include <QIODevice>
#include <QKeyEvent>
#include <QMainWindow>
#include <QMessageBox>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QSoundEffect>
#include <QString>
#include <QStringBuilder>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#ifdef SYSTEMD_AVAILABLE
#include <systemd/sd-bus.h>
#endif

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , fsWatcher(new QFileSystemWatcher(this))
    , serialPort(new QSerialPort(this))
    , sound(new QSoundEffect(this))
    , autoRetryTimer(new QTimer(this))
    , statusBarTimer(new QTimer(this))
    , statusBarText(new QLabel(this))
    , longTermRunModeTimer(new QTimer(this))
{

    const auto args = QApplication::arguments();

    QString portLocation;
    int baud {};

    switch (args.size()) {

    case 3: { // Port and baud in cmdline arg
        bool ok {};
        baud = args[2].toInt(&ok);
        if (!ok || !baud) {
            throw std::runtime_error("Invalid baud: " + args[2].toStdString());
        }

        [[fallthrough]];
    }
    case 2: // Filename in cmdline arg
        portLocation = args[1];
        {
        }
        break;

    default: // No args, show msgbox and get it from user
        std::tie(portLocation, baud) = getPortFromUser();
    }

    elapsedTimer.start();
    ui->setupUi(this);

    editor = KTextEditor::Editor::instance();
    doc = editor->createDocument(this);
    doc->setHighlightingMode(HIGHLIGHT_MODE);
    doc->setReadWrite(false);

    view = doc->createView(this);
    view->setStatusBarEnabled(false);

    ui->verticalLayout->insertWidget(0, view);

    setWindowTitle(QStringLiteral(PROJECT_NAME));

    connectToDevice(portLocation, baud);

    connect(ui->actionConnectToDevice, &QAction::triggered, this, &MainWindow::handleConnectAction);
    ui->actionConnectToDevice->setShortcut(QKeySequence::Open);
    ui->actionConnectToDevice->setIcon(QIcon::fromTheme(QStringLiteral("document-open")));

    connect(ui->actionSave, &QAction::triggered, this, &MainWindow::handleSaveAction);
    ui->actionSave->setShortcut(QKeySequence::Save);
    ui->actionSave->setIcon(QIcon::fromTheme(QStringLiteral("document-save")));

    connect(ui->actionQuit, &QAction::triggered, this, &MainWindow::handleQuitAction);
    ui->actionQuit->setShortcut(QKeySequence::Quit);
    ui->actionQuit->setIcon(QIcon::fromTheme(QStringLiteral("application-exit")));

    connect(ui->actionClear, &QAction::triggered, this, &MainWindow::handleClearAction);
    ui->actionClear->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K));
    ui->actionClear->setIcon(QIcon::fromTheme(QStringLiteral("edit-clear-all")));

    connect(ui->scrollToEndButton, &QPushButton::pressed, this, &MainWindow::handleScrollToEnd);
    ui->scrollToEndButton->setIcon(QIcon::fromTheme(QStringLiteral("go-bottom")));

    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::handleAboutAction);
    ui->actionAbout->setIcon(QIcon::fromTheme(QStringLiteral("help-about")));

    connect(ui->actionTrigger, &QAction::triggered, this, &MainWindow::handleTriggerSetupAction);
    ui->actionTrigger->setIcon(QIcon::fromTheme(QStringLiteral("mail-thread-watch")));

    ui->actionLongTermRunMode->setIcon(QIcon::fromTheme(QStringLiteral("media-record")));
    connect(ui->actionLongTermRunMode, &QAction::triggered, this, &MainWindow::handleLongTermRunModeAction);

    connect(ui->startStopButton, &QPushButton::pressed, this, &MainWindow::handleStartStopButton);

    connect(serialPort, &QSerialPort::readyRead, this, &MainWindow::handleReadyRead);
    connect(serialPort, &QSerialPort::errorOccurred, this, &MainWindow::handleError);

    connect(autoRetryTimer, &QTimer::timeout, this, &MainWindow::handleRetryConnection);
    connect(statusBarTimer, &QTimer::timeout, this, &MainWindow::handleStatusBarTimer);
    statusBarTimer->setSingleShot(true);
    connect(longTermRunModeTimer, &QTimer::timeout, this, &MainWindow::handleLongTermRunModeTimer);

    connect(fsWatcher, &QFileSystemWatcher::fileChanged, this, &MainWindow::handleFileWatchEvent);

    sound->setSource(QUrl::fromLocalFile(QStringLiteral(":/notify.wav")));

    ui->statusbar->addWidget(statusBarText);

    QDBusConnection connection = QDBusConnection::sessionBus();

    // NOLINTNEXTLINE(-Wclazy-qstring-allocations)
    connection.registerService(DBUS_SERVICE_NAME + (QStringLiteral("-") + QString::number(getpid())));
    connection.registerObject(QStringLiteral("/"), DBUS_INTERFACE_NAME, this, QDBusConnection::ExportScriptableSlots);

    ui->statusTextLabel->setVisible(false);
    ui->statusIconLabel->setVisible(false);
    const auto size = ui->statusIconLabel->size();
    ui->statusIconLabel->setPixmap(QIcon::fromTheme(QStringLiteral("media-record")).pixmap(QSize(size.width() / 2, size.height())));
    qDebug() << "Init complete in:" << elapsedTimer.elapsed();
}

MainWindow::~MainWindow()
{
    delete ui;
    ZSTD_freeCCtx(zstdCtx);
    zstdCtx = nullptr;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void MainWindow::control(const QString& port, const QString& action, QString& out)
{
    qWarning() << "DBus" << port << action;
    if (port != serialPort->portName()) {
        out = QStringLiteral("invalid port, currently connected to %1").arg(serialPort->portName());
        qWarning() << out;
        return;
    }

    if (action == DBUS_START) {
        if (!serialPort->isOpen()) {
            start();
        }
    } else if (action == DBUS_STOP) {
        stop();
        statusBarText->setText(QStringLiteral("Stopped by yetty_suspend"));
    } else {
        out = QStringLiteral("Invalid action: %1").arg(action);
        qWarning() << out;
        return;
    }

    out = DBUS_RESULT_SUCCESS;
}

void MainWindow::portName(QString& out)
{
    out = serialPort->portName();
}

void MainWindow::setProgramState(const ProgramState newState)
{
    if (newState == currentProgramState) {
        return;
    }

#ifdef SYSTEMD_AVAILABLE
    setInhibit(newState == ProgramState::Started);
#endif
    if (newState == ProgramState::Started) {
        qInfo() << "Program started";
        statusBarText->setText(QStringLiteral("Running"));

        ui->startStopButton->setText(QStringLiteral("&Stop"));
        ui->startStopButton->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-stop")));

        Q_ASSERT(fsWatcher->files().isEmpty());
        fsWatcher->addPath(getSerialPortPath());

    } else if (newState == ProgramState::Stopped) {
        qInfo() << "Program stopped";
        statusBarText->setText(QStringLiteral("Stopped"));
        ui->startStopButton->setText(QStringLiteral("&Start"));
        ui->startStopButton->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")));

        fsWatcher->removePaths(fsWatcher->files());
    } else {
        qCritical() << "Invalid state";
    }

    currentProgramState = newState;
}

std::pair<QString, int> MainWindow::getPortFromUser()
{
    PortSelectionDialog dlg;
    if (!dlg.exec()) {
        qInfo() << "No port selection made";
        throw std::runtime_error("No selection made");
    }
    return dlg.getSelectedPortInfo();
}

void MainWindow::handleReadyRead()
{
    auto newData = serialPort->readAll();

    // Need to remove '\0' from the input or else we might mess up the text shown or
    // affect string operation downstream. We could replace it with "�" but
    // the replace operation with multi byte unicode char will become be very expensive.
    newData.replace('\0', ' ');

    if (triggerActive) {

        // We will keep pushing whatever data we get into a QByteArray till we reach end of line,
        // at which point we search for our trigger keyword in the constructed line.
        for (const auto i : std::as_const(newData)) {
            if (i == '\n') {
                if (triggerSearchLine.contains(triggerKeyword)) {
                    triggerMatchCount++;
                    statusBarText->setText(QStringLiteral("<b>%1 matches for %2</b>").arg(triggerMatchCount).arg(triggerKeyword.data()));
                    statusBarTimer->start(5000);

                    // TODO: This is broken. Qt plays the sound for a few times and then stops working.
                    sound->play();
                }

                // clear() will free memory, resize(0) will not
                triggerSearchLine.resize(0);
            } else {
                triggerSearchLine.push_back(i);
            }
        }
    }

    doc->setReadWrite(true);
    doc->insertText(doc->documentEnd(), newData);
    doc->setReadWrite(false);
}

void MainWindow::handleError(const QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::SerialPortError::NoError) {
        return;
    }

    stop();

    auto errMsg = QStringLiteral("Error: ") + QVariant::fromValue(error).toString();

    const auto portName = getSerialPortPath();

    if (!QFile::exists(portName)) {
        errMsg += QStringLiteral(": %1 detached").arg(portName);
    }
    qCritical() << "Serial port error: " << error;
    statusBarText->setText(errMsg);

    // When USB is disconnected we get a QSerialPort::ResourceError and then a QSerialPort::DeviceNotFoundError when our retry timer
    // tries to reconnect to the port. If the user has dismissed the first error message we should not show the second error message
    const auto duplicateError = (error == QSerialPort::DeviceNotFoundError && prevErrCode == QSerialPort::ResourceError);

    if (prevErrMsg != errMsg && !duplicateError) {
        doc->postMessage(new KTextEditor::Message(errMsg, KTextEditor::Message::Error)); // NOLINT(cppcoreguidelines-owning-memory)
    }

    prevErrMsg = errMsg;
    prevErrCode = error;
    setProgramState(ProgramState::Stopped);

    if (!autoRetryTimer->isActive()) {
        // Lets try to reconnect after a while
        autoRetryTimer->start(1000);
    }
}

void MainWindow::handleSaveAction()
{
    doc->documentSave();
}

void MainWindow::handleClearAction()
{
    doc->setReadWrite(true);
    doc->setModified(false);
    doc->closeUrl();
    if (!doc->setHighlightingMode(HIGHLIGHT_MODE)) {
        qWarning() << "Failed to set highlighting";
    }
    void(malloc_trim(0));
    doc->setReadWrite(false);
}

void MainWindow::handleQuitAction()
{
    qInfo() << "Exiting application";
    QCoreApplication::quit();
}

void MainWindow::handleScrollToEnd()
{
    view->setFocus();
    QCoreApplication::postEvent(view, new QKeyEvent(QEvent::KeyPress, Qt::Key_End, Qt::ControlModifier)); // NOLINT(cppcoreguidelines-owning-memory)
}

void MainWindow::handleAboutAction()
{
    QMessageBox::about(this, QStringLiteral("About"), QStringLiteral("%1\t\t\n%2\t").arg(PROJECT_NAME, PROJECT_VERSION));
}

void MainWindow::handleConnectAction()
{
    stop();
    autoRetryTimer->stop();
    const auto [location, baud] = getPortFromUser();
    handleClearAction();

    connectToDevice(location, baud);
}

void MainWindow::handleTriggerSetupAction()
{
    if (!triggerSetupDialog) {
        triggerSetupDialog = std::make_unique<TriggerSetupDialog>(this);
        connect(triggerSetupDialog.get(), &QDialog::finished, this, &MainWindow::handleTriggerSetupDialogDone);
    }
    triggerSetupDialog->open();
}

void MainWindow::handleTriggerSetupDialogDone(int result)
{
    if (result == QDialog::Accepted) {

        const auto newKeyword = triggerSetupDialog->getKeyword().toUtf8();
        if (newKeyword != triggerKeyword) {
            qInfo() << "Setting new trigger keyword: " << triggerKeyword << " " << newKeyword;
            triggerKeyword = newKeyword;
            triggerActive = !newKeyword.isEmpty();
            triggerMatchCount = 0;
        }
    }
}

void MainWindow::handleStartStopButton()
{
    if (currentProgramState == ProgramState::Started) {
        stop();
    } else {
        start();
    }
}

void MainWindow::handleRetryConnection()
{
    // This can end up opening the wrong port if the user is plugging in multiple serial devices
    // and a new device enumerates to the same name as the old one.
    if (!serialPort->isOpen()) {
        qInfo() << "Retrying connection";
        connectToDevice(serialPort->portName(), serialPort->baudRate(), false);
    } else {
        qWarning() << "Serial port is open, stopping retry timer";
        autoRetryTimer->stop();
    }
}

void MainWindow::handleLongTermRunModeDialogDone(int result)
{
    if (result == QDialog::Accepted) {
        longTermRunModeEnabled = longTermRunModeDialog->isEnabled();

        if (longTermRunModeEnabled) {
            longTermRunModeTimer->start(5000);
            longTermRunModeMaxTime = longTermRunModeDialog->getMinutes();
            longTermRunModeMaxMemory = longTermRunModeDialog->getMemory();
            longTermRunModeStartTime = elapsedTimer.elapsed();
            longTermRunModePath = longTermRunModeDialog->getDirectory().path();

            Q_ASSERT(!longTermRunModePath.isEmpty());
            qInfo() << "Long term run mode enabled:" << longTermRunModeMaxMemory << longTermRunModeMaxTime << longTermRunModePath;
            ui->statusTextLabel->setText(QStringLiteral("Long term run mode active (location: %1)")
                    .arg(longTermRunModePath));
        } else {
            qInfo() << "Long term run mode disabled";
            fileCounter = 0;
            errCtr = 0;
            longTermRunModeTimer->stop();
        }

        ui->statusIconLabel->setVisible(longTermRunModeEnabled);
        ui->statusTextLabel->setVisible(longTermRunModeEnabled);
    }
}

void MainWindow::handleLongTermRunModeTimer()
{
    Q_ASSERT(elapsedTimer.elapsed() > longTermRunModeStartTime);

    const auto timeSinceLastSave = elapsedTimer.elapsed() - longTermRunModeStartTime;
    bool shouldSave {};
    if (timeSinceLastSave > (static_cast<qint64>(longTermRunModeMaxTime) * 60 * 1000)) {
        shouldSave = true;
        qInfo() << "Time since last save: " << timeSinceLastSave;
    }

    if (const auto textSize = doc->text().size(); textSize > (longTermRunModeMaxMemory * 1024 * 1024)) {
        shouldSave = true;
        qInfo() << "text size: " << textSize;
    }

    if (shouldSave) {

        longTermRunModeStartTime = elapsedTimer.elapsed();

        const auto utfTxt = doc->text().toUtf8();
        try {
            writeCompressedFile(utfTxt, fileCounter);
            handleClearAction();
            fileCounter++;
        } catch (std::exception& e) {
            // In case of errors we don't clear the logs, hopefully in the next attempt it would work
            qCritical() << e.what();
            errCtr++;

            if (!longTermRunModeErrMsgActive) {
                longTermRunModeErrMsgActive = true;
                auto* msg = new KTextEditor::Message(e.what(), KTextEditor::Message::Error); // NOLINT(cppcoreguidelines-owning-memory)

                connect(msg, &KTextEditor::Message::closed, this, [this](KTextEditor::Message*) { longTermRunModeErrMsgActive = false; });
                doc->postMessage(msg);
            }
        }

        const QString errStr = errCtr ? (QStringLiteral(" <b>(%1 errors)</b>").arg(QString::number(errCtr))) : "";

        ui->statusTextLabel->setText(QStringLiteral("Long term run mode active (%1 files%2 saved in %3)")
                .arg(QString::number(fileCounter), errStr, longTermRunModePath));
    }
}

void MainWindow::handleFileWatchEvent(const QString& path)
{
    if (!QFile::exists(path) && serialPort->isOpen()) {
        Q_ASSERT(path == getSerialPortPath());
        handleError(QSerialPort::SerialPortError::ResourceError);
    }
}

void MainWindow::handleStatusBarTimer()
{
    auto txt = statusBarText->text();
    if (txt.startsWith(QStringLiteral("<b>"))) {
        txt.remove(0, 3);
        if (txt.endsWith(QStringLiteral("</b>"))) {
            txt.chop(4);
        } else {
            qWarning() << "Invalid string in status bar" << txt;
        }
    }

    statusBarText->setText(txt);
}

void MainWindow::start()
{
    connectToDevice(serialPort->portName(), serialPort->baudRate());
}

void MainWindow::stop()
{
    closeSerialPort();
}

void MainWindow::handleLongTermRunModeAction()
{
    if (!longTermRunModeDialog) {
        longTermRunModeDialog = std::make_unique<LongTermRunModeDialog>(this);
        connect(longTermRunModeDialog.get(), &QDialog::finished, this, &MainWindow::handleLongTermRunModeDialogDone);
    }
    longTermRunModeDialog->open();
}

void MainWindow::connectToDevice(const QString& port, const int baud, const bool showMsgOnOpenErr)
{
    serialPort->setPortName(port);
    serialPort->setBaudRate(baud);

    setWindowTitle(port);

    const auto [manufacturer, description] = getPortInfo(port);
    const QString portInfoText = port
        % " "
        % (!baud ? QString() : QStringLiteral("| ") + QString::number(baud))
        % (manufacturer.isEmpty() ? QString() : QStringLiteral("| ") + manufacturer)
        % (description.isEmpty() ? QString() : QStringLiteral("| ") + description);

    ui->portInfoLabel->setText(portInfoText);

    qInfo() << "Connecting to: " << port << baud;
    serialPort->clearError();
    if (serialPort->open(QIODevice::ReadOnly)) {
        ui->startStopButton->setEnabled(true);
        setProgramState(ProgramState::Started);
    } else {
        if (errno == EACCES) {

            bool isPermSetupCorrect = true;

            try {
                isPermSetupCorrect = isUserPermissionSetupCorrectly();
            } catch (std::runtime_error& e) {
                qCritical() << "Exception in permission check: " << e.what();
            }

            if (!isPermSetupCorrect) {
                auto username = qEnvironmentVariable("USER");
                if (username.isEmpty()) {
                    username = QStringLiteral("YOUR_USERNAME");
                }
                QMessageBox::critical(this, tr("Permission denied"),
                    tr("Permission denied when attempting to open ") % port % tr(". Add the current user to the dialout group by running the command given below and restart your system.\n\n")
                        % "sudo usermod -a -G dialout " % username);
                return;
            }

            // This can happen if the user added the account to the dialout group but hasn't restarted the system yet.

            // but it can be falsely triggered if the USB was just plugged in when we tried to open it.
            if (!isRecentlyEnumerated()) {

                qWarning() << "Permission denied, possibly not restarted";
                QMessageBox::critical(this, tr("Permission denied"),
                    tr("Permission denied when attempting to open ") % port % tr(".\nHave you restarted the system after adding the present user to dialout group?"));
                return;
            }
            qWarning() << "Permission denied, possibly just enumerated";
        }

        // We allow the user to open non-serial, static plain text files.
        QFile file(port);
        if (!file.open(QIODevice::ReadOnly) && showMsgOnOpenErr) {
            QMessageBox::warning(this,
                tr("Failed to open file"),
                tr("Failed to open file") % QStringLiteral(": ") % port % ' ' % QString::fromStdString(getErrorStr()));
        }
        doc->setText(file.readAll());
        ui->startStopButton->setEnabled(false);
    }
}

#ifdef SYSTEMD_AVAILABLE
void MainWindow::setInhibit(const bool enabled)
{
    if (!enabled) {
        if (inhibitFd == 0) {
            qWarning() << "Invalid systemd fd";
            return;
        }

        if (const auto result = ::close(inhibitFd); result) {
            qWarning() << "failed to close systemd fd: " << getErrorStr().c_str();
        };

        inhibitFd = 0;
        return;
    }

    sd_bus* bus {};

    auto result = sd_bus_open_system(&bus);
    if (result < 0) {
        qWarning() << "Failed to open systemd bus" << result;
        return;
    }

    int newFd {};
    sd_bus_message* reply {};
    sd_bus_error error {};

    result = sd_bus_call_method(bus, "org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager",
        "Inhibit", // Method to call
        &error, &reply,
        "ssss", // Types of function arguments. Four strings here
        "idle:sleep:shutdown", // What
        PROJECT_NAME, // Who
        tr("Serial communication in progress").toUtf8().constData(), // Why
        "block"); // Mode

    if (result <= 0) {
        qWarning() << "Failed to make systemd bus call" << result;
        return;
    }

    result = sd_bus_message_read_basic(reply, SD_BUS_TYPE_UNIX_FD, &newFd);
    if (result <= 0 || !newFd) {
        qWarning() << "failed to read systemd response" << result << newFd;
        return;
    }

    if (error.message || error.name) {
        qWarning() << "systemd Inhibit failed" << error.message << error.name;
        return;
    }

    inhibitFd = newFd;
}

#endif

std::string MainWindow::getErrorStr()
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
    std::array<char, 128> buffer;
    const auto* errStr = strerror_r(errno, buffer.data(), buffer.size());
    return errStr ? errStr : "";
}

void MainWindow::writeCompressedFile(const QByteArray& contents, const int counter)
{
    const auto contentsLen = contents.size();

    if (!contentsLen) {
        qWarning() << "Attempt to write empty file";
        return;
    }

    const auto curDate = QDateTime::currentDateTime();
    const auto filename = QString(QStringLiteral("%1/%2_%3_%4.txt.zst"))
                              .arg(longTermRunModePath, serialPort->portName(),
                                  curDate.toString("yyyy-MM-dd-hh_mm-ss"),
                                  (QStringLiteral("%1").arg(counter, 8, 10, QLatin1Char('0'))));

    qInfo() << "Saving" << filename << contentsLen;

    QFile file(filename);
    if (const auto result = file.open(QIODevice::WriteOnly | QIODevice::NewOnly); !result) {
        const auto msg = QString(QStringLiteral("Failed to open: %1 %2 %3")).arg(filename).arg(static_cast<int>(result)).arg(errno);
        qCritical() << msg;
        throw std::runtime_error(msg.toStdString());
    }

    if (!zstdCtx) {
        zstdCtx = ZSTD_createCCtx();
        if (!zstdCtx) {
            throw std::runtime_error("Failed to create zstd ctx");
        }
        validateZstdResult(ZSTD_CCtx_setParameter(zstdCtx, ZSTD_c_checksumFlag, 1));
        validateZstdResult(ZSTD_CCtx_setParameter(zstdCtx, ZSTD_c_strategy, ZSTD_fast));
        zstdOutBuffer.resize(ZSTD_CStreamOutSize()); // This returns approx 128KiB
    } else {
        validateZstdResult(ZSTD_CCtx_reset(zstdCtx, ZSTD_reset_session_only));
    }

    Q_ASSERT(contentsLen > 0);
    ZSTD_inBuffer input = { contents.data(), static_cast<size_t>(contentsLen), 0 };

    bool finished {};

    // NOLINTNEXTLINE(altera-id-dependent-backward-branch)
    while (!finished) {
        ZSTD_outBuffer out = { zstdOutBuffer.data(), zstdOutBuffer.size(), 0 };
        const auto remaining = ZSTD_compressStream2(zstdCtx, &out, &input, ZSTD_e_end);
        validateZstdResult(remaining);

        file.write(zstdOutBuffer.data(), static_cast<qint64>(out.pos));

        finished = (remaining == 0);
    }

    file.flush();
    fsync(file.handle());
    file.close();
}

void MainWindow::validateZstdResult(const size_t result, const std::experimental::source_location& srcLoc)
{
    if (ZSTD_isError(result)) {
        throw std::runtime_error(std::string("ZSTD error: ") + ZSTD_getErrorName(result) + " " + std::to_string(srcLoc.line()));
    }
}

QString MainWindow::getSerialPortPath() const
{
    return QString::fromLatin1(DEV_PREFIX.data(), DEV_PREFIX.size()) + serialPort->portName();
}

void MainWindow::closeSerialPort()
{
    if (serialPort->isOpen()) {
        serialPort->close();
    }
    setProgramState(ProgramState::Stopped);
}

std::pair<QString, QString> MainWindow::getPortInfo(QString portLocation)
{
    if (portLocation.startsWith(QString::fromLatin1(DEV_PREFIX.data(), DEV_PREFIX.size()))) {
        portLocation.remove(0, DEV_PREFIX.size());
    }
    const QSerialPortInfo portInfo(portLocation);

    return { portInfo.manufacturer(), portInfo.description() };
}

bool MainWindow::isUserPermissionSetupCorrectly()
{
    const auto username = qEnvironmentVariable("USER").toStdString();
    if (username.empty()) {
        throw std::runtime_error("Failed to read username");
    }

    const auto pwmnamBufferSize = sysconf(_SC_GETPW_R_SIZE_MAX);

    if (pwmnamBufferSize <= 0) {
        throw std::runtime_error("Failed to read pwmnambuf size");
    }

    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
    const auto pwnamBuffer = std::make_unique<char[]>(static_cast<size_t>(pwmnamBufferSize));
    struct passwd tmpPwd {};
    struct passwd* pwdResult {};

    if (getpwnam_r(username.c_str(), &tmpPwd, pwnamBuffer.get(), static_cast<size_t>(pwmnamBufferSize), &pwdResult)) {
        throw std::runtime_error("pwmnam failed");
    }

    if (!pwdResult) {
        throw std::runtime_error("pwmnam found no match");
    }

    constexpr size_t MAX_SIZE = 64;
    std::array<gid_t, MAX_SIZE> groupList {};
    int groupCount = MAX_SIZE;

    if (getgrouplist(username.c_str(), pwdResult->pw_gid, groupList.data(), &groupCount) < 0) {
        throw std::runtime_error("group size exceeded");
    }

    if (groupCount <= 0) {
        throw std::runtime_error("Failed to found group list");
    }

    const auto grnamBufferSize = sysconf(_SC_GETGR_R_SIZE_MAX);

    if (grnamBufferSize <= 0) {
        throw std::runtime_error("Failed to read grnambuf size");
    }

    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
    const auto grnamBuffer = std::make_unique<char[]>(static_cast<size_t>(grnamBufferSize));
    struct group tmpGroup {};
    struct group* groupResult {};

    if (getgrnam_r(GROUP_DIALOUT, &tmpGroup, grnamBuffer.get(), static_cast<size_t>(grnamBufferSize), &groupResult)) {
        throw std::runtime_error("grnam failed");
    }
    if (!groupResult) {
        throw std::runtime_error("grnam failed");
    }

    return std::any_of(groupList.begin(),
        groupList.begin() + groupCount,
        [&groupResult](const gid_t i) { return i == groupResult->gr_gid; });
}

bool MainWindow::isRecentlyEnumerated()
{
    const auto& portPath = getSerialPortPath();
    if (portPath.isEmpty()) {
        qWarning() << "port path error";
        return true;
    }

    struct statx fileStat {};
    const auto ba = portPath.toLocal8Bit();
    if (statx(0, ba.data(), 0, STATX_BTIME, &fileStat)) {
        return true;
    }

    return time(nullptr) - fileStat.stx_btime.tv_sec < 3;
}
