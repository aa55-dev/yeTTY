#include "longtermrunmodedialog.h"

#include "ui_longtermrunmodedialog.h"

#include <QDebug>
#include <QFileDialog>
#include <QIntValidator>
#include <QPushButton>
#include <QStandardPaths>

LongTermRunModeDialog::LongTermRunModeDialog(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::LongTermRunModeDialog)
{
    ui->setupUi(this);

    const auto dirs = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation);
    if (dirs.empty()) {
        qFatal("Failed to get dir location");
    }
    directory = dirs[0];
    const auto directoryStr = directory.toString();

    ui->timeLineEdit->setText(QString::number(timeInMinutes));
    ui->memoryLineEdit->setText(QString::number(memoryInMiB));

    ui->directoryLineEdit->setText(directoryStr);

    // resize the lineedit so that the default path can fit in comfortably.
    QFontMetrics fontMetrics(ui->directoryLineEdit->font());
    ui->directoryLineEdit->setMinimumWidth(fontMetrics.boundingRect(directoryStr + "     ").width());

    connect(ui->timeLineEdit, &QLineEdit::textChanged, this, &LongTermRunModeDialog::onInputChanged);
    connect(ui->memoryLineEdit, &QLineEdit::textChanged, this, &LongTermRunModeDialog::onInputChanged);
    connect(ui->toolButton, &QToolButton::pressed, this, &LongTermRunModeDialog::onToolButton);

    ui->memoryLineEdit->setValidator(new QIntValidator(1, 512, this));
    ui->timeLineEdit->setValidator(new QIntValidator(1, 60, this));

    onInputChanged();
}

LongTermRunModeDialog::~LongTermRunModeDialog()
{
    delete ui;
}

int LongTermRunModeDialog::getMinutes() const
{
    return timeInMinutes;
}

int LongTermRunModeDialog::getMemory() const
{
    return memoryInMiB;
}

bool LongTermRunModeDialog::isEnabled() const
{
    return ui->groupBox->isChecked();
}

void LongTermRunModeDialog::onInputChanged()
{

    bool timeOk {}, memoryOk {};

    timeInMinutes = ui->timeLineEdit->text().toInt(&timeOk);
    memoryInMiB = ui->memoryLineEdit->text().toInt(&memoryOk);

    if (!timeOk || !memoryOk) {
        ui->buttonBox->button(QDialogButtonBox::StandardButton::Ok)->setEnabled(false);
        return;
    }
    ui->buttonBox->button(QDialogButtonBox::StandardButton::Ok)->setEnabled(true);

    ui->msgLabel->setText(QStringLiteral("yeTTY will save the serial data every %1 minutes or %2 MiB"
                                         " (whichever is earlier) and clear the contents from memory(and view)")
                              .arg(QString::number(timeInMinutes), QString::number(memoryInMiB)));
}

void LongTermRunModeDialog::onToolButton()
{
    directory = QFileDialog::getExistingDirectory();
    qInfo() << "New directory to save files" << directory;
    ui->directoryLineEdit->setText(directory.toString());
}

QUrl LongTermRunModeDialog::getDirectory() const
{
    return directory;
}
