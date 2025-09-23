#include "filecomparetool.h"
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>

// 动态对象创建宏
REGISTER_DYNAMICOBJECT(FileCompareTool);

// MyersDiffAlgorithm 实现
QList<DiffItem> MyersDiffAlgorithm::compare(const QStringList& left, const QStringList& right) {
    // 简化版的Myers算法实现
    QList<DiffItem> result;

    int leftSize = left.size();
    int rightSize = right.size();

    // 使用动态规划方法计算编辑距离
    QVector<QVector<int>> dp(leftSize + 1, QVector<int>(rightSize + 1, 0));

    // 初始化边界条件
    for (int i = 0; i <= leftSize; ++i) {
        dp[i][0] = i;
    }
    for (int j = 0; j <= rightSize; ++j) {
        dp[0][j] = j;
    }

    // 填充DP表
    for (int i = 1; i <= leftSize; ++i) {
        for (int j = 1; j <= rightSize; ++j) {
            if (left[i-1] == right[j-1]) {
                dp[i][j] = dp[i-1][j-1];
            } else {
                dp[i][j] = 1 + qMin(dp[i-1][j], qMin(dp[i][j-1], dp[i-1][j-1]));
            }
        }
    }

    // 回溯构建差异序列
    int i = leftSize, j = rightSize;
    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && left[i-1] == right[j-1]) {
            result.prepend(DiffItem(DiffType::Unchanged, i, j, left[i-1], right[j-1]));
            i--; j--;
        } else if (i > 0 && (j == 0 || dp[i-1][j] <= dp[i][j-1])) {
            result.prepend(DiffItem(DiffType::Deleted, i, -1, left[i-1], ""));
            i--;
        } else {
            result.prepend(DiffItem(DiffType::Added, -1, j, "", right[j-1]));
            j--;
        }
    }

    return result;
}

// SimpleDiffAlgorithm 实现
QList<DiffItem> SimpleDiffAlgorithm::compare(const QStringList& left, const QStringList& right) {
    QList<DiffItem> result;

    int maxLines = qMax(left.size(), right.size());

    for (int i = 0; i < maxLines; ++i) {
        QString leftLine = (i < left.size()) ? left[i] : "";
        QString rightLine = (i < right.size()) ? right[i] : "";

        if (i >= left.size()) {
            // 右侧有额外行
            result.append(DiffItem(DiffType::Added, -1, i + 1, "", rightLine));
        } else if (i >= right.size()) {
            // 左侧有额外行
            result.append(DiffItem(DiffType::Deleted, i + 1, -1, leftLine, ""));
        } else if (leftLine == rightLine) {
            // 行相同
            result.append(DiffItem(DiffType::Unchanged, i + 1, i + 1, leftLine, rightLine));
        } else {
            // 行不同
            result.append(DiffItem(DiffType::Modified, i + 1, i + 1, leftLine, rightLine));
        }
    }

    return result;
}

// FileCompareWorker 实现
FileCompareWorker::FileCompareWorker(QObject* parent)
    : QObject(parent), m_algorithm(nullptr), m_recursive(false), m_shouldStop(false) {
}

void FileCompareWorker::compareFiles(const QString& leftFile, const QString& rightFile, IDiffAlgorithm* algorithm) {
    m_leftFile = leftFile;
    m_rightFile = rightFile;
    m_algorithm = algorithm;

    QTimer::singleShot(0, this, &FileCompareWorker::doFileCompare);
}

void FileCompareWorker::compareTexts(const QString& leftText, const QString& rightText, IDiffAlgorithm* algorithm) {
    m_leftText = leftText;
    m_rightText = rightText;
    m_algorithm = algorithm;

    QTimer::singleShot(0, this, &FileCompareWorker::doTextCompare);
}

void FileCompareWorker::doFileCompare() {
    FileCompareResult result;
    result.leftFile = m_leftFile;
    result.rightFile = m_rightFile;

    try {
        emit progressUpdated(0, 100, "读取文件...");

        QStringList leftLines = readFileLines(m_leftFile);
        if (leftLines.isEmpty() && QFileInfo::exists(m_leftFile)) {
            // 文件存在但为空
            leftLines.append("");
        }

        emit progressUpdated(25, 100, "读取文件...");

        QStringList rightLines = readFileLines(m_rightFile);
        if (rightLines.isEmpty() && QFileInfo::exists(m_rightFile)) {
            // 文件存在但为空
            rightLines.append("");
        }

        emit progressUpdated(50, 100, "比较文件...");

        if (m_algorithm) {
            result.differences = m_algorithm->compare(leftLines, rightLines);
        }

        emit progressUpdated(75, 100, "分析结果...");

        // 统计差异
        for (const DiffItem& item : result.differences) {
            switch (item.type) {
                case DiffType::Added:
                    result.addedLines++;
                    break;
                case DiffType::Deleted:
                    result.deletedLines++;
                    break;
                case DiffType::Modified:
                    result.modifiedLines++;
                    break;
                case DiffType::Unchanged:
                    result.unchangedLines++;
                    break;
            }
        }

        // 计算相似度
        result.similarity = calculateSimilarity(result.differences);

        emit progressUpdated(100, 100, "完成");
        emit fileCompareFinished(result);

    } catch (const std::exception& e) {
        result.error = QString("比较失败: %1").arg(e.what());
        emit fileCompareFinished(result);
    }
}

