#include "querytab.h"
#include <QMessageBox>
#include <QHeaderView>
#include <QDateTime>
#include <QTimer>
#include <QFont>
#include <QHBoxLayout>

// SqlSyntaxHighlighter 实现
SqlSyntaxHighlighter::SqlSyntaxHighlighter(QTextDocument* parent) : QSyntaxHighlighter(parent) {
    HighlightingRule rule;

    // SQL关键字格式
    keywordFormat.setForeground(QColor(86, 156, 214)); // VS Code蓝色
    keywordFormat.setFontWeight(QFont::Bold);

    // SQL关键字列表
    QStringList keywordPatterns;
    keywordPatterns << "\\bSELECT\\b" << "\\bFROM\\b" << "\\bWHERE\\b" << "\\bINSERT\\b"
        << "\\bUPDATE\\b" << "\\bDELETE\\b" << "\\bCREATE\\b" << "\\bDROP\\b"
        << "\\bALTER\\b" << "\\bINDEX\\b" << "\\bTABLE\\b" << "\\bDATABASE\\b"
        << "\\bVIEW\\b" << "\\bPROCEDURE\\b" << "\\bFUNCTION\\b" << "\\bTRIGGER\\b"
        << "\\bJOIN\\b" << "\\bINNER\\b" << "\\bLEFT\\b" << "\\bRIGHT\\b"
        << "\\bOUTER\\b" << "\\bUNION\\b" << "\\bGROUP\\b" << "\\bORDER\\b"
        << "\\bBY\\b" << "\\bHAVING\\b" << "\\bLIMIT\\b" << "\\bOFFSET\\b"
        << "\\bDISTINCT\\b" << "\\bCOUNT\\b" << "\\bSUM\\b" << "\\bAVG\\b"
        << "\\bMIN\\b" << "\\bMAX\\b" << "\\bAS\\b" << "\\bON\\b"
        << "\\bAND\\b" << "\\bOR\\b" << "\\bNOT\\b" << "\\bIN\\b"
        << "\\bEXISTS\\b" << "\\bBETWEEN\\b" << "\\bLIKE\\b" << "\\bIS\\b"
        << "\\bNULL\\b" << "\\bTRUE\\b" << "\\bFALSE\\b" << "\\bCASE\\b"
        << "\\bWHEN\\b" << "\\bTHEN\\b" << "\\bELSE\\b" << "\\bEND\\b"
        << "\\bIF\\b" << "\\bELSEIF\\b" << "\\bBEGIN\\b" << "\\bCOMMIT\\b"
        << "\\bROLLBACK\\b" << "\\bTRANSACTION\\b" << "\\bSTART\\b";

    foreach(const QString &pattern, keywordPatterns) {
        rule.pattern = QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption);
        rule.format = keywordFormat;
        highlightingRules.append(rule);
    }

    // 函数格式
    functionFormat.setForeground(QColor(220, 220, 170)); // VS Code淡黄色
    functionFormat.setFontWeight(QFont::Bold);
    rule.pattern = QRegularExpression("\\b[A-Za-z_][A-Za-z0-9_]*(?=\\s*\\()");
    rule.format = functionFormat;
    highlightingRules.append(rule);

    // 字符串格式
    stringFormat.setForeground(QColor(206, 145, 120)); // VS Code橙色
    rule.pattern = QRegularExpression("'([^'\\\\]|\\\\.)*'");
    rule.format = stringFormat;
    highlightingRules.append(rule);

    rule.pattern = QRegularExpression("\"([^\"\\\\]|\\\\.)*\"");
    rule.format = stringFormat;
    highlightingRules.append(rule);

    // 数字格式
    numberFormat.setForeground(QColor(181, 206, 168)); // VS Code绿色
    rule.pattern = QRegularExpression("\\b\\d+(\\.\\d+)?\\b");
    rule.format = numberFormat;
    highlightingRules.append(rule);

    // 操作符格式
    operatorFormat.setForeground(QColor(212, 212, 212)); // 浅灰色
    QStringList operatorPatterns;
    operatorPatterns << "=" << "<>" << "!=" << "<" << ">" << "<=" << ">="
        << "\\+" << "-" << "\\*" << "/" << "%" << "\\|\\|";
    foreach(const QString &pattern, operatorPatterns) {
        rule.pattern = QRegularExpression(pattern);
        rule.format = operatorFormat;
        highlightingRules.append(rule);
    }

    // 注释格式
    commentFormat.setForeground(QColor(106, 153, 85)); // VS Code绿色注释
    commentFormat.setFontItalic(true);
    rule.pattern = QRegularExpression("--[^\n]*");
    rule.format = commentFormat;
    highlightingRules.append(rule);

    rule.pattern = QRegularExpression("/\\*.*\\*/");
    rule.format = commentFormat;
    highlightingRules.append(rule);
}

