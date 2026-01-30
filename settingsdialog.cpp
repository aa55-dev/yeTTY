#include "settingsdialog.hpp"
#include "ui_settingsdialog.h"

SettingsDialog::SettingsDialog(const size_t newBufferSize, QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::SettingsDialog)
    , validator(1, 100 * 1024 * 1024, this)
    , bufferSize(newBufferSize)
{
    ui->setupUi(this);
    connect(ui->infiniteRadioButton, &QRadioButton::toggled, ui->lineEdit, &QLineEdit::setDisabled);
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
    if (ui->fixedRadioButton->isEnabled()) {
        bool ok {};
        const auto result = static_cast<quint32>(ui->lineEdit->text().toULong(&ok));
        if (!ok) {
            return 0;
        }
        return result;
    }
    return 0;
}
