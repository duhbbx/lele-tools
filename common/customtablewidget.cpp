#include "customtablewidget.h"
#include <QHeaderView>

CustomTableWidget::CustomTableWidget(QWidget *parent)
    : QTableWidget(parent)
{
    initStyle();
}

CustomTableWidget::CustomTableWidget(int rows, int columns, QWidget *parent)
    : QTableWidget(rows, columns, parent)
{
    initStyle();
}

void CustomTableWidget::initStyle()
{
    // 设置边框和网格线
    setShowGrid(true);
    setFrameShape(QFrame::Box);

    // 设置选择行为
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);

    // 启用交替行颜色
    setAlternatingRowColors(true);

    // 设置水平表头属性
    horizontalHeader()->setStretchLastSection(true);
    horizontalHeader()->setHighlightSections(false);
    horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // 设置垂直表头属性
    verticalHeader()->setVisible(true);
    verticalHeader()->setHighlightSections(false);
    verticalHeader()->setDefaultSectionSize(30);

    // 应用自定义样式，确保边框不重叠
    setStyleSheet(R"(
        /* 表格整体样式 */
        QTableWidget {
            border: 1px solid #dee2e6;
            gridline-color: #dee2e6;
            background-color: #ffffff;
            alternate-background-color: #f8f9fa;
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            font-size: 10pt;
            color: #333333;
        }

        /* 表格项样式 - 关键：去掉左边和上边的边框，避免重叠 */
        QTableWidget::item {
            padding: 4px;
            border: none;
            outline: none;
        }

        /* 选中项样式 */
        QTableWidget::item:selected {
            background-color: #007bff;
            color: white;
        }

        /* 悬停项样式 */
        QTableWidget::item:hover {
            background-color: #e9ecef;
        }

        /* 选中并悬停 */
        QTableWidget::item:selected:hover {
            background-color: #0056b3;
        }

        /* 水平表头样式 - 关键：只保留右边和下边的边框 */
        QHeaderView::section {
            background-color: #f8f9fa;
            padding: 6px 4px;
            border: none;
            border-right: 1px solid #dee2e6;
            border-bottom: 1px solid #dee2e6;
            font-weight: bold;
            color: #495057;
        }

        /* 第一个表头单元格需要左边框 */
        QHeaderView::section:first {
            border-left: none;
        }

        /* 最后一个表头单元格 */
        QHeaderView::section:last {
            border-right: none;
        }

        /* 表头悬停效果 */
        QHeaderView::section:hover {
            background-color: #e9ecef;
        }

        /* 角落按钮（左上角） */
        QTableCornerButton::section {
            background-color: #f8f9fa;
            border: none;
            border-right: 1px solid #dee2e6;
            border-bottom: 1px solid #dee2e6;
        }

        /* 滚动条样式 */
        QScrollBar:vertical {
            background: #f5f5f5;
            width: 8px;
            margin: 0px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical {
            background: #9ca3af;
            min-height: 20px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical:hover {
            background: #6b7280;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar:horizontal {
            background: #f5f5f5;
            height: 8px;
            margin: 0px;
            border-radius: 4px;
        }
        QScrollBar::handle:horizontal {
            background: #9ca3af;
            min-width: 20px;
            border-radius: 4px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #6b7280;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
        }
    )");
}