void FileCompareWorker::doTextCompare() {
    FileCompareResult result;
    result.leftFile = "文本A";
    result.rightFile = "文本B";

    try {
        emit progressUpdated(0, 100, "处理文本...");

        QStringList leftLines = m_leftText.split('\n');
        QStringList rightLines = m_rightText.split('\n');

        emit progressUpdated(50, 100, "比较文本...");

        if (m_algorithm) {
            result.differences = m_algorithm->compare(leftLines, rightLines);
        }

        // 统计差异
        for (const DiffItem& item : result.differences) {
            switch (item.type) {
                case DiffType::Added:
                    result.addedLines++;
                    break;
                case DiffType::Deleted:
                    result.deletedLines++;
                    break;
                case DiffType::Modified:
                    result.modifiedLines++;
                    break;
                case DiffType::Unchanged:
                    result.unchangedLines++;
                    break;
            }
        }

        // 计算相似度
        result.similarity = calculateSimilarity(result.differences);

        emit progressUpdated(100, 100, "完成");
        emit fileCompareFinished(result);

    } catch (const std::exception& e) {
        result.error = QString("比较失败: %1").arg(e.what());
        emit fileCompareFinished(result);
    }
}

void FileCompareWorker::doDirectoryCompare() {
    // 目录比较的实现（简化版）
    QList<DirCompareItem> items;
    emit directoryCompareFinished(items);
}

QStringList FileCompareWorker::readFileLines(const QString& filePath) {
    QStringList lines;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        throw std::runtime_error(QString("无法打开文件: %1").arg(filePath).toStdString());
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);

    while (!stream.atEnd()) {
        lines.append(stream.readLine());
    }

    return lines;
}

double FileCompareWorker::calculateSimilarity(const QList<DiffItem>& differences) {
    if (differences.isEmpty()) {
        return 100.0;
    }

    int totalLines = differences.size();
    int unchangedLines = 0;

    for (const DiffItem& item : differences) {
        if (item.type == DiffType::Unchanged) {
            unchangedLines++;
        }
    }

    return (double)unchangedLines / totalLines * 100.0;
}

// DiffTextEditor 实现
DiffTextEditor::DiffTextEditor(QWidget* parent)
    : QTextEdit(parent), m_isLeftEditor(true), m_highlightedLine(-1) {

    setReadOnly(true);
    setFont(QFont("Consolas", 10));
    setupSyntaxHighlighting();
}

void DiffTextEditor::setDiffItems(const QList<DiffItem>& items, bool isLeft) {
    m_diffItems = items;
    m_isLeftEditor = isLeft;

    clear();

    QTextCursor cursor(document());

    for (const DiffItem& item : items) {
        QString text = isLeft ? item.leftText : item.rightText;

        if ((isLeft && item.leftLineNum == -1) || (!isLeft && item.rightLineNum == -1)) {
            // 这行在当前侧不存在，跳过
            continue;
        }

        QTextCharFormat format;
        QColor bgColor = getLineColor(item.type);
        if (bgColor.isValid()) {
            format.setBackground(bgColor);
        }

        cursor.insertText(text + "\n", format);
    }

    // 回到文档开头
    moveCursor(QTextCursor::Start);
}

void DiffTextEditor::setupSyntaxHighlighting() {
    setStyleSheet(
        "QTextEdit {"
        "border: 1px solid #bdc3c7;"
        "background-color: #ffffff;"
        "selection-background-color: #3498db;"
        "}"
    );
}

QColor DiffTextEditor::getLineColor(DiffType type) const {
    switch (type) {
        case DiffType::Added:
            return QColor(220, 255, 220);      // 浅绿色
        case DiffType::Deleted:
            return QColor(255, 220, 220);      // 浅红色
        case DiffType::Modified:
            return QColor(255, 255, 220);      // 浅黄色
        case DiffType::Unchanged:
        default:
            return QColor();                    // 透明/默认
    }
}

void DiffTextEditor::highlightLine(int lineNumber) {
    m_highlightedLine = lineNumber;
    update();
}

void DiffTextEditor::clearHighlight() {
    m_highlightedLine = -1;
    update();
}

void DiffTextEditor::paintEvent(QPaintEvent* event) {
    QTextEdit::paintEvent(event);

    // 这里可以添加自定义绘制逻辑，比如行号显示
}

// FileContentCompare 实现
FileContentCompare::FileContentCompare(QWidget* parent)
    : QWidget(parent), m_workerThread(nullptr), m_worker(nullptr) {

    setupUI();
    updateUI();
}

