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
#include <kstandardshortcut.h>

#include <zstd.h>

#include <malloc.h>
#include <unistd.h>

#include <QApplication>
#include <QDateTime>
#include <QFile>
#include <QKeyEvent>
#include <QMessageBox>
#include <QSerialPortInfo>
#include <QSoundEffect>
#include <QTimer>
#include <QVBoxLayout>

#ifdef SYSTEMD_AVAILABLE
#include <systemd/sd-bus.h>
#endif

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , serialPort(new QSerialPort(this))
    , sound(new QSoundEffect(this))
    , timer(new QTimer(this))
    , longTermRunModeTimer(new QTimer(this))
{
    const auto args = QApplication::arguments();

    QString portname {};
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
        portname = args[1];
        break;

    default: // No args, show msgbox and get it from user
        std::tie(portname, baud) = getPortFromUser();
    }

    elapsedTimer.start();
    ui->setupUi(this);

    editor = KTextEditor::Editor::instance();
    doc = editor->createDocument(this);
    doc->setHighlightingMode(HIGHLIGHT_MODE);

    view = doc->createView(this);

    ui->verticalLayout->insertWidget(0, view);

    setWindowTitle(PROJECT_NAME);

    connectToDevice(portname, baud);

    connect(ui->actionConnectToDevice, &QAction::triggered, this, &MainWindow::handleConnectAction);
    ui->actionConnectToDevice->setShortcut(QKeySequence::Open);
    ui->actionConnectToDevice->setIcon(QIcon::fromTheme("document-open"));

    connect(ui->actionSave, &QAction::triggered, this, &MainWindow::handleSaveAction);
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
        ui->startStopButton->setText("Stop");
        ui->startStopButton->setIcon(QIcon::fromTheme("media-playback-stop"));
    } else if (newState == ProgramState::Stopped) {
        qInfo() << "Program stopped";
        ui->startStopButton->setText("Start");
        ui->startStopButton->setIcon(QIcon::fromTheme("media-playback-start"));
    } else {
        throw std::runtime_error("Unknown state");
    }

    currentProgramState = newState;
}

std::pair<QString, int> MainWindow::getPortFromUser() const
{
    PortSelectionDialog dlg;
    if (!dlg.exec()) {
        qInfo() << "No port selection made";
        throw std::runtime_error("No selection made");
    }
    const auto baud = dlg.getSelectedBaud();
    return { dlg.getSelectedPortLocation(), baud };
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

    const auto& portName = "/dev/" + serialPort->portName();

    if (!QFile::exists(portName)) {
        errMsg += QString(": %1 detached").arg(portName);
    }
    qCritical() << "Serial port error: " << error;
    ui->statusbar->showMessage(errMsg);
    if (!serialErrorMsg) {
        serialErrorMsg = new KTextEditor::Message(errMsg, KTextEditor::Message::Error);
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
    doc->setHighlightingMode(HIGHLIGHT_MODE);
    malloc_trim(0);
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
    QCoreApplication::postEvent(view, new QKeyEvent(QEvent::KeyPress, Qt::Key_End, Qt::ControlModifier));
}

void MainWindow::handleAboutAction()
{
    QMessageBox::about(this, "About", QString("%1\t\t\n%2\t").arg(PROJECT_NAME, PROJECT_VERSION));
}

void MainWindow::handleConnectAction()
{
    serialPort->close();
    const auto [port, baud] = getPortFromUser();
    handleClearAction();
    connectToDevice(port, baud);
}

void MainWindow::handleTriggerSetupAction()
{
    if (!triggerSetupDialog) {
        triggerSetupDialog = new TriggerSetupDialog(this);
        connect(triggerSetupDialog, &QDialog::finished, this, &MainWindow::handleTriggerSetupDialogDone);
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
    if (serialPort->error() != QSerialPort::NoError) {
        qInfo() << "Retrying connection";
        connectToDevice(serialPort->portName(), serialPort->baudRate(), false);
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
    if (timeSinceLastSave > (longTermRunModeMaxTime * 60 * 1000)) {
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

void MainWindow::handleLongTermRunModeAction()
{
    if (!longTermRunModeDialog) {
        longTermRunModeDialog = new LongTermRunModeDialog(this);
        connect(longTermRunModeDialog, &QDialog::finished, this, &MainWindow::handleLongTermRunModeDialogDone);
    }
    longTermRunModeDialog->open();
}

void MainWindow::connectToDevice(const QString& port, const int baud, const bool showMsgOnOpenErr)
{
    serialPort->setPortName(port);
    serialPort->setBaudRate(baud);

    setWindowTitle(PROJECT_NAME + QString(" ") + port);
    ui->portInfoLabel->setText(QString("%1 │ %2").arg(serialPort->portName(), QString::number(serialPort->baudRate())));

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
                tr("Failed to open file") + ": " + port + ' ' + strerror(errno));
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
            qWarning() << "failed to close systemd fd" << strerror(errno);
        };

        inhibitFd = 0;
        return;
    }

    sd_bus* bus {};

    auto result = sd_bus_open_system(&bus);
    if (result < 0) {
        qWarning() << "Failed to open bus" << result;
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
        qWarning() << "Failed to make bus call" << result;
        return;
    }

    result = sd_bus_message_read_basic(reply, SD_BUS_TYPE_UNIX_FD, &newFd);
    if (result <= 0 || !newFd) {
        qWarning() << "failed to read response" << result << newFd;
        return;
    }

    if (error.message || error.name) {
        qWarning() << "Inhibit failed" << error.message << error.name;
        return;
    }

    qInfo() << "Inhibted";
    inhibitFd = newFd;
}
#endif

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
    do {
        ZSTD_outBuffer out = { zstdOutBuffer.data(), zstdOutBuffer.size(), 0 };
        const auto remaining = ZSTD_compressStream2(zstdCtx, &out, &input, ZSTD_e_end);
        validateZstdResult(remaining);

        file.write(zstdOutBuffer.data(), static_cast<qint64>(out.pos));

        finished = (remaining == 0);

    } while (!finished);

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
