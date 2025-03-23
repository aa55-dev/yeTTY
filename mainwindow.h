#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <cstdint>
#include <qtypes.h>
#include <utility>
#include <vector>
// #include <source_location>
#include "portselectiondialog.h"
#include <QElapsedTimer>
#include <QMainWindow>
#include <QPointer>
#include <QString>
#include <QWidget>
#include <QtSerialPort/QSerialPort>
#include <cstdlib>
#include <experimental/source_location>
#include <qtconfigmacros.h>
#include <qtmetamacros.h>
#include <zstd.h>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
} // namespace Ui
QT_END_NAMESPACE

namespace KTextEditor {
class Editor; // NOLINT(cppcoreguidelines-virtual-class-destructor)
class Document;
class View;
class Message;
} // namespace KTextEditor

enum class ProgramState : std::uint8_t {
    Unknown,
    Started,
    Stopped
};

class QSoundEffect;
class QTimer;
class TriggerSetupDialog;
class LongTermRunModeDialog;
class QElapsedTimer;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    MainWindow(const MainWindow&) = delete;
    MainWindow(MainWindow&&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;
    MainWindow& operator=(MainWindow&&) = delete;
    ~MainWindow() override;

private slots:
    void handleReadyRead();
    void handleError(const QSerialPort::SerialPortError error);

    void handleSaveAction();
    void handleClearAction();
    static void handleQuitAction();
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
    QPointer<KTextEditor::Message> serialErrorMsg;

    void setProgramState(const ProgramState newState);
    [[nodiscard]] PortSelectionDialog::PortInfo getPortFromUser();

    void connectToDevice(const QString& port, const int baud, const bool showMsgOnOpenErr = true, const QString& description = "", const QString& manufacturer = "");

    QSerialPort* serialPort {};
    PortSelectionDialog::PortInfo selectedPortInfo {};
    QSoundEffect* sound {};
    std::unique_ptr<TriggerSetupDialog> triggerSetupDialog;

    QElapsedTimer elapsedTimer;

    QByteArray triggerKeyword;
    bool triggerActive {};
    int triggerMatchCount {};

    ProgramState currentProgramState = ProgramState::Unknown;
    QTimer* timer {};

    QByteArray triggerSearchLine;

    // Long term run mode
    std::unique_ptr<LongTermRunModeDialog> longTermRunModeDialog;
    bool longTermRunModeEnabled {};
    qsizetype longTermRunModeMaxMemory {};
    int longTermRunModeMaxTime {};
    QString longTermRunModePath;
    qint64 longTermRunModeStartTime {};
    QTimer* longTermRunModeTimer {};
    ZSTD_CCtx* zstdCtx {};
    std::vector<char> zstdOutBuffer;
    int fileCounter {};

    static constexpr auto HIGHLIGHT_MODE = "Log File (advanced)";

#ifdef SYSTEMD_AVAILABLE
    int inhibitFd {};
    void setInhibit(const bool enabled);
#endif

    [[nodiscard]] static std::string getErrorStr();
    void writeCompressedFile(const QByteArray& contents, const int counter);
    static void validateZstdResult(const size_t result, const std::experimental::source_location srcLoc = std::experimental::source_location::current());
};
#endif // MAINWINDOW_H
