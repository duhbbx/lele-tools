#include "salarycalc.h"

#include "../../common/sqlite/SqliteManager.h"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMargins>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPaintEvent>
#include <QPdfWriter>
#include <QPrinter>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariantMap>
#include <QWheelEvent>

REGISTER_DYNAMICOBJECT(SalaryCalc);

namespace {

// 比例 / 金额输入框：
//   · 禁用滚轮调整（防止误触）
//   · 删除清空 → 失焦/回车后强制为下限（金额/比例 min=0 → 0；加班费率 min=1 → 1）
class NoWheelDoubleSpinBox : public QDoubleSpinBox
{
public:
    using QDoubleSpinBox::QDoubleSpinBox;
protected:
    void wheelEvent(QWheelEvent* e) override { e->ignore(); }

    void focusOutEvent(QFocusEvent* e) override {
        clampIfEmpty();
        QDoubleSpinBox::focusOutEvent(e);
    }
    void keyPressEvent(QKeyEvent* e) override {
        if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
            clampIfEmpty();
        }
        QDoubleSpinBox::keyPressEvent(e);
    }

private:
    void clampIfEmpty() {
        QLineEdit* le = lineEdit();
        if (!le) return;
        QString t = le->text();
        const QString p = prefix(), s = suffix();
        if (!p.isEmpty() && t.startsWith(p)) t.remove(0, p.size());
        if (!s.isEmpty() && t.endsWith(s)) t.chop(s.size());
        if (t.trimmed().isEmpty()) {
            // 直接 setValue → 触发 valueChanged + 刷新文本，绕过 Qt 私有解析路径
            setValue(minimum());
        }
    }
};

// 简易 FlowLayout（参考 Qt 官方 Flow Layout 例程）
// 子项按从左到右排，遇到右边界就换行 — 用在按钮行避免横向滚动条
class FlowLayout : public QLayout
{
public:
    explicit FlowLayout(int margin = 0, int hSpacing = 6, int vSpacing = 6)
        : m_hSpace(hSpacing), m_vSpace(vSpacing) {
        setContentsMargins(margin, margin, margin, margin);
    }
    ~FlowLayout() override {
        while (auto* item = takeAt(0)) delete item;
    }
    void addItem(QLayoutItem* item) override { m_items.append(item); }
    int count() const override { return m_items.size(); }
    QLayoutItem* itemAt(int i) const override { return m_items.value(i); }
    QLayoutItem* takeAt(int i) override {
        return (i >= 0 && i < m_items.size()) ? m_items.takeAt(i) : nullptr;
    }
    Qt::Orientations expandingDirections() const override { return {}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override { return doLayout(QRect(0, 0, w, 0), true); }
    QSize sizeHint() const override { return minimumSize(); }
    QSize minimumSize() const override {
        QSize s;
        for (auto* item : m_items) s = s.expandedTo(item->minimumSize());
        const QMargins m = contentsMargins();
        s += QSize(m.left() + m.right(), m.top() + m.bottom());
        return s;
    }
    void setGeometry(const QRect& r) override {
        QLayout::setGeometry(r);
        doLayout(r, false);
    }
private:
    int doLayout(const QRect& rect, bool testOnly) const {
        QMargins m = contentsMargins();
        QRect r = rect.adjusted(m.left(), m.top(), -m.right(), -m.bottom());
        int x = r.x(), y = r.y(), lineH = 0;
        for (auto* item : m_items) {
            QSize sz = item->sizeHint();
            int nx = x + sz.width() + m_hSpace;
            if (nx - m_hSpace > r.right() && lineH > 0) {
                x = r.x();
                y += lineH + m_vSpace;
                nx = x + sz.width() + m_hSpace;
                lineH = 0;
            }
            if (!testOnly) item->setGeometry(QRect(QPoint(x, y), sz));
            x = nx;
            lineH = qMax(lineH, sz.height());
        }
        return y + lineH - rect.y() + m.bottom();
    }
    QList<QLayoutItem*> m_items;
    int m_hSpace, m_vSpace;
};

// 中国大陆法定节假日 + 调休工作日 数据
// 格式: 年份 → { 节假日列表, 调休工作日列表 }
// 数据来源：
//   1. Hardcoded:           国务院已正式发布的安排（2024、2025）
//   2. HardcodedPredicted:  内置预估（如 2026 在国务院发布前），不可靠
//   3. OnlineCached:        通过 timor.tech 在线获取并缓存到 AppDataLocation
enum class HolidaySource { Hardcoded, HardcodedPredicted, OnlineCached };

struct YearHolidays {
    QHash<QDate, QString> holidays;       // 节假日 → 名称
    QHash<QDate, QString> makeupDays;     // 调休工作日 → 因什么节日调休
    HolidaySource source = HolidaySource::Hardcoded;
};

void addRange(QHash<QDate, QString>& target, const char* label,
              std::initializer_list<const char*> dates, int year)
{
    for (const char* s : dates) {
        QDate d = QDate::fromString(s, "yyyy-MM-dd");
        if (d.isValid() && d.year() == year) target.insert(d, label);
    }
}

YearHolidays defaultHolidaysForYear(int year)
{
    YearHolidays y;
    if (year == 2024) {
        y.source = HolidaySource::Hardcoded;
        addRange(y.holidays, "元旦", {"2024-01-01"}, 2024);
        addRange(y.holidays, "春节", {"2024-02-10","2024-02-11","2024-02-12","2024-02-13",
                                       "2024-02-14","2024-02-15","2024-02-16","2024-02-17"}, 2024);
        addRange(y.holidays, "清明", {"2024-04-04","2024-04-05","2024-04-06"}, 2024);
        addRange(y.holidays, "劳动节", {"2024-05-01","2024-05-02","2024-05-03","2024-05-04","2024-05-05"}, 2024);
        addRange(y.holidays, "端午", {"2024-06-08","2024-06-09","2024-06-10"}, 2024);
        addRange(y.holidays, "中秋", {"2024-09-15","2024-09-16","2024-09-17"}, 2024);
        addRange(y.holidays, "国庆", {"2024-10-01","2024-10-02","2024-10-03","2024-10-04",
                                       "2024-10-05","2024-10-06","2024-10-07"}, 2024);
        addRange(y.makeupDays, "调休·春节",   {"2024-02-04","2024-02-18"}, 2024);
        addRange(y.makeupDays, "调休·清明",   {"2024-04-07"}, 2024);
        addRange(y.makeupDays, "调休·劳动节", {"2024-04-28","2024-05-11"}, 2024);
        addRange(y.makeupDays, "调休·中秋",   {"2024-09-14"}, 2024);
        addRange(y.makeupDays, "调休·国庆",   {"2024-09-29","2024-10-12"}, 2024);
    } else if (year == 2025) {
        y.source = HolidaySource::Hardcoded;
        addRange(y.holidays, "元旦", {"2025-01-01"}, 2025);
        addRange(y.holidays, "春节", {"2025-01-28","2025-01-29","2025-01-30","2025-01-31",
                                       "2025-02-01","2025-02-02","2025-02-03","2025-02-04"}, 2025);
        addRange(y.holidays, "清明", {"2025-04-04","2025-04-05","2025-04-06"}, 2025);
        addRange(y.holidays, "劳动节", {"2025-05-01","2025-05-02","2025-05-03","2025-05-04","2025-05-05"}, 2025);
        addRange(y.holidays, "端午", {"2025-05-31","2025-06-01","2025-06-02"}, 2025);
        addRange(y.holidays, "国庆·中秋", {"2025-10-01","2025-10-02","2025-10-03","2025-10-04",
                                             "2025-10-05","2025-10-06","2025-10-07","2025-10-08"}, 2025);
        addRange(y.makeupDays, "调休·春节",   {"2025-01-26","2025-02-08"}, 2025);
        addRange(y.makeupDays, "调休·劳动节", {"2025-04-27"}, 2025);
        addRange(y.makeupDays, "调休·国庆",   {"2025-09-28","2025-10-11"}, 2025);
    } else if (year == 2026) {
        // 2026 内置数据为预估，仅作为离线回退；正常情况会被在线数据覆盖
        y.source = HolidaySource::HardcodedPredicted;
        addRange(y.holidays, "元旦", {"2026-01-01","2026-01-02","2026-01-03"}, 2026);
        addRange(y.holidays, "春节", {"2026-02-15","2026-02-16","2026-02-17","2026-02-18",
                                       "2026-02-19","2026-02-20","2026-02-21","2026-02-22"}, 2026);
        addRange(y.holidays, "清明", {"2026-04-04","2026-04-05","2026-04-06"}, 2026);
        addRange(y.holidays, "劳动节", {"2026-05-01","2026-05-02","2026-05-03"}, 2026);
        addRange(y.holidays, "端午", {"2026-06-19","2026-06-20","2026-06-21"}, 2026);
        addRange(y.holidays, "中秋", {"2026-09-25","2026-09-26","2026-09-27"}, 2026);
        addRange(y.holidays, "国庆", {"2026-10-01","2026-10-02","2026-10-03","2026-10-04",
                                       "2026-10-05","2026-10-06","2026-10-07"}, 2026);
        addRange(y.makeupDays, "调休·春节", {"2026-02-14","2026-02-28"}, 2026);
    }
    return y;
}

QHash<int, YearHolidays>& holidayDbMut()
{
    static QHash<int, YearHolidays> db;
    return db;
}

const YearHolidays* holidayDataFor(int year)
{
    auto& db = holidayDbMut();
    auto it = db.find(year);
    if (it == db.end()) {
        YearHolidays def = defaultHolidaysForYear(year);
        if (def.holidays.isEmpty() && def.makeupDays.isEmpty()) return nullptr;
        it = db.insert(year, def);
    }
    return &it.value();
}

QString dayKindLabel(DayKind k)
{
    switch (k) {
        case DayKind::Workday:        return QObject::tr("工作");
        case DayKind::Weekend:        return QObject::tr("周末");
        case DayKind::Holiday:        return QObject::tr("节假日");
        case DayKind::MakeupWorkday:  return QObject::tr("调休工作");
        case DayKind::Leave:          return QObject::tr("请假");
        case DayKind::Overtime:       return QObject::tr("加班");
    }
    return {};
}

// 中国月度个税（2019 起综合所得 — 这里按"月度近似"算法，
// 用于直观展示，未做累计预扣预缴）
double calcMonthlyTax(double taxable)
{
    if (taxable <= 0) return 0;
    // 月度税率表（按年税率 / 12 推算的近似分档）
    struct Bracket { double upper; double rate; double quick; };
    static const Bracket b[] = {
        {     3000, 0.03, 0      },
        {    12000, 0.10, 210    },
        {    25000, 0.20, 1410   },
        {    35000, 0.25, 2660   },
        {    55000, 0.30, 4410   },
        {    80000, 0.35, 7160   },
        {1.0e12,    0.45, 15160  },
    };
    for (const auto& x : b) {
        if (taxable <= x.upper) return taxable * x.rate - x.quick;
    }
    return 0;
}

} // namespace


