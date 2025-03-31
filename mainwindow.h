#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDBusAbstractAdaptor>
#include <QDBusMessage>
#include <QElapsedTimer>
#include <QMainWindow>
#include <QPointer>
#include <QString>
#include <QWidget>
#include <QtSerialPort/QSerialPort>
#include <cstdint>
#include <cstdlib>
#include <experimental/source_location>
#include <utility>
#include <vector>
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
class QLabel;
class QFileSystemWatcher;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    MainWindow(const MainWindow&) = delete;
    MainWindow(MainWindow&&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;
    MainWindow& operator=(MainWindow&&) = delete;
    ~MainWindow() override;

public slots:
    Q_SCRIPTABLE void control(const QString& port, const QString& action, QString& out);
    Q_SCRIPTABLE void portName(QString& out);

private slots:
    void handleReadyRead();
    void handleError(const QSerialPort::SerialPortError error);

    void handleSaveAction();
    void handleClearAction(const bool force = false);
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
    void handleFileWatchEvent(const QString& path);
    void handleStatusBarTimer();

private:
    void start();
    void stop();
    static constexpr std::string_view DEV_PREFIX = "/dev/";
    Ui::MainWindow* ui {};

    KTextEditor::Editor* editor {};
    KTextEditor::Document* doc {};
    KTextEditor::View* view {};

    QSerialPort::SerialPortError prevErrCode = QSerialPort::SerialPortError::NoError;
    QString prevErrMsg;

    void setProgramState(const ProgramState newState);
    [[nodiscard]] static std::pair<QString, int> getPortFromUser();

    void connectToDevice(const QString& port, const int baud, const bool showMsgOnOpenErr = true);

    // Qt is unable to detect disconnection, we need to use inotify to monitor the serial port file path.
    QFileSystemWatcher* fsWatcher;

    QSerialPort* serialPort {};
    QSoundEffect* sound {};
    std::unique_ptr<TriggerSetupDialog> triggerSetupDialog;

    QElapsedTimer elapsedTimer;

    QByteArray triggerKeyword;
    bool triggerActive {};
    int triggerMatchCount {};

    ProgramState currentProgramState = ProgramState::Unknown;
    QTimer* autoRetryTimer {};
    size_t autoRetryCounter {};
    QTimer* statusBarTimer {};

    QLabel* statusBarText {};
    QByteArray triggerSearchLine;

    // Long term run mode
    std::unique_ptr<LongTermRunModeDialog> longTermRunModeDialog;
    bool longTermRunModeEnabled {};
    bool longTermRunModeErrMsgActive {};
    qsizetype longTermRunModeMaxMemory {};
    int longTermRunModeMaxTime {};
    QString longTermRunModePath;
    qint64 longTermRunModeStartTime {};
    QTimer* longTermRunModeTimer {};
    ZSTD_CCtx* zstdCtx {};
    std::vector<char> zstdOutBuffer;

    int fileCounter {};
    int errCtr {};

    static constexpr auto HIGHLIGHT_MODE = "Log File (advanced)";
    static constexpr auto GROUP_DIALOUT = "dialout";

#ifdef SYSTEMD_AVAILABLE
    int inhibitFd {};
    void setInhibit(const bool enabled);
#endif

    [[nodiscard]] static std::string getErrorStr();
    void writeCompressedFile(const QByteArray& contents);
    static void validateZstdResult(const size_t result, const std::experimental::source_location& srcLoc = std::experimental::source_location::current());
    [[nodiscard]] QString getSerialPortPath() const;
    void closeSerialPort();
    [[nodiscard]] static std::pair<QString, QString> getPortInfo(QString portLocation);
    // Checks if the user is in "dialout" group
    [[nodiscard]] static bool isUserPermissionSetupCorrectly();
    [[nodiscard]] bool isRecentlyEnumerated();
};
#endif // MAINWINDOW_H
