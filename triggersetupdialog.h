#ifndef TRIGGERSETUPDIALOG_H
#define TRIGGERSETUPDIALOG_H

#include <QDialog>

namespace Ui {
class TriggerSetupDialog;
} // namespace Ui

class TriggerSetupDialog : public QDialog {
    Q_OBJECT

public:
    explicit TriggerSetupDialog(QWidget* parent = nullptr);
    TriggerSetupDialog(const TriggerSetupDialog&) = delete;
    TriggerSetupDialog(TriggerSetupDialog&&) = delete;
    TriggerSetupDialog& operator=(const TriggerSetupDialog&) = delete;
    TriggerSetupDialog& operator=(TriggerSetupDialog&&) = delete;
    ~TriggerSetupDialog() override;

    enum class TriggerType : std::uint8_t {
        Disabled,
        StringMatch,
        Activity,
        Inactivity
    };

    [[nodiscard]] TriggerType getTriggerType() const;
    [[nodiscard]] QString getKeyword() const;

private:
    Ui::TriggerSetupDialog* ui {};

private slots:
    void handleStringRadioButton();
    void handleKeywordChanged();
};

#endif // TRIGGERSETUPDIALOG_H
