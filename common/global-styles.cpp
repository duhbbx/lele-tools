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
        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            border: 2px solid #e1e5e9;
            padding: 2px 2px;
            font-size: 10pt;
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            background-color: white;
            color: #333333;
        }
        QTextEdit, QPlainTextEdit {
            border: 2px solid #e1e5e9;
            padding: 4px 4px;
            font-size: 10pt;
            font-family: Consolas;
            background-color: white;
            color: #333333;
        }
        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
            border-color: #4CAF50;
            outline: none;
        }
        QLineEdit:disabled, QTextEdit:disabled, QPlainTextEdit:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled, QComboBox:disabled {
            background-color: #f5f5f5;
            color: #999999;
            border-color: #e0e0e0;
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
        QTableWidget {
            border: 1px solid #dee2e6;
            gridline-color: #e9ecef;
            font-size: 10pt;
            background-color: white;
        }
        QTableWidget::item {
            padding: 4px 8px;
            font-size: 10pt;
            border-bottom: 1px solid #e9ecef;
        }
        QTableWidget::item:selected {
            background-color: #e8f5e8;
            color: #333333;
        }
        QHeaderView::section {
            background-color: #f8f9fa;
            border: 1px solid #dee2e6;
            padding: 4px 8px;
            font-weight: bold;
            font-size: 10pt;
        }
        QLabel {
            font-size: 10pt;
        }
    )") + getCheckBoxStyle() + getButtonStyle() + getInputStyle();
}
