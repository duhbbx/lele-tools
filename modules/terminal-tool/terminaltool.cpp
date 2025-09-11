#include "terminaltool.h"
#include <QApplication>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDateTime>
#include <QDebug>
#include <QTextBlock>
#include <QScrollBar>
#include <QSplitter>
#include <QRegularExpression>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QMenu>

REGISTER_DYNAMICOBJECT(TerminalTool);

// 内置命令列表
const QStringList TerminalTool::BUILTIN_COMMANDS = {
    "clear", "cls", "exit", "quit", "pwd", "cd", "help", "history"
};

// IntegratedTerminal 实现
IntegratedTerminal::IntegratedTerminal(QWidget *parent)
    : QTextEdit(parent)
    , m_maxLines(1000)
    , m_historyIndex(-1)
    , m_promptStartPos(0)
    , m_commandStartPos(0)
    , m_completer(nullptr)
    , m_fileModel(nullptr)
{
    setupCompleter();
    
    // 设置终端字体 - 尝试多种编程字体
    QFont terminalFont;
    QStringList fontFamilies = {
        "Consolas",
        "Source Code Pro", 
        "Monaco",
        "Menlo"
    };
    
    // 尝试找到可用的编程字体
    for (const QString& fontFamily : fontFamilies) {
        terminalFont.setFamily(fontFamily);
        terminalFont.setPointSize(10);
        terminalFont.setFixedPitch(true);
        if (terminalFont.exactMatch()) {
            qDebug() << "使用字体:" << fontFamily;
            break;
        }
    }
    
    setFont(terminalFont);
    
    // 在样式表中也强制指定字体，确保生效
    QString fontFamily = terminalFont.family();
    QString styleSheet = QString(R"(
        QTextEdit {
            background-color: #000000;
            color: #ffffff;
            border: 1px solid #333333;
            selection-background-color: #444444;
            font-family: '%1', 'Consolas', 'Source Code Pro', 'Monaco', monospace;
            font-size: 11pt;
            font-weight: normal;
        }
        QScrollBar:vertical {
            background: #2b2b2b;
            width: 12px;
            border-radius: 6px;
        }
        QScrollBar::handle:vertical {
            background: #555555;
            border-radius: 6px;
            min-height: 20px;
        }
        QScrollBar::handle:vertical:hover {
            background: #666666;
        }
    )").arg(fontFamily);
    
    setStyleSheet(styleSheet);
    
    // 强制设置字体
    setCurrentFont(terminalFont);
    
    // 设置默认字符格式
    QTextCharFormat format;
    format.setFont(terminalFont);
    setCurrentCharFormat(format);
    
    // 设置文本编辑属性
    setUndoRedoEnabled(false);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setLineWrapMode(QTextEdit::WidgetWidth);
    setAcceptRichText(true);
    
    qDebug() << "IntegratedTerminal 字体设置完成:" << terminalFont.family();
}

void IntegratedTerminal::setupCompleter()
{
    // 设置文件系统自动补全
    m_fileModel = new QFileSystemModel(this);
    m_fileModel->setRootPath("");
    m_fileModel->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
    
    m_completer = new QCompleter(this);
    m_completer->setModel(m_fileModel);
    m_completer->setCompletionMode(QCompleter::InlineCompletion);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
}

