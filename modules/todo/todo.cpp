#include "todo.h"

#include "../../common/sqlite/SqliteManager.h"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QDateTimeEdit>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontMetricsF>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPdfWriter>
#include <QPrinter>
#include <QPushButton>
#include <QMenu>
#include <QSet>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariantMap>

REGISTER_DYNAMICOBJECT(Todo);

namespace {

constexpr const char* kSql_Create = R"(
CREATE TABLE IF NOT EXISTS todo_tasks (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    title TEXT NOT NULL,
    notes TEXT DEFAULT '',
    deadline TEXT,
    completed INTEGER DEFAULT 0,
    completed_at TEXT,
    priority INTEGER DEFAULT 0,
    category TEXT DEFAULT '',
    reminder_minutes_before INTEGER DEFAULT 0,
    reminded INTEGER DEFAULT 0,
    created_at TEXT DEFAULT CURRENT_TIMESTAMP
)
)";

QString priorityName(int p)
{
    switch (p) {
        case 1: return QObject::tr("高");
        case 2: return QObject::tr("紧急");
        default: return QObject::tr("普通");
    }
}

QColor priorityColor(int p)
{
    switch (p) {
        case 1: return QColor(0xff, 0xc1, 0x07);   // amber
        case 2: return QColor(0xfa, 0x52, 0x52);   // red
        default: return QColor(0xad, 0xb5, 0xbd);  // gray
    }
}

QString humanDeadline(const QDateTime& dt)
{
    if (!dt.isValid()) return QString();
    QDateTime now = QDateTime::currentDateTime();
    int days = now.date().daysTo(dt.date());
    QString clock = dt.toString("HH:mm");
    if (days == 0) return QObject::tr("今天 %1").arg(clock);
    if (days == 1) return QObject::tr("明天 %1").arg(clock);
    if (days == -1) return QObject::tr("昨天 %1").arg(clock);
    if (days > 1 && days < 7) return QObject::tr("%1 天后 %2").arg(days).arg(clock);
    if (days < 0) return QObject::tr("逾期 %1").arg(dt.toString("M-d HH:mm"));
    return dt.toString("yyyy-MM-dd HH:mm");
}

QList<int> parseOffsets(const QString& s)
{
    QList<int> out;
    if (s.trimmed().isEmpty()) return out;
    for (const QString& part : s.split(',', Qt::SkipEmptyParts)) {
        bool ok = false;
        int v = part.trimmed().toInt(&ok);
        if (ok && v >= 0) out << v;
    }
    return out;
}

QString joinOffsets(const QList<int>& offsets)
{
    QStringList parts;
    for (int v : offsets) parts << QString::number(v);
    return parts.join(',');
}

QString reminderSummary(const QList<int>& offsets)
{
    if (offsets.isEmpty()) return QObject::tr("不提醒");
    QStringList readable;
    QList<int> sorted = offsets;
    std::sort(sorted.begin(), sorted.end());
    for (int v : sorted) {
        if (v == 0) readable << QObject::tr("截止时刻");
        else if (v < 60) readable << QObject::tr("提前 %1 分").arg(v);
        else if (v < 60 * 24) readable << QObject::tr("提前 %1 时").arg(v / 60);
        else readable << QObject::tr("提前 %1 天").arg(v / (60 * 24));
    }
    if (readable.size() == 1) return readable.first();
    return QObject::tr("已选 %1 项").arg(readable.size());
}

TodoItem rowToItem(const QVariantMap& row)
{
    TodoItem t;
    t.id = row["id"].toLongLong();
    t.title = row["title"].toString();
    t.notes = row["notes"].toString();
    QString dl = row["deadline"].toString();
    if (!dl.isEmpty()) t.deadline = QDateTime::fromString(dl, Qt::ISODate);
    t.completed = row["completed"].toInt() != 0;
    QString ca = row["completed_at"].toString();
    if (!ca.isEmpty()) t.completedAt = QDateTime::fromString(ca, Qt::ISODate);
    t.priority = row["priority"].toInt();
    t.category = row["category"].toString();
    t.reminderMinutesBefore = row["reminder_minutes_before"].toInt();
    t.reminded = row["reminded"].toInt() != 0;
    t.reminderOffsets = parseOffsets(row["reminder_offsets"].toString());
    t.remindedOffsets = parseOffsets(row["reminded_offsets"].toString());
    // 旧数据兼容：如果新字段为空但有旧的 reminder_minutes_before，迁移为单元素列表
    if (t.reminderOffsets.isEmpty() && t.reminderMinutesBefore > 0) {
        t.reminderOffsets << t.reminderMinutesBefore;
        if (t.reminded) t.remindedOffsets = t.reminderOffsets;
    }
    QString cr = row["created_at"].toString();
    if (!cr.isEmpty()) t.createdAt = QDateTime::fromString(cr, Qt::ISODate);
    return t;
}

} // namespace


// ============= 统计视图 =============

class StatsView : public QWidget {
public:
    struct DayBucket { QDate date; int count = 0; };

    explicit StatsView(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumHeight(280);
    }

