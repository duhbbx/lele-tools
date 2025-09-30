//
// Created for lele-tools
// ComboBoxEditDelegate - 支持下拉选择和手动输入的表格单元格编辑器
//

#ifndef LELE_TOOLS_COMBOBOXEDITDELEGATE_H
#define LELE_TOOLS_COMBOBOXEDITDELEGATE_H

#include <qabstractitemview.h>
#include <QStyledItemDelegate>
#include <QComboBox>
#include <QLineEdit>
#include <QCompleter>
#include <QStringListModel>

class ComboBoxEditDelegate final : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit ComboBoxEditDelegate(const QStringList& options, QObject* parent = nullptr)
        : QStyledItemDelegate(parent), m_options(options) {}

    // 设置下拉选项
    void setOptions(const QStringList& options) {
        m_options = options;
    }

    // 获取下拉选项
    [[nodiscard]] QStringList options() const {
        return m_options;
    }

    // 创建编辑器
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                         const QModelIndex& index) const override {
        Q_UNUSED(option)
        Q_UNUSED(index)

        // 创建可编辑的 QComboBox
        auto* comboBox = new QComboBox(parent);
        comboBox->setEditable(true);
        comboBox->setInsertPolicy(QComboBox::NoInsert); // 不自动插入新项到列表
        comboBox->addItems(m_options);

        // 设置自动完成
        auto* completer = new QCompleter(m_options, comboBox);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setFilterMode(Qt::MatchContains); // 支持包含匹配
        comboBox->setCompleter(completer);

        // 设置高度与行高一致
        int rowHeight = option.rect.height();
        comboBox->setFixedHeight(rowHeight);

        // 设置 ComboBox 和 LineEdit 样式，消除间隙
        comboBox->setStyleSheet(R"(
            QComboBox {
                padding: 0px;
                margin: 0px;
                border: none;
                border-radius: 0px;
            }
            QComboBox::drop-down {
                border: none;
                width: 20px;
            }
            QComboBox QAbstractItemView {
                min-height: 20px;
            }
            QComboBox QAbstractItemView::item {
                height: 20px;
                padding: 4px 4px;
            }
        )");

        // 设置内部 LineEdit 样式
        if (QLineEdit* lineEdit = comboBox->lineEdit()) {
            lineEdit->setStyleSheet(R"(
                QLineEdit {
                    padding: 0px 0px;
                    margin: 0px;
                    border: none;
                    background: transparent;
                }
            )");
            lineEdit->setContentsMargins(0, 0, 0, 0);
            lineEdit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        }

        // —— 这里设置 QCompleter popup 样式 ——
        if (QCompleter* completer = comboBox->completer()) {
            if (QAbstractItemView* popup = completer->popup()) {
                popup->setStyleSheet(R"(
                QAbstractItemView {
                    border: 1px solid #c0c0c0;
                    background: #ffffff;
                    selection-background-color: #409eff;
                    selection-color: #ffffff;
                    outline: 0;
                }
                QAbstractItemView::item {
                    height: 20px;
                    padding: 4px 4px;
                }
            )");
            }
        }

        return comboBox;
    }

    // 将数据设置到编辑器
    void setEditorData(QWidget* editor, const QModelIndex& index) const override {
        const QString value = index.model()->data(index, Qt::EditRole).toString();
        if (auto* comboBox = qobject_cast<QComboBox*>(editor)) {
            // 设置当前文本（无论是否在列表中）
            comboBox->setCurrentText(value);

            // 如果值在选项列表中，选中它
            const int idx = comboBox->findText(value);
            if (idx >= 0) {
                comboBox->setCurrentIndex(idx);
            }
        }
    }

    // 将编辑器的数据提交到模型
    void setModelData(QWidget* editor, QAbstractItemModel* model,const QModelIndex& index) const override {
        if (const auto* comboBox = qobject_cast<QComboBox*>(editor)) {
            // 获取当前文本（可能是手动输入的，也可能是选中的）
            const QString value = comboBox->currentText().trimmed();
            model->setData(index, value, Qt::EditRole);
        }
    }

    // 更新编辑器几何形状
    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        Q_UNUSED(index)
        editor->setGeometry(option.rect);
    }

private:
    QStringList m_options;
};

#endif // LELE_TOOLS_COMBOBOXEDITDELEGATE_H