// ============= CalendarWidget =============

CalendarWidget::CalendarWidget(QWidget* parent) : QWidget(parent)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    // 高度按行数确定，不随父布局拉伸（让出垂直空间给设置/明细面板）
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

QSize CalendarWidget::sizeHint() const
{
    const int headerH = 26;
    const int cellH = 56;
    int rows = (m_year && m_month) ? rowsNeeded() : 6;
    return {560, headerH + rows * cellH};
}

void CalendarWidget::setMonth(int year, int month)
{
    m_year = year;
    m_month = month;
    QDate first(year, month, 1);
    int dow = first.dayOfWeek();   // 1=Mon..7=Sun
    m_firstCell = first.addDays(-(dow - 1));
    updateGeometry();
    update();
}

void CalendarWidget::setBaseKind(const QDate& d, DayKind k)
{
    m_baseKinds[d] = k;
    update();
}

void CalendarWidget::setBaseLabel(const QDate& d, const QString& label)
{
    m_baseLabels[d] = label;
    update();
}

void CalendarWidget::setActiveRange(const QDate& start, const QDate& end)
{
    m_rangeStart = start;
    m_rangeEnd = end;
    update();
}

void CalendarWidget::clearActiveRange()
{
    m_rangeStart = QDate();
    m_rangeEnd = QDate();
    update();
}

bool CalendarWidget::inActiveRange(const QDate& d) const
{
    if (!m_rangeStart.isValid() || !m_rangeEnd.isValid()) return true;
    return d >= m_rangeStart && d <= m_rangeEnd;
}

void CalendarWidget::setUserMark(const QDate& d, DayKind k)
{
    m_userMarks[d] = k;
    update();
}

void CalendarWidget::clearUserMark(const QDate& d)
{
    m_userMarks.remove(d);
    update();
}

DayKind CalendarWidget::effectiveKind(const QDate& d) const
{
    if (m_userMarks.contains(d)) return m_userMarks[d];
    if (m_baseKinds.contains(d)) return m_baseKinds[d];
    int dow = d.dayOfWeek();
    return (dow >= 6) ? DayKind::Weekend : DayKind::Workday;
}

bool CalendarWidget::dateInMonth(const QDate& d) const
{
    return d.year() == m_year && d.month() == m_month;
}

int CalendarWidget::rowsNeeded() const
{
    QDate last(m_year, m_month, QDate(m_year, m_month, 1).daysInMonth());
    int days = m_firstCell.daysTo(last) + 1;
    return (days + 6) / 7;
}

QRect CalendarWidget::cellRect(int row, int col) const
{
    const int headerH = 26;
    int rows = rowsNeeded();
    int cellW = width() / 7;
    int cellH = (height() - headerH) / qMax(1, rows);
    return QRect(col * cellW, headerH + row * cellH, cellW, cellH);
}

QDate CalendarWidget::cellDate(int row, int col) const
{
    return m_firstCell.addDays(row * 7 + col);
}

QPair<int, int> CalendarWidget::dateToCell(const QDate& d) const
{
    int days = m_firstCell.daysTo(d);
    return {days / 7, days % 7};
}

void CalendarWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0xff, 0xff, 0xff));
    p.setRenderHint(QPainter::Antialiasing, true);

    static const QStringList weekHead = {
        QObject::tr("一"), QObject::tr("二"), QObject::tr("三"),
        QObject::tr("四"), QObject::tr("五"), QObject::tr("六"), QObject::tr("日")
    };

    // 表头
    QFont headerFont = font();
    headerFont.setBold(true);
    p.setFont(headerFont);
    int cellW = width() / 7;
    for (int c = 0; c < 7; ++c) {
        QRect h(c * cellW, 0, cellW, 26);
        p.fillRect(h, QColor(0xf8, 0xf9, 0xfa));
        p.setPen(c >= 5 ? QColor(0xc6, 0x28, 0x28) : QColor(0x49, 0x50, 0x57));
        p.drawText(h, Qt::AlignCenter, weekHead[c]);
    }
    p.setPen(QColor(0xde, 0xe2, 0xe6));
    p.drawLine(0, 26, width(), 26);

    int rows = rowsNeeded();
    QFont bigFont = font();
    bigFont.setPointSizeF(bigFont.pointSizeF() * 1.4);
    QFont labelFont = font();
    labelFont.setPointSizeF(labelFont.pointSizeF() * 0.85);

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < 7; ++c) {
            QDate d = cellDate(r, c);
            QRect cr = cellRect(r, c);
            bool inMonth = dateInMonth(d);
            DayKind k = effectiveKind(d);

            // 背景色
            QColor bg = QColor(0xff, 0xff, 0xff);
            switch (k) {
                case DayKind::Holiday:        bg = QColor(0xfe, 0xe2, 0xe2); break;  // 浅红
                case DayKind::MakeupWorkday:  bg = QColor(0xff, 0xf3, 0xcd); break;  // 浅黄
                case DayKind::Weekend:        bg = QColor(0xf1, 0xf3, 0xf5); break;  // 浅灰
                case DayKind::Leave:          bg = QColor(0xff, 0xeb, 0xee); break;  // 粉红
                case DayKind::Overtime:       bg = QColor(0xd0, 0xeb, 0xff); break;  // 浅蓝
                case DayKind::Workday:        bg = QColor(0xff, 0xff, 0xff); break;
            }
            if (!inMonth) bg = QColor(0xfa, 0xfb, 0xfc);
            p.fillRect(cr, bg);

            // 边框
            p.setPen(QColor(0xe9, 0xec, 0xef));
            p.drawRect(cr.adjusted(0, 0, -1, -1));

            // 日期数字
            QColor numColor = inMonth ? QColor(0x21, 0x25, 0x29) : QColor(0xad, 0xb5, 0xbd);
            if (inMonth && (k == DayKind::Holiday)) numColor = QColor(0xc6, 0x28, 0x28);
            else if (inMonth && c >= 5 && k == DayKind::Weekend) numColor = QColor(0xc6, 0x28, 0x28);
            p.setPen(numColor);
            p.setFont(bigFont);
            p.drawText(cr.adjusted(8, 4, -8, -8), Qt::AlignTop | Qt::AlignLeft,
                       QString::number(d.day()));

            // 标签：优先用自定义标签（节日名），否则按 kind 默认
            if (inMonth) {
                p.setFont(labelFont);
                QColor lblColor = QColor(0x49, 0x50, 0x57);
                if (k == DayKind::Holiday) lblColor = QColor(0xc6, 0x28, 0x28);
                else if (k == DayKind::Leave) lblColor = QColor(0xe0, 0x3d, 0x6c);
                else if (k == DayKind::Overtime) lblColor = QColor(0x19, 0x71, 0xc2);
                else if (k == DayKind::MakeupWorkday) lblColor = QColor(0xb5, 0x8a, 0x05);
                p.setPen(lblColor);
                QString lbl = m_baseLabels.contains(d) ? m_baseLabels[d] : dayKindLabel(k);
                // 用户标记覆盖：请假/加班 用动作标签
                if (m_userMarks.contains(d)) lbl = dayKindLabel(k);
                p.drawText(cr.adjusted(2, 0, -4, -4),
                           Qt::AlignBottom | Qt::AlignRight,
                           p.fontMetrics().elidedText(lbl, Qt::ElideRight, cr.width() - 8));
            }

            // 不在工作时间区间：覆盖一层灰色蒙层（视觉表示"不计入工资"）
            if (inMonth && !inActiveRange(d)) {
                p.fillRect(cr.adjusted(0, 0, -1, -1), QColor(0, 0, 0, 35));
            }

            p.setFont(font());
        }
    }
}

