#include "triggersetupdialog.h"
#include "ui_triggersetupdialog.h"

#include <QPushButton>

TriggerSetupDialog::TriggerSetupDialog(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::TriggerSetupDialog)
{
    ui->setupUi(this);

    connect(ui->groupBox, &QGroupBox::toggled, this, &TriggerSetupDialog::handleLineEditTextChange);
    // Disable the OK button if a line edit is empty
    connect(ui->keywordLineEdit, &QLineEdit::textChanged, this, &TriggerSetupDialog::handleLineEditTextChange);
    connect(ui->cmdLineEdit, &QLineEdit::textChanged, this, &TriggerSetupDialog::handleLineEditTextChange);

    connect(ui->stringRadioButton, &QRadioButton::toggled, this, &TriggerSetupDialog::handleStringRadioButton);
    connect(ui->execCmdRadioButton, &QRadioButton::toggled, this, &TriggerSetupDialog::handleExecCmdRadioButton);

    ui->stringRadioButton->toggle();
    ui->playSoundRadioButton->setChecked(true);
    handleExecCmdRadioButton();

    for (const auto& i : { ui->alignLabel, ui->alignLabel2, ui->alignLabel3 }) {
        // These labels are used to align the widgets
        i->setText(QStringLiteral("    "));
    }
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

QString TriggerSetupDialog::getTriggerActionCommand() const
{
    Q_ASSERT(ui->execCmdRadioButton->isChecked());
    return ui->cmdLineEdit->text();
}

TriggerSetupDialog::TriggerActionType TriggerSetupDialog::getTriggerActionType() const
{
    qInfo() << ui->playSoundRadioButton->isChecked();
    return (ui->playSoundRadioButton->isChecked() ? TriggerActionType::PlaySound : TriggerActionType::ExecuteCommand);
}

void TriggerSetupDialog::handleStringRadioButton()
{
    const auto isEnabled = ui->stringRadioButton->isChecked();
    ui->label->setEnabled(isEnabled);
    ui->keywordLineEdit->setEnabled(isEnabled);

    handleLineEditTextChange();
}

void TriggerSetupDialog::handleExecCmdRadioButton()
{
    const auto isEnabled = ui->execCmdRadioButton->isChecked();
    ui->cmdLineEdit->setEnabled(isEnabled);

    handleLineEditTextChange();
}

void TriggerSetupDialog::handleLineEditTextChange()
{
    bool enable = true;
    if (ui->groupBox->isChecked()) {
        if (ui->stringRadioButton->isChecked() && ui->keywordLineEdit->text().isEmpty()) {
            enable = false;
        }

        if (ui->execCmdRadioButton->isChecked() && ui->cmdLineEdit->text().isEmpty()) {
            enable = false;
        }
    }

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(enable);
}
