#include "longtermrunmodedialog.h"
#include "ui_longtermrunmodedialog.h"

#include <QDebug>
#include <QDialog>
#include <QFileDialog>
#include <QFontMetrics>
#include <QIntValidator>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QStringLiteral>
#include <QToolButton>
#include <QUuid>
#include <QWidget>

LongTermRunModeDialog::LongTermRunModeDialog(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::LongTermRunModeDialog)
{
    ui->setupUi(this);

    const auto dirs = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation);
    if (dirs.empty()) {
        qFatal("Failed to get dir location");
    }
    directory = dirs.at(0);
    const auto directoryStr = directory.toString();

    ui->timeLineEdit->setText(QString::number(timeInMinutes));
    ui->memoryLineEdit->setText(QString::number(memoryInMiB));

    ui->directoryLineEdit->setText(directoryStr);

    // resize the lineedit so that the default path can fit in comfortably.
    const QFontMetrics fontMetrics(ui->directoryLineEdit->font());
    ui->directoryLineEdit->setMinimumWidth(fontMetrics.boundingRect(directoryStr + QStringLiteral("     ")).width());

    connect(ui->timeLineEdit, &QLineEdit::textChanged, this, &LongTermRunModeDialog::onInputChanged);
    connect(ui->memoryLineEdit, &QLineEdit::textChanged, this, &LongTermRunModeDialog::onInputChanged);
    connect(ui->directoryLineEdit, &QLineEdit::textChanged, this, &LongTermRunModeDialog::onInputChanged);
    connect(ui->toolButton, &QToolButton::clicked, this, &LongTermRunModeDialog::onToolButton);

    ui->memoryLineEdit->setValidator(new QIntValidator(1, 512, this)); // NOLINT(cppcoreguidelines-owning-memory)
    ui->timeLineEdit->setValidator(new QIntValidator(1, 60, this)); // NOLINT(cppcoreguidelines-owning-memory)

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
    bool timeOk {};
    bool memoryOk {};

    timeInMinutes = ui->timeLineEdit->text().toInt(&timeOk);
    memoryInMiB = ui->memoryLineEdit->text().toInt(&memoryOk);
    directory = ui->directoryLineEdit->text();

    if (!timeOk || !memoryOk || ui->directoryLineEdit->text().isEmpty()) {
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
    const auto tmp = QFileDialog::getExistingDirectory();
    if (!tmp.isEmpty()) {
        directory = tmp;
        qInfo() << "New directory to save files" << directory;
        ui->directoryLineEdit->setText(directory.toString());
    }
}

std::pair<bool, QString> LongTermRunModeDialog::isDirectoryWritable(const QString& directory)
{
    const QUuid uid = QUuid::createUuid();
    const auto filename = QStringLiteral("/.yetty-test-") + uid.toString(QUuid::StringFormat::Id128);

    QFile testFile(directory + filename);

    if (testFile.open(QIODeviceBase::WriteOnly)) {

        const auto result = testFile.write("Hello") > 0;
        const auto errMsg = testFile.errorString();

        testFile.close();
        testFile.remove();

        return { result, errMsg };
    }

    return { false, testFile.errorString() };
}

void LongTermRunModeDialog::accept()
{
    const auto [result, errStr] = isDirectoryWritable(directory.path());
    if (result) {
        QDialog::accept();
    } else {
        QMessageBox::warning(this, QStringLiteral("Write error"),
            QStringLiteral("Failed to write to %1: %2").arg(directory.path(), errStr), QMessageBox::Ok);
    }
}

QUrl LongTermRunModeDialog::getDirectory() const
{
    return directory;
}
