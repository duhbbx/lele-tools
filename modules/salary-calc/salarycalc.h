#ifndef SALARYCALC_H
#define SALARYCALC_H

#include <QWidget>
#include <QDate>
#include <QHash>
#include <QSet>

#include "../../common/dynamicobjectbase.h"

namespace SqliteWrapper { class SqliteManager; }

class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QPushButton;
class QLabel;
class QTextEdit;
class QFrame;
class QCheckBox;
class QNetworkAccessManager;
class QNetworkReply;

// 单日状态：工作 / 周末 / 法定节假日 / 调休工作日 / 请假 / 加班
enum class DayKind {
    Workday,        // 普通工作日
    Weekend,        // 周末（不上班）
    Holiday,        // 法定节假日（不上班，照发工资）
    MakeupWorkday,  // 调休工作日（周末但要上班）
    Leave,          // 请假（用户标记）
    Overtime        // 加班（用户标记，可能在周末/节假日）
};

class CalendarWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CalendarWidget(QWidget* parent = nullptr);
    void setMonth(int year, int month);
    void setBaseKind(const QDate& d, DayKind k);   // 节假日 / 调休等"基础日历"
    void setBaseLabel(const QDate& d, const QString& label);  // 自定义标签（如"春节"）
    void setUserMark(const QDate& d, DayKind k);   // 用户的请假/加班标记
    void clearUserMark(const QDate& d);
    DayKind effectiveKind(const QDate& d) const;
    QHash<QDate, DayKind> userMarks() const { return m_userMarks; }
    void setUserMarks(const QHash<QDate, DayKind>& m) { m_userMarks = m; update(); }
    void setActiveRange(const QDate& start, const QDate& end);  // 限定工作时间区间
    void clearActiveRange();
    bool inActiveRange(const QDate& d) const;

signals:
    void dayClicked(const QDate& d);
    void dayContextMenu(const QDate& d, const QPoint& globalPos);

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return {420, 240}; }

private:
    QRect cellRect(int row, int col) const;
    QDate cellDate(int row, int col) const;
    bool dateInMonth(const QDate& d) const;
    int rowsNeeded() const;
    QPair<int, int> dateToCell(const QDate& d) const;

    int m_year = 0, m_month = 0;
    QDate m_firstCell;       // 首格日期（可能是上月最后几天）
    QHash<QDate, DayKind> m_baseKinds;
    QHash<QDate, QString> m_baseLabels;
    QHash<QDate, DayKind> m_userMarks;
    QDate m_rangeStart;      // 有效工作区间（无效则不限制）
    QDate m_rangeEnd;
};


class SalaryCalc : public QWidget, public DynamicObjectBase
{
    Q_OBJECT
public:
    explicit SalaryCalc();
    ~SalaryCalc() override;

private slots:
    void onYearMonthChanged();
    void onCalculate();
    void onSaveSettings();
    void onResetMonth();
    void onDayClicked(const QDate& d);
    void onDayContextMenu(const QDate& d, const QPoint& globalPos);
    void onFetchHolidaysOnline();
    void onExportPdf();
    void onResetAll();

private:
    void setupUI();
    void initDb();
    void loadAttendanceForCurrentMonth();
    void saveDayMark(const QDate& d, DayKind k);
    void clearDayMark(const QDate& d);
    void applyHolidayCalendar();   // 把当年法定节假日 / 调休写到 calendar 的 baseKinds
    void loadSettings();
    void saveSettings();
    void recomputeAndShow();
    bool loadHolidaysFromCache(int year);   // 读 AppDataLocation/holiday-cn/<year>.json
    bool saveHolidaysToCache(int year, const QByteArray& json);

    struct Snapshot {
        int workdaysExpected = 0, holidayDays = 0, workedDays = 0, leaveDays = 0, overtimeDays = 0;
        double base = 0, dailyRate = 0, presentPay = 0, overtimePay = 0, gross = 0;
        double cbase = 0, cbFloor = 0, cbCeil = 0;
        double pPension = 0, pMedical = 0, pUnemploy = 0, pHousing = 0, personSocialTotal = 0;
        double cPension = 0, cMedical = 0, cUnemploy = 0, cInjury = 0, cMaternity = 0;
        double cHousing = 0, companySocialTotal = 0;
        double taxFree = 0, otherDeduct = 0, taxable = 0, taxRate = 0, taxQuick = 0, tax = 0;
        double net = 0, totalCost = 0, otRate = 0;
        bool autoBase = true;
        bool rangeLimited = false;
        int rangeStart = 0, rangeEnd = 0;
    };
    Snapshot m_snap;
    QString buildBreakdownHtml(bool includeCompany) const;

