#ifndef SETTINGSDIALOG_HPP
#define SETTINGSDIALOG_HPP

#include <QDialog>
#include <QIntValidator>

namespace Ui {
class SettingsDialog;
}

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(const size_t newBufferSize, QWidget* parent = nullptr);
    ~SettingsDialog() override;

    [[nodiscard]] quint32 getBufferSize() const;

private:
    Ui::SettingsDialog* ui;
    QIntValidator validator;
    size_t bufferSize;
};

#endif // SETTINGSDIALOG_HPP