    void setData(int doneTotal, int pendingTotal, const QList<DayBucket>& last7Days,
                 const QList<QPair<QString,int>>& byCategory)
    {
        m_done = doneTotal;
        m_pending = pendingTotal;
        m_buckets = last7Days;
        m_categories = byCategory;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.fillRect(rect(), QColor(0xff, 0xff, 0xff));

        QFont titleFont = font(); titleFont.setPointSize(11); titleFont.setBold(true);
        QFont bigFont = font(); bigFont.setPointSize(28); bigFont.setBold(true);
        QFont smallFont = font(); smallFont.setPointSize(9);

        const int padX = 24, padY = 18;

        // 头部数字
        p.setFont(bigFont);
        p.setPen(QColor(0x22, 0x8b, 0xe6));
        p.drawText(QRect(padX, padY, 200, 50),
                   Qt::AlignLeft | Qt::AlignVCenter, QString::number(m_done));
        p.setFont(smallFont);
        p.setPen(QColor(0x86, 0x8e, 0x96));
        p.drawText(QRect(padX, padY + 50, 200, 18),
                   Qt::AlignLeft | Qt::AlignVCenter, tr("最近 7 天已完成"));

        p.setFont(bigFont);
        p.setPen(QColor(0xfa, 0x52, 0x52));
        p.drawText(QRect(padX + 220, padY, 200, 50),
                   Qt::AlignLeft | Qt::AlignVCenter, QString::number(m_pending));
        p.setFont(smallFont);
        p.setPen(QColor(0x86, 0x8e, 0x96));
        p.drawText(QRect(padX + 220, padY + 50, 200, 18),
                   Qt::AlignLeft | Qt::AlignVCenter, tr("待完成"));

        // 7 日柱状图
        p.setFont(titleFont);
        p.setPen(QColor(0x49, 0x50, 0x57));
        p.drawText(QRect(padX, padY + 90, width() - 2 * padX, 20),
                   Qt::AlignLeft, tr("最近 7 天每日完成数"));

        const int chartTop = padY + 116;
        const int chartH = 130;
        const int chartLeft = padX;
        const int chartRight = width() - padX;
        const int chartW = chartRight - chartLeft;

        int maxCount = 1;
        for (const auto& b : m_buckets) maxCount = qMax(maxCount, b.count);

        if (m_buckets.isEmpty()) {
            p.setPen(QColor(0xad, 0xb5, 0xbd));
            p.drawText(QRect(chartLeft, chartTop, chartW, chartH),
                       Qt::AlignCenter, tr("暂无数据"));
            return;
        }

        int n = m_buckets.size();
        int barW = qMax(20, chartW / (n * 2 + 1));
        int gap = (chartW - barW * n) / (n + 1);

        for (int i = 0; i < n; ++i) {
            const auto& b = m_buckets[i];
            int x = chartLeft + gap + i * (barW + gap);
            double ratio = double(b.count) / maxCount;
            int barH = int(ratio * (chartH - 30));
            int y = chartTop + (chartH - 30) - barH;
            QColor c = b.count > 0 ? QColor(0x22, 0x8b, 0xe6) : QColor(0xe9, 0xec, 0xef);
            p.setPen(Qt::NoPen);
            p.setBrush(c);
            p.drawRoundedRect(x, y, barW, barH > 0 ? barH : 2, 4, 4);

            // 数字
            p.setPen(QColor(0x49, 0x50, 0x57));
            p.setFont(smallFont);
            p.drawText(QRect(x - 10, y - 18, barW + 20, 16),
                       Qt::AlignCenter, QString::number(b.count));
            // 日期标签
            p.setPen(QColor(0x86, 0x8e, 0x96));
            QString dayLabel = b.date.toString("M-d");
            p.drawText(QRect(x - 10, chartTop + chartH - 18, barW + 20, 16),
                       Qt::AlignCenter, dayLabel);
        }
    }

private:
    int m_done = 0;
    int m_pending = 0;
    QList<DayBucket> m_buckets;
    QList<QPair<QString, int>> m_categories;
};


// ============= Todo =============

Todo::Todo() : QWidget(nullptr), DynamicObjectBase()
{
    setupUI();
    initDb();
    reloadList();

    // 提醒定时器：每 30 秒检查一次
    m_reminderTimer = new QTimer(this);
    m_reminderTimer->setInterval(30 * 1000);
    connect(m_reminderTimer, &QTimer::timeout, this, &Todo::onReminderTick);
    m_reminderTimer->start();

    // 系统托盘（可选 — 失败也不致命）
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        m_tray = new QSystemTrayIcon(this);
        m_tray->setIcon(QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation));
        m_tray->setToolTip(tr("Todo 提醒"));
        m_tray->show();
    }
}

Todo::~Todo()
{
    if (m_tray) m_tray->hide();
}

// ============= UI =============

