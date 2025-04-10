#ifndef LONGTERMRUNMODEDIALOG_H
#define LONGTERMRUNMODEDIALOG_H

#include <QDialog>
#include <QUrl>
#include <QWidget>

namespace Ui {
class LongTermRunModeDialog;
} // namespace Ui

class LongTermRunModeDialog : public QDialog {
    Q_OBJECT

public:
    explicit LongTermRunModeDialog(QWidget* parent = nullptr);
    LongTermRunModeDialog(const LongTermRunModeDialog&) = delete;
    LongTermRunModeDialog(LongTermRunModeDialog&&) = delete;
    LongTermRunModeDialog& operator=(const LongTermRunModeDialog&) = delete;
    LongTermRunModeDialog& operator=(LongTermRunModeDialog&&) = delete;
    ~LongTermRunModeDialog() override;

    [[nodiscard]] int getMinutes() const;
    [[nodiscard]] int getMemory() const;
    [[nodiscard]] bool isEnabled() const;
    [[nodiscard]] QUrl getDirectory() const;

private slots:
    void onInputChanged();
    void onToolButton();

private:
    Ui::LongTermRunModeDialog* ui;
    int timeInMinutes = 15;
    int memoryInMiB = 8;
    QUrl directory;

    [[nodiscard]] static std::pair<bool, QString> isDirectoryWritable(const QString &directory);

    // QDialog interface
public slots:
    void accept() override;
};

#endif // LONGTERMRUNMODEDIALOG_H
