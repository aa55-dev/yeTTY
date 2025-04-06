#ifndef PORTALREADYINUSEDIALOG_H
#define PORTALREADYINUSEDIALOG_H

#include <QDialog>

namespace Ui {
class PortAlreadyInUseDialog;
} // namespace Ui

class PortAlreadyInUseDialog : public QDialog {
    Q_OBJECT

public:
    explicit PortAlreadyInUseDialog(QWidget* parent, const QString& pid, const QString& command);

    PortAlreadyInUseDialog(const PortAlreadyInUseDialog&) = delete;
    PortAlreadyInUseDialog(PortAlreadyInUseDialog&&) = delete;
    PortAlreadyInUseDialog& operator=(const PortAlreadyInUseDialog&) = delete;
    PortAlreadyInUseDialog& operator=(PortAlreadyInUseDialog&&) = delete;
    ~PortAlreadyInUseDialog() override;

private:
    Ui::PortAlreadyInUseDialog* ui;
};

#endif // PORTALREADYINUSEDIALOG_H