void Todo::setupUI()
{
    setStyleSheet(R"(
        QLineEdit, QTextEdit, QSpinBox, QComboBox, QDateTimeEdit {
            border: 1px solid #ced4da; border-radius: 4px;
            padding: 5px 8px; background: #fff; min-height: 22px;
            selection-background-color: #b8d4ff;
        }
        QLineEdit:focus, QTextEdit:focus, QSpinBox:focus,
        QComboBox:focus, QDateTimeEdit:focus { border-color: #228be6; }
        QPushButton {
            border: 1px solid #ced4da; border-radius: 4px;
            padding: 5px 14px; background: #ffffff; min-height: 22px; color: #343a40;
        }
        QPushButton:hover  { background: #f1f3f5; border-color: #adb5bd; }
        QPushButton:pressed{ background: #e9ecef; }
        QPushButton:checked { background: #d0ebff; border-color: #74c0fc; color: #1864ab; }
        QPushButton:disabled { color: #adb5bd; background: #f8f9fa; }
        QPushButton#primaryBtn {
            background: #228be6; border: 1px solid #1c7ed6; color: white; font-weight: bold;
        }
        QPushButton#primaryBtn:hover  { background: #1c7ed6; }
        QPushButton#dangerBtn {
            background: #fa5252; border: 1px solid #f03e3e; color: white; font-weight: bold;
        }
        QPushButton#dangerBtn:hover  { background: #f03e3e; }
        QFrame#card {
            background: #f8f9fa; border: 1px solid #dee2e6; border-radius: 6px;
        }
        QListWidget {
            border: 1px solid #dee2e6; border-radius: 6px;
            background: #ffffff; outline: none;
        }
        QListWidget::item {
            padding: 8px 6px; border-bottom: 1px solid #f1f3f5;
        }
        QListWidget::item:hover { background: #f1f3f5; }
        QListWidget::item:selected { background: #e7f5ff; color: #1864ab; }
        QLabel { background: transparent; }
    )");

    auto* main = new QVBoxLayout(this);
    main->setContentsMargins(12, 10, 12, 10);
    main->setSpacing(10);

    // 顶部工具栏
    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(6);

    m_addBtn = new QPushButton(tr("➕ 新建任务"));
    m_addBtn->setObjectName("primaryBtn");
    topRow->addWidget(m_addBtn);
    topRow->addSpacing(10);

    m_filterAllBtn = new QPushButton(tr("全部"));
    m_filterTodayBtn = new QPushButton(tr("今天"));
    m_filterWeekBtn = new QPushButton(tr("本周"));
    m_filterDoneBtn = new QPushButton(tr("已完成"));
    for (auto* b : {m_filterAllBtn, m_filterTodayBtn, m_filterWeekBtn, m_filterDoneBtn}) {
        b->setCheckable(true);
        topRow->addWidget(b);
    }
    m_filterAllBtn->setChecked(true);

    topRow->addSpacing(10);
    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText(tr("🔍 搜索"));
    m_searchEdit->setMinimumWidth(180);
    topRow->addWidget(m_searchEdit);
    topRow->addStretch();

    m_statsBtn = new QPushButton(tr("📊 统计"));
    m_statsBtn->setCheckable(true);
    topRow->addWidget(m_statsBtn);
    m_exportPdfBtn = new QPushButton(tr("📄 导出 PDF"));
    topRow->addWidget(m_exportPdfBtn);
    main->addLayout(topRow);

    // 状态条
    m_statusLabel = new QLabel(tr("就绪"));
    m_statusLabel->setStyleSheet(
        "color:#2e7d32; padding:5px 10px; background:#e8f5e8;"
        " border-radius:4px; font-weight:bold;");
    main->addWidget(m_statusLabel);

    // 中间堆叠：列表+详情 / 统计视图
    m_stack = new QStackedWidget();

    // 页 0：列表 + 详情
    {
        auto* page = new QWidget();
        auto* h = new QHBoxLayout(page);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(8);

        auto* split = new QSplitter(Qt::Horizontal);
        m_list = new QListWidget();
        m_list->setAlternatingRowColors(false);
        split->addWidget(m_list);

        // 详情面板
        m_detailWidget = new QFrame();
        m_detailWidget->setObjectName("card");
        auto* col = new QVBoxLayout(m_detailWidget);
        col->setContentsMargins(12, 10, 12, 10);
        col->setSpacing(8);

        auto* title = new QLabel(tr("任务详情"));
        title->setStyleSheet("font-weight: bold; color: #495057;");
        col->addWidget(title);

        col->addWidget(new QLabel(tr("标题")));
        m_titleEdit = new QLineEdit();
        m_titleEdit->setPlaceholderText(tr("如：写周报、买菜、调试 bug ..."));
        col->addWidget(m_titleEdit);

        col->addWidget(new QLabel(tr("备注")));
        m_notesEdit = new QTextEdit();
        m_notesEdit->setPlaceholderText(tr("可选的描述/上下文"));
        m_notesEdit->setMaximumHeight(120);
        col->addWidget(m_notesEdit);

        // 截止时间
        auto* dlRow = new QHBoxLayout();
        m_hasDeadlineCheck = new QCheckBox(tr("设定截止时间"));
        dlRow->addWidget(m_hasDeadlineCheck);
        m_deadlineEdit = new QDateTimeEdit(QDateTime::currentDateTime().addDays(1));
        m_deadlineEdit->setCalendarPopup(true);
        m_deadlineEdit->setDisplayFormat("yyyy-MM-dd HH:mm");
        m_deadlineEdit->setEnabled(false);
        m_deadlineEdit->setMinimumWidth(180);
        m_deadlineEdit->setMaximumWidth(220);   // 别撑得太宽
        dlRow->addWidget(m_deadlineEdit);
        dlRow->addStretch();
        col->addLayout(dlRow);
        connect(m_hasDeadlineCheck, &QCheckBox::toggled, m_deadlineEdit, &QDateTimeEdit::setEnabled);

        // 优先级 + 分类
        auto* metaRow = new QHBoxLayout();
        metaRow->addWidget(new QLabel(tr("优先级")));
        m_priorityCombo = new QComboBox();
        m_priorityCombo->addItem(tr("普通"), 0);
        m_priorityCombo->addItem(tr("高"), 1);
        m_priorityCombo->addItem(tr("紧急"), 2);
        m_priorityCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        m_priorityCombo->setMinimumContentsLength(6);
        metaRow->addWidget(m_priorityCombo);
        metaRow->addSpacing(10);
        metaRow->addWidget(new QLabel(tr("分类")));
        m_categoryEdit = new QLineEdit();
        m_categoryEdit->setPlaceholderText(tr("如：工作 / 学习 / 生活"));
        metaRow->addWidget(m_categoryEdit, 1);
        col->addLayout(metaRow);

        // 提醒（多选 — QToolButton 弹菜单，菜单项可勾选）
        auto* remRow = new QHBoxLayout();
        remRow->addWidget(new QLabel(tr("提醒")));
        m_reminderBtn = new QToolButton();
        m_reminderBtn->setPopupMode(QToolButton::InstantPopup);
        m_reminderBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        m_reminderBtn->setText(tr("不提醒"));
        m_reminderBtn->setStyleSheet(
            "QToolButton {"
            "  border: 1px solid #ced4da; border-radius: 4px;"
            "  padding: 5px 12px; background: #fff; min-height: 22px;"
            "  text-align: left;"
            "}"
            "QToolButton:hover { background: #f1f3f5; }"
            "QToolButton::menu-indicator { right: 6px; }");
        m_reminderMenu = new QMenu(m_reminderBtn);
        struct OffsetItem { int min; QString label; };
        QList<OffsetItem> offsets = {
            {0,        tr("截止时刻")},
            {5,        tr("提前 5 分钟")},
            {15,       tr("提前 15 分钟")},
            {30,       tr("提前 30 分钟")},
            {60,       tr("提前 1 小时")},
            {60 * 3,   tr("提前 3 小时")},
            {60 * 24,  tr("提前 1 天")},
            {60 * 24 * 3, tr("提前 3 天")},
        };
        for (const auto& o : offsets) {
            QAction* a = m_reminderMenu->addAction(o.label);
            a->setCheckable(true);
            a->setData(o.min);
        }
        // 任一项勾选状态变化都更新按钮文字
        connect(m_reminderMenu, &QMenu::triggered, this, [this](QAction*){
            QList<int> sel;
            for (QAction* a : m_reminderMenu->actions())
                if (a->isChecked()) sel << a->data().toInt();
            m_reminderBtn->setText(reminderSummary(sel));
        });
        m_reminderBtn->setMenu(m_reminderMenu);
        remRow->addWidget(m_reminderBtn, 1);
        col->addLayout(remRow);

        // 操作
        auto* btnRow = new QHBoxLayout();
        m_saveDetailBtn = new QPushButton(tr("💾 保存"));
        m_saveDetailBtn->setObjectName("primaryBtn");
        m_deleteBtn = new QPushButton(tr("🗑 删除"));
        m_deleteBtn->setObjectName("dangerBtn");
        btnRow->addWidget(m_saveDetailBtn);
        btnRow->addWidget(m_deleteBtn);
        btnRow->addStretch();
        col->addLayout(btnRow);
        col->addStretch();

        split->addWidget(m_detailWidget);
        split->setStretchFactor(0, 1);
        split->setStretchFactor(1, 1);
        split->setSizes({500, 380});
        h->addWidget(split);
        m_stack->addWidget(page);
    }

    // 页 1：统计视图
    {
        m_statsView = new StatsView();
        auto* card = new QFrame();
        card->setObjectName("card");
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(0, 0, 0, 0);
        v->addWidget(m_statsView);
        m_stack->addWidget(card);
    }

    main->addWidget(m_stack, 1);

    clearDetail();

    connect(m_addBtn, &QPushButton::clicked, this, &Todo::onAddTask);
    connect(m_saveDetailBtn, &QPushButton::clicked, this, &Todo::onSaveDetail);
    connect(m_deleteBtn, &QPushButton::clicked, this, &Todo::onDeleteTask);
    connect(m_list, &QListWidget::currentItemChanged, this, [this](){ onTaskSelected(); });
    connect(m_list, &QListWidget::itemChanged, this, &Todo::onCompletedToggled);
    connect(m_filterAllBtn,   &QPushButton::clicked, this, [this](){ m_filter = FilterAll;   onFilterChanged(); });
    connect(m_filterTodayBtn, &QPushButton::clicked, this, [this](){ m_filter = FilterToday; onFilterChanged(); });
    connect(m_filterWeekBtn,  &QPushButton::clicked, this, [this](){ m_filter = FilterWeek;  onFilterChanged(); });
    connect(m_filterDoneBtn,  &QPushButton::clicked, this, [this](){ m_filter = FilterDone;  onFilterChanged(); });
    connect(m_searchEdit, &QLineEdit::textChanged, this, &Todo::onSearchChanged);
    connect(m_statsBtn, &QPushButton::clicked, this, &Todo::onShowStats);
    connect(m_exportPdfBtn, &QPushButton::clicked, this, &Todo::onExportPdf);
}

// ============= DB =============

void Todo::initDb()
{
    m_db = SqliteWrapper::SqliteManager::getDefaultInstance();
    if (!m_db) {
        m_statusLabel->setText(tr("数据库不可用"));
        return;
    }
    m_db->execute(kSql_Create);

    // schema 迁移：补齐多提醒所需的两列
    auto info = m_db->select("PRAGMA table_info(todo_tasks)");
    QSet<QString> cols;
    if (info.success) {
        for (const QVariant& row : info.data)
            cols.insert(row.toMap()["name"].toString());
    }
    if (!cols.contains("reminder_offsets")) {
        m_db->execute(
            "ALTER TABLE todo_tasks ADD COLUMN reminder_offsets TEXT DEFAULT ''");
    }
    if (!cols.contains("reminded_offsets")) {
        m_db->execute(
            "ALTER TABLE todo_tasks ADD COLUMN reminded_offsets TEXT DEFAULT ''");
    }
}

// ============= 列表渲染 =============

void Todo::reloadList()
{
    if (!m_db) return;

    QString where;
    QString keyword = m_searchEdit->text().trimmed();
    QStringList conds;
    QVariantMap params;

    QDateTime now = QDateTime::currentDateTime();
    QDate today = now.date();
    switch (m_filter) {
        case FilterToday:
            conds << "completed = 0";
            conds << "deadline IS NOT NULL";
            conds << "DATE(deadline) <= :today";
            params["today"] = today.toString(Qt::ISODate);
            break;
        case FilterWeek:
            conds << "completed = 0";
            conds << "deadline IS NOT NULL";
            conds << "DATE(deadline) <= :weekend";
            params["weekend"] = today.addDays(7 - today.dayOfWeek()).toString(Qt::ISODate);
            break;
        case FilterDone:
            conds << "completed = 1";
            break;
        case FilterAll:
        default:
            break;
    }
    if (!keyword.isEmpty()) {
        conds << "(title LIKE :kw OR notes LIKE :kw OR category LIKE :kw)";
        params["kw"] = "%" + keyword + "%";
    }
    if (!conds.isEmpty()) where = "WHERE " + conds.join(" AND ");

    QString sql = "SELECT id, title, notes, deadline, completed, completed_at, "
                  "priority, category, reminder_minutes_before, reminded, "
                  "reminder_offsets, reminded_offsets, created_at "
                  "FROM todo_tasks " + where +
                  " ORDER BY completed ASC, "
                  "  CASE WHEN deadline IS NULL THEN 1 ELSE 0 END, "
                  "  deadline ASC, priority DESC, id DESC";
    auto res = m_db->select(sql, params);
    if (!res.success) {
        m_statusLabel->setText(tr("查询失败：%1").arg(res.errorMessage));
        return;
    }

    m_cache.clear();
    for (const QVariant& row : res.data)
        m_cache.append(rowToItem(row.toMap()));

    m_list->blockSignals(true);
    m_list->clear();
    for (const TodoItem& t : m_cache) {
        auto* it = new QListWidgetItem();
        it->setData(Qt::UserRole, qint64(t.id));
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(t.completed ? Qt::Checked : Qt::Unchecked);
        renderItem(it, t);
        m_list->addItem(it);
    }
    m_list->blockSignals(false);

    int pending = 0, done = 0;
    for (const auto& t : m_cache) (t.completed ? done : pending)++;
    m_statusLabel->setText(tr("待完成 %1，已完成 %2").arg(pending).arg(done));
}

void Todo::renderItem(QListWidgetItem* item, const TodoItem& t)
{
    QString line1 = t.title.isEmpty() ? tr("(无标题)") : t.title;

    QStringList tags;
    if (t.deadline.isValid()) tags << "⏰ " + humanDeadline(t.deadline);
    if (t.priority > 0) tags << "🚩 " + priorityName(t.priority);
    if (!t.category.isEmpty()) tags << "🏷 " + t.category;

    QString line2 = tags.join("    ");
    QString text = line2.isEmpty() ? line1 : line1 + "\n" + line2;
    item->setText(text);

    QFont f = m_list->font();
    if (t.completed) {
        f.setStrikeOut(true);
        item->setForeground(QBrush(QColor(0xad, 0xb5, 0xbd)));
    } else {
        f.setStrikeOut(false);
        item->setForeground(QBrush(QColor(0x21, 0x25, 0x29)));
    }
    item->setFont(f);

    // 逾期未完成：左侧加红色标记（用 background gradient 效果模拟）
    if (!t.completed && t.deadline.isValid()
        && t.deadline < QDateTime::currentDateTime()) {
        item->setBackground(QBrush(QColor(0xff, 0xeb, 0xee)));
    } else if (!t.completed && t.priority > 0) {
        QColor c = priorityColor(t.priority);
        c.setAlpha(28);
        item->setBackground(QBrush(c));
    } else {
        item->setBackground(QBrush(Qt::transparent));
    }
}

// ============= 增删改 =============

void Todo::onAddTask()
{
    if (!m_db) return;
    QVariantMap data;
    data["title"] = tr("新任务");
    auto res = m_db->insert("todo_tasks", data);
    if (!res.success) {
        m_statusLabel->setText(tr("新建失败：%1").arg(res.errorMessage));
        return;
    }
    reloadList();
    // 选中刚加的项
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->data(Qt::UserRole).toLongLong() == res.lastInsertId) {
            m_list->setCurrentRow(i);
            break;
        }
    }
    m_titleEdit->setFocus();
    m_titleEdit->selectAll();
}

void Todo::onSaveDetail()
{
    if (!m_db || m_currentId == 0) return;
    QVariantMap data;
    data["title"] = m_titleEdit->text();
    data["notes"] = m_notesEdit->toPlainText();
    if (m_hasDeadlineCheck->isChecked()) {
        data["deadline"] = m_deadlineEdit->dateTime().toString(Qt::ISODate);
    } else {
        data["deadline"] = QVariant();
    }
    data["priority"] = m_priorityCombo->currentData().toInt();
    data["category"] = m_categoryEdit->text();

    // 多选提醒
    QList<int> offsets;
    if (m_reminderMenu) {
        for (QAction* a : m_reminderMenu->actions())
            if (a->isChecked()) offsets << a->data().toInt();
    }
    std::sort(offsets.begin(), offsets.end());
    data["reminder_offsets"] = joinOffsets(offsets);
    data["reminder_minutes_before"] = offsets.isEmpty() ? 0 : offsets.first();  // 保留旧字段
    // 保存即重置已提醒标记
    data["reminded"] = 0;
    data["reminded_offsets"] = "";
    QVariantMap whereParams; whereParams["id"] = m_currentId;
    auto res = m_db->update("todo_tasks", data, "id = :id", whereParams);
    if (!res.success) {
        m_statusLabel->setText(tr("保存失败：%1").arg(res.errorMessage));
        return;
    }
    reloadList();
    // 重新选回
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->data(Qt::UserRole).toLongLong() == m_currentId) {
            m_list->setCurrentRow(i);
            break;
        }
    }
    m_statusLabel->setText(tr("已保存"));
}

