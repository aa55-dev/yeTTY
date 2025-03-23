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
    struct PortInfo {
        QString location;
        QString manufacturer;
        QString description;
        int baud;
    };

    explicit PortSelectionDialog(QWidget* parent = nullptr);
    PortSelectionDialog(const PortSelectionDialog&) = delete;
    PortSelectionDialog(PortSelectionDialog&&) = delete;
    PortSelectionDialog& operator=(const PortSelectionDialog&) = delete;
    PortSelectionDialog& operator=(PortSelectionDialog&&) = delete;
    ~PortSelectionDialog() override;

    [[nodiscard]] PortInfo getSelectedPortInfo() const;

public slots:
    void onCurrentIdxChanged(int idx);

private:
    Ui::PortSelectionDialog* ui {};
    QString selectedPortLocation;
    QList<QSerialPortInfo> availablePorts;
    int getSelectedBaud() const;
};

#endif // PORTSELECTIONDIALOG_H