    // UI
    QSpinBox* m_yearSpin = nullptr;
    QSpinBox* m_monthSpin = nullptr;
    CalendarWidget* m_calendar = nullptr;

    // 工作时间范围（中途入职 / 离职 / 部分月份）
    QCheckBox* m_rangeLimitCheck = nullptr;
    QSpinBox* m_rangeStartDay = nullptr;   // 1-31
    QSpinBox* m_rangeEndDay = nullptr;     // 1-31

    QDoubleSpinBox* m_baseSalary = nullptr;
    // 缴费基数（社保/公积金按这个算，不是月薪）
    QDoubleSpinBox* m_contribBase = nullptr;     // 实际缴费基数
    QDoubleSpinBox* m_baseFloor = nullptr;       // 下限（如武汉 2026 = 4494）
    QDoubleSpinBox* m_baseCeiling = nullptr;     // 上限
    QCheckBox* m_baseAutoCheck = nullptr;        // 跟随月薪（钳制到下限/上限）

    // 个人五险 + 公积金比例
    QDoubleSpinBox* m_personPension = nullptr;     // 养老 8%
    QDoubleSpinBox* m_personMedical = nullptr;     // 医疗 2%
    QDoubleSpinBox* m_personUnemploy = nullptr;    // 失业 0.3%
    QDoubleSpinBox* m_personHousing = nullptr;     // 公积金 12%

    // 单位五险 + 公积金（仅参考，不影响个人到手）
    QDoubleSpinBox* m_companyPension = nullptr;    // 16%
    QDoubleSpinBox* m_companyMedical = nullptr;    // 8%
    QDoubleSpinBox* m_companyUnemploy = nullptr;   // 0.7%
    QDoubleSpinBox* m_companyInjury = nullptr;     // 0.5%
    QDoubleSpinBox* m_companyMaternity = nullptr;  // 0.7%
    QDoubleSpinBox* m_companyHousing = nullptr;    // 12%

    QDoubleSpinBox* m_taxThreshold = nullptr;    // 个税起征点（默认 5000）
    QDoubleSpinBox* m_otherDeduct = nullptr;     // 其他专项扣除
    QDoubleSpinBox* m_overtimeRate = nullptr;    // 加班费倍率（默认 2.0）

    QLabel* m_totalDaysLabel = nullptr;
    QLabel* m_workDaysLabel = nullptr;
    QLabel* m_holidayDaysLabel = nullptr;
    QLabel* m_workedLabel = nullptr;
    QLabel* m_leaveLabel = nullptr;
    QLabel* m_overtimeLabel = nullptr;

    QLabel* m_grossLabel = nullptr;
    QLabel* m_personPensionLabel = nullptr;
    QLabel* m_personMedicalLabel = nullptr;
    QLabel* m_personUnemployLabel = nullptr;
    QLabel* m_personHousingLabel = nullptr;
    QLabel* m_personSocialSubtotal = nullptr;
    QLabel* m_taxableLabel = nullptr;
    QLabel* m_taxLabel = nullptr;
    QLabel* m_netLabel = nullptr;
    QLabel* m_companySocialLabel = nullptr;     // 单位承担合计
    QLabel* m_totalCostLabel = nullptr;         // 单位总成本

    QPushButton* m_calcBtn = nullptr;
    QPushButton* m_saveBtn = nullptr;
    QPushButton* m_resetBtn = nullptr;
    QPushButton* m_resetAllBtn = nullptr;
    QPushButton* m_fetchHolidayBtn = nullptr;
    QPushButton* m_pdfBtn = nullptr;
    QLabel* m_holidaySourceLabel = nullptr;
    QTextEdit* m_breakdownText = nullptr;

    SqliteWrapper::SqliteManager* m_db = nullptr;
    QNetworkAccessManager* m_net = nullptr;
    QSet<int> m_autoFetchedYears;   // 本会话已尝试在线拉取过的年份
};

#endif // SALARYCALC_H