void Todo::onDeleteTask()
{
    if (!m_db || m_currentId == 0) return;
    if (QMessageBox::question(this, tr("确认删除"),
                              tr("确定删除这个任务吗？"))
        != QMessageBox::Yes) return;
    QVariantMap p; p["id"] = m_currentId;
    m_db->remove("todo_tasks", "id = :id", p);
    m_currentId = 0;
    clearDetail();
    reloadList();
}

void Todo::onTaskSelected()
{
    auto* it = m_list->currentItem();
    if (!it) { clearDetail(); return; }
    qint64 id = it->data(Qt::UserRole).toLongLong();
    for (const TodoItem& t : m_cache) {
        if (t.id == id) {
            m_currentId = id;
            loadDetail(t);
            break;
        }
    }
}

void Todo::onCompletedToggled(QListWidgetItem* item)
{
    if (!m_db || !item) return;
    qint64 id = item->data(Qt::UserRole).toLongLong();
    bool done = item->checkState() == Qt::Checked;
    QVariantMap data;
    data["completed"] = done ? 1 : 0;
    data["completed_at"] = done ? QDateTime::currentDateTime().toString(Qt::ISODate)
                                : QVariant();
    QVariantMap p; p["id"] = id;
    m_db->update("todo_tasks", data, "id = :id", p);
    reloadList();
}