void CalendarWidget::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        if (e->position().y() < 26) return;
        int rows = rowsNeeded();
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < 7; ++c) {
                if (cellRect(r, c).contains(e->position().toPoint())) {
                    QDate d = cellDate(r, c);
                    if (dateInMonth(d)) emit dayClicked(d);
                    return;
                }
            }
        }
    }
}

void CalendarWidget::contextMenuEvent(QContextMenuEvent* e)
{
    if (e->y() < 26) return;
    int rows = rowsNeeded();
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < 7; ++c) {
            if (cellRect(r, c).contains(e->pos())) {
                QDate d = cellDate(r, c);
                if (dateInMonth(d)) emit dayContextMenu(d, e->globalPos());
                return;
            }
        }
    }
}


// ============= SalaryCalc =============

SalaryCalc::SalaryCalc() : QWidget(nullptr), DynamicObjectBase()
{
    setupUI();
    initDb();
    loadSettings();

    QDate now = QDate::currentDate();
    m_yearSpin->setValue(now.year());
    m_monthSpin->setValue(now.month());
    onYearMonthChanged();
}

SalaryCalc::~SalaryCalc() = default;

void SalaryCalc::setupUI()
{
    setStyleSheet(R"(
        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            border: 1px solid #ced4da; border-radius: 4px;
            padding: 5px 8px; background: #fff; min-height: 22px;
            selection-background-color: #b8d4ff;
        }
        QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus { border-color: #228be6; }
        QPushButton {
            border: 1px solid #ced4da; border-radius: 4px;
            padding: 5px 14px; background: #ffffff; min-height: 22px; color: #343a40;
        }
        QPushButton:hover  { background: #f1f3f5; border-color: #adb5bd; }
        QPushButton:pressed{ background: #e9ecef; }
        QPushButton#primaryBtn {
            background: #228be6; border: 1px solid #1c7ed6; color: white; font-weight: bold;
        }
        QPushButton#primaryBtn:hover  { background: #1c7ed6; }
        QFrame#card {
            background: #f8f9fa; border: 1px solid #dee2e6; border-radius: 6px;
        }
        QLabel { background: transparent; }
        QLabel.statBig { font-size: 16pt; font-weight: bold; color: #1864ab; }
        QLabel.statSmall { font-size: 9pt; color: #868e96; }
    )");

    auto* main = new QVBoxLayout(this);
    main->setContentsMargins(12, 10, 12, 10);
    main->setSpacing(10);

    // 顶部：年月 + 操作
    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(6);
    topRow->addWidget(new QLabel(tr("年")));
    m_yearSpin = new QSpinBox();
    m_yearSpin->setRange(2020, 2030);
    m_yearSpin->setValue(QDate::currentDate().year());
    topRow->addWidget(m_yearSpin);
    topRow->addWidget(new QLabel(tr("月")));
    m_monthSpin = new QSpinBox();
    m_monthSpin->setRange(1, 12);
    m_monthSpin->setValue(QDate::currentDate().month());
    topRow->addWidget(m_monthSpin);

    topRow->addSpacing(16);
    m_rangeLimitCheck = new QCheckBox(tr("只计算"));
    m_rangeLimitCheck->setToolTip(tr("勾选后只把指定日期范围内的天数计入工资 — 适合月中入职/离职"));
    topRow->addWidget(m_rangeLimitCheck);
    m_rangeStartDay = new QSpinBox();
    m_rangeStartDay->setRange(1, 31);
    m_rangeStartDay->setValue(1);
    m_rangeStartDay->setSuffix(tr(" 日"));
    m_rangeStartDay->setEnabled(false);
    topRow->addWidget(m_rangeStartDay);
    topRow->addWidget(new QLabel(tr("~")));
    m_rangeEndDay = new QSpinBox();
    m_rangeEndDay->setRange(1, 31);
    m_rangeEndDay->setValue(31);
    m_rangeEndDay->setSuffix(tr(" 日"));
    m_rangeEndDay->setEnabled(false);
    topRow->addWidget(m_rangeEndDay);

    topRow->addSpacing(16);
    auto legendLabel = new QLabel(tr(
        "<span style='background:#fee2e2;padding:2px 6px;border-radius:3px;'>节日</span>  "
        "<span style='background:#fff3cd;padding:2px 6px;border-radius:3px;'>调休</span>  "
        "<span style='background:#f1f3f5;padding:2px 6px;border-radius:3px;'>周末</span>  "
        "<span style='background:#ffebee;padding:2px 6px;border-radius:3px;color:#e03d6c;'>请假</span>  "
        "<span style='background:#d0ebff;padding:2px 6px;border-radius:3px;color:#1971c2;'>加班</span>"));
    topRow->addWidget(legendLabel);
    topRow->addStretch();

    // 范围控件信号
    auto applyRange = [this]() {
        if (m_rangeLimitCheck->isChecked()) {
            int y = m_yearSpin->value();
            int mo = m_monthSpin->value();
            QDate first(y, mo, 1);
            int dim = first.daysInMonth();
            int s = qBound(1, m_rangeStartDay->value(), dim);
            int e = qBound(s,  m_rangeEndDay->value(),  dim);
            m_calendar->setActiveRange(QDate(y, mo, s), QDate(y, mo, e));
        } else {
            m_calendar->clearActiveRange();
        }
        recomputeAndShow();
    };
    connect(m_rangeLimitCheck, &QCheckBox::toggled, this, [this, applyRange](bool on){
        m_rangeStartDay->setEnabled(on);
        m_rangeEndDay->setEnabled(on);
        applyRange();
    });
    connect(m_rangeStartDay, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [applyRange](int){ applyRange(); });
    connect(m_rangeEndDay, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [applyRange](int){ applyRange(); });

    m_fetchHolidayBtn = new QPushButton(tr("📥 从网络更新"));
    m_fetchHolidayBtn->setToolTip(tr(
        "从 timor.tech 获取所选年份的中国法定节假日 / 调休安排，并缓存到本地。\n"
        "失败时继续使用内置数据。"));
    m_resetBtn = new QPushButton(tr("🗑 清当月标记"));
    m_resetAllBtn = new QPushButton(tr("🔄 全部重置"));
    m_resetAllBtn->setToolTip(tr("清空所有月份的请假/加班记录，并把所有薪资设置恢复成默认值"));
    m_calcBtn = new QPushButton(tr("📊 重新计算"));
    m_calcBtn->setObjectName("primaryBtn");
    m_saveBtn = new QPushButton(tr("💾 保存设置"));
    m_pdfBtn = new QPushButton(tr("📄 导出 PDF"));
    m_pdfBtn->setToolTip(tr("将当月工资条与全部计算过程导出为 PDF（不含单位/公司成本）"));
    main->addLayout(topRow);

    // 动作按钮单独一行，宽度不够时自动换行（避免横向滚动条）
    auto* btnRow = new FlowLayout(0, 6, 6);
    btnRow->addWidget(m_fetchHolidayBtn);
    btnRow->addWidget(m_resetBtn);
    btnRow->addWidget(m_resetAllBtn);
    btnRow->addWidget(m_calcBtn);
    btnRow->addWidget(m_saveBtn);
    btnRow->addWidget(m_pdfBtn);
    main->addLayout(btnRow);

    // 节假日数据来源指示
    auto* srcRow = new QHBoxLayout();
    srcRow->setContentsMargins(2, 0, 2, 0);
    m_holidaySourceLabel = new QLabel();
    m_holidaySourceLabel->setStyleSheet("color:#868e96;font-size:9pt;");
    srcRow->addWidget(m_holidaySourceLabel);
    srcRow->addStretch();
    main->addLayout(srcRow);

    // 中间：左日历 + 右薪资设置/明细
    auto* split = new QSplitter(Qt::Horizontal);
    split->setHandleWidth(6);

    // 左
    auto* leftCard = new QFrame();
    leftCard->setObjectName("card");
    auto* lc = new QVBoxLayout(leftCard);
    lc->setContentsMargins(10, 8, 10, 8);
    lc->setSpacing(6);
    auto* hint = new QLabel(tr("点击日格切换「请假 → 加班 → 默认」；右键展开更多状态"));
    hint->setStyleSheet("color:#868e96; font-size:8pt;");
    lc->addWidget(hint);
    m_calendar = new CalendarWidget();
    lc->addWidget(m_calendar);

    // 日历下方：计算过程（占满剩余垂直空间，避免日历下面留大白）
    auto* breakdownTitle = new QLabel(tr("📝 计算过程"));
    breakdownTitle->setStyleSheet("font-weight:bold;color:#495057;margin-top:6px;");
    lc->addWidget(breakdownTitle);
    m_breakdownText = new QTextEdit();
    m_breakdownText->setReadOnly(true);
    lc->addWidget(m_breakdownText, 1);
    split->addWidget(leftCard);

    // 右
    auto* rightWrap = new QWidget();
    auto* rc = new QVBoxLayout(rightWrap);
    rc->setContentsMargins(0, 0, 0, 0);
    rc->setSpacing(8);

    // 薪资设置卡
    {
        auto* card = new QFrame();
        card->setObjectName("card");
        auto* g = new QVBoxLayout(card);
        g->setContentsMargins(12, 10, 12, 10);
        g->setSpacing(4);
        auto* h = new QLabel(tr("💰 薪资设置"));
        h->setStyleSheet("font-weight:bold;color:#495057;");
        g->addWidget(h);

        auto addRow = [&](const QString& label, QDoubleSpinBox*& w,
                          double minV, double maxV, double def, const QString& suffix,
                          int decimals = 2) {
            auto* row = new QHBoxLayout();
            auto* l = new QLabel(label);
            l->setMinimumWidth(98);
            row->addWidget(l);
            w = new NoWheelDoubleSpinBox();
            w->setRange(minV, maxV);
            w->setValue(def);
            w->setDecimals(decimals);
            w->setKeyboardTracking(false);
            if (!suffix.isEmpty()) w->setSuffix(suffix);
            row->addWidget(w, 1);
            g->addLayout(row);
        };

        addRow(tr("税前月薪"),    m_baseSalary,    0,        1e7,   10000, tr(" 元"));

        // 缴费基数行
        {
            auto* row = new QHBoxLayout();
            auto* l = new QLabel(tr("缴费基数"));
            l->setMinimumWidth(98);
            row->addWidget(l);
            m_contribBase = new NoWheelDoubleSpinBox();
            m_contribBase->setRange(0, 1e7);
            m_contribBase->setValue(10000);
            m_contribBase->setDecimals(2);
            m_contribBase->setKeyboardTracking(false);
            m_contribBase->setSuffix(tr(" 元"));
            row->addWidget(m_contribBase, 1);
            m_baseAutoCheck = new QCheckBox(tr("自动"));
            m_baseAutoCheck->setChecked(true);
            m_baseAutoCheck->setToolTip(tr("勾选则自动跟随月薪并钳到下限/上限"));
            row->addWidget(m_baseAutoCheck);
            g->addLayout(row);
        }
        addRow(tr("基数下限"),    m_baseFloor,     0,        1e7,   4494,  tr(" 元"));
        addRow(tr("基数上限"),    m_baseCeiling,   0,        1e7,   24000, tr(" 元"));

        // 个人比例分组
        auto sub = [](const QString& s) {
            auto* l = new QLabel(s);
            l->setStyleSheet("color:#868e96;font-size:9pt;margin-top:6px;");
            return l;
        };
        g->addWidget(sub(tr("个人比例（按缴费基数计）")));
        addRow(tr("  养老"),      m_personPension,    0, 100, 8.0,  tr(" %"));
        addRow(tr("  医疗"),      m_personMedical,    0, 100, 2.0,  tr(" %"));
        addRow(tr("  失业"),      m_personUnemploy,   0, 100, 0.3,  tr(" %"));
        addRow(tr("  公积金"),    m_personHousing,    0, 100, 12.0, tr(" %"));

        g->addWidget(sub(tr("单位比例（仅参考，不扣自工资）")));
        addRow(tr("  养老"),      m_companyPension,   0, 100, 16.0, tr(" %"));
        addRow(tr("  医疗"),      m_companyMedical,   0, 100, 8.0,  tr(" %"));
        addRow(tr("  失业"),      m_companyUnemploy,  0, 100, 0.7,  tr(" %"));
        addRow(tr("  工伤"),      m_companyInjury,    0, 100, 0.5,  tr(" %"));
        addRow(tr("  生育"),      m_companyMaternity, 0, 100, 0.7,  tr(" %"));
        addRow(tr("  公积金"),    m_companyHousing,   0, 100, 12.0, tr(" %"));

        g->addWidget(sub(tr("个税与其他")));
        addRow(tr("个税起征点"),   m_taxThreshold,  0, 1e7, 5000, tr(" 元"));
        addRow(tr("专项附加扣除"), m_otherDeduct,   0, 1e6, 0,    tr(" 元"));
        addRow(tr("加班费倍率"),   m_overtimeRate,  1.0, 5.0, 2.0, tr("×"));
        rc->addWidget(card);
    }

    // 当月统计卡
    {
        auto* card = new QFrame();
        card->setObjectName("card");
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(12, 10, 12, 10);
        v->setSpacing(6);
        auto* h = new QLabel(tr("📅 当月出勤"));
        h->setStyleSheet("font-weight:bold;color:#495057;");
        v->addWidget(h);

        auto rowOf = [](const QString& l, QLabel*& out) {
            auto* r = new QHBoxLayout();
            r->addWidget(new QLabel(l));
            r->addStretch();
            out = new QLabel("0");
            out->setStyleSheet("font-weight:bold;color:#212529;");
            r->addWidget(out);
            return r;
        };
        v->addLayout(rowOf(tr("应工作日"), m_workDaysLabel));
        v->addLayout(rowOf(tr("法定节假日"), m_holidayDaysLabel));
        v->addLayout(rowOf(tr("实际出勤"), m_workedLabel));
        v->addLayout(rowOf(tr("请假"), m_leaveLabel));
        v->addLayout(rowOf(tr("加班"), m_overtimeLabel));
        rc->addWidget(card);
    }

    // 计算结果卡
    {
        auto* card = new QFrame();
        card->setObjectName("card");
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(12, 10, 12, 10);
        v->setSpacing(4);
        auto* h = new QLabel(tr("🧾 工资明细"));
        h->setStyleSheet("font-weight:bold;color:#495057;");
        v->addWidget(h);

        auto rowOf = [](const QString& l, QLabel*& out, const QString& color = "#212529") {
            auto* r = new QHBoxLayout();
            r->addWidget(new QLabel(l));
            r->addStretch();
            out = new QLabel("¥0.00");
            out->setStyleSheet(QString("font-weight:bold;color:%1;").arg(color));
            r->addWidget(out);
            return r;
        };
        auto sub = [](const QString& s) {
            auto* l = new QLabel(s);
            l->setStyleSheet("color:#868e96;font-size:9pt;margin-top:4px;");
            return l;
        };

        v->addLayout(rowOf(tr("应发工资"), m_grossLabel));

        v->addWidget(sub(tr("个人五险一金（基于缴费基数）")));
        v->addLayout(rowOf(tr("  养老"),    m_personPensionLabel,  "#c62828"));
        v->addLayout(rowOf(tr("  医疗"),    m_personMedicalLabel,  "#c62828"));
        v->addLayout(rowOf(tr("  失业"),    m_personUnemployLabel, "#c62828"));
        v->addLayout(rowOf(tr("  公积金"),  m_personHousingLabel,  "#c62828"));
        v->addLayout(rowOf(tr("  小计"),    m_personSocialSubtotal, "#c62828"));

        v->addLayout(rowOf(tr("应纳税额"), m_taxableLabel));
        v->addLayout(rowOf(tr("个人所得税"), m_taxLabel, "#c62828"));

        auto* sep = new QFrame();
        sep->setFrameShape(QFrame::HLine);
        sep->setStyleSheet("color:#e9ecef;");
        v->addWidget(sep);

        m_netLabel = new QLabel(tr("¥0.00"));
        m_netLabel->setStyleSheet("font-size:18pt;font-weight:bold;color:#1864ab;");
        m_netLabel->setAlignment(Qt::AlignCenter);
        auto* lblTitle = new QLabel(tr("实发工资"));
        lblTitle->setStyleSheet("color:#868e96;font-size:9pt;");
        lblTitle->setAlignment(Qt::AlignCenter);
        v->addWidget(lblTitle);
        v->addWidget(m_netLabel);

        v->addWidget(sub(tr("单位承担（参考）")));
        v->addLayout(rowOf(tr("  五险一金合计"),  m_companySocialLabel, "#1864ab"));
        v->addLayout(rowOf(tr("  公司总成本"),    m_totalCostLabel,     "#1864ab"));

        rc->addWidget(card);
    }

    rc->addStretch();
    split->addWidget(rightWrap);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 0);
    split->setSizes({640, 360});
    main->addWidget(split, 1);

    // 信号
    connect(m_yearSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int){ onYearMonthChanged(); });
    connect(m_monthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int){ onYearMonthChanged(); });
    connect(m_calcBtn, &QPushButton::clicked, this, &SalaryCalc::onCalculate);
    connect(m_saveBtn, &QPushButton::clicked, this, &SalaryCalc::onSaveSettings);
    connect(m_resetBtn, &QPushButton::clicked, this, &SalaryCalc::onResetMonth);
    connect(m_resetAllBtn, &QPushButton::clicked, this, &SalaryCalc::onResetAll);
    connect(m_fetchHolidayBtn, &QPushButton::clicked, this, &SalaryCalc::onFetchHolidaysOnline);
    connect(m_pdfBtn, &QPushButton::clicked, this, &SalaryCalc::onExportPdf);
    connect(m_calendar, &CalendarWidget::dayClicked, this, &SalaryCalc::onDayClicked);
    connect(m_calendar, &CalendarWidget::dayContextMenu, this, &SalaryCalc::onDayContextMenu);
    // 设置变化即时重算
    auto liveCalc = [this](double){ recomputeAndShow(); };
    QList<QDoubleSpinBox*> liveBoxes = {
        m_baseSalary, m_contribBase, m_baseFloor, m_baseCeiling,
        m_personPension, m_personMedical, m_personUnemploy, m_personHousing,
        m_companyPension, m_companyMedical, m_companyUnemploy,
        m_companyInjury, m_companyMaternity, m_companyHousing,
        m_taxThreshold, m_otherDeduct, m_overtimeRate
    };
    for (QDoubleSpinBox* b : liveBoxes) {
        connect(b, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, liveCalc);
    }
    connect(m_baseAutoCheck, &QCheckBox::toggled, this, [this](bool on){
        m_contribBase->setEnabled(!on);
        if (on) recomputeAndShow();
    });
    m_contribBase->setEnabled(!m_baseAutoCheck->isChecked());
}

