#include "portselectiondialog.h"
#include "ui_portselectiondialog.h"
#include <QPushButton>

#include <QDebug>
#include <QSerialPortInfo>
#include <QTimer>

PortSelectionDialog::PortSelectionDialog(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::PortSelectionDialog)
{
    ui->setupUi(this);
    connect(ui->portsComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PortSelectionDialog::onCurrentIdxChanged);

    ui->baudRateLineEdit->setText("115200");
    availablePorts = QSerialPortInfo::availablePorts();

    for (const auto& i : std::as_const(availablePorts)) {
        ui->portsComboBox->addItem(i.systemLocation());
    }

    // set focus so that enter works
    ui->buttonBox->button(QDialogButtonBox::StandardButton::Ok)->setFocus();

    ui->baudRateLineEdit->setValidator(new QIntValidator(1, 100 * 1000 * 1000, this)); // NOLINT(cppcoreguidelines-owning-memory)
}

PortSelectionDialog::~PortSelectionDialog()
{
    delete ui;
}

PortSelectionDialog::PortInfo PortSelectionDialog::getSelectedPortInfo() const
{
    const auto& port = availablePorts.at(ui->portsComboBox->currentIndex());
    return PortInfo {
        .location = port.systemLocation(),
        .manufacturer = port.manufacturer(),
        .description = port.description(),
        .baud = getSelectedBaud()
    };
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