void Todo::onFilterChanged()
{
    m_filterAllBtn->setChecked(m_filter == FilterAll);
    m_filterTodayBtn->setChecked(m_filter == FilterToday);
    m_filterWeekBtn->setChecked(m_filter == FilterWeek);
    m_filterDoneBtn->setChecked(m_filter == FilterDone);
    m_statsBtn->setChecked(false);
    m_stack->setCurrentIndex(0);
    reloadList();
}

void Todo::onSearchChanged() { reloadList(); }

// ============= 详情 =============

void Todo::clearDetail()
{
    m_currentId = 0;
    m_titleEdit->clear();
    m_notesEdit->clear();
    m_hasDeadlineCheck->setChecked(false);
    m_deadlineEdit->setDateTime(QDateTime::currentDateTime().addDays(1));
    m_priorityCombo->setCurrentIndex(0);
    m_categoryEdit->clear();
    if (m_reminderMenu) {
        for (QAction* a : m_reminderMenu->actions()) a->setChecked(false);
        m_reminderBtn->setText(tr("不提醒"));
    }
    m_saveDetailBtn->setEnabled(false);
    m_deleteBtn->setEnabled(false);
    m_titleEdit->setEnabled(false);
    m_notesEdit->setEnabled(false);
    m_hasDeadlineCheck->setEnabled(false);
    m_deadlineEdit->setEnabled(false);
    m_priorityCombo->setEnabled(false);
    m_categoryEdit->setEnabled(false);
    if (m_reminderBtn) m_reminderBtn->setEnabled(false);
}