void SalaryCalc::initDb()
{
    m_db = SqliteWrapper::SqliteManager::getDefaultInstance();
    if (!m_db) return;
    m_db->execute(
        "CREATE TABLE IF NOT EXISTS salary_attendance ("
        " date TEXT PRIMARY KEY, "
        " status TEXT NOT NULL, "
        " note TEXT DEFAULT ''"
        ")");
}

void SalaryCalc::loadAttendanceForCurrentMonth()
{
    if (!m_db) return;
    int y = m_yearSpin->value();
    int mo = m_monthSpin->value();
    QDate first(y, mo, 1);
    QDate last(y, mo, first.daysInMonth());

    QVariantMap p;
    p["from"] = first.toString(Qt::ISODate);
    p["to"] = last.toString(Qt::ISODate);
    auto res = m_db->select(
        "SELECT date, status FROM salary_attendance "
        "WHERE date >= :from AND date <= :to", p);

    QHash<QDate, DayKind> marks;
    if (res.success) {
        for (const QVariant& row : res.data) {
            auto m = row.toMap();
            QDate d = QDate::fromString(m["date"].toString(), Qt::ISODate);
            QString s = m["status"].toString();
            DayKind k = DayKind::Workday;
            if (s == "leave") k = DayKind::Leave;
            else if (s == "overtime") k = DayKind::Overtime;
            else continue;
            marks.insert(d, k);
        }
    }
    m_calendar->setUserMarks(marks);
}

