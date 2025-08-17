
#ifndef HTTPSTATUSCODE_H
#define HTTPSTATUSCODE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QComboBox>
#include <QCheckBox>
#include <QButtonGroup>
#include <QRadioButton>
#include <QScrollArea>
#include <QFrame>
#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QModelIndex>
#include <QStringConverter>

#include "../../common/dynamicobjectbase.h"

// HTTP状态码信息结构
struct HttpStatusInfo {
    int code;                    // 状态码
    QString phrase;              // 状态短语
    QString description;         // 详细描述
    QString category;            // 分类（1xx, 2xx, 3xx, 4xx, 5xx）
    QString categoryName;        // 分类名称
    bool isCommon;              // 是否为常用状态码
    QString rfc;                // RFC标准
    QString usage;              // 使用场景
    QStringList examples;       // 示例说明
    
    HttpStatusInfo() : code(0), isCommon(false) {}
    HttpStatusInfo(int c, const QString &p, const QString &d, const QString &cat, 
                   const QString &catName, bool common = false, const QString &r = "", 
                   const QString &u = "", const QStringList &ex = QStringList())
        : code(c), phrase(p), description(d), category(cat), categoryName(catName), 
          isCommon(common), rfc(r), usage(u), examples(ex) {}
};

class HttpStatusCode : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit HttpStatusCode();
    ~HttpStatusCode();

private slots:
    void onSearchTextChanged();
    void onCategoryFilterChanged();
    void onCommonOnlyToggled(bool checked);
    void onStatusCodeClicked(QTableWidgetItem *item);
    void onCopyCodeClicked();
    void onCopyDescriptionClicked();
    void onExportClicked();
    void onRefreshClicked();
    void onQuickSearchClicked();

private:
    void setupUI();
    void setupSearchArea();
    void setupFilterArea();
    void setupStatusTableArea();
    void setupDetailsArea();
    void setupToolbarArea();
    
    void initializeStatusCodes();
    void populateStatusTable();
    void filterStatusCodes();
    void updateDetailsPanel(const HttpStatusInfo &info);
    void clearDetailsPanel();
    
    void exportToFile(const QString &fileName);
    void copySelectedToClipboard();
    void performQuickSearch(const QString &searchTerm);
    void updateCountLabel();
    void updateSearchResultLabel();
    
    QString getColorForCategory(const QString &category);
    QString formatStatusCode(int code);
    
    // UI组件
    QVBoxLayout *mainLayout;
    QSplitter *mainSplitter;
    
    // 左侧面板
    QWidget *leftPanel;
    QVBoxLayout *leftLayout;
    
    // 搜索区域
    QGroupBox *searchGroup;
    QVBoxLayout *searchLayout;
    QLineEdit *searchEdit;
    QPushButton *quickSearchBtn;
    QLabel *searchResultLabel;
    
    // 筛选区域
    QGroupBox *filterGroup;
    QGridLayout *filterLayout;
    QComboBox *categoryCombo;
    QCheckBox *commonOnlyCheckBox;
    QCheckBox *show1xxCheckBox;
    QCheckBox *show2xxCheckBox;
    QCheckBox *show3xxCheckBox;
    QCheckBox *show4xxCheckBox;
    QCheckBox *show5xxCheckBox;
    QPushButton *resetFilterBtn;
    
    // 状态码表格区域
    QGroupBox *tableGroup;
    QVBoxLayout *tableLayout;
    QTableWidget *statusTable;
    QLabel *countLabel;
    
    // 右侧面板
    QWidget *rightPanel;
    QVBoxLayout *rightLayout;
    
    // 详细信息区域
    QGroupBox *detailsGroup;
    QVBoxLayout *detailsLayout;
    QLabel *codeLabel;
    QLabel *phraseLabel;
    QLabel *categoryLabel;
    QTextEdit *descriptionEdit;
    QTextEdit *usageEdit;
    QTextEdit *examplesEdit;
    QLabel *rfcLabel;
    
    // 工具栏区域
    QGroupBox *toolbarGroup;
    QHBoxLayout *toolbarLayout;
    QPushButton *copyCodeBtn;
    QPushButton *copyDescBtn;
    QPushButton *exportBtn;
    QPushButton *refreshBtn;
    
    // 数据成员
    QList<HttpStatusInfo> m_allStatusCodes;
    QList<HttpStatusInfo> m_filteredStatusCodes;
    HttpStatusInfo m_currentStatus;
    
    // 常量
    static const QStringList CATEGORY_NAMES;
    static const QStringList CATEGORY_COLORS;
};

#endif // HTTPSTATUSCODE_H