void IntegratedTerminal::appendOutput(const QString &text, const QString &type)
{
    if (text.isEmpty()) {
        return;
    }
    
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::End);
    
    // 设置文本颜色
    QTextCharFormat format;
    format.setForeground(QColor(getColorForType(type)));
    cursor.setCharFormat(format);
    
    // 插入文本
    cursor.insertText(text);
    
    // 更新位置信息
    m_promptStartPos = cursor.position();
    m_commandStartPos = cursor.position();
    
    // 限制行数
    limitLines();
    
    // 滚动到底部
    QScrollBar *scrollBar = verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void IntegratedTerminal::showPrompt(const QString &prompt)
{
    m_currentPrompt = prompt;
    
    // 移动到文档末尾
    moveToEndOfDocument();
    
    QTextCursor cursor = textCursor();
    m_promptStartPos = cursor.position();
    
    // 插入提示符
    QTextCharFormat promptFormat;
    promptFormat.setForeground(QColor(getColorForType("prompt")));
    cursor.setCharFormat(promptFormat);
    cursor.insertText(prompt);
    
    // 记录命令开始位置
    m_commandStartPos = cursor.position();
    
    // 设置光标位置
    setTextCursor(cursor);
    
    // 滚动到底部
    QScrollBar *scrollBar = verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void IntegratedTerminal::clear()
{
    QTextEdit::clear();
    m_promptStartPos = 0;
    m_commandStartPos = 0;
}

void IntegratedTerminal::setMaxLines(int maxLines)
{
    m_maxLines = maxLines;
    limitLines();
}

void IntegratedTerminal::setCommandHistory(const QStringList &history)
{
    m_commandHistory = history;
    m_historyIndex = -1;
}

void IntegratedTerminal::addToHistory(const QString &command)
{
    if (!command.trimmed().isEmpty() && 
        (m_commandHistory.isEmpty() || m_commandHistory.last() != command)) {
        m_commandHistory.append(command);
        
        // 限制历史记录数量
        if (m_commandHistory.size() > 1000) {
            m_commandHistory.removeFirst();
        }
    }
    m_historyIndex = -1;
}

void IntegratedTerminal::keyPressEvent(QKeyEvent *event)
{
    // 处理文本选择相关的快捷键（优先处理）
    if (event->modifiers() & Qt::ControlModifier) {
        switch (event->key()) {
        case Qt::Key_C:
            if (textCursor().hasSelection()) {
                copy();  // 有选中文本时复制
                return;
            }
            // 没有选中文本时，继续到后面的中断处理
            break;
        case Qt::Key_A:
            selectAll();  // 全选
            return;
        case Qt::Key_V:
            if (!isInEditableArea()) {
                moveToEndOfDocument();
            }
            paste();  // 粘贴
            return;
        }
    }
    
    // 处理Shift+方向键等文本选择操作
    if (event->modifiers() & Qt::ShiftModifier) {
        // 允许所有Shift+键的组合用于文本选择
        QTextEdit::keyPressEvent(event);
        return;
    }
    
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        processCurrentCommand();
        return;
        
    case Qt::Key_Up:
        navigateHistory(-1);
        return;
        
    case Qt::Key_Down:
        navigateHistory(1);
        return;
        
    case Qt::Key_Left:
        // 只有在没有修饰键时才限制光标移动
        if (!(event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier))) {
            if (textCursor().position() <= m_commandStartPos) {
                return;
            }
        }
        break;
        
    case Qt::Key_Home:
        // Home键移动到命令开始位置
        {
            QTextCursor cursor = textCursor();
            cursor.setPosition(m_commandStartPos);
            setTextCursor(cursor);
        }
        return;
        
    case Qt::Key_Backspace:
        // 只有在当前命令区域才允许删除
        if (!textCursor().hasSelection() && textCursor().position() <= m_commandStartPos) {
            return;
        }
        break;
        
    case Qt::Key_L:
        if (event->modifiers() & Qt::ControlModifier) {
            emit requestClear();
            return;
        }
        break;
        
    case Qt::Key_C:
        if (event->modifiers() & Qt::ControlModifier) {
            // 如果当前命令为空，发送中断信号
            if (getCurrentCommand().isEmpty()) {
                emit requestInterrupt();
                return;
            }
        }
        break;
        
    case Qt::Key_Tab:
        // Tab补全（简化实现）
        {
            QString command = getCurrentCommand();
            if (!command.isEmpty()) {
                // 简单的文件名补全
                QStringList parts = command.split(' ');
                if (!parts.isEmpty()) {
                    QString lastPart = parts.last();
                    if (QFileInfo(lastPart).exists() || lastPart.contains('/') || lastPart.contains('\\')) {
                        // 触发文件路径补全
                        QTextEdit::keyPressEvent(event);
                        return;
                    }
                }
            }
        }
        return;
    }
    
    // 默认处理
    QTextEdit::keyPressEvent(event);
}

void IntegratedTerminal::mousePressEvent(QMouseEvent *event)
{
    // 完全允许正常的鼠标事件处理，不干预文本选择
    QTextEdit::mousePressEvent(event);
}

void IntegratedTerminal::mouseReleaseEvent(QMouseEvent *event)
{
    QTextEdit::mouseReleaseEvent(event);
    
    // 延迟检查，确保选择操作完成
    QTimer::singleShot(0, this, [this, event]() {
        // 如果没有选中文本且点击在历史区域，自动跳转到输入位置
        if (!textCursor().hasSelection() && event->button() == Qt::LeftButton) {
            QTextCursor cursor = textCursor();
            if (cursor.position() < m_commandStartPos) {
                moveToEndOfDocument();
            }
        }
    });
}

