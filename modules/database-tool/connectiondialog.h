#pragma once

#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QListWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QStyle>

#include "../../common/connx/Connection.h"

// 现代化的连接类型列表项委托
class ConnectionTypeDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit ConnectionTypeDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        painter->setRenderHint(QPainter::Antialiasing);

        QRect rect = option.rect;
        QString text = index.data(Qt::DisplayRole).toString();
        QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();

        // 背景绘制
        QColor backgroundColor;
        if (option.state & QStyle::State_Selected) {
            backgroundColor = QColor(0xe7, 0xf5, 0xff);  // 浅蓝
        } else if (option.state & QStyle::State_MouseOver) {
            backgroundColor = QColor(0xe9, 0xec, 0xef);
        } else {
            backgroundColor = Qt::transparent;
        }

        painter->setBrush(backgroundColor);
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(rect.adjusted(4, 2, -4, -2), 4, 4);

        // 文字绘制
        QColor textColor = (option.state & QStyle::State_Selected) ?
                          QColor(0x22, 0x8b, 0xe6) : QColor(0x49, 0x50, 0x57);
        painter->setPen(textColor);

        QFont font = painter->font();
        font.setPointSize(9);
        font.setWeight((option.state & QStyle::State_Selected) ? QFont::Bold : QFont::Normal);
        painter->setFont(font);

        QRect textRect = rect.adjusted(36, 0, -6, 0);
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text);

        // 图标绘制
        if (!icon.isNull()) {
            QRect iconRect(rect.left() + 12, rect.center().y() - 7, 14, 14);
            icon.paint(painter, iconRect);
        }
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        Q_UNUSED(option)
        Q_UNUSED(index)
        return QSize(140, 32);
    }
};

// 连接配置对话框
class ConnectionDialog : public QDialog {
    Q_OBJECT

public:
    explicit ConnectionDialog(QWidget* parent = nullptr);
    explicit ConnectionDialog(const Connx::ConnectionConfig& config, QWidget* parent = nullptr);

    Connx::ConnectionConfig getConnectionConfig() const;
    void setConnectionConfig(const Connx::ConnectionConfig& config) const;

private slots:
    void onConnectionTypeChanged();
    void onTestConnection();
    void onAccept();

private:
    void setupUI();
    void setupLeftPanel();
    void setupRightPanel();
    void createFormControls();
    void updateFormForType(const QString& type) const;
    bool validateInput();

    // 主布局
    QHBoxLayout* m_mainLayout;
    QSplitter* m_splitter;

    // 左侧面板
    QWidget* m_leftPanel;
    QVBoxLayout* m_leftLayout;
    QLabel* m_typeLabel;
    QListWidget* m_typeList;

    // 右侧面板
    QWidget* m_rightPanel;
    QVBoxLayout* m_rightLayout;
    QGridLayout* m_gridLayout;
    QWidget* m_formWidget;

    // 表单控件
    QLineEdit* m_nameEdit;
    QLineEdit* m_hostEdit;
    QSpinBox* m_portSpin;
    QLineEdit* m_usernameEdit;
    QLineEdit* m_passwordEdit;
    QLineEdit* m_databaseEdit;
    QSpinBox* m_timeoutSpin;
    QCheckBox* m_sslCheck;

    // Redis特有控件
    QLabel* m_dbIndexLabel;
    QSpinBox* m_dbIndexSpin;

    // MySQL特有控件
    QLabel* m_charsetLabel;
    QComboBox* m_charsetCombo;

    // 底部按钮
    QWidget* m_buttonWidget;
    QHBoxLayout* m_buttonLayout;
    QPushButton* m_testBtn;
    QDialogButtonBox* m_buttonBox;
    QLabel* m_statusLabel;

    QString m_currentType;
};
