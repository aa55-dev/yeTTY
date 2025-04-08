#ifndef AUTOBAUDDETECTION_H
#define AUTOBAUDDETECTION_H

#include <QDialog>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>

namespace Ui {
class AutoBaudDetection;
} // namespace Ui

class AutoBaudDetection : public QDialog {
    Q_OBJECT

public:
    explicit AutoBaudDetection(QWidget* parent = nullptr);
    ~AutoBaudDetection() override;

    AutoBaudDetection(const AutoBaudDetection&) = delete;
    AutoBaudDetection(AutoBaudDetection&&) = delete;
    AutoBaudDetection& operator=(const AutoBaudDetection&) = delete;
    AutoBaudDetection& operator=(AutoBaudDetection&&) = delete;

private:
    Ui::AutoBaudDetection* ui;
    QList<QSerialPortInfo> availablePorts;

    static constexpr auto baudList = std::array<int, 18> { 9600,
        19200, 38400, 57600, 115200,
        230400, 460800, 500000, 576000, 921600,
        1000000, 1152000, 1500000, 2000000, 2500000,
        3000000, 3500000,
        4000000 };
    static inline const auto baudListSize = baudList.size();
    size_t nextBaudIdx {};
    QTimer timer;
    bool isActive {};
    QSerialPort serial;

    void tryNextBaud();
    void setResult(const bool isSuccess, const int baud = 0, const QString &errMsg = "");
    void stop();

private slots:
    void handleStartPressed();
    void handleTimeout();
};

#endif // AUTOBAUDDETECTION_H
