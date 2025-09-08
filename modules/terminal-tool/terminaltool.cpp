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

REGISTER_DYNAMICOBJECT(TerminalTool);

// 内置命令列表
const QStringList TerminalTool::BUILTIN_COMMANDS = {
    "clear", "cls", "exit", "quit", "pwd", "cd", "help", "history"
};

// TerminalInput 实现
TerminalInput::TerminalInput(QWidget *parent)
    : QLineEdit(parent)
    , m_historyIndex(-1)
    , m_completer(nullptr)
    , m_fileModel(nullptr)
{
    setupCompleter();
    setPlaceholderText("输入命令...");
}

void TerminalInput::setupCompleter()
{
    // 设置文件系统自动补全
    m_fileModel = new QFileSystemModel(this);
    m_fileModel->setRootPath("");
    m_fileModel->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
    
    m_completer = new QCompleter(this);
    m_completer->setModel(m_fileModel);
    m_completer->setCompletionMode(QCompleter::InlineCompletion);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    
    setCompleter(m_completer);
}

void TerminalInput::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (!text().trimmed().isEmpty()) {
            addToHistory(text());
            emit commandEntered(text());
            clear();
        }
        return;
        
    case Qt::Key_Up:
        navigateHistory(-1);
        return;
        
    case Qt::Key_Down:
        navigateHistory(1);
        return;
        
    case Qt::Key_L:
        if (event->modifiers() & Qt::ControlModifier) {
            emit requestClear();
            return;
        }
        break;
        
    case Qt::Key_C:
        if (event->modifiers() & Qt::ControlModifier) {
            if (text().isEmpty()) {
                emit requestInterrupt();
                return;
            }
        }
        break;
        
    case Qt::Key_Tab:
        // Tab补全由QCompleter处理
        break;
    }
    
    QLineEdit::keyPressEvent(event);
}

void TerminalInput::setCommandHistory(const QStringList &history)
{
    m_commandHistory = history;
    m_historyIndex = -1;
}

void TerminalInput::addToHistory(const QString &command)
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

void TerminalInput::clearHistory()
{
    m_commandHistory.clear();
    m_historyIndex = -1;
}

void TerminalInput::navigateHistory(int direction)
{
    if (m_commandHistory.isEmpty()) {
        return;
    }
    
    if (m_historyIndex == -1) {
        m_currentCommand = text();
    }
    
    m_historyIndex += direction;
    
    if (m_historyIndex < -1) {
        m_historyIndex = -1;
        setText(m_currentCommand);
    } else if (m_historyIndex >= m_commandHistory.size()) {
        m_historyIndex = m_commandHistory.size() - 1;
        setText(m_commandHistory[m_historyIndex]);
    } else if (m_historyIndex == -1) {
        setText(m_currentCommand);
    } else {
        setText(m_commandHistory[m_historyIndex]);
    }
}

// TerminalOutput 实现
TerminalOutput::TerminalOutput(QWidget *parent)
    : QTextEdit(parent)
    , m_maxLines(1000)
{
    setReadOnly(true);
    setUndoRedoEnabled(false);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    // 设置字体
    QFont font("Consolas", 10);
    if (!font.exactMatch()) {
        font.setFamily("Courier New");
    }
    font.setFixedPitch(true);
    setFont(font);
    
    // 设置样式
    setStyleSheet(R"(
        QTextEdit {
            background-color: #000000;
            color: #ffffff;
            border: 1px solid #333333;
            selection-background-color: #444444;
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
    )");
}

void TerminalOutput::appendOutput(const QString &text, const QString &type)
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
    
    // 限制行数
    limitLines();
    
    // 滚动到底部
    QScrollBar *scrollBar = verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void TerminalOutput::appendPrompt(const QString &prompt)
{
    m_currentPrompt = prompt;
    appendOutput(prompt, "prompt");
}

void TerminalOutput::clear()
{
    QTextEdit::clear();
}

void TerminalOutput::setMaxLines(int maxLines)
{
    m_maxLines = maxLines;
    limitLines();
}

void TerminalOutput::keyPressEvent(QKeyEvent *event)
{
    // 阻止在输出区域的编辑操作，但允许复制
    if (event->matches(QKeySequence::Copy)) {
        QTextEdit::keyPressEvent(event);
    }
    // 其他按键事件忽略，让父级处理
}

void TerminalOutput::mousePressEvent(QMouseEvent *event)
{
    // 允许文本选择
    QTextEdit::mousePressEvent(event);
}

void TerminalOutput::limitLines()
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
}

QString TerminalOutput::getColorForType(const QString &type) const
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
    m_output->appendOutput("终端工具 v1.0\n", "info");
    m_output->appendOutput("输入 'help' 查看可用命令\n", "info");
    m_output->appendOutput("使用 Ctrl+C 中断进程，Ctrl+L 清屏\n\n", "info");
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
    m_mainLayout->setSpacing(5);
    
    // 输出区域
    m_output = new TerminalOutput(this);
    m_mainLayout->addWidget(m_output);
    
    // 输入区域
    m_inputLayout = new QHBoxLayout();
    m_inputLayout->setContentsMargins(0, 0, 0, 0);
    m_inputLayout->setSpacing(5);
    
    m_promptLabel = new QLabel(this);
    m_promptLabel->setStyleSheet(R"(
        QLabel {
            color: #0be881;
            font-family: 'Consolas', 'Courier New', monospace;
            font-size: 10pt;
            font-weight: bold;
        }
    )");
    
    m_input = new TerminalInput(this);
    m_input->setStyleSheet(R"(
        QLineEdit {
            background-color: #1a1a1a;
            color: #ffffff;
            border: 1px solid #333333;
            border-radius: 4px;
            padding: 5px;
            font-family: 'Consolas', 'Courier New', monospace;
            font-size: 10pt;
        }
        QLineEdit:focus {
            border-color: #0be881;
        }
    )");
    
    m_inputLayout->addWidget(m_promptLabel);
    m_inputLayout->addWidget(m_input);
    
    m_mainLayout->addLayout(m_inputLayout);
    
    // 设置焦点
    m_input->setFocus();
    
    // 连接信号
    connect(m_input, &TerminalInput::commandEntered, 
            this, &TerminalTool::onCommandEntered);
    connect(m_input, &TerminalInput::requestClear, 
            this, &TerminalTool::onRequestClear);
    connect(m_input, &TerminalInput::requestInterrupt, 
            this, &TerminalTool::onRequestInterrupt);
    
    setupTerminalStyle();
}