void FileContentCompare::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(15);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);

    // 创建主分割器
    m_mainSplitter = new QSplitter(Qt::Horizontal);

    // 左侧控制区域
    m_fileWidget = new QWidget();
    m_fileWidget->setFixedWidth(400);
    m_fileLayout = new QVBoxLayout(m_fileWidget);

    // 文件选择组
    m_fileGroup = new QGroupBox("选择文件");
    m_fileGroup->setStyleSheet(
        "QGroupBox {"
        "font-weight: bold;"
        "border: 2px solid #bdc3c7;"
        "border-radius: 8px;"
        "margin-top: 10px;"
        "padding-top: 10px;"
        "}"
        "QGroupBox::title {"
        "subcontrol-origin: margin;"
        "left: 10px;"
        "padding: 0 5px 0 5px;"
        "}"
    );

    m_fileGridLayout = new QGridLayout(m_fileGroup);

    // 左侧文件
    m_leftFileLabel = new QLabel("文件A:");
    m_leftFileEdit = new QLineEdit();
    m_leftFileEdit->setPlaceholderText("选择第一个文件...");
    m_leftFileEdit->setStyleSheet("padding: 8px; border: 1px solid #bdc3c7; border-radius: 4px;");
    m_selectLeftBtn = new QPushButton("📁 浏览");
    m_selectLeftBtn->setStyleSheet(
        "QPushButton {"
        "background-color: #3498db;"
        "color: white;"
        "border: none;"
        "padding: 8px 16px;"
        "border-radius: 4px;"
        "font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "background-color: #2980b9;"
        "}"
    );

    // 右侧文件
    m_rightFileLabel = new QLabel("文件B:");
    m_rightFileEdit = new QLineEdit();
    m_rightFileEdit->setPlaceholderText("选择第二个文件...");
    m_rightFileEdit->setStyleSheet(m_leftFileEdit->styleSheet());
    m_selectRightBtn = new QPushButton("📁 浏览");
    m_selectRightBtn->setStyleSheet(m_selectLeftBtn->styleSheet());

    m_fileGridLayout->addWidget(m_leftFileLabel, 0, 0);
    m_fileGridLayout->addWidget(m_leftFileEdit, 0, 1);
    m_fileGridLayout->addWidget(m_selectLeftBtn, 0, 2);
    m_fileGridLayout->addWidget(m_rightFileLabel, 1, 0);
    m_fileGridLayout->addWidget(m_rightFileEdit, 1, 1);
    m_fileGridLayout->addWidget(m_selectRightBtn, 1, 2);

    // 比较选项组
    m_optionsGroup = new QGroupBox("比较选项");
    m_optionsGroup->setStyleSheet(m_fileGroup->styleSheet());
    m_optionsLayout = new QFormLayout(m_optionsGroup);

    m_algorithmCombo = new QComboBox();
    m_algorithmCombo->addItem("Myers算法", 0);
    m_algorithmCombo->addItem("简单算法", 1);
    m_algorithmCombo->setStyleSheet("padding: 8px; border: 1px solid #bdc3c7; border-radius: 4px;");

    m_ignoreWhitespaceCheck = new QCheckBox("忽略空白字符");
    m_ignoreCaseCheck = new QCheckBox("忽略大小写");

    m_optionsLayout->addRow("算法:", m_algorithmCombo);
    m_optionsLayout->addRow("", m_ignoreWhitespaceCheck);
    m_optionsLayout->addRow("", m_ignoreCaseCheck);

    // 控制按钮
    m_buttonLayout = new QHBoxLayout();
    m_compareBtn = new QPushButton("🔍 开始比较");
    m_clearBtn = new QPushButton("🗑️ 清空");
    m_saveReportBtn = new QPushButton("💾 保存报告");
    m_copyDiffBtn = new QPushButton("📋 复制差异");

    QString btnStyle =
        "QPushButton {"
        "background-color: #27ae60;"
        "color: white;"
        "border: none;"
        "padding: 10px 20px;"
        "border-radius: 6px;"
        "font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "background-color: #219a52;"
        "}"
        "QPushButton:disabled {"
        "background-color: #bdc3c7;"
        "}";

    m_compareBtn->setStyleSheet(btnStyle);
    m_clearBtn->setStyleSheet(btnStyle.replace("#27ae60", "#95a5a6").replace("#219a52", "#7f8c8d"));
    m_saveReportBtn->setStyleSheet(btnStyle.replace("#27ae60", "#3498db").replace("#219a52", "#2980b9"));
    m_copyDiffBtn->setStyleSheet(btnStyle.replace("#27ae60", "#e74c3c").replace("#219a52", "#c0392b"));

    m_saveReportBtn->setEnabled(false);
    m_copyDiffBtn->setEnabled(false);

    m_buttonLayout->addWidget(m_compareBtn);
    m_buttonLayout->addWidget(m_clearBtn);
    m_buttonLayout->addWidget(m_saveReportBtn);
    m_buttonLayout->addWidget(m_copyDiffBtn);

    // 状态显示组
    m_statusGroup = new QGroupBox("状态");
    m_statusGroup->setStyleSheet(m_fileGroup->styleSheet());
    m_statusLayout = new QVBoxLayout(m_statusGroup);

    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    m_statusLabel = new QLabel("准备就绪");
    m_statusLabel->setStyleSheet("color: #7f8c8d; font-style: italic;");

    m_statusLayout->addWidget(m_progressBar);
    m_statusLayout->addWidget(m_statusLabel);

    // 统计信息组
    m_statsGroup = new QGroupBox("比较结果");
    m_statsGroup->setStyleSheet(m_fileGroup->styleSheet());
    m_statsLayout = new QFormLayout(m_statsGroup);

    m_addedLabel = new QLabel("-");
    m_deletedLabel = new QLabel("-");
    m_modifiedLabel = new QLabel("-");
    m_similarityLabel = new QLabel("-");

    m_statsLayout->addRow("新增行:", m_addedLabel);
    m_statsLayout->addRow("删除行:", m_deletedLabel);
    m_statsLayout->addRow("修改行:", m_modifiedLabel);
    m_statsLayout->addRow("相似度:", m_similarityLabel);

    // 组装左侧布局
    m_fileLayout->addWidget(m_fileGroup);
    m_fileLayout->addWidget(m_optionsGroup);
    m_fileLayout->addLayout(m_buttonLayout);
    m_fileLayout->addWidget(m_statusGroup);
    m_fileLayout->addWidget(m_statsGroup);
    m_fileLayout->addStretch();

    // 右侧结果区域
    m_resultWidget = new QWidget();
    m_resultLayout = new QVBoxLayout(m_resultWidget);

    // 差异显示分割器
    m_diffSplitter = new QSplitter(Qt::Horizontal);

    // 左侧编辑器
    m_leftGroup = new QGroupBox("文件A");
    m_leftGroup->setStyleSheet(m_fileGroup->styleSheet());
    m_leftLayout = new QVBoxLayout(m_leftGroup);
    m_leftEditor = new DiffTextEditor();
    m_leftLayout->addWidget(m_leftEditor);

    // 右侧编辑器
    m_rightGroup = new QGroupBox("文件B");
    m_rightGroup->setStyleSheet(m_fileGroup->styleSheet());
    m_rightLayout = new QVBoxLayout(m_rightGroup);
    m_rightEditor = new DiffTextEditor();
    m_rightLayout->addWidget(m_rightEditor);

    m_diffSplitter->addWidget(m_leftGroup);
    m_diffSplitter->addWidget(m_rightGroup);
    m_diffSplitter->setSizes({500, 500});

    m_resultLayout->addWidget(m_diffSplitter);

    // 组装主布局
    m_mainSplitter->addWidget(m_fileWidget);
    m_mainSplitter->addWidget(m_resultWidget);
    m_mainSplitter->setSizes({400, 800});

    m_mainLayout->addWidget(m_mainSplitter);

    // 连接信号
    connect(m_selectLeftBtn, &QPushButton::clicked, this, &FileContentCompare::onSelectLeftFileClicked);
    connect(m_selectRightBtn, &QPushButton::clicked, this, &FileContentCompare::onSelectRightFileClicked);
    connect(m_compareBtn, &QPushButton::clicked, this, &FileContentCompare::onCompareClicked);
    connect(m_clearBtn, &QPushButton::clicked, this, &FileContentCompare::onClearClicked);
    connect(m_saveReportBtn, &QPushButton::clicked, this, &FileContentCompare::onSaveReportClicked);
    connect(m_copyDiffBtn, &QPushButton::clicked, this, &FileContentCompare::onCopyDiffClicked);
}

