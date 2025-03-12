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

    for (auto i : availablePorts) {
        if (i.systemLocation() == "/dev/ttyS0") {
            continue;
        }
        ui->portsComboBox->addItem(i.systemLocation());
    }

    // set focus so that enter works
    ui->buttonBox->button(QDialogButtonBox::StandardButton::Ok)->setFocus();

    ui->baudRateLineEdit->setValidator(new QIntValidator(1, 100 * 1000 * 1000, this));
}

PortSelectionDialog::~PortSelectionDialog()
{
    delete ui;
}

void PortSelectionDialog::onCurrentIdxChanged(int idx)
{
    const auto& port = availablePorts.at(idx);

    selectedPortLocation = port.systemLocation();

    ui->descriptionLabel->setText(port.description());
    ui->manufacturerLabel->setText(port.manufacturer());
    ui->pidvidLabel->setText(QString::asprintf("%04X:%04X", port.productIdentifier(), port.vendorIdentifier()));
}

const QString& PortSelectionDialog::getSelectedPortLocation() const
{
    return selectedPortLocation;
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
