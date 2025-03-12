#ifndef TRIGGERSETUPDIALOG_H
#define TRIGGERSETUPDIALOG_H

#include <QDialog>

namespace Ui {
class TriggerSetupDialog;
}

class TriggerSetupDialog : public QDialog {
    Q_OBJECT

public:
    explicit TriggerSetupDialog(QWidget* parent = nullptr);
    ~TriggerSetupDialog();

    const QString getKeyword() const;

private:
    Ui::TriggerSetupDialog* ui {};
};

#endif // TRIGGERSETUPDIALOG_H