void IntegratedTerminal::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu *menu = new QMenu(this);
    
    // 复制相关菜单
    if (textCursor().hasSelection()) {
        QAction *copyAction = menu->addAction("复制");
        copyAction->setShortcut(QKeySequence::Copy);
        connect(copyAction, &QAction::triggered, this, &QTextEdit::copy);
        menu->addSeparator();
    }
    
    // 粘贴菜单（如果剪贴板有内容）
    QClipboard *clipboard = QApplication::clipboard();
    if (!clipboard->text().isEmpty()) {
        QAction *pasteAction = menu->addAction("粘贴");
        pasteAction->setShortcut(QKeySequence::Paste);
        connect(pasteAction, &QAction::triggered, [this]() {
            // 确保粘贴在当前命令位置
            if (!isInEditableArea()) {
                moveToEndOfDocument();
            }
            paste();
        });
        menu->addSeparator();
    }
    
    // 全选菜单
    QAction *selectAllAction = menu->addAction("全选");
    selectAllAction->setShortcut(QKeySequence::SelectAll);
    connect(selectAllAction, &QAction::triggered, this, &QTextEdit::selectAll);
    
    menu->addSeparator();
    
    // 终端操作菜单
    QAction *clearAction = menu->addAction("清屏");
    clearAction->setShortcut(QKeySequence("Ctrl+L"));
    connect(clearAction, &QAction::triggered, this, &IntegratedTerminal::requestClear);
    
    QAction *interruptAction = menu->addAction("中断进程");
    interruptAction->setShortcut(QKeySequence("Ctrl+C"));
    connect(interruptAction, &QAction::triggered, this, &IntegratedTerminal::requestInterrupt);
    
    menu->exec(event->globalPos());
    delete menu;
}

void IntegratedTerminal::navigateHistory(int direction)
{
    if (m_commandHistory.isEmpty()) {
        return;
    }
    
    if (m_historyIndex == -1) {
        m_tempCommand = getCurrentCommand();
    }
    
    m_historyIndex += direction;
    
    if (m_historyIndex < -1) {
        m_historyIndex = -1;
        setCurrentCommand(m_tempCommand);
    } else if (m_historyIndex >= m_commandHistory.size()) {
        m_historyIndex = m_commandHistory.size() - 1;
        setCurrentCommand(m_commandHistory[m_historyIndex]);
    } else if (m_historyIndex == -1) {
        setCurrentCommand(m_tempCommand);
    } else {
        setCurrentCommand(m_commandHistory[m_historyIndex]);
    }
}

void IntegratedTerminal::limitLines()
{
    if (document()->blockCount() <= m_maxLines) {
        return;
    }
    
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::Start);
    
    int linesToRemove = document()->blockCount() - m_maxLines;
    for (int i = 0; i < linesToRemove; ++i) {
        cursor.select(QTextCursor::BlockUnderCursor);
        cursor.removeSelectedText();
        cursor.deleteChar(); // 删除换行符
    }
    
    // 重新计算位置
    m_promptStartPos = qMax(0, m_promptStartPos - linesToRemove);
    m_commandStartPos = qMax(0, m_commandStartPos - linesToRemove);
}

QString IntegratedTerminal::getColorForType(const QString &type) const
{
    if (type == "error") {
        return "#ff6b6b";  // 红色
    } else if (type == "warning") {
        return "#feca57";  // 黄色
    } else if (type == "success") {
        return "#48dbfb";  // 青色
    } else if (type == "prompt") {
        return "#0be881";  // 绿色
    } else if (type == "info") {
        return "#a55eea";  // 紫色
    }
    return "#ffffff";      // 白色（默认）
}

void IntegratedTerminal::processCurrentCommand()
{
    QString command = getCurrentCommand().trimmed();
    
    // 添加换行
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText("\n");
    
    // 发送命令
    if (!command.isEmpty()) {
        addToHistory(command);
        emit commandEntered(command);
    } else {
        // 空命令，直接显示新提示符
        emit commandEntered("");
    }
}

bool IntegratedTerminal::isInEditableArea() const
{
    return textCursor().position() >= m_commandStartPos;
}