void FileContentCompare::onSelectLeftFileClicked() {
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "选择文件A",
        "",
        "文本文件 (*.txt *.log *.csv *.json *.xml *.html *.css *.js *.cpp *.h *.py *.java *.cs);;所有文件 (*.*)"
    );

    if (!fileName.isEmpty()) {
        m_leftFileEdit->setText(fileName);
        updateUI();
    }
}

void FileContentCompare::onSelectRightFileClicked() {
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "选择文件B",
        "",
        "文本文件 (*.txt *.log *.csv *.json *.xml *.html *.css *.js *.cpp *.h *.py *.java *.cs);;所有文件 (*.*)"
    );

    if (!fileName.isEmpty()) {
        m_rightFileEdit->setText(fileName);
        updateUI();
    }
}

void FileContentCompare::onCompareClicked() {
    QString leftFile = m_leftFileEdit->text().trimmed();
    QString rightFile = m_rightFileEdit->text().trimmed();

    if (leftFile.isEmpty() || rightFile.isEmpty()) {
        QMessageBox::warning(this, "警告", "请选择两个要比较的文件");
        return;
    }

    if (!QFileInfo::exists(leftFile)) {
        QMessageBox::warning(this, "错误", QString("文件A不存在: %1").arg(leftFile));
        return;
    }

    if (!QFileInfo::exists(rightFile)) {
        QMessageBox::warning(this, "错误", QString("文件B不存在: %1").arg(rightFile));
        return;
    }

    // 创建工作线程
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait();
        m_workerThread->deleteLater();
    }

    m_workerThread = new QThread(this);
    m_worker = new FileCompareWorker();
    m_worker->moveToThread(m_workerThread);

    connect(m_worker, &FileCompareWorker::fileCompareFinished, this, &FileContentCompare::onFileCompareFinished);
    connect(m_worker, &FileCompareWorker::progressUpdated, this, &FileContentCompare::onProgressUpdated);
    connect(m_worker, &FileCompareWorker::errorOccurred, this, &FileContentCompare::onCompareError);

    // 选择算法
    IDiffAlgorithm* algorithm = nullptr;
    int algorithmIndex = m_algorithmCombo->currentData().toInt();
    if (algorithmIndex == 0) {
        algorithm = &m_myersAlgorithm;
    } else {
        algorithm = &m_simpleAlgorithm;
    }

    m_compareBtn->setEnabled(false);
    m_progressBar->setVisible(true);
    m_statusLabel->setText("正在比较文件...");

    m_workerThread->start();
    m_worker->compareFiles(leftFile, rightFile, algorithm);
}

void FileContentCompare::onClearClicked() {
    m_leftFileEdit->clear();
    m_rightFileEdit->clear();
    m_leftEditor->clear();
    m_rightEditor->clear();

    // 重置统计信息
    m_addedLabel->setText("-");
    m_deletedLabel->setText("-");
    m_modifiedLabel->setText("-");
    m_similarityLabel->setText("-");

    m_saveReportBtn->setEnabled(false);
    m_copyDiffBtn->setEnabled(false);
    m_statusLabel->setText("准备就绪");

    updateUI();
}

