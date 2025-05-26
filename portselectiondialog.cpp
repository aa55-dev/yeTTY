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

    const QSettings settings;
    const auto previouslyUsedPortInfo = settings.value(SETTINGS_LAST_USED_PORT).toStringList();

    if (previouslyUsedPortInfo.size() == 2) {
        previouslyUsedPort = previouslyUsedPortInfo[0];
        previouslyUsedBaud = previouslyUsedPortInfo[1];
    }

    connect(ui->portsComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PortSelectionDialog::onCurrentIdxChanged);
    connect(ui->refreshButton, &QPushButton::pressed, this, &PortSelectionDialog::onRefreshButtonPressed);

    // set focus so that enter works
    ui->portsComboBox->setFocus();
    ui->baudRateLineEdit->setValidator(new QIntValidator(1, BAUD_MAX_VALUE, this)); // NOLINT(cppcoreguidelines-owning-memory)

    onRefreshButtonPressed();
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

void PortSelectionDialog::onRefreshButtonPressed()
{
    ui->portsComboBox->clear();
    availablePorts = QSerialPortInfo::availablePorts();

    int idx = 0;
    int highlightIndex = -1;
    qInfo() << "Ports: " << availablePorts.size();
    for (const auto& i : std::as_const(availablePorts)) {
        qInfo() << i.description() << i.productIdentifier() << i.vendorIdentifier() << i.portName() <<i.serialNumber() << i.manufacturer();
        ui->portsComboBox->addItem(i.systemLocation());
        if (i.portName() == previouslyUsedPort) {
            highlightIndex = idx;
        }
        idx++;
    }

    if (highlightIndex >= 0) {
        ui->portsComboBox->setCurrentIndex(highlightIndex);

        bool ok = false;
        const auto baudInt = previouslyUsedBaud.toInt(&ok);
        if (ok && baudInt > 0 && baudInt < BAUD_MAX_VALUE) {
            ui->baudRateLineEdit->setText(previouslyUsedBaud);
        }
    }
    if (ui->baudRateLineEdit->text().isEmpty()) {
        ui->baudRateLineEdit->setText(QStringLiteral("115200"));
    }
}

void PortSelectionDialog::onCurrentIdxChanged(int idx)
{
    if (idx < 0) {
        ui->descriptionLabel->clear();
        ui->manufacturerLabel->clear();
        ui->pidvidLabel->clear();
        return;
    }

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