void Todo::loadDetail(const TodoItem& t)
{
    m_titleEdit->setText(t.title);
    m_notesEdit->setPlainText(t.notes);
    m_hasDeadlineCheck->setChecked(t.deadline.isValid());
    if (t.deadline.isValid()) m_deadlineEdit->setDateTime(t.deadline);
    m_priorityCombo->setCurrentIndex(qBound(0, t.priority, m_priorityCombo->count() - 1));
    m_categoryEdit->setText(t.category);

    // 同步多选提醒菜单
    if (m_reminderMenu) {
        QSet<int> chosen(t.reminderOffsets.begin(), t.reminderOffsets.end());
        for (QAction* a : m_reminderMenu->actions())
            a->setChecked(chosen.contains(a->data().toInt()));
        m_reminderBtn->setText(reminderSummary(t.reminderOffsets));
    }

    m_titleEdit->setEnabled(true);
    m_notesEdit->setEnabled(true);
    m_hasDeadlineCheck->setEnabled(true);
    m_deadlineEdit->setEnabled(t.deadline.isValid());
    m_priorityCombo->setEnabled(true);
    m_categoryEdit->setEnabled(true);
    if (m_reminderBtn) m_reminderBtn->setEnabled(true);
    m_saveDetailBtn->setEnabled(true);
    m_deleteBtn->setEnabled(true);
}

