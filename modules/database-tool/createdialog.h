#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QTabBar>

#include "../../common/connx/Connection.h"

class CreateDatabaseDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CreateDatabaseDialog(Connx::Connection* connection, QWidget* parent = nullptr);

    QString databaseName() const;

private slots:
    void onAccept();
    void updateSqlPreview();

private:
    void setupUI();
    void loadCharsets();

    Connx::Connection* m_connection;

    QTabWidget* m_tabs;

    // 常规 tab
    QLineEdit* m_nameEdit;
    QComboBox* m_charsetCombo;
    QComboBox* m_collationCombo;

    // SQL 预览 tab
    QTextEdit* m_sqlPreview;

    QDialogButtonBox* m_buttonBox;
};
