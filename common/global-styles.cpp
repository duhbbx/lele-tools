#include "global-styles.h"

QString GlobalStyles::getCheckBoxStyle() {
    return R"(
        QCheckBox {
            spacing: 10px;
            font-size: 10pt;
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            color: #333333;
            padding: 2px;
        }
        QCheckBox::indicator {
            width: 14px;
            height: 14px;
            border: 2px solid #bdc3c7;
            border-radius: 3px;
            background-color: #ffffff;
            margin: 1px;
        }
        QCheckBox::indicator:hover {
            border-color: #3498db;
            background-color: #ecf0f1;
        }
        QCheckBox::indicator:checked {
            background-color: #3498db;
            border-color: #2980b9;
            image: url(data:image/svg+xml;base64,PHN2ZyB3aWR0aD0iMTYiIGhlaWdodD0iMTYiIHZpZXdCb3g9IjAgMCAxNiAxNiIgZmlsbD0ibm9uZSIgeG1sbnM9Imh0dHA6Ly93d3cudzMub3JnLzIwMDAvc3ZnIj4KPHBhdGggZD0iTTEzLjMzMzMgNEw2IDExLjMzMzNMMi42NjY2NyA4IiBzdHJva2U9IndoaXRlIiBzdHJva2Utd2lkdGg9IjIuNSIgc3Ryb2tlLWxpbmVjYXA9InJvdW5kIiBzdHJva2UtbGluZWpvaW49InJvdW5kIi8+Cjwvc3ZnPgo=);
        }
        QCheckBox::indicator:checked:hover {
            background-color: #2980b9;
            border-color: #1f618d;
        }
        QCheckBox::indicator:checked:pressed {
            background-color: #1f618d;
            border-color: #154360;
        }
        QCheckBox::indicator:unchecked:pressed {
            background-color: #d5dbdb;
            border-color: #85929e;
        }
        QCheckBox::indicator:disabled {
            background-color: #f8f9fa;
            border-color: #dee2e6;
        }
        QCheckBox::indicator:checked:disabled {
            background-color: #adb5bd;
            border-color: #6c757d;
        }
        QCheckBox:disabled {
            color: #6c757d;
        }
        QCheckBox:focus {
            outline: none;
        }
        QCheckBox::indicator:focus {
            border-color: #3498db;
        }
    )";
}

QString GlobalStyles::getButtonStyle() {
    return R"(
        QPushButton {
            background-color: #f8f9fa;
            border: 1px solid #dee2e6;
            padding: 4px 8px;
            font-size: 10pt;
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            color: #333333;
            min-height: 16px;
        }
        QPushButton:hover {
            background-color: #e9ecef;
            border-color: #adb5bd;
        }
        QPushButton:pressed {
            background-color: #dee2e6;
            border-color: #6c757d;
        }
        QPushButton:disabled {
            background-color: #f5f5f5;
            color: #999999;
            border-color: #e0e0e0;
        }
        QPushButton:default:hover {
            background-color: #45a049;
            border-color: #45a049;
        }
        QPushButton:default:pressed {
            background-color: #3d8b40;
            border-color: #3d8b40;
        }
    )";
}

QString GlobalStyles::getInputStyle() {
    return R"(
        QLineEdit, QDoubleSpinBox {
            border: 1px solid #e1e5e9;
            padding: 2px 2px;
            font-size: 10pt;
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            background-color: white;
            color: #333333;
        }
        QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {
            width: 0px;
            height: 0px;
            border: none;
            background: transparent;
        }
        QTextEdit, QPlainTextEdit {
            border: 1px solid #e1e5e9;
            padding: 4px 4px;
            font-size: 10pt;
            font-family: Consolas;
            background-color: white;
            color: #333333;
        }
        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus, QDoubleSpinBox:focus {
            border-color: #4CAF50;
            outline: none;
        }
        QLineEdit:disabled, QTextEdit:disabled, QPlainTextEdit:disabled, QDoubleSpinBox:disabled {
            background-color: #f5f5f5;
            color: #999999;
            border-color: #e0e0e0;
        }

        QComboBox {
            border: 1px solid #ccc;
            background-color: #f9f9f9;
            font-size: 14px;
        }
        QComboBox:hover {
            border: 1px solid #0078d7; /* 鼠标悬停边框 */
        }

        QComboBox QAbstractItemView {
            border: 1px solid #ccc;
            background: white;
            selection-background-color: #0078d7;
            selection-color: white;
            min-height: 30px;   /* 每个选项的最小高度 */
        }


  QSpinBox {
        border: 1px solid #cccccc;
        padding: 2px 8px;
        font-size: 13px;
        background-color: #ffffff;
        selection-background-color: #2563eb; /* Fluent 蓝色 */
    }
    QSpinBox:focus {
        border: 1px solid #2563eb;
    }
    QSpinBox::up-button, QSpinBox::down-button {
        width: 0px;
        height: 0px;
        border: none;
        background: transparent;
    }

    )";
}

QString GlobalStyles::getScrollBarStyle() {
    return R"(
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
)";

}

QString GlobalStyles::getGlobalStyle() {
    return QString(R"(
        QWidget {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            font-size: 10pt;
            color: #333333;
        }
        QGroupBox {
            font-weight: bold;
            border: 2px solid #dee2e6;
            margin-top: 1ex;
            padding-top: 8px;
            font-size: 12pt;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 8px;
            padding: 0 4px 0 4px;
            color: #495057;
            font-size: 12pt;
        }
        QProgressBar {
            border: 1px solid #dee2e6;
            border-radius: 6px;
            text-align: center;
            font-size: 10px;
        }
        QProgressBar::chunk {
            background-color: #4CAF50;
            border-radius: 5px;
        }
/* 表格整体样式 */
QTableWidget {
    border: 1px solid #dee2e6;
    gridline-color: #dee2e6;
    background-color: #ffffff;
    alternate-background-color: #f8f9fa;
    border-bottom: 1px solid #dee2e6;
}

/* 表格项样式 - 关键：完全去掉边框，避免与网格线重叠 */
QTableWidget::item {
    padding: 0px;
    margin: 0px;
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

/* 表头样式 - 关键：只保留右边和下边框，避免重叠 */
QHeaderView::section {
    background-color: #f8f9fa;
    padding: 6px 4px;
    border: none;
    border-right: 1px solid #dee2e6;
    border-bottom: 1px solid #dee2e6;
    font-weight: bold;
    color: #495057;
}

/* 第一个表头单元格 */
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

/* 角落按钮样式（左上角的交叉区域） */
QTableCornerButton::section {
    background-color: #f8f9fa;
    border: none;
    border-right: 1px solid #dee2e6;
    border-bottom: 1px solid #dee2e6;
}
        QLabel {
            font-size: 10pt;
        }
    )") + getCheckBoxStyle() + getButtonStyle() + getInputStyle() + getScrollBarStyle();
}
