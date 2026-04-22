#include "createdialog.h"
#include <QMessageBox>
#include <QFormLayout>

CreateDatabaseDialog::CreateDatabaseDialog(Connx::Connection* connection, QWidget* parent)
    : QDialog(parent)
    , m_connection(connection)
{
    setWindowTitle(tr("新建数据库"));
    setMinimumWidth(460);
    setupUI();
    loadCharsets();
}

QString CreateDatabaseDialog::databaseName() const
{
    return m_nameEdit->text().trimmed();
}

void CreateDatabaseDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 10, 12, 10);
    mainLayout->setSpacing(8);

    setStyleSheet(R"(
        QLabel { font-size: 9pt; color: #495057; }
        QLineEdit, QComboBox {
            font-size: 9pt; padding: 4px 8px;
            border: 1px solid #dee2e6; border-radius: 4px;
            min-height: 20px; background: #fff; color: #495057;
        }
        QLineEdit:focus, QComboBox:focus { border-color: #80bdff; }
        QComboBox::drop-down { border: none; width: 20px; }
        QComboBox::down-arrow {
            image: none; border-left: 4px solid transparent;
            border-right: 4px solid transparent;
            border-top: 5px solid #868e96; margin-right: 6px;
        }
        QTextEdit {
            font-family: 'Menlo', 'Consolas', monospace;
            font-size: 9pt; border: 1px solid #dee2e6;
            border-radius: 4px; background: #f8f9fa; color: #495057;
        }
        QTabBar::tab {
            padding: 5px 14px; font-size: 9pt;
            border: 1px solid #dee2e6; border-bottom: none;
            background: #f8f9fa; margin-right: -1px; min-width: 50px;
        }
        QTabBar::tab:selected { background: #fff; }
        QTabWidget::pane { border: 1px solid #dee2e6; border-top: none; }
        QPushButton {
            font-size: 9pt; padding: 5px 16px;
            border: 1px solid #dee2e6; border-radius: 4px;
            background: #fff; color: #495057;
        }
        QPushButton:hover { background: #e9ecef; }
        QPushButton:default, QPushButton[text="OK"] {
            background: #228be6; color: #fff; border-color: #228be6;
        }
        QPushButton:default:hover { background: #1c7ed6; }
    )");

    m_tabs = new QTabWidget();
    m_tabs->tabBar()->setExpanding(false);
    m_tabs->tabBar()->setElideMode(Qt::ElideNone);
    m_tabs->setDocumentMode(true);

    // ── 常规 tab ──
    auto* generalWidget = new QWidget();
    auto* formLayout = new QFormLayout(generalWidget);
    formLayout->setContentsMargins(16, 12, 16, 12);
    formLayout->setSpacing(10);
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_nameEdit = new QLineEdit();
    m_nameEdit->setPlaceholderText(tr("输入数据库名称"));
    formLayout->addRow(tr("数据库名:"), m_nameEdit);

    m_charsetCombo = new QComboBox();
    formLayout->addRow(tr("字符集:"), m_charsetCombo);

    m_collationCombo = new QComboBox();
    formLayout->addRow(tr("排序规则:"), m_collationCombo);

    m_tabs->addTab(generalWidget, tr("常规"));

    // ── SQL 预览 tab ──
    m_sqlPreview = new QTextEdit();
    m_sqlPreview->setReadOnly(true);
    m_tabs->addTab(m_sqlPreview, tr("SQL 预览"));

    mainLayout->addWidget(m_tabs, 1);

    // 按钮
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(m_buttonBox);

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &CreateDatabaseDialog::onAccept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // 实时更新 SQL 预览
    connect(m_nameEdit, &QLineEdit::textChanged, this, &CreateDatabaseDialog::updateSqlPreview);
    connect(m_charsetCombo, &QComboBox::currentTextChanged, this, &CreateDatabaseDialog::updateSqlPreview);
    connect(m_collationCombo, &QComboBox::currentTextChanged, this, &CreateDatabaseDialog::updateSqlPreview);
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int idx) {
        if (idx == 1) updateSqlPreview();
    });

    // 字符集变化时刷新排序规则
    connect(m_charsetCombo, &QComboBox::currentTextChanged, this, [this](const QString& charset) {
        m_collationCombo->clear();
        m_collationCombo->addItem(""); // 默认

        if (charset.isEmpty() || !m_connection) return;

        QString sql = QString("SHOW COLLATION WHERE Charset = '%1'").arg(charset);
        Connx::QueryResult result = m_connection->execute(sql);
        if (result.success) {
            for (const QVariantList& row : result.rows) {
                if (!row.isEmpty())
                    m_collationCombo->addItem(row[0].toString());
            }
        }
    });

    adjustSize();
    setFixedSize(size().expandedTo(QSize(460, 300)));
}

void CreateDatabaseDialog::loadCharsets()
{
    m_charsetCombo->clear();
    m_charsetCombo->addItem(""); // 默认

    if (!m_connection) {
        m_charsetCombo->addItems({"utf8mb4", "utf8", "latin1", "ascii", "binary", "gbk"});
        return;
    }

    Connx::QueryResult result = m_connection->execute("SHOW CHARACTER SET");
    if (result.success) {
        QStringList charsets;
        for (const QVariantList& row : result.rows) {
            if (!row.isEmpty())
                charsets.append(row[0].toString());
        }
        charsets.sort();
        m_charsetCombo->addItems(charsets);

        // 默认选 utf8mb4
        int idx = m_charsetCombo->findText("utf8mb4");
        if (idx >= 0) m_charsetCombo->setCurrentIndex(idx);
    }
}

void CreateDatabaseDialog::updateSqlPreview()
{
    QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty()) {
        m_sqlPreview->setPlainText("-- 请输入数据库名称");
        return;
    }

    QString sql = QString("CREATE DATABASE `%1`").arg(name);

    QString charset = m_charsetCombo->currentText();
    if (!charset.isEmpty())
        sql += QString("\n  CHARACTER SET %1").arg(charset);

    QString collation = m_collationCombo->currentText();
    if (!collation.isEmpty())
        sql += QString("\n  COLLATE %1").arg(collation);

    sql += ";";
    m_sqlPreview->setPlainText(sql);
}

void CreateDatabaseDialog::onAccept()
{
    QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("错误"), tr("请输入数据库名称"));
        m_nameEdit->setFocus();
        return;
    }

    if (!m_connection) {
        QMessageBox::warning(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    updateSqlPreview();
    QString sql = m_sqlPreview->toPlainText();

    Connx::QueryResult result = m_connection->execute(sql);
    if (result.success) {
        accept();
    } else {
        QMessageBox::critical(this, tr("创建失败"), result.errorMessage);
    }
}
