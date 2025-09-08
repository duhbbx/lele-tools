#ifndef TERMINALTOOL_H
#define TERMINALTOOL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QLineEdit>
#include <QLabel>
#include <QProcess>
#include <QTimer>
#include <QScrollBar>
#include <QKeyEvent>
#include <QTextCursor>
#include <QDir>
#include <QStringList>
#include <QCompleter>
#include <QFileSystemModel>
#include <QSettings>
#include <QFont>
#include <QFontMetrics>
#include <QApplication>
#include <QClipboard>

#include "../../common/dynamicobjectbase.h"

class TerminalOutput;

// 自定义命令行输入框
class TerminalInput : public QLineEdit
{
    Q_OBJECT

public:
    explicit TerminalInput(QWidget *parent = nullptr);
    
    void setCommandHistory(const QStringList &history);
    void addToHistory(const QString &command);
    void clearHistory();

signals:
    void commandEntered(const QString &command);
    void requestClear();
    void requestInterrupt();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    QStringList m_commandHistory;
    int m_historyIndex;
    QString m_currentCommand;
    QCompleter *m_completer;
    QFileSystemModel *m_fileModel;
    
    void setupCompleter();
    void navigateHistory(int direction);
};

// 自定义输出显示区域
class TerminalOutput : public QTextEdit
{
    Q_OBJECT

public:
    explicit TerminalOutput(QWidget *parent = nullptr);
    
    void appendOutput(const QString &text, const QString &type = "output");
    void appendPrompt(const QString &prompt);
    void clear();
    void setMaxLines(int maxLines);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    int m_maxLines;
    QString m_currentPrompt;
    
    void limitLines();
    QString getColorForType(const QString &type) const;
};

// 主终端工具类
class TerminalTool : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit TerminalTool(QWidget *parent = nullptr);
    ~TerminalTool();

private slots:
    void onCommandEntered(const QString &command);
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onProcessReadyReadStandardOutput();
    void onProcessReadyReadStandardError();
    void onRequestClear();
    void onRequestInterrupt();

private:
    void setupUI();
    void setupTerminalStyle();
    void executeCommand(const QString &command);
    void showPrompt();
    void changeDirectory(const QString &path);
    void handleBuiltinCommand(const QString &command);
    bool isBuiltinCommand(const QString &command) const;
    void saveSettings();
    void loadSettings();
    void updateWindowTitle();
    
    QString getCurrentPrompt() const;
    QString formatPath(const QString &path) const;
    
    // UI组件
    QVBoxLayout *m_mainLayout;
    TerminalOutput *m_output;
    QHBoxLayout *m_inputLayout;
    QLabel *m_promptLabel;
    TerminalInput *m_input;
    
    // 进程管理
    QProcess *m_process;
    bool m_processRunning;
    
    // 状态管理
    QString m_currentDirectory;
    QStringList m_commandHistory;
    QString m_currentUser;
    QString m_currentHost;
    
    // 设置
    QFont m_terminalFont;
    int m_maxOutputLines;
    bool m_autoScroll;
    
    // 内置命令
    static const QStringList BUILTIN_COMMANDS;
};

#endif // TERMINALTOOL_H
