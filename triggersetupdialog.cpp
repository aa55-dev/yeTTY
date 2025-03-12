#include "triggersetupdialog.h"
#include "ui_triggersetupdialog.h"

TriggerSetupDialog::TriggerSetupDialog(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::TriggerSetupDialog)
{
    ui->setupUi(this);
}

TriggerSetupDialog::~TriggerSetupDialog()
{
    delete ui;
}

const QString TriggerSetupDialog::getKeyword() const
{
    return ui->lineEdit->text();
}