void TerminalTool::setupTerminalStyle()
{
    // 设置整体样式
    setStyleSheet(R"(
        QWidget {
            background-color: #0d1117;
        }
    )");
    
    // 设置终端字体
    m_terminalFont = QFont("Consolas", 10);
    if (!m_terminalFont.exactMatch()) {
        m_terminalFont.setFamily("Courier New");
    }
    m_terminalFont.setFixedPitch(true);
}

void TerminalTool::onCommandEntered(const QString &command)
{
    QString trimmedCommand = command.trimmed();
    if (trimmedCommand.isEmpty()) {
        showPrompt();
        return;
    }
    
    // 显示输入的命令
    m_output->appendOutput(trimmedCommand + "\n", "output");
    
    // 添加到历史记录
    m_commandHistory.append(trimmedCommand);
    m_input->setCommandHistory(m_commandHistory);
    
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
        m_output->appendOutput("进程正在运行中，请等待完成或使用 Ctrl+C 中断\n", "warning");
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
        m_output->appendOutput("无法启动进程: " + m_process->errorString() + "\n", "error");
        showPrompt();
        return;
    }
    
    m_processRunning = true;
}

void TerminalTool::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_processRunning = false;
    
    if (exitStatus == QProcess::CrashExit) {
        m_output->appendOutput("进程异常终止\n", "error");
    } else if (exitCode != 0) {
        m_output->appendOutput(QString("进程退出，退出码: %1\n").arg(exitCode), "warning");
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
    
    m_output->appendOutput("进程错误: " + errorMsg + "\n", "error");
    showPrompt();
}

void TerminalTool::onProcessReadyReadStandardOutput()
{
    if (m_process) {
        QByteArray data = m_process->readAllStandardOutput();
        QString output = QString::fromLocal8Bit(data);
        m_output->appendOutput(output, "output");
    }
}

void TerminalTool::onProcessReadyReadStandardError()
{
    if (m_process) {
        QByteArray data = m_process->readAllStandardError();
        QString output = QString::fromLocal8Bit(data);
        m_output->appendOutput(output, "error");
    }
}

void TerminalTool::onRequestClear()
{
    m_output->clear();
    showPrompt();
}

void TerminalTool::onRequestInterrupt()
{
    if (m_processRunning && m_process) {
        m_output->appendOutput("^C\n", "warning");
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
    m_promptLabel->setText(prompt);
    m_output->appendPrompt(prompt);
    m_input->setFocus();
}

void TerminalTool::handleBuiltinCommand(const QString &command)
{
    QStringList parts = command.split(' ', Qt::SkipEmptyParts);
    QString cmd = parts[0].toLower();
    
    if (cmd == "clear" || cmd == "cls") {
        m_output->clear();
    }
    else if (cmd == "exit" || cmd == "quit") {
        m_output->appendOutput("再见！\n", "info");
        QTimer::singleShot(500, this, [this]() {
            parentWidget()->close();
        });
        return;
    }
    else if (cmd == "pwd") {
        m_output->appendOutput(QDir::toNativeSeparators(m_currentDirectory) + "\n", "output");
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
        m_output->appendOutput("内置命令:\n", "info");
        m_output->appendOutput("  clear/cls  - 清屏\n", "output");
        m_output->appendOutput("  cd <path>  - 更改目录\n", "output");
        m_output->appendOutput("  pwd        - 显示当前目录\n", "output");
        m_output->appendOutput("  history    - 显示命令历史\n", "output");
        m_output->appendOutput("  exit/quit  - 退出终端\n", "output");
        m_output->appendOutput("\n快捷键:\n", "info");
        m_output->appendOutput("  Ctrl+L     - 清屏\n", "output");
        m_output->appendOutput("  Ctrl+C     - 中断当前进程\n", "output");
        m_output->appendOutput("  Up/Down    - 浏览命令历史\n", "output");
        m_output->appendOutput("  Tab        - 文件名自动补全\n", "output");
    }
    else if (cmd == "history") {
        m_output->appendOutput("命令历史:\n", "info");
        for (int i = 0; i < m_commandHistory.size(); ++i) {
            m_output->appendOutput(QString("  %1  %2\n")
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
        m_output->appendOutput("", "output"); // 空行，表示成功
        updateWindowTitle();
    } else {
        m_output->appendOutput("目录不存在: " + QDir::toNativeSeparators(newPath) + "\n", "error");
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
    
    if (m_output) {
        m_output->setMaxLines(m_maxOutputLines);
    }
    
    if (m_input) {
        m_input->setCommandHistory(m_commandHistory);
    }
}
