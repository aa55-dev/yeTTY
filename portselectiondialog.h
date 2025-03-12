#ifndef PORTSELECTIONDIALOG_H
#define PORTSELECTIONDIALOG_H

#include <QDialog>
#include <QSerialPortInfo>

namespace Ui {
class PortSelectionDialog;
}

class PortSelectionDialog : public QDialog {
    Q_OBJECT

public:
    explicit PortSelectionDialog(QWidget* parent = nullptr);
    ~PortSelectionDialog();

    int getSelectedBaud() const;
    const QString& getSelectedPortLocation() const;

public slots:
    void onCurrentIdxChanged(int idx);

private:
    Ui::PortSelectionDialog* ui {};
    QString selectedPortLocation {};
    QList<QSerialPortInfo> availablePorts {};
};

#endif // PORTSELECTIONDIALOG_H
