//
// Created by duhbb on 2025/9/30.
//

#ifndef LELE_TOOLS_MYITEMEDITDELEGATE_H
#define LELE_TOOLS_MYITEMEDITDELEGATE_H
#include <QStyledItemDelegate>

class MyDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override {
        QWidget *editor = QStyledItemDelegate::createEditor(parent, option, index);

        if (auto lineEdit = qobject_cast<QLineEdit*>(editor)) {
            lineEdit->setStyleSheet(
                "border: none;"
                "padding: 0;"
                "margin: 0;"
                "background: #eafaea;"
            );

            // 关键：让 editor 高度和行高一致
            int rowHeight = option.rect.height();
            lineEdit->setFixedHeight(rowHeight);
            lineEdit->setAlignment(Qt::AlignVCenter | Qt::AlignLeft); // 垂直居中
        }
        return editor;
    }
};

#endif //LELE_TOOLS_MYITEMEDITDELEGATE_H