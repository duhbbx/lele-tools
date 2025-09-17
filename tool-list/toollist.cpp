#include "toollist.h"

#include "../main-widget/mainwindow.h"
#include <QListWidget>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QSizePolicy>

#include "module-meta.h"

ToolList::ToolList(MainWindow* mainWindow, QWidget* parent) : QWidget(parent) {
    this->mainWindow = mainWindow;

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);


    // 搜索区域
    auto* searchWidget = new QWidget;
    searchWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* searchLayout = new QVBoxLayout(searchWidget);
    searchLayout->setContentsMargins(5, 0, 5, 0);
    searchLayout->setSpacing(5);

    searchLineEdit = new QLineEdit;
    searchLineEdit->setPlaceholderText(tr("搜索工具..."));
    searchLineEdit->setFixedHeight(30);
    searchLineEdit->setStyleSheet("border: 1px solid #ccc; border-radius: 0px; padding: 5px;");

    searchLayout->addWidget(searchLineEdit);
    // 搜索区域不拉伸，保持固定大小
    layout->addWidget(searchWidget, 0);

    // 工具列表
    listWidget = new QListWidget;
    listWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    listWidget->setStyleSheet(
        "QListWidget {"
        "    border: none;"
        "    background-color: transparent;"
        "    outline: none;"
        "    font-size: 10pt;"
        "}"
        "QListWidget::item {"
        "    padding: 8px;"
        "    margin: 2px 5px;"
        "    border-radius: 0px;"
        "    border: none;"
        "}"
        "QListWidget::item:hover {"
        "    background-color: #e0e7ff;"
        "}"
        "QListWidget::item:selected {"
        "    background-color: #3b82f6;"
        "    color: white;"
        "}"
        "QScrollBar:vertical {"
        "    background: #f0f0f0;"
        "    width: 8px;"
        "    border-radius: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "    background: #c0c0c0;"
        "    border-radius: 0px;"
        "    min-height: 20px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "    background: #a0a0a0;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "    border: none;"
        "    background: none;"
        "}"
        "QScrollBar::up-arrow:vertical, QScrollBar::down-arrow:vertical {"
        "    background: none;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "    background: none;"
        "}"
    );

    for (const auto& [icon, title, className] : moduleMetaArray) {
        auto* item0 = new QListWidgetItem(QIcon(icon), title);
        item0->setData(Qt::UserRole, QVariant(className));
        item0->setData(Qt::DisplayRole, QVariant(title));
        listWidget->addItem(item0);
    }

    // 设置拉伸因子为1，让列表填满剩余空间
    layout->addWidget(listWidget, 1);

    connect(listWidget, &QListWidget::itemClicked, mainWindow, &MainWindow::itemClickedSlot);

    // 设置搜索功能
    setupSearchFunctionality();

    // 设置大小策略，确保组件能填满父容器
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    this->setLayout(layout);
}

void ToolList::setupSearchFunctionality() {
    // 连接搜索框的文本变化信号到过滤槽函数
    connect(searchLineEdit, &QLineEdit::textChanged, this, &ToolList::filterTools);
}

void ToolList::filterTools(const QString& text) const {
    // 如果搜索文本为空，显示所有项目
    if (text.isEmpty()) {
        for (int i = 0; i < listWidget->count(); ++i) {
            listWidget->item(i)->setHidden(false);
        }
        return;
    }

    // 根据搜索文本过滤项目（不区分大小写）
    const QString searchText = text.toLower();
    for (int i = 0; i < listWidget->count(); ++i) {
        QListWidgetItem* item = listWidget->item(i);
        QString itemText = item->text().toLower();

        // 如果项目文本包含搜索文本，则显示；否则隐藏
        const bool shouldShow = itemText.contains(searchText);
        item->setHidden(!shouldShow);
    }
}

ToolList::~ToolList() = default;
