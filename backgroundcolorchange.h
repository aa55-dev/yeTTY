#ifndef BACKGROUNDCOLORCHANGE_H
#define BACKGROUNDCOLORCHANGE_H

#include <QDialog>

namespace Ui {
class BackgroundColorChange;
} // namespace Ui

class BackgroundColorChange : public QDialog {
    Q_OBJECT

public:
    BackgroundColorChange(const BackgroundColorChange&) = delete;
    BackgroundColorChange(BackgroundColorChange&&) = delete;
    BackgroundColorChange& operator=(const BackgroundColorChange&) = delete;
    BackgroundColorChange& operator=(BackgroundColorChange&&) = delete;
    explicit BackgroundColorChange(QString& currentString,
        QWidget* parent = nullptr);
    ~BackgroundColorChange() override;

    QString getString();

private:
    Ui::BackgroundColorChange* ui;
private slots:
    void onTextChanged(const QString& newText);
};

#endif // BACKGROUNDCOLORCHANGE_H
