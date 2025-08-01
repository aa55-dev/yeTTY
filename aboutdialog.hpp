#ifndef ABOUTDIALOG_HPP
#define ABOUTDIALOG_HPP

#include <QDialog>

namespace Ui {
class AboutDialog;
} // namespace Ui

class AboutDialog : public QDialog {
    Q_OBJECT

public:
    explicit AboutDialog(QWidget* parent = nullptr);

    AboutDialog(const AboutDialog &) = delete;
    AboutDialog(AboutDialog &&) = delete;
    AboutDialog &operator=(const AboutDialog &) = delete;
    AboutDialog &operator=(AboutDialog &&) = delete;
    ~AboutDialog() override;

private:
    Ui::AboutDialog* ui;
};

#endif // ABOUTDIALOG_HPP
