#ifndef TODO_H
#define TODO_H

#include <QWidget>
#include <QString>
#include <QDateTime>
#include <QList>

#include "../../common/dynamicobjectbase.h"

namespace SqliteWrapper { class SqliteManager; }

class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QTextEdit;
class QDateTimeEdit;
class QComboBox;
class QPushButton;
class QToolButton;
class QSpinBox;
class QCheckBox;
class QLabel;
class QSplitter;
class QTimer;
class QSystemTrayIcon;
class QStackedWidget;
class QMenu;
class StatsView;     // forward, defined in cpp

struct TodoItem {
    qint64 id = 0;
    QString title;
    QString notes;
    QDateTime deadline;        // invalid = no deadline
    bool completed = false;
    QDateTime completedAt;
    int priority = 0;          // 0 normal, 1 high, 2 urgent
    QString category;
    int reminderMinutesBefore = 0;       // 旧字段（兼容）
    bool reminded = false;
    QList<int> reminderOffsets;          // 多个提醒偏移（分钟）
    QList<int> remindedOffsets;          // 已经触发过的偏移
    QDateTime createdAt;
};

class Todo : public QWidget, public DynamicObjectBase
{
    Q_OBJECT
public:
    explicit Todo();
    ~Todo() override;

private slots:
    void onAddTask();
    void onSaveDetail();
    void onDeleteTask();
    void onTaskSelected();
    void onCompletedToggled(QListWidgetItem* item);
    void onFilterChanged();
    void onSearchChanged();
    void onShowStats();
    void onExportPdf();
    void onReminderTick();

private:
    void setupUI();
    void initDb();
    void reloadList();
    void renderItem(QListWidgetItem* item, const TodoItem& t);
    void clearDetail();
    void loadDetail(const TodoItem& t);
    void notifyDeadline(const TodoItem& t);

    enum FilterMode { FilterAll, FilterToday, FilterWeek, FilterDone };

    // UI
    QPushButton* m_addBtn = nullptr;
    QPushButton* m_filterAllBtn = nullptr;
    QPushButton* m_filterTodayBtn = nullptr;
    QPushButton* m_filterWeekBtn = nullptr;
    QPushButton* m_filterDoneBtn = nullptr;
    QPushButton* m_statsBtn = nullptr;
    QPushButton* m_exportPdfBtn = nullptr;
    QLineEdit* m_searchEdit = nullptr;

    QStackedWidget* m_stack = nullptr;     // 0 = list+detail, 1 = stats
    StatsView* m_statsView = nullptr;

    QListWidget* m_list = nullptr;
    // detail panel widgets
    QWidget* m_detailWidget = nullptr;
    QLineEdit* m_titleEdit = nullptr;
    QTextEdit* m_notesEdit = nullptr;
    QDateTimeEdit* m_deadlineEdit = nullptr;
    QCheckBox* m_hasDeadlineCheck = nullptr;
    QComboBox* m_priorityCombo = nullptr;
    QLineEdit* m_categoryEdit = nullptr;
    QToolButton* m_reminderBtn = nullptr;
    QMenu* m_reminderMenu = nullptr;
    QPushButton* m_saveDetailBtn = nullptr;
    QPushButton* m_deleteBtn = nullptr;
    QLabel* m_statusLabel = nullptr;

    // 数据
    SqliteWrapper::SqliteManager* m_db = nullptr;
    QList<TodoItem> m_cache;
    qint64 m_currentId = 0;
    FilterMode m_filter = FilterAll;

    // 提醒
    QTimer* m_reminderTimer = nullptr;
    QSystemTrayIcon* m_tray = nullptr;
};

#endif // TODO_H
