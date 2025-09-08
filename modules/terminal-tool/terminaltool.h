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

// 一体化终端文本区域
class IntegratedTerminal : public QTextEdit
{
    Q_OBJECT

public:
    explicit IntegratedTerminal(QWidget *parent = nullptr);
    
    void appendOutput(const QString &text, const QString &type = "output");
    void showPrompt(const QString &prompt);
    void clear();
    void setMaxLines(int maxLines);
    void setCommandHistory(const QStringList &history);
    void addToHistory(const QString &command);

signals:
    void commandEntered(const QString &command);
    void requestClear();
    void requestInterrupt();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void setupCompleter();
    void navigateHistory(int direction);
    void limitLines();
    QString getColorForType(const QString &type) const;
    void insertPromptAndMoveCursor(const QString &prompt);
    void processCurrentCommand();
    bool isInEditableArea() const;
    void moveToEndOfDocument();
    QString getCurrentCommand() const;
    void setCurrentCommand(const QString &command);
    
    // 状态管理
    int m_maxLines;
    QString m_currentPrompt;
    QStringList m_commandHistory;
    int m_historyIndex;
    QString m_tempCommand;
    int m_promptStartPos;
    int m_commandStartPos;
    
    // 自动补全
    QCompleter *m_completer;
    QFileSystemModel *m_fileModel;
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
    IntegratedTerminal *m_terminal;
    
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