void SalaryCalc::saveDayMark(const QDate& d, DayKind k)
{
    if (!m_db) return;
    QString s = (k == DayKind::Leave) ? "leave"
              : (k == DayKind::Overtime) ? "overtime" : "";
    if (s.isEmpty()) { clearDayMark(d); return; }

    QVariantMap data;
    data["date"] = d.toString(Qt::ISODate);
    data["status"] = s;
    // upsert
    m_db->execute("DELETE FROM salary_attendance WHERE date = :d",
                  {{"d", d.toString(Qt::ISODate)}});
    m_db->insert("salary_attendance", data);
}

void SalaryCalc::clearDayMark(const QDate& d)
{
    if (!m_db) return;
    m_db->execute("DELETE FROM salary_attendance WHERE date = :d",
                  {{"d", d.toString(Qt::ISODate)}});
}

void SalaryCalc::applyHolidayCalendar()
{
    int y = m_yearSpin->value();
    int mo = m_monthSpin->value();
    QDate first(y, mo, 1);
    QDate last(y, mo, first.daysInMonth());

    // 重置当月 base
    for (QDate d = first; d <= last; d = d.addDays(1)) {
        DayKind k = (d.dayOfWeek() >= 6) ? DayKind::Weekend : DayKind::Workday;
        m_calendar->setBaseKind(d, k);
    }

    // 首次访问该年份且尚未拿到在线数据时，尝试从本地缓存加载（覆盖内置）
    auto& db = holidayDbMut();
    if (!db.contains(y) || db[y].source != HolidaySource::OnlineCached) {
        loadHolidaysFromCache(y);
    }

    const YearHolidays* yh = holidayDataFor(y);
    if (!yh) {
        if (m_holidaySourceLabel) {
            m_holidaySourceLabel->setText(
                tr("ℹ️ %1 年无内置节假日数据，请点「📥 从网络更新」").arg(y));
        }
        return;
    }

    bool predicted = (yh->source == HolidaySource::HardcodedPredicted);
    QString suffix = predicted ? QStringLiteral("⚠️") : QString();

    for (auto h = yh->holidays.constBegin(); h != yh->holidays.constEnd(); ++h) {
        const QDate& d = h.key();
        if (d.month() == mo) {
            m_calendar->setBaseKind(d, DayKind::Holiday);
            m_calendar->setBaseLabel(d, h.value() + suffix);
        }
    }
    for (auto h = yh->makeupDays.constBegin(); h != yh->makeupDays.constEnd(); ++h) {
        const QDate& d = h.key();
        if (d.month() == mo) {
            m_calendar->setBaseKind(d, DayKind::MakeupWorkday);
            m_calendar->setBaseLabel(d, h.value() + suffix);
        }
    }

    if (m_holidaySourceLabel) {
        QString src;
        switch (yh->source) {
            case HolidaySource::Hardcoded:
                src = tr("📚 内置（官方版）"); break;
            case HolidaySource::HardcodedPredicted:
                src = tr("⚠️ 内置（预估，未确认 — 自动尝试在线获取中…）"); break;
            case HolidaySource::OnlineCached:
                src = tr("🌐 在线缓存"); break;
        }
        m_holidaySourceLabel->setText(QString("%1 年节假日：%2").arg(y).arg(src));
    }

    // 预估数据自动拉一次在线版（每年份每会话一次，失败则保留预估作为回退）
    if (yh->source == HolidaySource::HardcodedPredicted && !m_autoFetchedYears.contains(y)) {
        m_autoFetchedYears.insert(y);
        QTimer::singleShot(0, this, &SalaryCalc::onFetchHolidaysOnline);
    }
}

void SalaryCalc::onYearMonthChanged()
{
    int y = m_yearSpin->value();
    int mo = m_monthSpin->value();
    m_calendar->setMonth(y, mo);
    applyHolidayCalendar();

    // 月份变化时，重新钳制范围 spinner 的上限到当月天数
    QDate first(y, mo, 1);
    int dim = first.daysInMonth();
    if (m_rangeStartDay && m_rangeEndDay) {
        m_rangeStartDay->setMaximum(dim);
        m_rangeEndDay->setMaximum(dim);
        if (m_rangeEndDay->value() > dim) m_rangeEndDay->setValue(dim);
        if (m_rangeLimitCheck && m_rangeLimitCheck->isChecked()) {
            int s = qBound(1, m_rangeStartDay->value(), dim);
            int e = qBound(s, m_rangeEndDay->value(), dim);
            m_calendar->setActiveRange(QDate(y, mo, s), QDate(y, mo, e));
        } else {
            m_calendar->clearActiveRange();
        }
    }

    loadAttendanceForCurrentMonth();
    recomputeAndShow();
}

void SalaryCalc::onDayClicked(const QDate& d)
{
    // 左键循环：默认 → 请假 → 加班 → 默认
    DayKind cur = m_calendar->effectiveKind(d);
    bool hasUserMark = m_calendar->userMarks().contains(d);

    if (!hasUserMark) {
        // 工作日 / 周末 / 节假日 → 标记为请假
        m_calendar->setUserMark(d, DayKind::Leave);
        saveDayMark(d, DayKind::Leave);
    } else if (cur == DayKind::Leave) {
        m_calendar->setUserMark(d, DayKind::Overtime);
        saveDayMark(d, DayKind::Overtime);
    } else {
        m_calendar->clearUserMark(d);
        clearDayMark(d);
    }
    recomputeAndShow();
}

void SalaryCalc::onDayContextMenu(const QDate& d, const QPoint& globalPos)
{
    QMenu menu(this);
    auto* aDefault = menu.addAction(tr("默认（按日历自动）"));
    auto* aLeave = menu.addAction(tr("请假"));
    auto* aOvertime = menu.addAction(tr("加班"));
    QAction* sel = menu.exec(globalPos);
    if (!sel) return;
    if (sel == aDefault) {
        m_calendar->clearUserMark(d);
        clearDayMark(d);
    } else if (sel == aLeave) {
        m_calendar->setUserMark(d, DayKind::Leave);
        saveDayMark(d, DayKind::Leave);
    } else if (sel == aOvertime) {
        m_calendar->setUserMark(d, DayKind::Overtime);
        saveDayMark(d, DayKind::Overtime);
    }
    recomputeAndShow();
}

void SalaryCalc::onResetMonth()
{
    if (!m_db) return;
    int y = m_yearSpin->value();
    int mo = m_monthSpin->value();
    QDate first(y, mo, 1);
    QDate last(y, mo, first.daysInMonth());
    QVariantMap p;
    p["from"] = first.toString(Qt::ISODate);
    p["to"] = last.toString(Qt::ISODate);
    m_db->execute("DELETE FROM salary_attendance "
                  "WHERE date >= :from AND date <= :to", p);
    m_calendar->setUserMarks({});
    recomputeAndShow();
}

