
#ifndef DATETIMEUTIL_H
#define DATETIMEUTIL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QTimer>
#include <QTabWidget>
#include <QGroupBox>
#include <QDateTime>
#include <QTimeZone>
#include <QApplication>
#include <QClipboard>
#include <QDebug>

#include "../../common/dynamicobjectbase.h"

class DateTimeUtil : public QWidget, public DynamicObjectBase {
    Q_OBJECT

public:
    explicit DateTimeUtil();

private slots:
    void updateCurrentTime();
    void onTimestampChanged();
    void onDateTimeChanged();
    void onTimezoneChanged();
    void copyCurrentTimestamp();
    void copyCurrentDateTime();
    void copyTimestampResult();
    void copyDateTimeResult();
    void insertCurrentTimestamp();
    void insertCurrentDateTime();
    void processBatchConversion();
    void calculateTimeDifference();

private:
    // UI组件
    QTimer *updateTimer;
    
    // 当前时间页面
    QLabel *currentTimeLabel;
    QLabel *currentTimestampLabel;
    QComboBox *timezoneCombo;
    
    // 转换页面
    QLineEdit *timestampInput;
    QComboBox *timestampUnitCombo;
    QPushButton *insertCurrentTsBtn;
    QTextEdit *timestampResult;
    QPushButton *copyTimestampBtn;
    
    QDateTimeEdit *dateTimeInput;
    QComboBox *dateTimeUnitCombo;
    QPushButton *insertCurrentDtBtn;
    QLineEdit *dateTimeResult;
    QPushButton *copyDateTimeBtn;
    
    // 批量转换页面
    QComboBox *batchModeCombo;
    QTextEdit *batchInput;
    QTextEdit *batchOutput;
    
    // 时间计算页面
    QDateTimeEdit *startDateTime;
    QDateTimeEdit *endDateTime;
    QTextEdit *timeDiffResult;
    
    // 格式示例页面
    QTextEdit *formatExamples;
    
    void setupUI();
    void setupCurrentTimeSection(QWidget *parent);
    void setupConversionSection(QWidget *parent);
    void setupBatchSection(QWidget *parent);
    void setupCalculationSection(QWidget *parent);
    void setupFormatSection(QWidget *parent);
    void populateTimezones();
    void updateTimestampConversion();
    void updateDateTimeConversion();
    QStringList getCommonFormats(const QDateTime &dateTime);
    void showMessage(const QString &message);
};

#endif // DATETIMEUTIL_H