void IntegratedTerminal::moveToEndOfDocument()
{
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::End);
    setTextCursor(cursor);
}

QString IntegratedTerminal::getCurrentCommand() const
{
    QTextCursor cursor = textCursor();
    cursor.setPosition(m_commandStartPos);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    return cursor.selectedText();
}

void IntegratedTerminal::setCurrentCommand(const QString &command)
{
    QTextCursor cursor = textCursor();
    cursor.setPosition(m_commandStartPos);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    
    // 设置命令文本格式
    QTextCharFormat commandFormat;
    commandFormat.setForeground(QColor("#ffffff"));
    cursor.setCharFormat(commandFormat);
    
    cursor.insertText(command);
    setTextCursor(cursor);
}

// TerminalTool 实现
TerminalTool::TerminalTool(QWidget *parent)
    : QWidget(parent), DynamicObjectBase()
    , m_process(nullptr)
    , m_processRunning(false)
    , m_maxOutputLines(1000)
    , m_autoScroll(true)
{
    setupUI();
    loadSettings();
    
    // 初始化当前目录
    m_currentDirectory = QDir::currentPath();
    
    // 获取用户信息
    m_currentUser = qgetenv("USERNAME");
    if (m_currentUser.isEmpty()) {
        m_currentUser = qgetenv("USER");
    }
    if (m_currentUser.isEmpty()) {
        m_currentUser = "user";
    }
    
    m_currentHost = qgetenv("COMPUTERNAME");
    if (m_currentHost.isEmpty()) {
        m_currentHost = qgetenv("HOSTNAME");
    }
    if (m_currentHost.isEmpty()) {
        m_currentHost = "localhost";
    }
    
    // 显示欢迎信息和初始提示符
    m_terminal->appendOutput("终端工具 v1.0\n", "info");
    m_terminal->appendOutput("输入 'help' 查看可用命令\n", "info");
    m_terminal->appendOutput("使用 Ctrl+C 中断进程，Ctrl+L 清屏\n\n", "info");
    showPrompt();
    
    updateWindowTitle();
}

TerminalTool::~TerminalTool()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
    saveSettings();
}

void TerminalTool::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(5, 5, 5, 5);
    m_mainLayout->setSpacing(0);
    
    // 创建一体化终端
    m_terminal = new IntegratedTerminal(this);
    m_mainLayout->addWidget(m_terminal);
    
    // 连接信号
    connect(m_terminal, &IntegratedTerminal::commandEntered, 
            this, &TerminalTool::onCommandEntered);
    connect(m_terminal, &IntegratedTerminal::requestClear, 
            this, &TerminalTool::onRequestClear);
    connect(m_terminal, &IntegratedTerminal::requestInterrupt, 
            this, &TerminalTool::onRequestInterrupt);
    
    // 设置焦点
    m_terminal->setFocus();
    
    setupTerminalStyle();
    
    // 强制应用字体设置
    QTimer::singleShot(0, this, [this]() {
        if (m_terminal) {
            m_terminal->setFont(m_terminalFont);
            qDebug() << "延迟设置终端字体:" << m_terminalFont.family();
        }
    });
}

void TerminalTool::setupTerminalStyle()
{
    // 设置整体样式
    setStyleSheet(R"(
        QWidget {
            background-color: #0d1117;
        }
    )");
    
    // 设置终端字体 - 尝试多种编程字体
    QStringList fontFamilies = {
        "Consolas",
        "Source Code Pro", 
        "Monaco",
        "Menlo",
        "Ubuntu Mono",
        "DejaVu Sans Mono",
        "Liberation Mono",
        "Courier New"
    };
    
    // 尝试找到可用的编程字体
    for (const QString& fontFamily : fontFamilies) {
        m_terminalFont.setFamily(fontFamily);
        m_terminalFont.setPointSize(10);
        m_terminalFont.setFixedPitch(true);
        if (m_terminalFont.exactMatch()) {
            qDebug() << "终端使用字体:" << fontFamily;
            break;
        }
    }
    
    // 强制应用字体到终端
    if (m_terminal) {
        m_terminal->setFont(m_terminalFont);
        
        // 强制设置字体属性
        QTextCursor cursor = m_terminal->textCursor();
        QTextCharFormat format = cursor.charFormat();
        format.setFont(m_terminalFont);
        m_terminal->setCurrentCharFormat(format);
        
        qDebug() << "终端字体已设置为:" << m_terminalFont.family() << "11pt";
    }
}