void SalaryCalc::onCalculate() { recomputeAndShow(); }

void SalaryCalc::onSaveSettings() { saveSettings(); }

void SalaryCalc::onResetAll()
{
    auto rc = QMessageBox::warning(this, tr("全部重置"),
        tr("此操作将清空：\n"
           "  • 所有月份的请假 / 加班记录（无法恢复）\n"
           "  • 全部薪资设置（恢复为默认值）\n"
           "  • 工作时间区间设置\n\n"
           "确定继续吗？"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (rc != QMessageBox::Yes) return;

    // 1. 清掉所有月份的考勤记录
    if (m_db) m_db->execute("DELETE FROM salary_attendance");

    // 2. 清掉 QSettings 里 SalaryCalc 分组下的所有键
    {
        QSettings s;
        s.beginGroup("SalaryCalc");
        s.remove("");   // 删除当前分组下所有键
        s.endGroup();
    }

    // 3. 工作时间区间复位
    if (m_rangeLimitCheck) m_rangeLimitCheck->setChecked(false);
    if (m_rangeStartDay)   m_rangeStartDay->setValue(1);
    if (m_rangeEndDay)     m_rangeEndDay->setValue(QDate::currentDate().daysInMonth());

    // 4. 重新加载（QSettings 已空 → 取代码里写的默认值）
    loadSettings();

    // 5. 清掉日历上的用户标记，重新应用节假日 + 重算
    m_calendar->setUserMarks({});
    applyHolidayCalendar();
    recomputeAndShow();
}

void SalaryCalc::loadSettings()
{
    QSettings s;
    s.beginGroup("SalaryCalc");
    m_baseSalary->setValue(s.value("baseSalary", 10000.0).toDouble());
    m_contribBase->setValue(s.value("contribBase", 10000.0).toDouble());
    m_baseFloor->setValue(s.value("baseFloor", 4494.0).toDouble());
    m_baseCeiling->setValue(s.value("baseCeiling", 24000.0).toDouble());
    m_baseAutoCheck->setChecked(s.value("baseAuto", true).toBool());
    m_personPension->setValue(s.value("personPension", 8.0).toDouble());
    m_personMedical->setValue(s.value("personMedical", 2.0).toDouble());
    m_personUnemploy->setValue(s.value("personUnemploy", 0.3).toDouble());
    m_personHousing->setValue(s.value("personHousing", 12.0).toDouble());
    m_companyPension->setValue(s.value("companyPension", 16.0).toDouble());
    m_companyMedical->setValue(s.value("companyMedical", 8.0).toDouble());
    m_companyUnemploy->setValue(s.value("companyUnemploy", 0.7).toDouble());
    m_companyInjury->setValue(s.value("companyInjury", 0.5).toDouble());
    m_companyMaternity->setValue(s.value("companyMaternity", 0.7).toDouble());
    m_companyHousing->setValue(s.value("companyHousing", 12.0).toDouble());
    m_taxThreshold->setValue(s.value("taxThreshold", 5000.0).toDouble());
    m_otherDeduct->setValue(s.value("otherDeduct", 0.0).toDouble());
    m_overtimeRate->setValue(s.value("overtimeRate", 2.0).toDouble());
    s.endGroup();
}

void SalaryCalc::saveSettings()
{
    QSettings s;
    s.beginGroup("SalaryCalc");
    s.setValue("baseSalary", m_baseSalary->value());
    s.setValue("contribBase", m_contribBase->value());
    s.setValue("baseFloor", m_baseFloor->value());
    s.setValue("baseCeiling", m_baseCeiling->value());
    s.setValue("baseAuto", m_baseAutoCheck->isChecked());
    s.setValue("personPension", m_personPension->value());
    s.setValue("personMedical", m_personMedical->value());
    s.setValue("personUnemploy", m_personUnemploy->value());
    s.setValue("personHousing", m_personHousing->value());
    s.setValue("companyPension", m_companyPension->value());
    s.setValue("companyMedical", m_companyMedical->value());
    s.setValue("companyUnemploy", m_companyUnemploy->value());
    s.setValue("companyInjury", m_companyInjury->value());
    s.setValue("companyMaternity", m_companyMaternity->value());
    s.setValue("companyHousing", m_companyHousing->value());
    s.setValue("taxThreshold", m_taxThreshold->value());
    s.setValue("otherDeduct", m_otherDeduct->value());
    s.setValue("overtimeRate", m_overtimeRate->value());
    s.endGroup();
}

void SalaryCalc::recomputeAndShow()
{
    int y = m_yearSpin->value();
    int mo = m_monthSpin->value();
    QDate first(y, mo, 1);
    QDate last(y, mo, first.daysInMonth());

    int workdaysExpected = 0;   // 应工作日（普通工作日 + 调休工作日）
    int holidayDays = 0;
    int workedDays = 0;          // 实际出勤
    int leaveDays = 0;
    int overtimeDays = 0;        // 加班（按 1.0 个工作日折算）

    for (QDate d = first; d <= last; d = d.addDays(1)) {
        if (!m_calendar->inActiveRange(d)) continue;   // 限定区间外的日子不计

        DayKind k = m_calendar->effectiveKind(d);
        bool isWorkRequired = (k == DayKind::Workday || k == DayKind::MakeupWorkday);
        if (isWorkRequired || k == DayKind::Leave) workdaysExpected++;
        if (k == DayKind::Holiday) holidayDays++;
        if (k == DayKind::Leave) leaveDays++;
        else if (k == DayKind::Overtime) overtimeDays++;
        else if (isWorkRequired) workedDays++;
    }

    m_workDaysLabel->setText(QString::number(workdaysExpected));
    m_holidayDaysLabel->setText(QString::number(holidayDays));
    m_workedLabel->setText(QString::number(workedDays));
    m_leaveLabel->setText(QString::number(leaveDays));
    m_overtimeLabel->setText(QString::number(overtimeDays));

    double base = m_baseSalary->value();
    double taxFree = m_taxThreshold->value();
    double otherDeduct = m_otherDeduct->value();
    double otRate = m_overtimeRate->value();

    // —— 缴费基数（社保/公积金按这个算，不是月薪）——
    double cbFloor = m_baseFloor->value();
    double cbCeil  = m_baseCeiling->value();
    double cbase;
    if (m_baseAutoCheck->isChecked()) {
        cbase = qBound(cbFloor, base, cbCeil > 0 ? cbCeil : base);
        m_contribBase->blockSignals(true);
        m_contribBase->setValue(cbase);
        m_contribBase->blockSignals(false);
    } else {
        cbase = m_contribBase->value();
    }

    // —— 个人各项扣除（按缴费基数）——
    double pPension  = cbase * m_personPension->value()  / 100.0;
    double pMedical  = cbase * m_personMedical->value()  / 100.0;
    double pUnemploy = cbase * m_personUnemploy->value() / 100.0;
    double pHousing  = cbase * m_personHousing->value()  / 100.0;
    double personSocialTotal = pPension + pMedical + pUnemploy + pHousing;

    // —— 单位各项（参考）——
    double cPension   = cbase * m_companyPension->value()   / 100.0;
    double cMedical   = cbase * m_companyMedical->value()   / 100.0;
    double cUnemploy  = cbase * m_companyUnemploy->value()  / 100.0;
    double cInjury    = cbase * m_companyInjury->value()    / 100.0;
    double cMaternity = cbase * m_companyMaternity->value() / 100.0;
    double cHousing   = cbase * m_companyHousing->value()   / 100.0;
    double companySocialTotal = cPension + cMedical + cUnemploy + cInjury + cMaternity + cHousing;

    // —— 应发（按月薪折算请假/加班，不动缴费基数）——
    double standardDays = 21.75;
    double dailyRate = base / standardDays;
    double presentPay = qMax(0.0, dailyRate * (workdaysExpected - leaveDays));
    double overtimePay = dailyRate * overtimeDays * otRate;
    double gross = presentPay + overtimePay;

    // —— 个税 ——
    double taxableBase = gross - personSocialTotal - otherDeduct - taxFree;
    double taxable = qMax(0.0, taxableBase);
    double tax = calcMonthlyTax(taxable);

    double net = gross - personSocialTotal - tax;
    double totalCost = gross + companySocialTotal;

    auto money = [](double v) { return QString("¥%1").arg(QString::number(v, 'f', 2)); };

    m_grossLabel->setText(money(gross));
    m_personPensionLabel->setText("- " + money(pPension));
    m_personMedicalLabel->setText("- " + money(pMedical));
    m_personUnemployLabel->setText("- " + money(pUnemploy));
    m_personHousingLabel->setText("- " + money(pHousing));
    m_personSocialSubtotal->setText("- " + money(personSocialTotal));
    m_taxableLabel->setText(money(taxable));
    m_taxLabel->setText("- " + money(tax));
    m_netLabel->setText(money(net));
    m_companySocialLabel->setText("+ " + money(companySocialTotal));
    m_totalCostLabel->setText(money(totalCost));

    // 把所有计算结果写入 snapshot，供 buildBreakdownHtml / onExportPdf 复用
    m_snap = {};
    m_snap.workdaysExpected = workdaysExpected;
    m_snap.holidayDays = holidayDays;
    m_snap.workedDays = workedDays;
    m_snap.leaveDays = leaveDays;
    m_snap.overtimeDays = overtimeDays;
    m_snap.base = base; m_snap.dailyRate = dailyRate;
    m_snap.presentPay = presentPay; m_snap.overtimePay = overtimePay; m_snap.gross = gross;
    m_snap.cbase = cbase; m_snap.cbFloor = cbFloor; m_snap.cbCeil = cbCeil;
    m_snap.pPension = pPension; m_snap.pMedical = pMedical;
    m_snap.pUnemploy = pUnemploy; m_snap.pHousing = pHousing;
    m_snap.personSocialTotal = personSocialTotal;
    m_snap.cPension = cPension; m_snap.cMedical = cMedical;
    m_snap.cUnemploy = cUnemploy; m_snap.cInjury = cInjury;
    m_snap.cMaternity = cMaternity; m_snap.cHousing = cHousing;
    m_snap.companySocialTotal = companySocialTotal;
    m_snap.taxFree = taxFree; m_snap.otherDeduct = otherDeduct;
    m_snap.taxable = taxable;
    {
        struct Bracket { double upper; double rate; double quick; };
        static const Bracket b[] = {
            {     3000, 0.03, 0      },
            {    12000, 0.10, 210    },
            {    25000, 0.20, 1410   },
            {    35000, 0.25, 2660   },
            {    55000, 0.30, 4410   },
            {    80000, 0.35, 7160   },
            {1.0e12,    0.45, 15160  },
        };
        for (const auto& x : b) {
            if (taxable <= x.upper) { m_snap.taxRate = x.rate; m_snap.taxQuick = x.quick; break; }
        }
    }
    m_snap.tax = tax;
    m_snap.net = net; m_snap.totalCost = totalCost; m_snap.otRate = otRate;
    m_snap.autoBase = m_baseAutoCheck->isChecked();
    m_snap.rangeLimited = m_rangeLimitCheck && m_rangeLimitCheck->isChecked();
    m_snap.rangeStart = m_rangeStartDay ? m_rangeStartDay->value() : 0;
    m_snap.rangeEnd = m_rangeEndDay ? m_rangeEndDay->value() : 0;

    m_breakdownText->setHtml(buildBreakdownHtml(true));
}

QString SalaryCalc::buildBreakdownHtml(bool includeCompany) const
{
    int y = m_yearSpin->value();
    int mo = m_monthSpin->value();
    const Snapshot& s = m_snap;
    auto fmt = [](double v, int dec = 2) { return QString::number(v, 'f', dec); };

    QString rangeNote;
    if (s.rangeLimited) {
        rangeNote = QString("（仅计 %1 ~ %2 日）").arg(s.rangeStart).arg(s.rangeEnd);
    }
    QString clampNote;
    if (s.autoBase && (s.base < s.cbFloor || s.base > s.cbCeil)) {
        clampNote = QString("（自动跟随月薪，钳到 [¥%1, ¥%2]）")
                        .arg(fmt(s.cbFloor, 0)).arg(fmt(s.cbCeil, 0));
    } else if (s.autoBase) {
        clampNote = QStringLiteral("（自动跟随月薪）");
    }

    QString html;
    QTextStream ts(&html);
    ts << R"(<style>
        body { font-family:'PingFang SC','Microsoft YaHei',sans-serif; color:#212529; }
        h4 { color:#1864ab; margin:10px 0 4px 0; padding-bottom:2px;
             border-bottom:1px solid #dee2e6; font-size:11pt; }
        .step { margin-left:14px; line-height:1.7; }
        .num  { color:#212529; font-weight:bold; }
        .ded  { color:#c62828; font-weight:bold; }
        .add  { color:#1864ab; font-weight:bold; }
        .res  { color:#1864ab; font-weight:bold; font-size:11pt; }
        .note { color:#868e96; font-size:9.5pt; }
        .form { font-family:'SF Mono','Consolas',monospace; color:#495057; }
    </style>)";

    ts << QString("<h4>📅 %1 年 %2 月%3</h4>").arg(y).arg(mo).arg(rangeNote);
    ts << QString("<div class='step'>"
                  "应工作日 <span class='num'>%1</span> 天 · "
                  "节假日 <span class='num'>%2</span> 天 · "
                  "实际出勤 <span class='num'>%3</span> 天 · "
                  "请假 <span class='num'>%4</span> 天 · "
                  "加班 <span class='num'>%5</span> 天</div>")
              .arg(s.workdaysExpected).arg(s.holidayDays)
              .arg(s.workedDays).arg(s.leaveDays).arg(s.overtimeDays);

    ts << "<h4>① 应发工资（按 21.75 天折算日薪）</h4>";
    ts << QString("<div class='step'><span class='form'>"
                  "日工资 = ¥%1 ÷ 21.75 = <span class='num'>¥%2</span></span></div>")
              .arg(fmt(s.base)).arg(fmt(s.dailyRate));
    ts << QString("<div class='step'><span class='form'>"
                  "出勤工资 = ¥%1 × (%2 - %3) = <span class='num'>¥%4</span>"
                  "</span> <span class='note'>（应工作日 - 请假）</span></div>")
              .arg(fmt(s.dailyRate)).arg(s.workdaysExpected).arg(s.leaveDays).arg(fmt(s.presentPay));
    ts << QString("<div class='step'><span class='form'>"
                  "加班费 = ¥%1 × %2 × %3 = <span class='num'>¥%4</span></span></div>")
              .arg(fmt(s.dailyRate)).arg(s.overtimeDays).arg(fmt(s.otRate, 1)).arg(fmt(s.overtimePay));
    ts << QString("<div class='step'><span class='form'>"
                  "<b>应发</b> = ¥%1 + ¥%2 = <span class='res'>¥%3</span></span></div>")
              .arg(fmt(s.presentPay)).arg(fmt(s.overtimePay)).arg(fmt(s.gross));

    ts << "<h4>② 缴费基数</h4>";
    ts << QString("<div class='step'><span class='form'>"
                  "缴费基数 = <span class='num'>¥%1</span></span> "
                  "<span class='note'>%2</span></div>")
              .arg(fmt(s.cbase)).arg(clampNote);

    ts << "<h4>③ 个人五险一金（按缴费基数）</h4>";
    auto deductRow = [&](const QString& name, double rate, double amt) {
        ts << QString("<div class='step'><span class='form'>"
                      "%1 &nbsp; %2%% × ¥%3 = <span class='ded'>¥%4</span></span></div>")
                  .arg(name).arg(fmt(rate, 1)).arg(fmt(s.cbase)).arg(fmt(amt));
    };
    deductRow("养老",   m_personPension->value(),  s.pPension);
    deductRow("医疗",   m_personMedical->value(),  s.pMedical);
    deductRow("失业",   m_personUnemploy->value(), s.pUnemploy);
    deductRow("公积金", m_personHousing->value(),  s.pHousing);
    ts << QString("<div class='step'><span class='form'>"
                  "<b>个人合计</b> = <span class='ded'>¥%1</span></span></div>")
              .arg(fmt(s.personSocialTotal));

    ts << "<h4>④ 个人所得税（月度近似分档表）</h4>";
    ts << QString("<div class='step'><span class='form'>"
                  "应纳税额 = 应发 ¥%1 - 五险一金 ¥%2 - 起征点 ¥%3 - 专项 ¥%4 "
                  "= <span class='num'>¥%5</span></span></div>")
              .arg(fmt(s.gross)).arg(fmt(s.personSocialTotal)).arg(fmt(s.taxFree))
              .arg(fmt(s.otherDeduct)).arg(fmt(s.taxable));
    if (s.taxable > 0) {
        ts << QString("<div class='step'><span class='form'>"
                      "适用税率 %1%%，速算扣除 ¥%2</span></div>")
                  .arg(fmt(s.taxRate * 100, 1)).arg(fmt(s.taxQuick, 0));
        ts << QString("<div class='step'><span class='form'>"
                      "个税 = ¥%1 × %2%% - ¥%3 = <span class='ded'>¥%4</span></span></div>")
                  .arg(fmt(s.taxable)).arg(fmt(s.taxRate * 100, 1))
                  .arg(fmt(s.taxQuick, 0)).arg(fmt(s.tax));
    } else {
        ts << "<div class='step'><span class='note'>应纳税额 ≤ 0，不缴个税</span></div>";
    }

    ts << "<h4>⑤ 实发到手</h4>";
    ts << QString("<div class='step'><span class='form'>"
                  "<b>实发</b> = 应发 ¥%1 - 五险一金 ¥%2 - 个税 ¥%3 "
                  "= <span class='res'>¥%4</span></span></div>")
              .arg(fmt(s.gross)).arg(fmt(s.personSocialTotal)).arg(fmt(s.tax)).arg(fmt(s.net));

    if (includeCompany) {
        ts << "<h4>⑥ 单位承担（参考，不扣自工资）</h4>";
        auto cRow = [&](const QString& name, double rate, double amt) {
            ts << QString("<div class='step'><span class='form'>"
                          "%1 &nbsp; %2%% × ¥%3 = <span class='add'>¥%4</span></span></div>")
                      .arg(name).arg(fmt(rate, 1)).arg(fmt(s.cbase)).arg(fmt(amt));
        };
        cRow("养老",   m_companyPension->value(),   s.cPension);
        cRow("医疗",   m_companyMedical->value(),   s.cMedical);
        cRow("失业",   m_companyUnemploy->value(),  s.cUnemploy);
        cRow("工伤",   m_companyInjury->value(),    s.cInjury);
        cRow("生育",   m_companyMaternity->value(), s.cMaternity);
        cRow("公积金", m_companyHousing->value(),   s.cHousing);
        ts << QString("<div class='step'><span class='form'>"
                      "<b>单位合计</b> = <span class='add'>¥%1</span></span></div>")
                  .arg(fmt(s.companySocialTotal));
        ts << QString("<div class='step'><span class='form'>"
                      "<b>公司总成本</b> = 应发 ¥%1 + 单位五险一金 ¥%2 "
                      "= <span class='res'>¥%3</span></span></div>")
                  .arg(fmt(s.gross)).arg(fmt(s.companySocialTotal)).arg(fmt(s.totalCost));
    }

    ts << "<h4>📌 说明</h4>"
       << "<div class='step note'>· 个税为月度近似分档表，未做累计预扣预缴<br>"
       << "· 社保/公积金按"
       << "<b>缴费基数</b>计算（与月薪可不同，武汉 2026 下限 ¥4494）<br>"
       << "· 日历右键可单日覆盖：默认 / 请假 / 加班<br>"
       << "· 节假日数据：2024、2025 内置官方版；2026+ 自动从 timor.tech 拉取</div>";

    return html;
}


// ============= 在线节假日 =============

bool SalaryCalc::loadHolidaysFromCache(int year)
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                   + "/holiday-cn/" + QString::number(year) + ".json";
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QByteArray bytes = f.readAll();
    f.close();

    QJsonDocument doc = QJsonDocument::fromJson(bytes);
    if (!doc.isObject()) return false;
    QJsonObject root = doc.object();
    if (root.value("code").toInt(-1) != 0) return false;

    QJsonObject hol = root.value("holiday").toObject();
    YearHolidays y;
    y.source = HolidaySource::OnlineCached;
    for (auto it = hol.begin(); it != hol.end(); ++it) {
        QJsonObject e = it.value().toObject();
        QDate d = QDate::fromString(e.value("date").toString(), "yyyy-MM-dd");
        if (!d.isValid() || d.year() != year) continue;
        QString name = e.value("name").toString();
        bool isHoliday = e.value("holiday").toBool();
        if (isHoliday) y.holidays.insert(d, name);
        else y.makeupDays.insert(d, QStringLiteral("调休·") + name);
    }
    if (y.holidays.isEmpty() && y.makeupDays.isEmpty()) return false;
    holidayDbMut().insert(year, y);
    return true;
}

bool SalaryCalc::saveHolidaysToCache(int year, const QByteArray& json)
{
    QString dirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                      + "/holiday-cn";
    QDir dir(dirPath);
    if (!dir.exists() && !dir.mkpath(".")) return false;
    QFile f(dir.filePath(QString::number(year) + ".json"));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(json);
    f.close();
    return true;
}

void SalaryCalc::onFetchHolidaysOnline()
{
    int year = m_yearSpin->value();
    if (!m_net) m_net = new QNetworkAccessManager(this);

    m_fetchHolidayBtn->setEnabled(false);
    m_holidaySourceLabel->setText(tr("⏳ 正在获取 %1 年节假日…").arg(year));

    QNetworkRequest req(QUrl(QString("https://timor.tech/api/holiday/year/%1").arg(year)));
    req.setRawHeader("User-Agent", "lele-tools/1.0");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, year]() {
        reply->deleteLater();
        m_fetchHolidayBtn->setEnabled(true);

        if (reply->error() != QNetworkReply::NoError) {
            m_holidaySourceLabel->setText(
                tr("❌ 获取失败：%1（继续使用内置数据）").arg(reply->errorString()));
            return;
        }
        QByteArray bytes = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(bytes);
        if (!doc.isObject() || doc.object().value("code").toInt(-1) != 0) {
            m_holidaySourceLabel->setText(tr("❌ 数据格式异常（继续使用内置数据）"));
            return;
        }
        if (!saveHolidaysToCache(year, bytes)) {
            m_holidaySourceLabel->setText(tr("❌ 写缓存失败"));
            return;
        }
        // 强制重新加载缓存覆盖内置
        holidayDbMut().remove(year);
        if (!loadHolidaysFromCache(year)) {
            m_holidaySourceLabel->setText(tr("⚠️ 该年份在线无数据（可能尚未公布）"));
            return;
        }
        applyHolidayCalendar();
        recomputeAndShow();
    });
}


// ============= PDF 导出 =============

void SalaryCalc::onExportPdf()
{
    recomputeAndShow();   // 确保 m_snap 是最新的

    int y = m_yearSpin->value();
    int mo = m_monthSpin->value();

    QString defName = QString("工资单_%1年%2月.pdf")
                          .arg(y).arg(mo, 2, 10, QChar('0'));
    QString defDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString path = QFileDialog::getSaveFileName(
        this, tr("导出 PDF 工资单"),
        defDir + "/" + defName,
        tr("PDF 文档 (*.pdf)"));
    if (path.isEmpty()) return;
    if (!path.endsWith(".pdf", Qt::CaseInsensitive)) path += ".pdf";

    // 顶部信息条 + 关键数字 + 详细计算（不含单位/公司成本）
    auto fmt = [](double v, int dec = 2) { return QString::number(v, 'f', dec); };
    QString rangeText;
    if (m_snap.rangeLimited) {
        rangeText = QString(" · 工作区间 %1 ~ %2 日")
                        .arg(m_snap.rangeStart).arg(m_snap.rangeEnd);
    }

    QString head;
    QTextStream hs(&head);
    hs << R"(<style>
        body { font-family:'PingFang SC','Microsoft YaHei',sans-serif; color:#212529; font-size:9pt; }
        .title { font-size:13pt; font-weight:bold; color:#1864ab; margin-bottom:2px; }
        .sub   { color:#868e96; font-size:8pt; margin-bottom:8px; }
        table.summary { border-collapse:collapse; width:100%; margin:6px 0 10px 0; }
        table.summary td { padding:4px 8px; border:1px solid #dee2e6; font-size:9pt; }
        table.summary td.k { background:#f8f9fa; color:#495057; width:30%; }
        table.summary td.v { font-weight:bold; color:#212529; }
        table.summary td.net { font-weight:bold; color:#1864ab; font-size:11pt; }
    </style>)";
    hs << QString("<div class='title'>工资计算明细</div>");
    hs << QString("<div class='sub'>%1 年 %2 月%3 · 生成时间 %4</div>")
              .arg(y).arg(mo).arg(rangeText)
              .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm"));

    hs << "<table class='summary'>";
    hs << QString("<tr><td class='k'>税前月薪</td><td class='v'>¥%1</td></tr>").arg(fmt(m_snap.base));
    hs << QString("<tr><td class='k'>缴费基数</td><td class='v'>¥%1</td></tr>").arg(fmt(m_snap.cbase));
    hs << QString("<tr><td class='k'>应发工资</td><td class='v'>¥%1</td></tr>").arg(fmt(m_snap.gross));
    hs << QString("<tr><td class='k'>个人五险一金</td><td class='v'>- ¥%1</td></tr>").arg(fmt(m_snap.personSocialTotal));
    hs << QString("<tr><td class='k'>个人所得税</td><td class='v'>- ¥%1</td></tr>").arg(fmt(m_snap.tax));
    hs << QString("<tr><td class='k'>实发到手</td><td class='net'>¥%1</td></tr>").arg(fmt(m_snap.net));
    hs << "</table>";

    QString full = head + buildBreakdownHtml(/*includeCompany=*/false);

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageMargins(QMarginsF(18, 16, 18, 16), QPageLayout::Millimeter);

    QTextDocument doc;
    QFont docFont;
#ifdef Q_OS_MACOS
    docFont.setFamily("PingFang SC");
#elif defined(Q_OS_WIN)
    docFont.setFamily("Microsoft YaHei");
#endif
    docFont.setPointSizeF(9.0);
    doc.setDefaultFont(docFont);
    doc.setHtml(full);
    // 不手动 setPageSize / setPaintDevice：QTextDocument::print 内部会按
    // 打印机的 DPI 与 A4 尺寸正确换算成像素。手动设置时若误用 Point 单位，
    // 文档以为页面只有 595px 宽，导致一字一页。
    doc.print(&printer);

    QMessageBox::information(this, tr("导出完成"),
        tr("工资单已保存：\n%1").arg(QDir::toNativeSeparators(path)));
}
