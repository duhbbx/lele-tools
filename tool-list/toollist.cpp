#include "toollist.h"

#include <QTextEdit>
#include "../main-widget/mainwindow.h"
#include <QListWidget>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QLabel>

#include "module-meta.h"
ToolList::ToolList(MainWindow* mainWindow, QWidget *parent) : QWidget(parent)
{
    this->mainWindow = mainWindow;

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);



    // 搜索区域
    QWidget *searchWidget = new QWidget;
    QVBoxLayout *searchLayout = new QVBoxLayout(searchWidget);
    searchLayout->setContentsMargins(5, 0, 5, 0);
    searchLayout->setSpacing(5);

    searchLineEdit = new QLineEdit;
    searchLineEdit->setPlaceholderText("搜索工具...");
    searchLineEdit->setFixedHeight(30);
    searchLineEdit->setStyleSheet("border: 1px solid #ccc; border-radius: 5px; padding: 5px;");

    searchLayout->addWidget(searchLineEdit);
    layout->addWidget(searchWidget);

    // 工具列表
    listWidget = new QListWidget;
    listWidget->setStyleSheet(
        "QListWidget {"
        "    border: none;"
        "    background-color: transparent;"
        "    outline: none;"
        "}"
        "QListWidget::item {"
        "    padding: 8px;"
        "    margin: 2px 5px;"
        "    border-radius: 5px;"
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
        "    border-radius: 4px;"
        "}"
        "QScrollBar::handle:vertical {"
        "    background: #c0c0c0;"
        "    border-radius: 4px;"
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

    for (const ModuleMeta &moduleMeta : moduleMetaArray) {
        QListWidgetItem *item0 = new QListWidgetItem(QIcon(moduleMeta.icon), moduleMeta.title);
        item0->setData(Qt::UserRole, QVariant(moduleMeta.className));
        item0->setData(Qt::DisplayRole, QVariant(moduleMeta.title));
        listWidget->addItem(item0);
    }

    layout->addWidget(listWidget);

    QObject::connect(listWidget, &QListWidget::itemClicked, mainWindow, &MainWindow::itemClickedSlot);

    // 设置搜索功能
    setupSearchFunctionality();

    this->setLayout(layout);
}

void ToolList::setupSearchFunctionality()
{
    // 连接搜索框的文本变化信号到过滤槽函数
    connect(searchLineEdit, &QLineEdit::textChanged, this, &ToolList::filterTools);
}

void ToolList::filterTools(const QString &text)
{
    // 如果搜索文本为空，显示所有项目
    if (text.isEmpty()) {
        for (int i = 0; i < listWidget->count(); ++i) {
            listWidget->item(i)->setHidden(false);
        }
        return;
    }
    
    // 根据搜索文本过滤项目（不区分大小写）
    QString searchText = text.toLower();
    for (int i = 0; i < listWidget->count(); ++i) {
        QListWidgetItem *item = listWidget->item(i);
        QString itemText = item->text().toLower();
        
        // 如果项目文本包含搜索文本，则显示；否则隐藏
        bool shouldShow = itemText.contains(searchText);
        item->setHidden(!shouldShow);
    }
}

ToolList::~ToolList()
{

}