// ============= 提醒 =============

void Todo::notifyDeadline(const TodoItem& t)
{
    QString title = tr("Todo 提醒");
    QString body;
    if (!t.deadline.isValid()) body = t.title;
    else {
        body = QString("%1\n⏰ %2").arg(t.title, t.deadline.toString("yyyy-MM-dd HH:mm"));
    }

    if (m_tray) {
        m_tray->showMessage(title, body, QSystemTrayIcon::Information, 8000);
    } else {
        // 兜底：只在工具仍然可见时弹消息框
        if (isVisible()) {
            QMessageBox::information(this, title, body);
        }
    }
}

void Todo::onReminderTick()
{
    if (!m_db) return;
    QDateTime now = QDateTime::currentDateTime();
    auto res = m_db->select(
        "SELECT id, title, notes, deadline, completed, completed_at, "
        "priority, category, reminder_minutes_before, reminded, "
        "reminder_offsets, reminded_offsets, created_at "
        "FROM todo_tasks "
        "WHERE completed = 0 AND deadline IS NOT NULL "
        "  AND (reminder_offsets IS NOT NULL AND reminder_offsets != '' "
        "       OR reminder_minutes_before > 0)",
        {});
    if (!res.success) return;
    for (const QVariant& row : res.data) {
        TodoItem t = rowToItem(row.toMap());
        if (!t.deadline.isValid()) continue;
        if (t.reminderOffsets.isEmpty()) continue;

        // 找出"该触发但还没触发"的偏移
        QList<int> newlyFiredOffsets;
        for (int offset : t.reminderOffsets) {
            if (t.remindedOffsets.contains(offset)) continue;
            QDateTime fireAt = t.deadline.addSecs(-60 * offset);
            if (now >= fireAt) newlyFiredOffsets << offset;
        }
        if (newlyFiredOffsets.isEmpty()) continue;

        // 弹一次通知（合并提示）
        notifyDeadline(t);

        // 标记为已触发
        QList<int> updated = t.remindedOffsets;
        for (int v : newlyFiredOffsets) updated << v;
        QVariantMap data;
        data["reminded_offsets"] = joinOffsets(updated);
        QVariantMap p; p["id"] = t.id;
        m_db->update("todo_tasks", data, "id = :id", p);
    }
}

// ============= 统计 =============

void Todo::onShowStats()
{
    bool show = m_statsBtn->isChecked();
    m_stack->setCurrentIndex(show ? 1 : 0);
    if (!show) return;
    if (!m_db || !m_statsView) return;

    QDate today = QDate::currentDate();
    QDate from = today.addDays(-6);  // 最近 7 天

    QVariantMap p;
    p["from"] = from.toString(Qt::ISODate);
    auto res = m_db->select(
        "SELECT DATE(completed_at) AS d, COUNT(*) AS c "
        "FROM todo_tasks "
        "WHERE completed = 1 AND DATE(completed_at) >= :from "
        "GROUP BY DATE(completed_at)",
        p);

    QMap<QDate, int> countByDate;
    if (res.success) {
        for (const QVariant& row : res.data) {
            QVariantMap m = row.toMap();
            QDate d = QDate::fromString(m["d"].toString(), Qt::ISODate);
            countByDate[d] = m["c"].toInt();
        }
    }

    QList<StatsView::DayBucket> buckets;
    int doneTotal = 0;
    for (int i = 0; i < 7; ++i) {
        QDate d = from.addDays(i);
        StatsView::DayBucket b;
        b.date = d;
        b.count = countByDate.value(d, 0);
        doneTotal += b.count;
        buckets.append(b);
    }

    auto pr = m_db->select(
        "SELECT COUNT(*) AS c FROM todo_tasks WHERE completed = 0", {});
    int pending = 0;
    if (pr.success && !pr.data.isEmpty()) pending = pr.data.first().toMap()["c"].toInt();

    m_statsView->setData(doneTotal, pending, buckets, {});
}

// ============= PDF 导出 =============

