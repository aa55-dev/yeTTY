#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "autobauddetection.h"
#include "common.hpp"
#include "dbus_common.hpp"
#include "longtermrunmodedialog.h"
#include "portalreadyinusedialog.h"
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
#include <iostream>
#include <malloc.h>
#include <pwd.h>
#include <random>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

#include <QApplication>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileSystemWatcher>
#include <QIODevice>
#include <QKeyEvent>
#include <QMainWindow>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QSettings>
#include <QSocketNotifier>
#include <QSoundEffect>
#include <QString>
#include <QStringBuilder>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#ifdef SYSTEMD_AVAILABLE
#include <systemd/sd-bus.h>
#endif

MainWindow::MainWindow(const std::optional<std::tuple<SourceType, QString, int>>& portParams, QWidget* parent)
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
    QString portLocation;
    int baud {};

    if (portParams.has_value()) {
        std::tie(srcType, portLocation, baud) = portParams.value();
    } else {
        std::tie(portLocation, baud) = getPortFromUser();
        srcType = SourceType::Serial;
    }

    Q_ASSERT(!portLocation.isEmpty());
    if (srcType != SourceType::Stdin) {
        Q_ASSERT(baud > 0);
    } else {
        Q_ASSERT(baud < 0);
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

    if (srcType == SourceType::Stdin) {
        connectToStdin();
    } else {
        Q_ASSERT(srcType == SourceType::Serial);
        connectToSerialDevice(portLocation, baud);
    }

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

    ui->actionAuto_baud_rate_detection->setIcon(QIcon::fromTheme(QStringLiteral("search")));
    connect(ui->actionAuto_baud_rate_detection, &QAction::triggered, this, &MainWindow::handleAutoBaudRateDetection);

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
    if (!connection.registerService(DBUS_SERVICE_NAME + (QStringLiteral(".id-") + QString::number(getRandomNumber())))) {
        qWarning() << "Failed to register dbus service";
    }
    connection.registerObject(QStringLiteral("/"), DBUS_INTERFACE_NAME, this, QDBusConnection::ExportScriptableSlots);

    ui->LTRTextLabel->setVisible(false);
    ui->LTRIconLabel->setVisible(false);
    const auto size = ui->LTRIconLabel->size();
    ui->LTRIconLabel->setPixmap(QIcon::fromTheme(QStringLiteral("media-record")).pixmap(QSize(size.width() / 2, size.height())));

    ui->autoRetryTextLabel->setVisible(false);
    ui->autoRetryCancelButton->setVisible(false);
    ui->autoRetryTextLabel->setText(QStringLiteral("Attempting to reconnect to port"));
    connect(ui->autoRetryCancelButton, &QPushButton::pressed, this, &MainWindow::handleCancelAutoRetry);

    inactivityTimer = new QTimer(this); // NOLINT(cppcoreguidelines-owning-memory)
    connect(inactivityTimer, &QTimer::timeout, this, &MainWindow::executeTriggerAction);

    qDebug() << "Init complete in:" << elapsedTimer.elapsed();
}

