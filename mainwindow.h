#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <zstd.h>

#include <QElapsedTimer>
#include <QMainWindow>
#include <QPointer>
#include <QtSerialPort/QSerialPort>

#include <vector>
// #include <source_location>
#include <experimental/source_location>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

namespace KTextEditor {
class Editor;
class Document;
class View;
class Message;
}

enum class ProgramState {
    Unknown,
    Started,
    Stopped
};

class QSoundEffect;
class QTimer;
class TriggerSetupDialog;
class LongTermRunModeDialog;
class QElapsedTimer;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void handleReadyRead();
    void handleError(const QSerialPort::SerialPortError error);

    void handleSaveAction();
    void handleClearAction();
    void handleQuitAction();
    void handleScrollToEnd();
    void handleAboutAction();
    void handleConnectAction();
    void handleTriggerSetupAction();
    void handleTriggerSetupDialogDone(int result);
    void handleStartStopButton();
    void handleRetryConnection();
    void handleLongTermRunModeAction();
    void handleLongTermRunModeDialogDone(int result);
    void handleLongTermRunModeTimer();

private:
    Ui::MainWindow* ui {};

    KTextEditor::Editor* editor {};
    KTextEditor::Document* doc {};
    KTextEditor::View* view {};
    QPointer<KTextEditor::Message> serialErrorMsg {};

    void setProgramState(const ProgramState newState);
    [[nodiscard]] std::pair<QString, int> getPortFromUser() const;

    void connectToDevice(const QString& port, const int baud, const bool showMsgOnOpenErr = true);
    QSerialPort* serialPort {};
    QSoundEffect* sound {};
    TriggerSetupDialog* triggerSetupDialog {};

    QElapsedTimer elapsedTimer;

    QByteArray triggerKeyword {};
    bool triggerActive {};
    int triggerMatchCount {};

    ProgramState currentProgramState = ProgramState::Unknown;
    QTimer* timer {};

    QByteArray triggerSearchLine {};

    // Long term run mode
    LongTermRunModeDialog* longTermRunModeDialog {};
    bool longTermRunModeEnabled {};
    int longTermRunModeMaxMemory {};
    int longTermRunModeMaxTime {};
    QString longTermRunModePath {};
    qint64 longTermRunModeStartTime {};
    QTimer* longTermRunModeTimer {};
    ZSTD_CCtx* zstdCtx {};
    std::vector<char> zstdOutBuffer {};
    int fileCounter {};

    static inline constexpr auto HIGHLIGHT_MODE = "Log File (advanced)";

#ifdef SYSTEMD_AVAILABLE
    int inhibitFd {};

    void setInhibit(const bool enabled);

#endif

    void writeCompressedFile(const QByteArray& contents, const int counter);
    static void validateZstdResult(const size_t result, const std::experimental::source_location = std::experimental::source_location::current());
};
#endif // MAINWINDOW_H
