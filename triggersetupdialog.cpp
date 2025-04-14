#include "triggersetupdialog.h"
#include "ui_triggersetupdialog.h"

#include <QPushButton>

TriggerSetupDialog::TriggerSetupDialog(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::TriggerSetupDialog)
{
    ui->setupUi(this);

    connect(ui->groupBox, &QGroupBox::toggled, this, &TriggerSetupDialog::handleKeywordChanged);
    connect(ui->keywordLineEdit, &QLineEdit::textChanged, this, &TriggerSetupDialog::handleKeywordChanged);
    connect(ui->stringRadioButton, &QRadioButton::toggled, this, &TriggerSetupDialog::handleStringRadioButton);
    ui->stringRadioButton->toggle();
}

TriggerSetupDialog::~TriggerSetupDialog()
{
    delete ui;
}

TriggerSetupDialog::TriggerType TriggerSetupDialog::getTriggerType() const
{
    if (!ui->groupBox->isChecked()) {
        return TriggerType::Disabled;
    }

    if (ui->stringRadioButton->isChecked()) {
        return TriggerType::StringMatch;
    }

    if (ui->activityRadioButton->isChecked()) {
        return TriggerType::Activity;
    }

    if (ui->inactivityRadioButton->isChecked()) {
        return TriggerType::Inactivity;
    }

    Q_ASSERT(false);
    return TriggerType::Disabled;
}

QString TriggerSetupDialog::getKeyword() const
{
    Q_ASSERT(ui->stringRadioButton->isChecked());
    return ui->keywordLineEdit->text();
}

void TriggerSetupDialog::handleStringRadioButton()
{
    const auto isEnabled = ui->stringRadioButton->isChecked();
    ui->label->setEnabled(isEnabled);
    ui->keywordLineEdit->setEnabled(isEnabled);

    handleKeywordChanged();
}

void TriggerSetupDialog::handleKeywordChanged()
{
    bool enable = true;
    if (ui->groupBox->isChecked() && ui->stringRadioButton->isChecked() && ui->keywordLineEdit->text().isEmpty()) {
        enable = false;
    }

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(enable);
}