MainWindow::~MainWindow()
{
    const auto portName = serialPort->portName();
    if (!portName.isEmpty()) {
        QSettings settings;
        const auto baud = QString::number(serialPort->baudRate());
        Q_ASSERT(!baud.isEmpty());
        settings.setValue(SETTINGS_LAST_USED_PORT, QStringList { serialPort->portName(), baud });
    }
    delete ui;
    ZSTD_freeCCtx(zstdCtx);
    zstdCtx = nullptr;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void MainWindow::control(const QString& port, const QString& action, QString& out)
{
    qInfo() << "DBus" << port << action;
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

void MainWindow::handleNewData(QByteArray newData)
{
    // Need to remove '\0' from the input or else we might mess up the text shown or
    // affect string operation downstream
    newData.replace('\0', ' ');

    processTriggers(newData);

    doc->setReadWrite(true);
    doc->insertText(doc->documentEnd(), newData);
    doc->setReadWrite(false);
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

        if (srcType == SourceType::Serial) {
            Q_ASSERT(fsWatcher->files().isEmpty());
            fsWatcher->addPath(getSerialPortPath());
        }

    } else if (newState == ProgramState::Stopped) {
        qInfo() << "Program stopped";
        statusBarText->setText(QStringLiteral("Stopped"));
        ui->startStopButton->setText(QStringLiteral("&Start"));
        ui->startStopButton->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")));

        if (srcType == SourceType::Serial) {
            fsWatcher->removePaths(fsWatcher->files());
        }
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
    handleNewData(serialPort->readAll());
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
    // Only log once when auto retrying
    if (autoRetryCounter <= 1) {
        qWarning() << "Serial port error: " << error;
    }
    statusBarText->setText(errMsg);

    if (!serialPortErrMsgShown) {
        serialPortErrMsg = new KTextEditor::Message(errMsg, KTextEditor::Message::Error); // NOLINT(cppcoreguidelines-owning-memory)
        connect(serialPortErrMsg, &KTextEditor::Message::closed, this, [this](KTextEditor::Message*) { serialPortErrMsgActive = false; });
        doc->postMessage(serialPortErrMsg);
        serialPortErrMsgShown = serialPortErrMsgActive = true;
    }

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

void MainWindow::handleClearAction(const bool force)
{
    if (longTermRunModeEnabled && !force) {
        QMessageBox::critical(this, QStringLiteral("Long term run mode active"),
            QStringLiteral("Long term run mode is active, please disable it before attempting to clear text"));
        return;
    }
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
    if (longTermRunModeEnabled) {
        QMessageBox::critical(this, QStringLiteral("Long term run mode active"),
            QStringLiteral("Long term run mode is active, please disable it before opening new port"));
        return;
    }
    stop();
    autoRetryTimer->stop();
    try {
        const auto [location, baud] = getPortFromUser();
        handleClearAction();
        srcType = SourceType::Serial;
        connectToSerialDevice(location, baud);
    } catch (std::runtime_error& e) {
        QMessageBox::critical(this, QStringLiteral("Error"), e.what());
    }
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
        triggerType = triggerSetupDialog->getTriggerType();

        if (triggerType == TriggerSetupDialog::TriggerType::StringMatch) {
            const auto newKeyword = triggerSetupDialog->getKeyword().toUtf8();
            if (newKeyword != triggerKeyword) {
                qInfo() << "Setting new trigger keyword: " << triggerKeyword << " " << newKeyword;
                triggerKeyword = newKeyword;
                triggerMatchCount = 0;
            }
        }

        if (triggerType == TriggerSetupDialog::TriggerType::Inactivity) {
            inactivityTimer->start(INACTIVITY_TIMEOUT);
        } else {
            inactivityTimer->stop();
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

void MainWindow::stopAutoRetryTimer(const bool deleteMsg)
{
    ui->autoRetryCancelButton->setVisible(false);
    ui->autoRetryTextLabel->setVisible(false);
    autoRetryTimer->stop();
    if (serialPortErrMsgActive) {
        serialPortErrMsgActive = false;
        if (serialPortErrMsg && deleteMsg) {
            serialPortErrMsg->deleteLater();
        }
    }
    serialPortErrMsgShown = false;
    autoRetryCounter = 0;
}

void MainWindow::handleRetryConnection()
{
    Q_ASSERT(srcType == SourceType::Serial);
    if (!ui->autoRetryTextLabel->isVisible()) {
        ui->autoRetryTextLabel->setVisible(true);
        ui->autoRetryCancelButton->setVisible(true);
        qInfo() << "Auto reconnect started";
    }

    if (!serialPort->isOpen()) {
        autoRetryCounter++;
        const auto prevSerial = serialNumber;
        const auto prevManufacturer = manufacturer;
        const auto prevDescription = description;

        connectToSerialDevice(serialPort->portName(), serialPort->baudRate(), false);

        if (serialPort->isOpen()) {
            // This can happen if the user plugged in a new serial device.
            if (serialNumber != prevSerial || manufacturer != prevManufacturer || description != prevDescription) {
                stop();

                const auto msg = QStringLiteral("Serial port info mismatch on %1\nBefore: %2, %3, %4\nNow: %5, %6, %7")
                                     .arg(serialPort->portName(),
                                         prevManufacturer, prevDescription, prevSerial,
                                         manufacturer, description, serialNumber);

                QMessageBox::critical(this, QStringLiteral("Serial port info mismatch"), msg);
            }

            auto* msg = new KTextEditor::Message(QStringLiteral("Reconnected to port"), KTextEditor::Message::Positive);
            msg->setAutoHide(2000);
            doc->postMessage(msg);
            stopAutoRetryTimer();
        } else {
            if (autoRetryCounter % 10 == 0) {
                qInfo() << "Auto reconnect attempt " << autoRetryCounter;
            }
            statusBarText->setText(QStringLiteral("Auto reconnect attempts: %1").arg(autoRetryCounter));
        }
    } else {
        // This should not happen since we stop the timer above
        qWarning() << "retry timer still active";
        stopAutoRetryTimer();
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
            ui->LTRTextLabel->setText(QStringLiteral("Long term run mode active (location: %1)")
                    .arg(longTermRunModePath));
        } else {
            qInfo() << "Long term run mode disabled";
            fileCounter = 0;
            errCtr = 0;
            longTermRunModeTimer->stop();
        }

        ui->LTRIconLabel->setVisible(longTermRunModeEnabled);
        ui->LTRTextLabel->setVisible(longTermRunModeEnabled);
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
        if (utfTxt.isEmpty()) {
            qInfo() << "Nothing to save";
            return;
        }
        try {
            writeCompressedFile(utfTxt);
            handleClearAction(true);
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

        const QString errStr = errCtr ? (QStringLiteral(" <b>(%1 errors)</b>").arg(QString::number(errCtr))) : QLatin1String("");

        ui->LTRTextLabel->setText(QStringLiteral("Long term run mode active (%1 files%2 saved in %3)")
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

void MainWindow::handleCancelAutoRetry()
{
    stopAutoRetryTimer(false);
    statusBarText->setText(QStringLiteral("Port disconnected, auto retry cancelled"));
}

void MainWindow::handleAutoBaudRateDetection()
{
    auto abd = std::make_unique<AutoBaudDetection>(this);
    abd->exec();
}

void MainWindow::start()
{
    switch (srcType) {
    case SourceType::Serial:
        connectToSerialDevice(serialPort->portName(), serialPort->baudRate());
        break;
    case SourceType::Stdin:
        connectToStdin();
        break;
    case SourceType::Unknown:
        [[fallthrough]];
    default:
        Q_ASSERT(false);
        break;
    }
}

void MainWindow::stop()
{
    switch (srcType) {
    case SourceType::Serial:
        closeSerialPort();
        break;
    case SourceType::Stdin:
        closeStdin();
        break;
    case SourceType::Unknown:
        [[fallthrough]];
    default:
        Q_ASSERT(false);
        break;
    }
    setProgramState(ProgramState::Stopped);
}

void MainWindow::handleLongTermRunModeAction()
{
    if (!longTermRunModeDialog) {
        longTermRunModeDialog = std::make_unique<LongTermRunModeDialog>(this);
        connect(longTermRunModeDialog.get(), &QDialog::finished, this, &MainWindow::handleLongTermRunModeDialogDone);
    }
    longTermRunModeDialog->open();
}

void MainWindow::handleSocketNotifierActivated(QSocketDescriptor socket, QSocketNotifier::Type)
{
    if (!socket.isValid()) {
        stop();
        return;
    }

    if (std::string line; std::getline(std::cin, line)) {
        handleNewData((line + "\n").c_str());
    } else {
        // Handle EOF in input
        std::cin.clear();
        if (freopen("/dev/tty", "r", stdin) == nullptr) { // NOLINT(cppcoreguidelines-owning-memory)
            qCritical() << "Failed to reopen stdin" << errno;
        }
        qInfo() << std::cin.good() << std::cin.eof();
    }
}

void MainWindow::connectToStdin()
{
    Q_ASSERT(sockNotifier == nullptr);

    sockNotifier = new QSocketNotifier(fileno(stdin), QSocketNotifier::Read, this); // NOLINT(cppcoreguidelines-owning-memory)
    connect(sockNotifier, &QSocketNotifier::activated, this, &MainWindow::handleSocketNotifierActivated);
    setProgramState(ProgramState::Started);

    ui->portInfoLabel->setText(QStringLiteral("stdin"));
}

void MainWindow::connectToSerialDevice(const QString& port, const int baud, const bool showMsgOnOpenErr)
{
    Q_ASSERT(srcType == SourceType::Serial);
    serialPort->setPortName(port);
    serialPort->setBaudRate(baud);

    setWindowTitle(port);

    const auto [tmpManufacturer, tmpDescription, tmpSerialNumber] = getPortInfo(port);
    const QString portInfoText = port
        % " "
        % (!baud ? QString() : QStringLiteral("| ") + QString::number(baud))
        % (tmpManufacturer.isEmpty() ? QString() : QStringLiteral("| ") + tmpManufacturer)
        % (tmpDescription.isEmpty() ? QString() : QStringLiteral("| ") + tmpDescription);

    ui->portInfoLabel->setText(portInfoText);

    // Only log once when auto retrying
    if (autoRetryCounter <= 1) {
        qInfo() << "Connecting to: " << port << baud;
    }
    serialPort->clearError();

    if (serialPort->open(QIODevice::ReadOnly)) {
        manufacturer = tmpManufacturer;
        description = tmpDescription;
        serialNumber = tmpSerialNumber;
        ui->startStopButton->setEnabled(true);
        setProgramState(ProgramState::Started);
        return;
    }

    if (errno == EACCES) {
        // Handle permission errors
        handlePortAccessError(port);
    } else if (errno == EBUSY) {
        // Handle port already in use by another process
        const auto result = handlePortBusy(port);
        if (result) {
            QTimer::singleShot(0, Qt::PreciseTimer, this,
                [this, port, baud, showMsgOnOpenErr]() { connectToSerialDevice(port, baud, showMsgOnOpenErr); });
        }
    } else {
        // Handle unknown error
        if (showMsgOnOpenErr) {
            static constexpr const char* ERR_MSG = "Failed to open serial port";
            QMessageBox::warning(this, ERR_MSG, ERR_MSG % QStringLiteral(" %1: ").arg(port) % QString::fromStdString(getErrorStr()));
        }
    }
    ui->startStopButton->setEnabled(false);
}

std::tuple<QString, QString> MainWindow::findProcessUsingPort(const QString& portName)
{
    static constexpr const char* PROC = "/proc/";

    const QDir procDir(PROC);
    const auto procList = procDir.entryList(QDir::Dirs | QDir::Filter::NoDotAndDotDot);

    for (const auto& pid : procList) {
        bool ok {};
        pid.toInt(&ok);
        if (!ok) {
            continue;
        }

        const QString fdDirPath = PROC % pid % "/fd/";
        const QDir fdDir(fdDirPath);

        const auto fdList = fdDir.entryList(QDir::Filter::NoDotAndDotDot);
        if (fdList.empty()) {
            continue;
        }

        for (const auto& fileDescriptor : fdList) {
            const QString fullPath = fdDirPath + fileDescriptor;

            std::array<char, 64> filePath {};
            const auto result = readlink(fullPath.toLocal8Bit(), filePath.data(), filePath.size() - 1);
            if (result < 0 || result == filePath.size() - 1) {
                continue;
            }
            if (filePath.data() != portName) {
                continue;
            }

            QFile command(PROC % pid % "/cmdline");
            if (!command.open(QIODevice::ReadOnly)) {
                continue;
            }

            const auto fileContents = QString::fromUtf8(command.readAll());
            if (fileContents.isEmpty()) {
                continue;
            }

            auto argsList = fileContents.split('\0');

            if (argsList.isEmpty()) {
                continue;
            }

            argsList.replace(0, argsList.at(0).toLower());

            return { pid, argsList.join(QLatin1String(" ")).trimmed() };
        }
    }

    return {};
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

void MainWindow::executeTriggerAction()
{
    switch (triggerSetupDialog->getTriggerActionType()) {
    case TriggerSetupDialog::TriggerActionType::PlaySound:
        audioAlert();
        break;
    case TriggerSetupDialog::TriggerActionType::ExecuteCommand: {
        auto parts = triggerSetupDialog->getTriggerActionCommand().split(' ');
        Q_ASSERT(!parts.isEmpty());

        auto* process = new QProcess(this); // NOLINT(cppcoreguidelines-owning-memory)
        process->setProgram(parts.at(0));

        if (parts.size() > 1) {
            parts.removeFirst();
            process->setArguments(parts);
        }

        connect(process, &QProcess::finished, process, &QObject::deleteLater);
        process->start();

    } break;
    default:
        Q_ASSERT(false);
    }
}

void MainWindow::audioAlert()
{
    if (!sound->isPlaying()) {
        sound->play();
    }
}

std::string MainWindow::getErrorStr()
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
    std::array<char, 128> buffer;
    const auto* errStr = strerror_r(errno, buffer.data(), buffer.size());
    return errStr ? errStr : "";
}

void MainWindow::writeCompressedFile(const QByteArray& contents)
{
    const auto contentsLen = contents.size();

    if (!contentsLen) {
        qWarning() << "Attempt to write empty file";
        return;
    }

    QString portName = QStringLiteral("yetty");

    switch (srcType) {
    case SourceType::Serial:
        portName = serialPort->portName();
        break;
    case SourceType::Stdin:
        portName = QStringLiteral("stdin");
        break;
    case SourceType::Unknown:
        [[fallthrough]];
    default:
        Q_ASSERT(false);
        break;
    }

    const auto curDate = QDateTime::currentDateTime();
    const auto filename = QString(QStringLiteral("%1/%2_%3_%4.txt.zst"))
                              .arg(longTermRunModePath, portName,
                                  curDate.toString(QStringLiteral("yyyy-MM-dd-hh_mm-ss")),
                                  (QStringLiteral("%1").arg(fileCounter, 8, 10, QLatin1Char('0'))));

    qInfo() << "Saving" << filename << contentsLen << "bytes";

    QFile file(filename);
    if (const auto result = file.open(QIODevice::WriteOnly | QIODevice::NewOnly); !result) {
        const auto msg = QString(QStringLiteral("Failed to open: %1 %2")).arg(filename, file.errorString());
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
    fileCounter++;
}

void MainWindow::validateZstdResult(const size_t result, const std::experimental::source_location& srcLoc)
{
    if (ZSTD_isError(result)) {
        throw std::runtime_error(std::string("ZSTD error: ") + ZSTD_getErrorName(result) + " " + std::to_string(srcLoc.line()));
    }
}

QString MainWindow::getSerialPortPath() const
{
    Q_ASSERT(srcType == SourceType::Serial);
    return QString::fromLatin1(DEV_PREFIX.data(), DEV_PREFIX.size()) + serialPort->portName();
}

void MainWindow::closeStdin()
{
    Q_ASSERT(srcType == SourceType::Stdin);
    sockNotifier->setEnabled(false);
    delete sockNotifier;
    sockNotifier = nullptr;
}

void MainWindow::closeSerialPort()
{
    Q_ASSERT(srcType == SourceType::Serial);
    if (serialPort->isOpen()) {
        serialPort->close();
    }
}

std::tuple<QString, QString, QString> MainWindow::getPortInfo(QString portLocation)
{
    if (portLocation.startsWith(QString::fromLatin1(DEV_PREFIX.data(), DEV_PREFIX.size()))) {
        portLocation.remove(0, DEV_PREFIX.size());
    }
    const QSerialPortInfo portInfo(portLocation);

    return { portInfo.manufacturer(), portInfo.description(), portInfo.serialNumber() };
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
    Q_ASSERT(srcType == SourceType::Serial);
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

bool MainWindow::handlePortBusy(const QString& port)
{
    const auto [pid, command] = findProcessUsingPort(port);

    qInfo() << pid << command;
    if (pid.isEmpty() || command.isEmpty()) {
        // The port is probably in use by another user
        QMessageBox::critical(this, QStringLiteral("Failed to open port"), QStringLiteral("Port is already in use by another process"));
        return false;
    }
    auto* const dlg = new PortAlreadyInUseDialog(this, pid, command); // NOLINT(cppcoreguidelines-owning-memory)
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    if (dlg->exec() != QDialog::Accepted) {
        return false;
    }

    const auto [pidNow, commandNow] = findProcessUsingPort(port);
    if (pidNow != pid || commandNow != command) {
        qWarning() << "process mismatch" << pid << pidNow << command << commandNow;
        return false;
    }
    bool ok {};
    const auto pidInt = pidNow.toInt(&ok, 10);
    if (!ok || pidInt <= 1) {
        qWarning() << "Failed to convert PID" << pidNow;
        return false;
    }
    qInfo() << "Killing " << pidInt;
    if (kill(pidInt, SIGTERM) < 0) {
        qWarning() << "Failed to kill" << errno;
        return false;
    }

    return true;
}

void MainWindow::handlePortAccessError(const QString& port)
{
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
            tr("Permission denied when attempting to open ") % port % tr(". Add the current user to the dialout group by "
                                                                         "running the command given below and restart your system.\n\n")
                % "sudo usermod -a -G dialout " % username);
        return;
    }

    // This can happen if the user added the account to the dialout group but hasn't restarted the system yet
    // but it can be falsely triggered if the USB was just plugged in when we tried to open it.
    if (!isRecentlyEnumerated()) {
        qWarning() << "Permission denied, possibly not restarted";
        QMessageBox::critical(this, tr("Permission denied"),
            tr("Permission denied when attempting to open ") % port % tr(".\nHave you restarted the system after adding the present user to dialout group?"));
        return;
    }
    qWarning() << "Permission denied, possibly just enumerated";
}

int MainWindow::stringMatchCount(QByteArray haystack, const QByteArray& needle)
{
    qsizetype idx {};
    int count {};

    while ((idx = haystack.indexOf(needle)) >= 0) {
        count++;
        haystack.slice(idx + needle.size());
    }

    return count;
}

void MainWindow::processTriggers(const QByteArray& newData)
{
    if (triggerType == TriggerSetupDialog::TriggerType::Disabled) {
        return;
    }

    if (triggerType == TriggerSetupDialog::TriggerType::StringMatch) {
        triggerSearchLine.append(newData);

        if (triggerSearchLine.contains(triggerKeyword)) {
            triggerMatchCount += stringMatchCount(triggerSearchLine, triggerKeyword);
            statusBarText->setText(QStringLiteral("<b>%1 matches for %2</b>").arg(triggerMatchCount).arg(triggerKeyword.data()));
            statusBarTimer->start(5000);

            executeTriggerAction();
            triggerSearchLine.resize(0);
        }

        if (const auto lastIdx = triggerSearchLine.lastIndexOf('\n'); lastIdx > 0) {
            triggerSearchLine.slice(lastIdx + 1);
        }

    } else if (triggerType == TriggerSetupDialog::TriggerType::Activity) {
        executeTriggerAction();
    } else if (triggerType == TriggerSetupDialog::TriggerType::Inactivity) {
        inactivityTimer->start(INACTIVITY_TIMEOUT);
    }
}

int MainWindow::getRandomNumber()
{
    std::random_device rd;
    std::mt19937 mt(rd());

    std::uniform_int_distribution<int> distribution(0, std::numeric_limits<int>::max());
    return distribution(mt);
}