void FileContentCompare::onSaveReportClicked() {
    if (!m_lastResult.isValid()) {
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(
        this,
        "保存比较报告",
        QString("file_compare_report_%1.txt")
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")),
        "文本文件 (*.txt);;HTML文件 (*.html);;所有文件 (*.*)"
    );

    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream.setEncoding(QStringConverter::Utf8);

            stream << "文件比较报告\n";
            stream << "===============\n\n";
            stream << QString("文件A: %1\n").arg(m_lastResult.leftFile);
            stream << QString("文件B: %1\n").arg(m_lastResult.rightFile);
            stream << QString("比较时间: %1\n").arg(m_lastResult.compareTime.toString());
            stream << QString("算法: %1\n\n").arg(m_algorithmCombo->currentText());

            stream << "统计信息:\n";
            stream << QString("- 新增行: %1\n").arg(m_lastResult.addedLines);
            stream << QString("- 删除行: %1\n").arg(m_lastResult.deletedLines);
            stream << QString("- 修改行: %1\n").arg(m_lastResult.modifiedLines);
            stream << QString("- 未变更行: %1\n").arg(m_lastResult.unchangedLines);
            stream << QString("- 相似度: %1%\n\n").arg(m_lastResult.similarity, 0, 'f', 1);

            stream << "详细差异:\n";
            stream << "----------\n";

            for (const DiffItem& item : m_lastResult.differences) {
                QString prefix;
                switch (item.type) {
                    case DiffType::Added:
                        prefix = "+ ";
                        break;
                    case DiffType::Deleted:
                        prefix = "- ";
                        break;
                    case DiffType::Modified:
                        prefix = "~ ";
                        break;
                    case DiffType::Unchanged:
                        prefix = "  ";
                        break;
                }

                if (item.type != DiffType::Unchanged) { // 只显示有差异的行
                    if (item.leftLineNum != -1) {
                        stream << QString("%1[%2] %3\n").arg(prefix).arg(item.leftLineNum).arg(item.leftText);
                    }
                    if (item.rightLineNum != -1 && item.type == DiffType::Modified) {
                        stream << QString("+ [%1] %2\n").arg(item.rightLineNum).arg(item.rightText);
                    } else if (item.rightLineNum != -1 && item.type == DiffType::Added) {
                        stream << QString("+ [%1] %2\n").arg(item.rightLineNum).arg(item.rightText);
                    }
                }
            }

            QMessageBox::information(this, "成功", "比较报告已保存");
        } else {
            QMessageBox::warning(this, "错误", "无法保存报告文件");
        }
    }
}

void FileContentCompare::onCopyDiffClicked() {
    if (!m_lastResult.isValid()) {
        return;
    }

    QString diffText = QString("文件比较结果 - %1\n").arg(QDateTime::currentDateTime().toString());
    diffText += QString("文件A: %1\n").arg(m_lastResult.leftFile);
    diffText += QString("文件B: %1\n").arg(m_lastResult.rightFile);
    diffText += QString("相似度: %1%\n\n").arg(m_lastResult.similarity, 0, 'f', 1);

    for (const DiffItem& item : m_lastResult.differences) {
        if (item.type != DiffType::Unchanged) { // 只复制有差异的行
            QString prefix;
            switch (item.type) {
                case DiffType::Added:
                    prefix = "+ ";
                    break;
                case DiffType::Deleted:
                    prefix = "- ";
                    break;
                case DiffType::Modified:
                    prefix = "~ ";
                    break;
                default:
                    continue;
            }

            if (item.leftLineNum != -1) {
                diffText += QString("%1[%2] %3\n").arg(prefix).arg(item.leftLineNum).arg(item.leftText);
            }
            if (item.rightLineNum != -1 && item.type == DiffType::Added) {
                diffText += QString("+ [%1] %2\n").arg(item.rightLineNum).arg(item.rightText);
            }
        }
    }

    QApplication::clipboard()->setText(diffText);
    QMessageBox::information(this, "成功", "差异内容已复制到剪贴板");
}

void FileContentCompare::onFileCompareFinished(const FileCompareResult& result) {
    m_lastResult = result;
    m_compareBtn->setEnabled(true);
    m_progressBar->setVisible(false);

    if (result.isValid()) {
        displayCompareResult(result);
        m_statusLabel->setText("比较完成");
        m_saveReportBtn->setEnabled(true);
        m_copyDiffBtn->setEnabled(true);
    } else {
        m_statusLabel->setText(QString("比较失败: %1").arg(result.error));
        QMessageBox::warning(this, "比较失败", result.error);
    }
}

void FileContentCompare::onProgressUpdated(int value, int maximum, const QString& message) {
    m_progressBar->setMaximum(maximum);
    m_progressBar->setValue(value);
    m_statusLabel->setText(message);
}

void FileContentCompare::onCompareError(const QString& error) {
    m_compareBtn->setEnabled(true);
    m_progressBar->setVisible(false);
    m_statusLabel->setText(QString("错误: %1").arg(error));
    QMessageBox::warning(this, "比较错误", error);
}

