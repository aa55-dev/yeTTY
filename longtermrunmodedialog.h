#ifndef LONGTERMRUNMODEDIALOG_H
#define LONGTERMRUNMODEDIALOG_H

#include <QDialog>
#include <QUrl>

namespace Ui {
class LongTermRunModeDialog;
}

class LongTermRunModeDialog : public QDialog {
    Q_OBJECT

public:
    explicit LongTermRunModeDialog(QWidget* parent = nullptr);
    ~LongTermRunModeDialog();

    int getMinutes() const;
    int getMemory() const;
    bool isEnabled() const;
    QUrl getDirectory() const;

private slots:
    void onInputChanged();
    void onToolButton();

private:
    Ui::LongTermRunModeDialog* ui;
    int timeInMinutes = 15;
    int memoryInMiB = 8;
    QUrl directory;
};

#endif // LONGTERMRUNMODEDIALOG_H