void SqlSyntaxHighlighter::highlightBlock(const QString& text) {
    foreach(const HighlightingRule &rule, highlightingRules) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
}

// QueryTab 实现
QueryTab::QueryTab(Connx::Connection* connection, QWidget* parent)
    : QWidget(parent), m_connection(connection) {
    setupUI();
}

void QueryTab::setupUI() {
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(5, 5, 5, 5);
    m_layout->setSpacing(5);

    // 工具栏
    m_toolbar = new QToolBar();
    m_toolbar->setIconSize(QSize(16, 16));

    m_executeAction = new QAction("▶️ 执行", this);
    m_executeAction->setShortcut(QKeySequence("F5"));
    m_executeAction->setToolTip("执行查询 (F5)");
    QObject::connect(m_executeAction, &QAction::triggered, this, &QueryTab::onExecuteClicked);
    m_toolbar->addAction(m_executeAction);

    m_clearAction = new QAction("🗑️ 清空", this);
    m_clearAction->setToolTip("清空查询");
    QObject::connect(m_clearAction, &QAction::triggered, this, &QueryTab::onClearClicked);
    m_toolbar->addAction(m_clearAction);

    m_formatAction = new QAction("🎨 格式化", this);
    m_formatAction->setToolTip("格式化查询");
    QObject::connect(m_formatAction, &QAction::triggered, this, &QueryTab::onFormatClicked);
    m_toolbar->addAction(m_formatAction);

    // 弹性空间把选择器推到右侧
    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolbar->addWidget(spacer);

    // 数据库选择器
    auto* dbLabel = new QLabel(tr("库:"));
    dbLabel->setStyleSheet("font-size:9pt; color:#495057; margin-left:4px;");
    m_toolbar->addWidget(dbLabel);

    m_databaseCombo = new QComboBox();
    m_databaseCombo->setMinimumWidth(120);
    m_databaseCombo->setToolTip(tr("切换数据库"));
    m_toolbar->addWidget(m_databaseCombo);

    // Schema 选择器（PG/Oracle 用，MySQL 隐藏）
    m_schemaCombo = new QComboBox();
    m_schemaCombo->setMinimumWidth(100);
    m_schemaCombo->setToolTip(tr("切换 Schema"));
    m_schemaCombo->setVisible(false); // 默认隐藏，PG/Oracle 时显示

    auto* schemaLabel = new QLabel(tr("模式:"));
    schemaLabel->setStyleSheet("font-size:9pt; color:#495057; margin-left:4px;");
    schemaLabel->setVisible(false);
    m_toolbar->addWidget(schemaLabel);
    m_toolbar->addWidget(m_schemaCombo);

    connect(m_databaseCombo, &QComboBox::currentTextChanged, this, &QueryTab::onDatabaseChanged);
    connect(m_schemaCombo, &QComboBox::currentTextChanged, this, &QueryTab::onSchemaChanged);

    m_layout->addWidget(m_toolbar);

    // 加载数据库列表
    loadDatabaseList();

    // 查询编辑器
    m_queryEdit = new QTextEdit();
    m_queryEdit->setFont(QFont("Consolas", 11));
    m_queryEdit->setPlaceholderText(tr("在此输入SQL查询语句..."));
    m_queryEdit->setMaximumHeight(150);

    // 配置为纯文本行为 - 禁用富文本，粘贴时只保留文本
    m_queryEdit->setAcceptRichText(false);

    // 应用SQL语法高亮器
    m_syntaxHighlighter = new SqlSyntaxHighlighter(m_queryEdit->document());

    m_layout->addWidget(m_queryEdit);

    // 结果区域
    m_resultTabs = new QTabWidget();

    // 结果表格
    m_resultTable = new QTableWidget();
    m_resultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultTable->setAlternatingRowColors(true);
    m_resultTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    // 设置表头调整策略
    m_resultTable->horizontalHeader()->setStretchLastSection(false);
    m_resultTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_resultTable->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_resultTable->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    m_resultTabs->addTab(m_resultTable, tr("📊 结果"));

    // 消息文本
    m_messagesText = new QTextEdit();
    m_messagesText->setReadOnly(true);
    m_messagesText->setFont(QFont("Consolas", 10));
    m_resultTabs->addTab(m_messagesText, tr("📝 消息"));

    m_layout->addWidget(m_resultTabs, 1); // 拉伸因子为1

    // 状态栏
    QHBoxLayout* statusLayout = new QHBoxLayout();
    m_statusLabel = new QLabel(tr("就绪"));
    m_timeLabel = new QLabel(tr("执行时间: -"));
    m_rowsLabel = new QLabel(tr("行数: 0"));

    statusLayout->addWidget(m_statusLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(m_timeLabel);
    statusLayout->addWidget(m_rowsLabel);

    m_layout->addLayout(statusLayout);
}

void QueryTab::setQuery(const QString& query) {
    m_queryEdit->setPlainText(query);
}

QString QueryTab::getQuery() const {
    return m_queryEdit->toPlainText();
}

void QueryTab::executeQuery() {
    onExecuteClicked();
}

void QueryTab::selectDatabase(const QString& database) {
    if (database.isEmpty() || !m_connection) return;
    m_currentDatabase = database;
    m_connection->execute(QString("USE %1").arg(database));

    // 同步下拉框选择（不触发信号）
    m_databaseCombo->blockSignals(true);
    int idx = m_databaseCombo->findText(database);
    if (idx >= 0) m_databaseCombo->setCurrentIndex(idx);
    m_databaseCombo->blockSignals(false);
}

void QueryTab::loadDatabaseList() {
    if (!m_connection) return;

    m_databaseCombo->blockSignals(true);
    m_databaseCombo->clear();

    QStringList databases = m_connection->getDatabases();
    if (!databases.isEmpty()) {
        m_databaseCombo->addItems(databases);
        // 如果已有当前数据库，选中它
        if (!m_currentDatabase.isEmpty()) {
            int idx = m_databaseCombo->findText(m_currentDatabase);
            if (idx >= 0) m_databaseCombo->setCurrentIndex(idx);
        }
    }

    m_databaseCombo->blockSignals(false);
}

void QueryTab::onDatabaseChanged(const QString& database) {
    if (database.isEmpty() || !m_connection) return;
    m_currentDatabase = database;
    m_connection->execute(QString("USE %1").arg(database));
    m_statusLabel->setText(tr("已切换到: %1").arg(database));
}

void QueryTab::onSchemaChanged(const QString& schema) {
    if (schema.isEmpty() || !m_connection) return;
    // PostgreSQL: SET search_path TO schema
    // Oracle: ALTER SESSION SET CURRENT_SCHEMA = schema
    m_connection->execute(QString("SET search_path TO %1").arg(schema));
    m_statusLabel->setText(tr("已切换到模式: %1").arg(schema));
}

void QueryTab::onExecuteClicked() {
    if (!m_connection || !m_connection->isConnected()) {
        QMessageBox::warning(this, tr("执行错误"), tr("数据库连接无效"));
        return;
    }

    QString query;
    if (m_queryEdit->textCursor().hasSelection()) {
        query = m_queryEdit->textCursor().selectedText();
    } else {
        query = m_queryEdit->toPlainText();
    }

    query = query.trimmed();
    if (query.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请输入要执行的命令"));
        return;
    }

    m_statusLabel->setText(tr("正在执行..."));

    // 不再做手工解析，直接交给连接执行
    Connx::QueryResult result = m_connection->execute(query);
    updateResults(result);

    emit queryExecuted(result);
}


void QueryTab::onClearClicked() const {
    m_queryEdit->clear();
    m_resultTable->setRowCount(0);
    m_resultTable->setColumnCount(0);
    m_messagesText->clear();
    m_statusLabel->setText(tr("就绪"));
    m_timeLabel->setText(tr("执行时间: -"));
    m_rowsLabel->setText(tr("行数: 0"));
}

void QueryTab::onFormatClicked() const {
    // Redis命令格式化（简单实现）
    QString query = m_queryEdit->toPlainText().trimmed();
    if (query.isEmpty())
        return;

    // 转换为大写并格式化
    QStringList parts = query.split(' ', Qt::SkipEmptyParts);
    if (!parts.isEmpty()) {
        parts[0] = parts[0].toUpper();
        m_queryEdit->setPlainText(parts.join(' '));
    }
}

void QueryTab::updateResults(const Connx::QueryResult& result) const {
    m_timeLabel->setText(tr("执行时间: %1ms").arg(result.executionTime));

    if (result.success) {
        m_statusLabel->setText(tr("✅ 执行成功"));

        // 调试输出
        qDebug() << "QueryResult data size:" << result.data.size();
        for (int i = 0; i < qMin(3, result.data.size()); ++i) {
            qDebug() << "Row" << i << ":" << result.data[i];
            if (result.data[i].typeId() == QMetaType::QVariantList) {
                QVariantList row = result.data[i].toList();
                qDebug() << "  Row as list size:" << row.size();
                for (int j = 0; j < qMin(5, row.size()); ++j) {
                    qDebug() << "    Col" << j << ":" << row[j];
                }
            }
        }

        // 更新结果表格 - 性能优化版本
        if (!result.columns.isEmpty() && !result.rows.isEmpty()) {
            // 新的结构化数据
            int columnCount = result.columns.size();
            int rowCount = result.rows.size();

            // 禁用排序和更新以提高性能
            m_resultTable->setSortingEnabled(false);
            m_resultTable->setUpdatesEnabled(false);

            // 清除旧数据
            m_resultTable->clear();

            // 设置行列数
            m_resultTable->setRowCount(rowCount);
            m_resultTable->setColumnCount(columnCount);

            // 设置列头
            m_resultTable->setHorizontalHeaderLabels(result.columns);

            // 批量填充数据行 - 性能优化
            QVector<QTableWidgetItem*> items;
            items.reserve(rowCount * columnCount);

            for (int i = 0; i < rowCount; ++i) {
                const QVariantList& row = result.rows[i];
                for (int j = 0; j < qMin(row.size(), columnCount); ++j) {
                    QTableWidgetItem* item = new QTableWidgetItem(row[j].toString());
                    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                    items.append(item);
                    m_resultTable->setItem(i, j, item);
                }
            }

            // 重新启用更新
            m_resultTable->setUpdatesEnabled(true);

            // 延迟调整列宽以避免阻塞UI
            QTimer::singleShot(0, [this, columnCount]() {
                // 智能列宽调整 - 只对前几列进行自动调整
                for (int j = 0; j < qMin(columnCount, 10); ++j) {
                    m_resultTable->resizeColumnToContents(j);
                    int width = m_resultTable->columnWidth(j);
                    if (width > 300) {
                        m_resultTable->setColumnWidth(j, 300);
                    } else if (width < 80) {
                        m_resultTable->setColumnWidth(j, 80);
                    }
                }

                // 其余列设置默认宽度
                for (int j = 10; j < columnCount; ++j) {
                    m_resultTable->setColumnWidth(j, 120);
                }
            });

            // 确保表头可见
            m_resultTable->horizontalHeader()->setVisible(true);
            m_resultTable->verticalHeader()->setVisible(true);

            // 启用排序
            m_resultTable->setSortingEnabled(true);

            m_rowsLabel->setText(tr("行数: %1").arg(rowCount));
        } else if (!result.data.isEmpty() && result.data.size() > 0) {
            // 兼容旧格式 - 处理Redis等其他类型的结果
            QVariant firstData = result.data[0];

            if (firstData.typeId() == QMetaType::QVariantList) {
                // 数组结果（单列数据）
                QVariantList list = firstData.toList();
                m_resultTable->setRowCount(list.size());
                m_resultTable->setColumnCount(1);
                m_resultTable->setHorizontalHeaderLabels({ "值" });

                for (int i = 0; i < list.size(); ++i) {
                    QTableWidgetItem* item = new QTableWidgetItem(list[i].toString());
                    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                    m_resultTable->setItem(i, 0, item);
                }

                m_rowsLabel->setText(tr("行数: %1").arg(list.size()));
            } else {
                // 单一结果（非数组）
                m_resultTable->setRowCount(1);
                m_resultTable->setColumnCount(1);
                m_resultTable->setHorizontalHeaderLabels({ "结果" });

                QTableWidgetItem* item = new QTableWidgetItem(firstData.toString());
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                m_resultTable->setItem(0, 0, item);

                m_rowsLabel->setText(tr("行数: 1"));
            }
        } else {
            m_resultTable->setRowCount(0);
            m_resultTable->setColumnCount(0);
            m_rowsLabel->setText(tr("行数: 0"));
        }

        m_messagesText->append(tr("[%1] 命令执行成功").arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
    } else {
        m_statusLabel->setText(tr("❌ 执行失败"));
        m_messagesText->append(tr("[%1] 错误: %2").arg(QDateTime::currentDateTime().toString("hh:mm:ss")).arg(result.errorMessage));
        m_resultTable->setRowCount(0);
        m_resultTable->setColumnCount(0);
        m_rowsLabel->setText(tr("行数: 0"));
    }
}
