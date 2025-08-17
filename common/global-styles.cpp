#include "global-styles.h"

QString GlobalStyles::getCheckBoxStyle()
{
    return R"(
        QCheckBox {
            spacing: 8px;
            font-size: 11pt;
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            color: #333333;
        }
        QCheckBox::indicator {
            width: 20px;
            height: 20px;
            border: 2px solid #cccccc;
            border-radius: 4px;
            background-color: white;
        }
        QCheckBox::indicator:hover {
            border-color: #4CAF50;
            background-color: #f0f8f0;
        }
        QCheckBox::indicator:checked {
            background-color: #4CAF50;
            border-color: #4CAF50;
            image: url(data:image/svg+xml;base64,PHN2ZyB3aWR0aD0iMTQiIGhlaWdodD0iMTQiIHZpZXdCb3g9IjAgMCAxNCAxNCIgZmlsbD0ibm9uZSIgeG1sbnM9Imh0dHA6Ly93d3cudzMub3JnLzIwMDAvc3ZnIj4KPHBhdGggZD0iTTExLjMzMzMgMy41TDUuMjUgOS41ODMzTDIuNjY2NjcgNyIgc3Ryb2tlPSJ3aGl0ZSIgc3Ryb2tlLXdpZHRoPSIyLjMzMzMzIiBzdHJva2UtbGluZWNhcD0icm91bmQiIHN0cm9rZS1saW5lam9pbj0icm91bmQiLz4KPC9zdmc+);
        }
        QCheckBox::indicator:checked:hover {
            background-color: #45a049;
            border-color: #45a049;
        }
        QCheckBox::indicator:checked:pressed {
            background-color: #3d8b40;
            border-color: #3d8b40;
        }
        QCheckBox::indicator:disabled {
            background-color: #f5f5f5;
            border-color: #e0e0e0;
        }
        QCheckBox::indicator:checked:disabled {
            background-color: #cccccc;
            border-color: #cccccc;
        }
        QCheckBox:disabled {
            color: #999999;
        }
    )";
}

QString GlobalStyles::getButtonStyle()
{
    return R"(
        QPushButton {
            background-color: #f8f9fa;
            border: 1px solid #dee2e6;
            border-radius: 6px;
            padding: 8px 16px;
            font-size: 11pt;
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
        QPushButton:default {
            background-color: #4CAF50;
            border-color: #4CAF50;
            color: white;
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

QString GlobalStyles::getInputStyle()
{
    return R"(
        QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            border: 2px solid #e1e5e9;
            border-radius: 6px;
            padding: 6px 10px;
            font-size: 11pt;
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
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

QString GlobalStyles::getGlobalStyle()
{
    return QString(R"(
        QWidget {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            font-size: 11pt;
            color: #333333;
        }
        QGroupBox {
            font-weight: bold;
            border: 2px solid #dee2e6;
            border-radius: 8px;
            margin-top: 1ex;
            padding-top: 12px;
            font-size: 11pt;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 8px 0 8px;
            color: #495057;
        }
        QProgressBar {
            border: 1px solid #dee2e6;
            border-radius: 6px;
            text-align: center;
            font-size: 11pt;
        }
        QProgressBar::chunk {
            background-color: #4CAF50;
            border-radius: 5px;
        }
        QTableWidget {
            border: 1px solid #dee2e6;
            border-radius: 6px;
            gridline-color: #e9ecef;
            font-size: 11pt;
            background-color: white;
        }
        QTableWidget::item {
            padding: 6px;
            border-bottom: 1px solid #e9ecef;
        }
        QTableWidget::item:selected {
            background-color: #e8f5e8;
            color: #333333;
        }
        QHeaderView::section {
            background-color: #f8f9fa;
            border: 1px solid #dee2e6;
            padding: 8px;
            font-weight: bold;
            font-size: 11pt;
        }
    )") + getCheckBoxStyle() + getButtonStyle() + getInputStyle();
}
