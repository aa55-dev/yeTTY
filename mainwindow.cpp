#include "mainwindow.h"
#include "./ui_mainwindow.h"
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
#include <malloc.h>
#include <stdexcept>
#include <tuple>
#include <unistd.h>
#include <utility>

#include <QApplication>
#include <QCoreApplication>
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
    , timer(new QTimer(this))
    , longTermRunModeTimer(new QTimer(this))
{
    const auto args = QApplication::arguments();

    QString portLocation;
    int baud {};
    QString manufacturer;
    QString description;

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
            if (portLocation.startsWith(QString::fromLatin1(DEV_PREFIX.data(), DEV_PREFIX.size()))) {
                const auto portName = portLocation.remove(0, DEV_PREFIX.size());
                const QSerialPortInfo portInfo(portLocation);
                manufacturer = portInfo.manufacturer();
                description = portInfo.description();
            }
        }
        break;

    default: // No args, show msgbox and get it from user
        const auto portInfo = getPortFromUser();
        portLocation = portInfo.location;
        manufacturer = portInfo.manufacturer;
        description = portInfo.description;
        baud = portInfo.baud;
    }

    elapsedTimer.start();
    ui->setupUi(this);

    editor = KTextEditor::Editor::instance();
    doc = editor->createDocument(this);
    doc->setHighlightingMode(HIGHLIGHT_MODE);

    view = doc->createView(this);
    view->setStatusBarEnabled(false);

    ui->verticalLayout->insertWidget(0, view);

    setWindowTitle(PROJECT_NAME);

    connectToDevice(portLocation, baud, true, manufacturer, description);

    connect(ui->actionConnectToDevice, &QAction::triggered, this, &MainWindow::handleConnectAction);
    ui->actionConnectToDevice->setShortcut(QKeySequence::Open);
    ui->actionConnectToDevice->setIcon(QIcon::fromTheme("document-open"));

    connect(ui->actionSave, &QAction::triggered, this, &MainWindow::handleSaveAction);
    ui->actionSave->setShortcut(QKeySequence::Save);
    ui->actionSave->setIcon(QIcon::fromTheme("document-save"));

    connect(ui->actionQuit, &QAction::triggered, this, &MainWindow::handleQuitAction);
    ui->actionQuit->setShortcut(QKeySequence::Quit);
    ui->actionQuit->setIcon(QIcon::fromTheme("application-exit"));

    connect(ui->actionClear, &QAction::triggered, this, &MainWindow::handleClearAction);
    ui->actionClear->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K));
    ui->actionClear->setIcon(QIcon::fromTheme("edit-clear-all"));

    connect(ui->scrollToEndButton, &QPushButton::pressed, this, &MainWindow::handleScrollToEnd);
    ui->scrollToEndButton->setIcon(QIcon::fromTheme("go-bottom"));

    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::handleAboutAction);
    ui->actionAbout->setIcon(QIcon::fromTheme("help-about"));

    connect(ui->actionTrigger, &QAction::triggered, this, &MainWindow::handleTriggerSetupAction);
    ui->actionTrigger->setIcon(QIcon::fromTheme("mail-thread-watch"));

    ui->actionLongTermRunMode->setIcon(QIcon::fromTheme("media-record"));
    connect(ui->actionLongTermRunMode, &QAction::triggered, this, &MainWindow::handleLongTermRunModeAction);

    connect(ui->startStopButton, &QPushButton::pressed, this, &MainWindow::handleStartStopButton);

    connect(serialPort, &QSerialPort::readyRead, this, &MainWindow::handleReadyRead);
    connect(serialPort, &QSerialPort::errorOccurred, this, &MainWindow::handleError);

    connect(timer, &QTimer::timeout, this, &MainWindow::handleRetryConnection);
    connect(longTermRunModeTimer, &QTimer::timeout, this, &MainWindow::handleLongTermRunModeTimer);

    connect(fsWatcher, &QFileSystemWatcher::fileChanged, this, &MainWindow::handleFileWatchEvent);

    sound->setSource(QUrl::fromLocalFile(":/notify.wav"));

    qDebug() << "Init complete in:" << elapsedTimer.elapsed();
}

MainWindow::~MainWindow()
{
    delete ui;
    ZSTD_freeCCtx(zstdCtx);
    zstdCtx = nullptr;
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
        if (serialErrorMsg) {
            serialErrorMsg->deleteLater();
        }
        ui->startStopButton->setText("&Stop");
        ui->startStopButton->setIcon(QIcon::fromTheme("media-playback-stop"));

        Q_ASSERT(fsWatcher->files().isEmpty());
        fsWatcher->addPath(getSerialPortPath());

    } else if (newState == ProgramState::Stopped) {
        qInfo() << "Program stopped";
        ui->startStopButton->setText("&Start");
        ui->startStopButton->setIcon(QIcon::fromTheme("media-playback-start"));

        fsWatcher->removePaths(fsWatcher->files());
    } else {
        throw std::runtime_error("Unknown state");
    }

    currentProgramState = newState;
}

