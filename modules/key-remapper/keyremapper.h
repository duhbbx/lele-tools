#ifndef KEYREMAPPER_H
#define KEYREMAPPER_H

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QMessageBox>
#include <QGroupBox>
#include <QCheckBox>
#include <QTimer>
#include <QApplication>
#include <QKeyEvent>
#include <QFrame>
#include <QLineEdit>
#include <QComboBox>
#include <QSplitter>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QStyledItemDelegate>
#include "../../common/dynamicobjectbase.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <map>
#endif

enum class MappingType {
    KeyToKey,        // 按键到按键映射
    KeyToApp,        // 按键启动应用程序
    KeyToCommand     // 按键执行命令
};

struct KeyMapping {
    int originalKey;
    int mappedKey;
    QString originalKeyName;
    QString mappedKeyName;
    bool enabled;

    // 扩展字段
    MappingType type;
    QString appPath;        // 应用程序路径
    QString appName;        // 应用程序显示名
    QString command;        // 要执行的命令
    QString workingDir;     // 工作目录
    QString arguments;      // 命令行参数
};

// 按键捕获单元格编辑器
class KeyCaptureEditor : public QLineEdit
{
    Q_OBJECT

public:
    explicit KeyCaptureEditor(QWidget *parent = nullptr);

signals:
    void keyCaptured(int keyCode, const QString& keyName);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;

private:
    QString getKeyNameFromEvent(QKeyEvent *event, int &keyCode);
};

// 按键捕获委托
class KeyCaptureDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit KeyCaptureDelegate(QObject *parent = nullptr);

    QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

signals:
    void keyCaptured(int row, int column, int keyCode, const QString& keyName) const;
};

// 按键捕获窗口（保留用于兼容）
class KeyCaptureWidget : public QFrame
{
    Q_OBJECT

public:
    explicit KeyCaptureWidget(QWidget *parent = nullptr);
    void startCapture(const QString& prompt);
    void stopCapture();

signals:
    void keyCaptured(int keyCode, const QString& keyName);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    bool m_capturing;
    QString m_promptText;
    QTimer* m_blinkTimer;
    bool m_showCursor;
};

class KeyRemapper : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit KeyRemapper(QWidget *parent = nullptr);
    ~KeyRemapper();

private slots:
    void addNewMapping();
    void addAppLauncher();
    void addCommandLauncher();
    void removeMapping();
    void editMapping();
    void toggleMapping(int row, int column);
    void clearAllMappings();
    void saveProfile();
    void loadProfile();
    void resetToDefault();
    void onKeyMappingDoubleClick(int row, int column);
    void onOriginalKeyCaptured(int keyCode, const QString& keyName);
    void onMappedKeyCaptured(int keyCode, const QString& keyName);
    void selectApplicationFile();
    void browseWorkingDirectory();

private:
    // UI 组件
    QVBoxLayout* mainLayout;
    QHBoxLayout* controlLayout;
    QSplitter* mainSplitter;

    // 控制区域
    QGroupBox* controlGroup;
    QPushButton* addButton;
    QPushButton* addAppButton;
    QPushButton* addCommandButton;
    QPushButton* removeButton;
    QPushButton* editButton;
    QPushButton* clearButton;
    QPushButton* saveButton;
    QPushButton* loadButton;
    QPushButton* resetButton;
    QCheckBox* enableGlobalHook;
    QLabel* statusLabel;

    // 应用程序选择对话框组件
    QWidget* appSelectorDialog;
    QLineEdit* appPathEdit;
    QLineEdit* appNameEdit;
    QLineEdit* workingDirEdit;
    QLineEdit* argumentsEdit;
    QPushButton* browseAppButton;
    QPushButton* browseWorkDirButton;
    QPushButton* confirmAppButton;
    QPushButton* cancelAppButton;

    // 映射表格
    QTableWidget* mappingTable;
    KeyCaptureDelegate* keyCaptureDelegate;

    // 按键捕获
    KeyCaptureWidget* keyCaptureWidget;
    QGroupBox* captureGroup;

    // 数据
    QList<KeyMapping> keyMappings;
    int currentEditingRow;
    bool capturingOriginal;
    MappingType currentMappingType;

#ifdef Q_OS_WIN
    static HHOOK s_keyboardHook;
    static KeyRemapper* s_instance;
    static std::map<int, int> s_keyMappings;
    static bool s_hookEnabled;
#endif

    // 功能方法
    void setupUI();
    void setupTable();
    void setupConnections();
    void setupAppSelectorDialog();
    void populateTable();
    void updateMapping(int row, const KeyMapping& mapping);
    void refreshMappings();
    QString getKeyName(int keyCode);
    int getKeyCode(const QString& keyName);
    void installGlobalHook();
    void uninstallGlobalHook();
    void updateGlobalMappings();
    void showStatusMessage(const QString& message, int timeout = 3000);
    void applyStyles();
    void showAppSelector(MappingType type);
    void hideAppSelector();
    void executeMapping(const KeyMapping& mapping);
    QString getMappingTypeString(MappingType type);
    QString getMappingDisplayText(const KeyMapping& mapping);

    // 数据操作
    void saveProfileToFile(const QString& filePath);
    void loadProfileFromFile(const QString& filePath);
    QJsonObject mappingToJson(const KeyMapping& mapping);
    KeyMapping mappingFromJson(const QJsonObject& obj);

#ifdef Q_OS_WIN
    // Windows 钩子回调
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
#endif
};

#endif // KEYREMAPPER_H