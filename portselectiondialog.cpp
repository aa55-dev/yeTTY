#include "portselectiondialog.h"
#include "common.hpp"
#include "ui_portselectiondialog.h"

#include <QDebug>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QSettings>
#include <QTimer>

PortSelectionDialog::PortSelectionDialog(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::PortSelectionDialog)
{
    ui->setupUi(this);
    connect(ui->portsComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PortSelectionDialog::onCurrentIdxChanged);

    ui->baudRateLineEdit->setText(QStringLiteral("115200"));
    availablePorts = QSerialPortInfo::availablePorts();

    const QSettings settings;
    const auto previouslyUsedPortInfo = settings.value(SETTINGS_LAST_USED_PORT).toStringList();
    QString prevPort;
    QString prevBaud;

    if (previouslyUsedPortInfo.size() == 2) {
        prevPort = previouslyUsedPortInfo[0];
        prevBaud = previouslyUsedPortInfo[1];
    }

    int idx = 0;
    int highlightIndex = -1;
    for (const auto& i : std::as_const(availablePorts)) {
        ui->portsComboBox->addItem(i.systemLocation());
        if (i.portName() == prevPort) {
            highlightIndex = idx;
        }
        idx++;
    }

    constexpr int BAUD_MAX_VALUE = 100 * 1000 * 1000;
    // set focus so that enter works
    ui->portsComboBox->setFocus();
    ui->baudRateLineEdit->setValidator(new QIntValidator(1, BAUD_MAX_VALUE, this)); // NOLINT(cppcoreguidelines-owning-memory)

    if (highlightIndex >= 0) {
        ui->portsComboBox->setCurrentIndex(highlightIndex);

        bool ok = false;
        const auto baudInt = prevBaud.toInt(&ok);
        if (ok && baudInt > 0 && baudInt < BAUD_MAX_VALUE) {
            ui->baudRateLineEdit->setText(prevBaud);
        }
    }
}

PortSelectionDialog::~PortSelectionDialog()
{
    delete ui;
}

std::pair<QString, int> PortSelectionDialog::getSelectedPortInfo() const
{
    const auto& port = availablePorts.at(ui->portsComboBox->currentIndex());
    return { port.systemLocation(), getSelectedBaud() };
}

void PortSelectionDialog::onCurrentIdxChanged(int idx)
{
    const auto& port = availablePorts.at(idx);

    ui->descriptionLabel->setText(port.description());
    ui->manufacturerLabel->setText(port.manufacturer());
    ui->pidvidLabel->setText(QString::asprintf("%04X:%04X", port.productIdentifier(), port.vendorIdentifier()));
}

int PortSelectionDialog::getSelectedBaud() const
{
    bool ok {};
    const auto baudInt = ui->baudRateLineEdit->text().toInt(&ok);
    // A validator has been set, int conversion should not fail
    if (!ok) {
        // We can't throw exception here. Terminate application instead
        qFatal("Baud error");
    }
    return baudInt;
}
