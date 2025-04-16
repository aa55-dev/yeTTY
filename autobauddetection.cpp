#include "autobauddetection.h"
#include "ui_autobauddetection.h"

#include <cctype>

#include <QButtonGroup>
#include <QDebug>
#include <QIcon>

AutoBaudDetection::AutoBaudDetection(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::AutoBaudDetection)
{
    ui->setupUi(this);

    availablePorts = QSerialPortInfo::availablePorts();

    for (const auto& i : std::as_const(availablePorts)) {
        ui->portsComboBox->addItem(i.systemLocation());
    }

    QSerialPortInfo::standardBaudRates();

    connect(ui->pushButton, &QPushButton::clicked, this, &AutoBaudDetection::handleStartPressed);
    connect(&timer, &QTimer::timeout, this, &AutoBaudDetection::handleTimeout);
    ui->pushButton->setText(QStringLiteral("&Start"));

    ui->progressBar->setVisible(false);
    ui->progressBar->setMaximum(static_cast<int>(baudListSize));

    ui->buttonBox->button(QDialogButtonBox::Close)->setFocus();

    timer.setSingleShot(true);
}

AutoBaudDetection::~AutoBaudDetection()
{
    delete ui;
}

void AutoBaudDetection::handleStartPressed()
{
    if (isActive) {
        stop();
    } else {
        isActive = true;

        ui->statusIconLabel->setVisible(false);
        ui->portsComboBox->setEnabled(false);
        ui->progressBar->setVisible(true);
        ui->pushButton->setText(QStringLiteral("&Stop"));

        tryNextBaud();
    }
}
void AutoBaudDetection::tryNextBaud()
{
    if (nextBaudIdx >= baudList.size()) {
        setResult(false);
        return;
    }

    serial.close();

    const auto portName = ui->portsComboBox->currentText();
    const auto baud = baudList.at(nextBaudIdx);

    const auto msg = QStringLiteral("Checking baud rate %3").arg(baud);
    ui->progressBar->setValue(static_cast<int>(nextBaudIdx + 1));
    qInfo() << msg;
    ui->statusTextLabel->setText(msg);
    serial.setPortName(portName);
    serial.setBaudRate(baud);

    if (!serial.open(QIODevice::ReadOnly)) {
        qInfo() << serial.error() << serial.errorString();
        const auto& errStr = (serial.error() == QSerialPort::PermissionError) ? QStringLiteral(" is port already in use?") : serial.errorString();
        setResult(false, 0, errStr);
    } else {
        timer.start(3000);
    }

    nextBaudIdx++;
}

void AutoBaudDetection::setResult(const bool isSuccess, const int baud, const QString& errMsg)
{
    qInfo() << "ABD complete:" << isSuccess << baud << errMsg;

    const auto height = ui->statusTextLabel->height();

    ui->statusIconLabel->setVisible(true);

    QPixmap pixmap;
    QString text;
    if (isSuccess) {
        Q_ASSERT(baud);
        pixmap = QIcon::fromTheme(QStringLiteral("data-success")).pixmap(height, height);
        text = QStringLiteral("Baud rate for %1 is %2").arg(serial.portName()).arg(baud);
    } else {
        pixmap = QIcon::fromTheme(QStringLiteral("data-error")).pixmap(height, height);
        text = QStringLiteral("Failed to find baud for %1: %2")
                   .arg(serial.portName(), errMsg.isEmpty() ? QLatin1String("") : errMsg);
    }

    ui->statusIconLabel->setPixmap(pixmap);
    ui->statusTextLabel->setText(text);

    stop();
}

void AutoBaudDetection::stop()
{
    ui->progressBar->setVisible(false);
    ui->portsComboBox->setEnabled(true);
    ui->pushButton->setText(QStringLiteral("&Start"));
    isActive = false;

    serial.close();
    timer.stop();
    nextBaudIdx = 0;
}

void AutoBaudDetection::handleTimeout()
{
    if (!isActive) {
        qWarning() << "ABD not active";
        timer.stop();
        return;
    }

    if (!serial.isReadable()) {
        qInfo() << "No data read on baud: " << baudList.at(nextBaudIdx - 1);
        tryNextBaud();
    }

    const auto bytesRead = serial.readAll();
    const auto bytesReadSize = bytesRead.size();
    const auto asciiCount = std::ranges::count_if(bytesRead, [](char ch) noexcept { return isascii(ch); });
    const auto allEmpty = std::ranges::all_of(bytesRead, [](char ch) noexcept { return ch == '\0'; });

    if (bytesReadSize && asciiCount == bytesReadSize && !allEmpty) {
        setResult(true, baudList.at(nextBaudIdx - 1));
    } else {
        tryNextBaud();
    }
}