void FileContentCompare::displayCompareResult(const FileCompareResult& result) {
    // 更新统计信息
    m_addedLabel->setText(QString::number(result.addedLines));
    m_deletedLabel->setText(QString::number(result.deletedLines));
    m_modifiedLabel->setText(QString::number(result.modifiedLines));
    m_similarityLabel->setText(QString("%1%").arg(result.similarity, 0, 'f', 1));

    // 设置颜色
    m_addedLabel->setStyleSheet("color: #27ae60; font-weight: bold;");
    m_deletedLabel->setStyleSheet("color: #e74c3c; font-weight: bold;");
    m_modifiedLabel->setStyleSheet("color: #f39c12; font-weight: bold;");

    if (result.similarity >= 80) {
        m_similarityLabel->setStyleSheet("color: #27ae60; font-weight: bold;");
    } else if (result.similarity >= 60) {
        m_similarityLabel->setStyleSheet("color: #f39c12; font-weight: bold;");
    } else {
        m_similarityLabel->setStyleSheet("color: #e74c3c; font-weight: bold;");
    }

    // 更新编辑器内容
    m_leftEditor->setDiffItems(result.differences, true);
    m_rightEditor->setDiffItems(result.differences, false);

    // 更新组标题
    QFileInfo leftInfo(result.leftFile);
    QFileInfo rightInfo(result.rightFile);
    m_leftGroup->setTitle(QString("文件A - %1").arg(leftInfo.fileName()));
    m_rightGroup->setTitle(QString("文件B - %1").arg(rightInfo.fileName()));
}

void FileContentCompare::updateUI() {
    bool hasFiles = !m_leftFileEdit->text().isEmpty() && !m_rightFileEdit->text().isEmpty();
    m_compareBtn->setEnabled(hasFiles);
}

// 其他组件的基本实现...

// TextContentCompare 简化实现
TextContentCompare::TextContentCompare(QWidget* parent)
    : QWidget(parent), m_workerThread(nullptr), m_worker(nullptr) {
    setupUI();
}

void TextContentCompare::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);

    // 创建分割器
    m_mainSplitter = new QSplitter(Qt::Horizontal);

    // 左侧文本
    m_leftWidget = new QWidget();
    m_leftLayout = new QVBoxLayout(m_leftWidget);
    m_leftGroup = new QGroupBox("文本A");
    m_leftGroup->setStyleSheet(
        "QGroupBox {"
        "font-weight: bold;"
        "border: 2px solid #bdc3c7;"
        "border-radius: 8px;"
        "margin-top: 10px;"
        "padding-top: 10px;"
        "}"
    );
    m_leftGroupLayout = new QVBoxLayout(m_leftGroup);

    m_leftButtonLayout = new QHBoxLayout();
    m_loadLeftBtn = new QPushButton("📁 加载文件");
    m_saveLeftBtn = new QPushButton("💾 保存");
    m_leftButtonLayout->addWidget(m_loadLeftBtn);
    m_leftButtonLayout->addWidget(m_saveLeftBtn);
    m_leftButtonLayout->addStretch();

    m_leftTextEdit = new QPlainTextEdit();
    m_leftTextEdit->setPlaceholderText("在此输入或粘贴文本A...");
    m_leftTextEdit->setFont(QFont("Consolas", 10));

    m_leftGroupLayout->addLayout(m_leftButtonLayout);
    m_leftGroupLayout->addWidget(m_leftTextEdit);
    m_leftLayout->addWidget(m_leftGroup);

    // 右侧文本
    m_rightWidget = new QWidget();
    m_rightLayout = new QVBoxLayout(m_rightWidget);
    m_rightGroup = new QGroupBox("文本B");
    m_rightGroup->setStyleSheet(m_leftGroup->styleSheet());
    m_rightGroupLayout = new QVBoxLayout(m_rightGroup);

    m_rightButtonLayout = new QHBoxLayout();
    m_loadRightBtn = new QPushButton("📁 加载文件");
    m_saveRightBtn = new QPushButton("💾 保存");
    m_rightButtonLayout->addWidget(m_loadRightBtn);
    m_rightButtonLayout->addWidget(m_saveRightBtn);
    m_rightButtonLayout->addStretch();

    m_rightTextEdit = new QPlainTextEdit();
    m_rightTextEdit->setPlaceholderText("在此输入或粘贴文本B...");
    m_rightTextEdit->setFont(QFont("Consolas", 10));

    m_rightGroupLayout->addLayout(m_rightButtonLayout);
    m_rightGroupLayout->addWidget(m_rightTextEdit);
    m_rightLayout->addWidget(m_rightGroup);

    // 控制区域
    m_controlWidget = new QWidget();
    m_controlWidget->setFixedWidth(250);
    m_controlLayout = new QVBoxLayout(m_controlWidget);

    m_controlGroup = new QGroupBox("比较控制");
    m_controlGroup->setStyleSheet(m_leftGroup->styleSheet());
    m_controlFormLayout = new QFormLayout(m_controlGroup);

    m_algorithmCombo = new QComboBox();
    m_algorithmCombo->addItem("Myers算法", 0);
    m_algorithmCombo->addItem("简单算法", 1);

    m_compareBtn = new QPushButton("🔍 开始比较");
    m_clearBtn = new QPushButton("🗑️ 清空");

    QString btnStyle =
        "QPushButton {"
        "background-color: #3498db;"
        "color: white;"
        "border: none;"
        "padding: 10px 20px;"
        "border-radius: 6px;"
        "font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "background-color: #2980b9;"
        "}";

    m_compareBtn->setStyleSheet(btnStyle);
    m_clearBtn->setStyleSheet(btnStyle.replace("#3498db", "#95a5a6").replace("#2980b9", "#7f8c8d"));

    m_controlFormLayout->addRow("算法:", m_algorithmCombo);
    m_controlFormLayout->addRow("", m_compareBtn);
    m_controlFormLayout->addRow("", m_clearBtn);

    // 结果显示
    m_resultGroup = new QGroupBox("比较结果");
    m_resultGroup->setStyleSheet(m_leftGroup->styleSheet());
    m_resultLayout = new QVBoxLayout(m_resultGroup);

    m_diffTable = new QTableWidget();
    m_diffTable->setColumnCount(4);
    m_diffTable->setHorizontalHeaderLabels({"类型", "行号", "文本A", "文本B"});
    m_diffTable->horizontalHeader()->setStretchLastSection(true);
    m_diffTable->setAlternatingRowColors(true);

    m_resultLayout->addWidget(m_diffTable);

    m_controlLayout->addWidget(m_controlGroup);
    m_controlLayout->addWidget(m_resultGroup);
    m_controlLayout->addStretch();

    // 组装布局
    m_mainSplitter->addWidget(m_leftWidget);
    m_mainSplitter->addWidget(m_rightWidget);
    m_mainSplitter->addWidget(m_controlWidget);
    m_mainSplitter->setSizes({400, 400, 250});

    m_mainLayout->addWidget(m_mainSplitter);

    // 连接信号
    connect(m_compareBtn, &QPushButton::clicked, this, &TextContentCompare::onCompareClicked);
    connect(m_clearBtn, &QPushButton::clicked, this, &TextContentCompare::onClearClicked);
    connect(m_loadLeftBtn, &QPushButton::clicked, this, &TextContentCompare::onLoadLeftFileClicked);
    connect(m_loadRightBtn, &QPushButton::clicked, this, &TextContentCompare::onLoadRightFileClicked);
    connect(m_saveLeftBtn, &QPushButton::clicked, this, &TextContentCompare::onSaveLeftFileClicked);
    connect(m_saveRightBtn, &QPushButton::clicked, this, &TextContentCompare::onSaveRightFileClicked);
}