void TerminalTool::onCommandEntered(const QString &command)
{
    QString trimmedCommand = command.trimmed();
    
    if (trimmedCommand.isEmpty()) {
        showPrompt();
        return;
    }
    
    // 添加到历史记录
    m_commandHistory.append(trimmedCommand);
    m_terminal->setCommandHistory(m_commandHistory);
    
    // 检查是否是内置命令
    if (isBuiltinCommand(trimmedCommand)) {
        handleBuiltinCommand(trimmedCommand);
    } else {
        executeCommand(trimmedCommand);
    }
}

void TerminalTool::executeCommand(const QString &command)
{
    if (m_processRunning) {
        m_terminal->appendOutput("进程正在运行中，请等待完成或使用 Ctrl+C 中断\n", "warning");
        showPrompt();
        return;
    }
    
    // 创建进程
    if (m_process) {
        delete m_process;
    }
    
    m_process = new QProcess(this);
    m_process->setWorkingDirectory(m_currentDirectory);
    
    // 设置环境变量
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    m_process->setProcessEnvironment(env);
    
    // 连接信号
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &TerminalTool::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred,
            this, &TerminalTool::onProcessError);
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &TerminalTool::onProcessReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &TerminalTool::onProcessReadyReadStandardError);
    
    // 在Windows上使用cmd执行命令，在Unix系统上使用sh
#ifdef Q_OS_WIN
    m_process->start("cmd", QStringList() << "/c" << command);
#else
    m_process->start("sh", QStringList() << "-c" << command);
#endif
    
    if (!m_process->waitForStarted(3000)) {
        m_terminal->appendOutput("无法启动进程: " + m_process->errorString() + "\n", "error");
        showPrompt();
        return;
    }
    
    m_processRunning = true;
}

void TerminalTool::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_processRunning = false;
    
    if (exitStatus == QProcess::CrashExit) {
        m_terminal->appendOutput("进程异常终止\n", "error");
    } else if (exitCode != 0) {
        m_terminal->appendOutput(QString("进程退出，退出码: %1\n").arg(exitCode), "warning");
    }
    
    showPrompt();
}

void TerminalTool::onProcessError(QProcess::ProcessError error)
{
    m_processRunning = false;
    
    QString errorMsg;
    switch (error) {
    case QProcess::FailedToStart:
        errorMsg = "无法启动进程";
        break;
    case QProcess::Crashed:
        errorMsg = "进程崩溃";
        break;
    case QProcess::Timedout:
        errorMsg = "进程超时";
        break;
    case QProcess::WriteError:
        errorMsg = "写入错误";
        break;
    case QProcess::ReadError:
        errorMsg = "读取错误";
        break;
    default:
        errorMsg = "未知错误";
        break;
    }
    
    m_terminal->appendOutput("进程错误: " + errorMsg + "\n", "error");
    showPrompt();
}

void TerminalTool::onProcessReadyReadStandardOutput()
{
    if (m_process) {
        QByteArray data = m_process->readAllStandardOutput();
        QString output = QString::fromLocal8Bit(data);
        m_terminal->appendOutput(output, "output");
    }
}

void TerminalTool::onProcessReadyReadStandardError()
{
    if (m_process) {
        QByteArray data = m_process->readAllStandardError();
        QString output = QString::fromLocal8Bit(data);
        m_terminal->appendOutput(output, "error");
    }
}

void TerminalTool::onRequestClear()
{
    m_terminal->clear();
    showPrompt();
}

void TerminalTool::onRequestInterrupt()
{
    if (m_processRunning && m_process) {
        m_terminal->appendOutput("^C\n", "warning");
        m_process->kill();
        if (!m_process->waitForFinished(3000)) {
            m_process->terminate();
        }
        m_processRunning = false;
        showPrompt();
    }
}

void TerminalTool::showPrompt()
{
    QString prompt = getCurrentPrompt();
    m_terminal->showPrompt(prompt);
}