PortSelectionDialog::PortInfo MainWindow::getPortFromUser()
{
    PortSelectionDialog dlg;
    if (!dlg.exec()) {
        qInfo() << "No port selection made";
        throw std::runtime_error("No selection made");
    }
    selectedPortInfo = dlg.getSelectedPortInfo();
    return selectedPortInfo;
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
        for (const auto i : newData) {
            if (i == '\n') {
                if (triggerSearchLine.contains(triggerKeyword)) {
                    triggerMatchCount++;
                    ui->statusbar->showMessage(QString("%1 matches").arg(triggerMatchCount), 3000);

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

    if (serialPort->isOpen()) {
        serialPort->close();
    }

    auto errMsg = "Error: " + QVariant::fromValue(error).toString();

    const auto portName = getSerialPortPath();

    if (!QFile::exists(portName)) {
        errMsg += QString(": %1 detached").arg(portName);
    }
    qCritical() << "Serial port error: " << error;
    ui->statusbar->showMessage(errMsg);
    if (!serialErrorMsg) {
        serialErrorMsg = new KTextEditor::Message(errMsg, KTextEditor::Message::Error); // NOLINT(cppcoreguidelines-owning-memory)
    }

    if (errMsg != serialErrorMsg->text()) {
        serialErrorMsg->setText(errMsg);
        doc->postMessage(serialErrorMsg);
    }
    setProgramState(ProgramState::Stopped);

    if (!timer->isActive()) {
        // Lets try to reconnect after a while
        timer->start(1000);
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
    QMessageBox::about(this, "About", QString("%1\t\t\n%2\t").arg(PROJECT_NAME, PROJECT_VERSION));
}

void MainWindow::handleConnectAction()
{
    serialPort->close();
    timer->stop();
    const auto [location, manufacturer, description, baud] = getPortFromUser();
    handleClearAction();
    connectToDevice(location, baud, true, manufacturer, description);
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
        qInfo() << "Closing connection on button press";
        serialPort->close();
        setProgramState(ProgramState::Stopped);
    } else {
        qInfo() << "Starting connection on button press";
        connectToDevice(serialPort->portName(), serialPort->baudRate());
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
        timer->stop();
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

            qInfo() << "Long term run mode enabled:" << longTermRunModeMaxMemory << longTermRunModeMaxTime << longTermRunModePath;
        } else {
            qInfo() << "Long term run mode disabled";
            fileCounter = 0;
            longTermRunModeTimer->stop();
        }
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
        Q_ASSERT(!longTermRunModePath.isEmpty());

        longTermRunModeStartTime = elapsedTimer.elapsed();

        const auto utfTxt = doc->text().toUtf8();
        writeCompressedFile(utfTxt, fileCounter++);

        handleClearAction();
    }
}

void MainWindow::handleFileWatchEvent(const QString& path)
{
    if (!QFile::exists(path) && serialPort->isOpen()) {
        Q_ASSERT(path == getSerialPortPath());
        handleError(QSerialPort::SerialPortError::ResourceError);
    }
}

void MainWindow::handleLongTermRunModeAction()
{
    if (!longTermRunModeDialog) {
        longTermRunModeDialog = std::make_unique<LongTermRunModeDialog>(this);
        connect(longTermRunModeDialog.get(), &QDialog::finished, this, &MainWindow::handleLongTermRunModeDialogDone);
    }
    longTermRunModeDialog->open();
}

void MainWindow::connectToDevice(const QString& port, const int baud, const bool showMsgOnOpenErr, const QString& manufacturer, const QString& description)
{
    serialPort->setPortName(port);
    serialPort->setBaudRate(baud);

    setWindowTitle(port);

    const QString portInfoText = port
        % " "
        % (!baud ? QString() : "| " + QString::number(baud))
        % (manufacturer.isEmpty() ? QString() : "| " + manufacturer)
        % (description.isEmpty() ? QString() : "| " + description);

    ui->portInfoLabel->setText(portInfoText);

    qInfo() << "Connecting to: " << port << baud;
    serialPort->clearError();
    if (serialPort->open(QIODevice::ReadOnly)) {
        ui->startStopButton->setEnabled(true);
        ui->statusbar->showMessage("Running...");
        setProgramState(ProgramState::Started);
    } else {
        // We allow the user to open non-serial, static plain text files.
        qInfo() << "Opening as ordinary file";
        QFile file(port);
        if (!file.open(QIODevice::ReadOnly) && showMsgOnOpenErr) {
            QMessageBox::warning(this,
                tr("Failed to open file"),
                tr("Failed to open file") + ": " + port + ' ' + QString::fromStdString(getErrorStr()));
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

        if (auto result = ::close(inhibitFd); !result) {
            qWarning() << "failed to close systemd fd: " << getErrorStr();
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
    return strerror_r(errno, buffer.data(), buffer.size());
}

void MainWindow::writeCompressedFile(const QByteArray& contents, const int counter)
{
    const auto contentsLen = contents.size();

    if (!contentsLen) {
        qWarning() << "Attempt to write empty file";
        return;
    }

    // In case this function gets called multiple times rapidly, the `currentDateTime()` function
    // will return the same value and therefore we will attempt to use the same filename more than
    // once. In order to prevent this we prepend the filename with an incrementing counter.
    const auto filename = QString("%1/%2_%3.txt.zst")
                              .arg(longTermRunModePath,
                                  QDateTime::currentDateTime().toString(Qt::DateFormat::ISODate),
                                  (QStringLiteral("%1").arg(counter, 8, 10, QLatin1Char('0'))));

    qInfo() << "Saving" << filename << contentsLen;

    QFile file(filename);
    if (const auto result = file.open(QIODevice::WriteOnly | QIODevice::NewOnly); !result) {
        const auto msg = QString("Failed to open: %1 %2 %3").arg(filename).arg(static_cast<int>(result)).arg(errno);
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

void MainWindow::validateZstdResult(const size_t result, const std::experimental::source_location srcLoc)
{
    if (ZSTD_isError(result)) {
        throw std::runtime_error(std::string("ZSTD error: ") + ZSTD_getErrorName(result) + " " + std::to_string(srcLoc.line()));
    }
}

QString MainWindow::getSerialPortPath() const
{
    return QString::fromLatin1(DEV_PREFIX.data(), DEV_PREFIX.size()) + serialPort->portName();
}