void TextContentCompare::onCompareClicked() {
    QString leftText = m_leftTextEdit->toPlainText();
    QString rightText = m_rightTextEdit->toPlainText();

    if (leftText.isEmpty() && rightText.isEmpty()) {
        QMessageBox::warning(this, "警告", "请输入要比较的文本");
        return;
    }

    // 创建工作线程
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait();
        m_workerThread->deleteLater();
    }

    m_workerThread = new QThread(this);
    m_worker = new FileCompareWorker();
    m_worker->moveToThread(m_workerThread);

    connect(m_worker, &FileCompareWorker::fileCompareFinished, this, &TextContentCompare::onTextCompareFinished);

    // 选择算法
    IDiffAlgorithm* algorithm = nullptr;
    int algorithmIndex = m_algorithmCombo->currentData().toInt();
    if (algorithmIndex == 0) {
        algorithm = &m_myersAlgorithm;
    } else {
        algorithm = &m_simpleAlgorithm;
    }

    m_compareBtn->setEnabled(false);

    m_workerThread->start();
    m_worker->compareTexts(leftText, rightText, algorithm);
}

void TextContentCompare::onClearClicked() {
    m_leftTextEdit->clear();
    m_rightTextEdit->clear();
    m_diffTable->setRowCount(0);
}

void TextContentCompare::onLoadLeftFileClicked() {
    QString fileName = QFileDialog::getOpenFileName(this, "加载文本文件A", "", "文本文件 (*.txt);;所有文件 (*.*)");
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream.setEncoding(QStringConverter::Utf8);
            m_leftTextEdit->setPlainText(stream.readAll());
        }
    }
}

void TextContentCompare::onLoadRightFileClicked() {
    QString fileName = QFileDialog::getOpenFileName(this, "加载文本文件B", "", "文本文件 (*.txt);;所有文件 (*.*)");
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream.setEncoding(QStringConverter::Utf8);
            m_rightTextEdit->setPlainText(stream.readAll());
        }
    }
}

void TextContentCompare::onSaveLeftFileClicked() {
    QString fileName = QFileDialog::getSaveFileName(this, "保存文本A", "", "文本文件 (*.txt);;所有文件 (*.*)");
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream.setEncoding(QStringConverter::Utf8);
            stream << m_leftTextEdit->toPlainText();
        }
    }
}

void TextContentCompare::onSaveRightFileClicked() {
    QString fileName = QFileDialog::getSaveFileName(this, "保存文本B", "", "文本文件 (*.txt);;所有文件 (*.*)");
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream.setEncoding(QStringConverter::Utf8);
            stream << m_rightTextEdit->toPlainText();
        }
    }
}

void TextContentCompare::onTextCompareFinished(const FileCompareResult& result) {
    m_compareBtn->setEnabled(true);

    if (result.isValid()) {
        displayCompareResult(result);
    } else {
        QMessageBox::warning(this, "比较失败", result.error);
    }
}