void TerminalTool::handleBuiltinCommand(const QString &command)
{
    QStringList parts = command.split(' ', Qt::SkipEmptyParts);
    QString cmd = parts[0].toLower();
    
    if (cmd == "clear" || cmd == "cls") {
        m_terminal->clear();
    }
    else if (cmd == "exit" || cmd == "quit") {
        m_terminal->appendOutput("再见！\n", "info");
        QTimer::singleShot(500, this, [this]() {
            parentWidget()->close();
        });
        return;
    }
    else if (cmd == "pwd") {
        m_terminal->appendOutput(QDir::toNativeSeparators(m_currentDirectory) + "\n", "output");
    }
    else if (cmd == "cd") {
        if (parts.size() > 1) {
            QString path = parts.mid(1).join(' ');
            changeDirectory(path);
        } else {
            // cd 无参数，切换到用户主目录
            changeDirectory(QDir::homePath());
        }
    }
    else if (cmd == "help") {
        m_terminal->appendOutput("内置命令:\n", "info");
        m_terminal->appendOutput("  clear/cls  - 清屏\n", "output");
        m_terminal->appendOutput("  cd <path>  - 更改目录\n", "output");
        m_terminal->appendOutput("  pwd        - 显示当前目录\n", "output");
        m_terminal->appendOutput("  history    - 显示命令历史\n", "output");
        m_terminal->appendOutput("  exit/quit  - 退出终端\n", "output");
        m_terminal->appendOutput("\n快捷键:\n", "info");
        m_terminal->appendOutput("  Ctrl+L     - 清屏\n", "output");
        m_terminal->appendOutput("  Ctrl+C     - 中断当前进程\n", "output");
        m_terminal->appendOutput("  Up/Down    - 浏览命令历史\n", "output");
        m_terminal->appendOutput("  Tab        - 文件名自动补全\n", "output");
    }
    else if (cmd == "history") {
        m_terminal->appendOutput("命令历史:\n", "info");
        for (int i = 0; i < m_commandHistory.size(); ++i) {
            m_terminal->appendOutput(QString("  %1  %2\n")
                                 .arg(i + 1, 3)
                                 .arg(m_commandHistory[i]), "output");
        }
    }
    
    showPrompt();
}

bool TerminalTool::isBuiltinCommand(const QString &command) const
{
    QString cmd = command.split(' ', Qt::SkipEmptyParts).value(0).toLower();
    return BUILTIN_COMMANDS.contains(cmd);
}

void TerminalTool::changeDirectory(const QString &path)
{
    QString newPath = path;
    
    // 处理相对路径
    if (!QDir::isAbsolutePath(newPath)) {
        newPath = QDir(m_currentDirectory).absoluteFilePath(newPath);
    }
    
    // 规范化路径
    newPath = QDir::cleanPath(newPath);
    
    QDir dir(newPath);
    if (dir.exists()) {
        m_currentDirectory = dir.absolutePath();
        updateWindowTitle();
    } else {
        m_terminal->appendOutput("目录不存在: " + QDir::toNativeSeparators(newPath) + "\n", "error");
    }
}

QString TerminalTool::getCurrentPrompt() const
{
    QString path = formatPath(m_currentDirectory);
    return QString("%1@%2:%3$ ").arg(m_currentUser, m_currentHost, path);
}

QString TerminalTool::formatPath(const QString &path) const
{
    QString homePath = QDir::homePath();
    if (path.startsWith(homePath)) {
        return "~" + path.mid(homePath.length());
    }
    return QDir::toNativeSeparators(path);
}

void TerminalTool::updateWindowTitle()
{
    // 如果是主窗口的一部分，可以设置标题
    QString title = QString("终端 - %1").arg(formatPath(m_currentDirectory));
    if (window()) {
        window()->setWindowTitle(title);
    }
}

void TerminalTool::saveSettings()
{
    QSettings settings;
    settings.setValue("TerminalTool/currentDirectory", m_currentDirectory);
    settings.setValue("TerminalTool/maxOutputLines", m_maxOutputLines);
    settings.setValue("TerminalTool/commandHistory", m_commandHistory);
}

void TerminalTool::loadSettings()
{
    QSettings settings;
    
    QString savedDir = settings.value("TerminalTool/currentDirectory", 
                                     QDir::currentPath()).toString();
    if (QDir(savedDir).exists()) {
        m_currentDirectory = savedDir;
    }
    
    m_maxOutputLines = settings.value("TerminalTool/maxOutputLines", 1000).toInt();
    m_commandHistory = settings.value("TerminalTool/commandHistory", 
                                     QStringList()).toStringList();
    
    if (m_terminal) {
        m_terminal->setMaxLines(m_maxOutputLines);
        m_terminal->setCommandHistory(m_commandHistory);
    }
}