#ifndef PORTSELECTIONDIALOG_H
#define PORTSELECTIONDIALOG_H

#include <QDialog>
#include <QSerialPortInfo>

namespace Ui {
class PortSelectionDialog;
} // namespace Ui

class PortSelectionDialog : public QDialog {
    Q_OBJECT

public:
    explicit PortSelectionDialog(QWidget* parent = nullptr);
    PortSelectionDialog(const PortSelectionDialog&) = delete;
    PortSelectionDialog(PortSelectionDialog&&) = delete;
    PortSelectionDialog& operator=(const PortSelectionDialog&) = delete;
    PortSelectionDialog& operator=(PortSelectionDialog&&) = delete;
    ~PortSelectionDialog() override;

    [[nodiscard]] std::pair<QString, int> getSelectedPortInfo() const;

private slots:
    void onRefreshButtonPressed();
    void onCurrentIdxChanged(int idx);

private:
    static constexpr int BAUD_MAX_VALUE = 100 * 1000 * 1000;
    Ui::PortSelectionDialog* ui {};
    QString selectedPortLocation;
    QList<QSerialPortInfo> availablePorts;
    [[nodiscard]] int getSelectedBaud() const;

    // Port used by the user when the application was used the last time
    QString previouslyUsedPort;
    QString previouslyUsedBaud;
};

#endif // PORTSELECTIONDIALOG_H
