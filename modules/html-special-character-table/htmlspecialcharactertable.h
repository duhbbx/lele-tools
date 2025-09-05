#ifndef HTMLSPECIALCHARACTERTABLE_H
#define HTMLSPECIALCHARACTERTABLE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QTabWidget>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QScrollArea>
#include <QComboBox>
#include <QCheckBox>
#include <QTextEdit>
#include <QFrame>
#include <QClipboard>
#include <QApplication>
#include <QMessageBox>
#include <QTimer>
#include <QToolTip>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QFont>
#include <QFontMetrics>
#include <QRegularExpression>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFileDialog>
#include <QProgressBar>
#include <QListWidget>
#include <QListWidgetItem>
#include <QButtonGroup>
#include <QRadioButton>

#include "../../common/dynamicobjectbase.h"

// HTML字符数据结构
struct HtmlCharacter {
    QString character;      // 显示字符
    QString entity;         // HTML实体 (&amp;)
    QString numeric;        // 数字实体 (&#38;)
    QString hex;           // 十六进制实体 (&#x26;)
    QString description;    // 描述
    QString category;       // 分类
    int unicode;           // Unicode码点
    
    HtmlCharacter() : unicode(0) {}
    HtmlCharacter(const QString &ch, const QString &ent, const QString &num, 
                  const QString &h, const QString &desc, const QString &cat, int uni)
        : character(ch), entity(ent), numeric(num), hex(h), description(desc), category(cat), unicode(uni) {}
    
    // 比较运算符
    bool operator==(const HtmlCharacter &other) const {
        return character == other.character && 
               entity == other.entity && 
               unicode == other.unicode;
    }
    
    bool operator!=(const HtmlCharacter &other) const {
        return !(*this == other);
    }
};


// 主HTML特殊字符表类
class HtmlSpecialCharacterTable : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit HtmlSpecialCharacterTable();
    ~HtmlSpecialCharacterTable();

private slots:
    void onSearchTextChanged();
    void onCategoryChanged();
    void onCharacterClicked(const HtmlCharacter &character);
    void onCopyRequested(const QString &text, const QString &type);
    void onClearSearch();
    void onExportList();
    void onImportList();
    void onShowFavorites();
    void onAddToFavorites();
    void onRemoveFromFavorites();

private:
    void setupUI();
    void setupSearchArea();
    void setupCategoryFilter();
    void setupCharacterDisplay();
    void setupDetailPanel();
    void setupStatusArea();
    
    void initializeCharacters();
    void addCharacter(const QString &ch, const QString &entity, const QString &numeric,
                     const QString &hex, const QString &description, const QString &category, int unicode);
    void filterCharacters();
    void updateCharacterDisplay();
    void updateDetailPanel(const HtmlCharacter &character);
    void clearDetailPanel();
    
    
    // 数据管理
    void loadSettings();
    void saveSettings();
    void loadFavorites();
    void saveFavorites();
    
    // UI组件
    QVBoxLayout *mainLayout;
    QSplitter *mainSplitter;
    
    // 搜索和过滤区域
    QGroupBox *searchGroup;
    QHBoxLayout *searchLayout;
    QLineEdit *searchEdit;
    QPushButton *clearSearchBtn;
    QComboBox *categoryCombo;
    QCheckBox *favoritesOnlyCheck;
    
    // 字符显示区域 (只保留表格视图)
    QTableWidget *characterTable;
    
    // 详情面板
    QGroupBox *detailGroup;
    QVBoxLayout *detailLayout;
    QLabel *detailCharLabel;
    QLabel *detailEntityLabel;
    QLabel *detailNumericLabel;
    QLabel *detailHexLabel;
    QLabel *detailUnicodeLabel;
    QLabel *detailDescLabel;
    QLabel *detailCategoryLabel;
    QTextEdit *usageExample;
    QHBoxLayout *detailButtonLayout;
    QPushButton *copyCharDetailBtn;
    QPushButton *copyEntityDetailBtn;
    QPushButton *copyNumericDetailBtn;
    QPushButton *copyHexDetailBtn;
    QPushButton *addToFavoritesBtn;
    QPushButton *removeFromFavoritesBtn;
    
    // 操作按钮
    QHBoxLayout *buttonLayout;
    QPushButton *exportBtn;
    QPushButton *importBtn;
    QPushButton *showFavoritesBtn;
    
    // 状态栏
    QHBoxLayout *statusLayout;
    QLabel *statusLabel;
    QLabel *countLabel;
    QProgressBar *progressBar;
    
    // 数据成员
    QList<HtmlCharacter> m_allCharacters;
    QList<HtmlCharacter> m_filteredCharacters;
    QList<HtmlCharacter> m_favoriteCharacters;
    HtmlCharacter m_currentCharacter;
    
    QString m_currentSearchText;
    QString m_currentCategory;
    bool m_showFavoritesOnly;
    
    QTimer *m_searchTimer;
    QTimer *m_statusTimer;
    
};

#endif // HTMLSPECIALCHARACTERTABLE_H