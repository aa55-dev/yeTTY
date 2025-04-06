#include "portalreadyinusedialog.h"
#include "ui_portalreadyinusedialog.h"

#include <QPushButton>

PortAlreadyInUseDialog::PortAlreadyInUseDialog(QWidget* parent, const QString& pid, const QString& command)
    : QDialog(parent)
    , ui(new Ui::PortAlreadyInUseDialog)
{
    ui->setupUi(this);
    ui->pidLabel->setText(pid);
    ui->commandLabel->setText(command);

    ui->buttonBox->button(QDialogButtonBox::Cancel)->setDefault(true);
}

PortAlreadyInUseDialog::~PortAlreadyInUseDialog()
{
    delete ui;
}