void Todo::onExportPdf()
{
    if (!m_db) return;
    // 取本周（周一到周日）
    QDate today = QDate::currentDate();
    QDate weekStart = today.addDays(1 - today.dayOfWeek());
    QDate weekEnd = weekStart.addDays(6);

    QVariantMap p;
    p["from"] = weekStart.toString(Qt::ISODate);
    p["to"] = weekEnd.toString(Qt::ISODate);
    // 本周完成或本周到期的任务
    auto res = m_db->select(
        "SELECT id, title, notes, deadline, completed, completed_at, "
        "priority, category, reminder_minutes_before, reminded, created_at "
        "FROM todo_tasks "
        "WHERE (completed = 1 AND DATE(completed_at) >= :from AND DATE(completed_at) <= :to) "
        "   OR (completed = 0 AND deadline IS NOT NULL "
        "       AND DATE(deadline) >= :from AND DATE(deadline) <= :to) "
        "ORDER BY completed ASC, COALESCE(completed_at, deadline) ASC",
        p);
    if (!res.success) {
        QMessageBox::warning(this, tr("导出失败"), res.errorMessage);
        return;
    }
    QList<TodoItem> items;
    for (const QVariant& row : res.data) items.append(rowToItem(row.toMap()));

    if (items.isEmpty()) {
        QMessageBox::information(this, tr("提示"),
                                 tr("本周（%1 至 %2）暂无任务。")
                                     .arg(weekStart.toString("M-d"))
                                     .arg(weekEnd.toString("M-d")));
        return;
    }

    QString defaultPath =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +
        QString("/Todo_%1.pdf").arg(weekStart.toString("yyyyMMdd"));
    QString savePath = QFileDialog::getSaveFileName(
        this, tr("导出本周事项"), defaultPath, tr("PDF (*.pdf)"));
    if (savePath.isEmpty()) return;

    // 构建 HTML
    int doneCount = 0, pendingCount = 0;
    for (const auto& t : items) (t.completed ? doneCount : pendingCount)++;

    QString html;
    html += QString(R"(<style>
        body { font-family: -apple-system, "PingFang SC", "Microsoft YaHei", sans-serif;
               font-size: 11pt; color: #212529; line-height: 1.5; }
        h1 { font-size: 22pt; border-bottom: 2px solid #dee2e6; padding-bottom: 6px; }
        h2 { font-size: 14pt; color: #1864ab; margin-top: 16px; border-bottom: 1px solid #e9ecef; padding-bottom: 4px; }
        .summary { background: #f1f3f5; padding: 8px 12px; border-radius: 6px; margin: 8px 0; }
        table { border-collapse: collapse; width: 100%%; margin: 8px 0; }
        th, td { border: 1px solid #dee2e6; padding: 6px 10px; text-align: left; }
        th { background: #f8f9fa; }
        .done { color: #2e7d32; }
        .pending { color: #c62828; }
        .priority-high { color: #f08c00; font-weight: bold; }
        .priority-urgent { color: #c92a2a; font-weight: bold; }
    </style>)");

    html += QString("<h1>本周待办 · %1 ~ %2</h1>")
                .arg(weekStart.toString("yyyy-MM-dd"))
                .arg(weekEnd.toString("yyyy-MM-dd"));
    html += QString("<div class='summary'>共 <b>%1</b> 项 — "
                    "<span class='done'>已完成 %2</span>，"
                    "<span class='pending'>待完成 %3</span></div>")
                .arg(items.size()).arg(doneCount).arg(pendingCount);

    auto rowsHtml = [](const QList<TodoItem>& list) {
        QString out;
        out += "<table><tr><th>状态</th><th>标题</th><th>截止 / 完成时间</th><th>分类</th><th>优先级</th></tr>";
        for (const auto& t : list) {
            QString status = t.completed
                                 ? "<span class='done'>✓ 已完成</span>"
                                 : "<span class='pending'>○ 待办</span>";
            QString when = t.completed && t.completedAt.isValid()
                               ? t.completedAt.toString("M-d HH:mm")
                               : (t.deadline.isValid() ? t.deadline.toString("M-d HH:mm") : "-");
            QString prClass = t.priority == 2 ? "priority-urgent"
                              : t.priority == 1 ? "priority-high" : "";
            QString prText = priorityName(t.priority);
            out += QString("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td class='%5'>%6</td></tr>")
                       .arg(status, t.title.toHtmlEscaped(), when,
                            t.category.toHtmlEscaped(), prClass, prText);
        }
        out += "</table>";
        return out;
    };

    QList<TodoItem> done, pending;
    for (const auto& t : items) (t.completed ? done : pending).append(t);

    if (!done.isEmpty()) {
        html += "<h2>已完成（" + QString::number(done.size()) + "）</h2>";
        html += rowsHtml(done);
    }
    if (!pending.isEmpty()) {
        html += "<h2>待完成（" + QString::number(pending.size()) + "）</h2>";
        html += rowsHtml(pending);
    }

    // 写 PDF
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(savePath);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageMargins(QMarginsF(20, 20, 20, 20), QPageLayout::Millimeter);

    QTextDocument doc;
    doc.documentLayout()->setPaintDevice(&printer);   // 关键：pt → 像素 用 printer 真实 DPI
    doc.setHtml(html);
    doc.setPageSize(printer.pageRect(QPrinter::DevicePixel).size());
    doc.print(&printer);

    QMessageBox box(this);
    box.setWindowTitle(tr("导出完成"));
    box.setText(tr("PDF 已保存:\n%1").arg(savePath));
    box.setIcon(QMessageBox::Information);
    auto* openBtn = box.addButton(tr("打开 PDF"), QMessageBox::ActionRole);
    auto* dirBtn = box.addButton(tr("打开所在目录"), QMessageBox::ActionRole);
    box.addButton(QMessageBox::Ok);
    box.exec();
    if (box.clickedButton() == openBtn)
        QDesktopServices::openUrl(QUrl::fromLocalFile(savePath));
    else if (box.clickedButton() == dirBtn)
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(savePath).absolutePath()));
}