void TextContentCompare::displayCompareResult(const FileCompareResult& result) {
    m_diffTable->setRowCount(0);

    int row = 0;
    for (const DiffItem& item : result.differences) {
        if (item.type == DiffType::Unchanged) continue; // 跳过未变更的行

        m_diffTable->insertRow(row);

        QString typeText;
        QColor bgColor;
        switch (item.type) {
            case DiffType::Added:
                typeText = "新增";
                bgColor = QColor(220, 255, 220);
                break;
            case DiffType::Deleted:
                typeText = "删除";
                bgColor = QColor(255, 220, 220);
                break;
            case DiffType::Modified:
                typeText = "修改";
                bgColor = QColor(255, 255, 220);
                break;
            default:
                continue;
        }

        auto* typeItem = new QTableWidgetItem(typeText);
        typeItem->setBackground(bgColor);

        QString lineNumText = "";
        if (item.leftLineNum != -1 && item.rightLineNum != -1) {
            lineNumText = QString("%1 → %2").arg(item.leftLineNum).arg(item.rightLineNum);
        } else if (item.leftLineNum != -1) {
            lineNumText = QString("%1").arg(item.leftLineNum);
        } else if (item.rightLineNum != -1) {
            lineNumText = QString("%1").arg(item.rightLineNum);
        }

        auto* lineItem = new QTableWidgetItem(lineNumText);
        lineItem->setBackground(bgColor);

        auto* leftItem = new QTableWidgetItem(item.leftText);
        leftItem->setBackground(bgColor);

        auto* rightItem = new QTableWidgetItem(item.rightText);
        rightItem->setBackground(bgColor);

        m_diffTable->setItem(row, 0, typeItem);
        m_diffTable->setItem(row, 1, lineItem);
        m_diffTable->setItem(row, 2, leftItem);
        m_diffTable->setItem(row, 3, rightItem);

        row++;
    }

    m_diffTable->resizeColumnsToContents();
}

// DirectoryCompare 简化实现
DirectoryCompare::DirectoryCompare(QWidget* parent)
    : QWidget(parent), m_workerThread(nullptr), m_worker(nullptr) {
    setupUI();
}

void DirectoryCompare::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);

    // 基本布局，暂时显示占位文本
    auto* label = new QLabel("目录比较功能正在开发中...");
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet("font-size: 16px; color: #7f8c8d; font-style: italic;");

    m_mainLayout->addWidget(label);
}

// FileCompareTool 主类实现
FileCompareTool::FileCompareTool(QWidget* parent) : QWidget(parent), DynamicObjectBase() {
    setupUI();
}

void FileCompareTool::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);

    // 创建标签页控件
    m_tabWidget = new QTabWidget();
    m_tabWidget->setStyleSheet(
        "QTabWidget::pane {"
        "border: 1px solid #bdc3c7;"
        "background-color: white;"
        "}"
        "QTabBar::tab {"
        "background-color: #ecf0f1;"
        "color: #2c3e50;"
        "padding: 12px 24px;"
        "margin-right: 2px;"
        "border-top-left-radius: 6px;"
        "border-top-right-radius: 6px;"
        "font-weight: bold;"
        "}"
        "QTabBar::tab:selected {"
        "background-color: white;"
        "border-bottom: 2px solid #3498db;"
        "}"
        "QTabBar::tab:hover {"
        "background-color: #d5dbdb;"
        "}"
    );

    // 创建各个标签页
    m_fileCompare = new FileContentCompare();
    m_tabWidget->addTab(m_fileCompare, "📄 文件对比");

    m_textCompare = new TextContentCompare();
    m_tabWidget->addTab(m_textCompare, "📝 文本对比");

    m_directoryCompare = new DirectoryCompare();
    m_tabWidget->addTab(m_directoryCompare, "📁 目录对比");

    m_mainLayout->addWidget(m_tabWidget);
}

// FileContentCompare 缺少的槽函数实现
void FileContentCompare::onAlgorithmChanged() {
    // TODO: 实现算法变化处理
}

void FileContentCompare::onIgnoreWhitespaceToggled(bool enabled) {
    Q_UNUSED(enabled);
    // TODO: 实现忽略空白字符切换处理
}

void FileContentCompare::onIgnoreCaseToggled(bool enabled) {
    Q_UNUSED(enabled);
    // TODO: 实现忽略大小写切换处理
}

// DirectoryCompare 缺少的槽函数实现
void DirectoryCompare::onSelectLeftDirClicked() {
    // TODO: 实现选择左侧目录
}

void DirectoryCompare::onSelectRightDirClicked() {
    // TODO: 实现选择右侧目录
}

void DirectoryCompare::onCompareClicked() {
    // TODO: 实现目录对比
}

void DirectoryCompare::onClearClicked() {
    // TODO: 实现清除
}

void DirectoryCompare::onExportReportClicked() {
    // TODO: 实现导出报告
}

void DirectoryCompare::onDirectoryCompareFinished(const QList<DirCompareItem>& results) {
    Q_UNUSED(results);
    // TODO: 实现目录对比完成处理
}

void DirectoryCompare::onProgressUpdated(int current, int total, const QString& message) {
    Q_UNUSED(current);
    Q_UNUSED(total);
    Q_UNUSED(message);
    // TODO: 实现进度更新处理
}

void DirectoryCompare::onItemDoubleClicked(QTreeWidgetItem* item, int column) {
    Q_UNUSED(item);
    Q_UNUSED(column);
    // TODO: 实现项目双击处理
}

