#include "settingsdialog.hpp"
#include "ui_settingsdialog.h"
#include <QPushButton>

SettingsDialog::SettingsDialog(const size_t newBufferSize, QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::SettingsDialog)
    , validator(1, 100 * 1024 * 1024, this)
    , bufferSize(newBufferSize)
{
    ui->setupUi(this);
    connect(ui->infiniteRadioButton, &QRadioButton::toggled, ui->lineEdit, &QLineEdit::setDisabled);
    connect(ui->fixedRadioButton, &QRadioButton::toggled, this, &SettingsDialog::onFixedRadioButtonToggled);
    connect(ui->lineEdit, &QLineEdit::textChanged, this, &SettingsDialog::onEditingFinished);

    ui->lineEdit->setValidator(&validator);
    if (newBufferSize) {
        ui->fixedRadioButton->toggle();
        ui->lineEdit->setText(QString::number(newBufferSize));
    } else {
        ui->infiniteRadioButton->toggle();
    }
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

quint32 SettingsDialog::getBufferSize() const
{
    if (ui->fixedRadioButton->isChecked()) {
        bool ok {};
        const auto result = static_cast<quint32>(ui->lineEdit->text().toULong(&ok));
        if (!ok) {
            return 0;
        }
        return result;
    }

    return 0;
}

void SettingsDialog::updateOkButtonState()
{
    const auto& txt = ui->lineEdit->text();
    const bool isEmpty = txt.isEmpty();

    bool ok {};
    auto result = static_cast<quint32>(txt.toULong(&ok));
    if (!ok) {
        result = 0;
    }
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!isEmpty && result);
}

void SettingsDialog::onFixedRadioButtonToggled(const bool checked)
{
    if (checked) {
        updateOkButtonState();
    } else {
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
    }
}

void SettingsDialog::onEditingFinished()
{
    updateOkButtonState();
}
