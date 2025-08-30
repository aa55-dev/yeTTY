#include "backgroundcolorchange.h"
#include "ui_backgroundcolorchange.h"

BackgroundColorChange::BackgroundColorChange(QString& currentString, QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::BackgroundColorChange)
{
    ui->setupUi(this);

    if (!currentString.isEmpty()) {
        ui->lineEdit->setText(currentString);
        ui->checkBox->setChecked(true);
    }
    connect(ui->lineEdit, &QLineEdit::textChanged, this, &BackgroundColorChange::onTextChanged);
    ui->lineEdit->setFocus();
}

BackgroundColorChange::~BackgroundColorChange()
{
    delete ui;
}

QString BackgroundColorChange::getString()
{
    if (ui->checkBox->isChecked()) {
        Q_ASSERT(!ui->lineEdit->text().isEmpty());
        return ui->lineEdit->text();
    }

    return {};
}

void BackgroundColorChange::onTextChanged(const QString& newText)
{
    ui->checkBox->setChecked(!newText.isEmpty());
}
