#include "datetimeutil.h"

#include "../../common/datetime-utils/datetimeutils.h"

#include <QLabel>
#include <QTimer>
#include <QLineEdit>

#include <QComboBox>

#include <QTimeZone>

REGISTER_DYNAMICOBJECT(DateTimeUtil);

QString getTimezone() {
    // 获取当前系统时区
    QTimeZone systemTimeZone = QTimeZone::systemTimeZone();

    // 获取时区的标识符（例如：Asia/Shanghai）
    QByteArray timeZoneId = systemTimeZone.id();

    // 获取时区的显示名称（例如：中国标准时间）
    QString timeZoneName = systemTimeZone.displayName(QTimeZone::StandardTime);

    // 输出时区信息
    qDebug() << "时区标识符:" << timeZoneId;
    qDebug() << "时区名称:" << timeZoneName;

    return QString("时区标识符: ") + timeZoneId + QString("   时区名称: ") + timeZoneName;
}


DateTimeUtil::DateTimeUtil() : QWidget(nullptr), DynamicObjectBase()
{
    QVBoxLayout * layout = new QVBoxLayout;
    QHBoxLayout * displayCurrent = new QHBoxLayout;
    layout->setContentsMargins(0,0,0,0);
    // 创建 QLabel 来显示秒数
    QLabel * timeLabel = new QLabel(this);
    timeLabel->setText("Seconds: 0");

    timeLabel->setFixedSize(400, 30);

    this->timeLabel = timeLabel;

    // 创建 QTimer，每秒触发一次 timeout 信号
    QTimer * timer = new QTimer(this);

    this->timer = timer;
    timer->setInterval(1000); // 设置为1秒
    connect(timer, &QTimer::timeout, this, &DateTimeUtil::updateTime);

    updateTime();
    // timeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);

    displayCurrent->addWidget(timeLabel);
    displayCurrent->setAlignment(Qt::AlignLeft);

    // 初始化秒数
    seconds = 0;

    // 启动定时器
    timer->start();

    layout->addLayout(displayCurrent);


    ///////////////////////////////////////
    /// 时间戳转日期
    ///
    /////////////////////////////////////
    ///


    QHBoxLayout * timestampToDate = new QHBoxLayout;


    QLabel * label = new QLabel("Unix时间戳（Unix timestamp）");
    label->setFixedSize(200, 30);
    timestampToDate->addWidget(label);
    timestampToDate->setAlignment(Qt::AlignLeft);




    QLineEdit * timestampInput = new QLineEdit;


    connect(timestampInput, SIGNAL(textChanged(const QString&)), this, SLOT(timestampTextChangedSlot(const QString&)));


    QHBoxLayout * zoneLayout = new QHBoxLayout;

    QLabel * timeZoneLabel = new QLabel(getTimezone());
    timeZoneLabel->setFixedSize(400, 30);
    zoneLayout->addWidget(timeZoneLabel);
    zoneLayout->setAlignment(Qt::AlignLeft);

    layout->addLayout(zoneLayout);


    layout->addLayout(timestampToDate);
    // 去掉下划线
    timestampInput->setStyleSheet("QLineEdit { border: none; }");

    timestampInput->setFixedSize(200, 30);
    timestampToDate->addWidget(timestampInput);





    // 创建一个 QComboBox 控件
    QComboBox * comboBox = new QComboBox;

    // 添加选项到下拉列表
    comboBox->addItem("秒");
    comboBox->addItem("毫秒");

    // 设置默认选中项
    comboBox->setCurrentIndex(0);

    comboBox->setFixedSize(50, 30);

    // 将 QComboBox 添加到布局
    timestampToDate->addWidget(comboBox);

    this->timestampToDateOption = comboBox;

    QLabel * label2 = new QLabel("转换为日期格式");
    label2->setFixedSize(200, 30);
    timestampToDate->addWidget(label2);


    QLineEdit * dateDisplay = new QLineEdit;

    // 去掉下划线
    dateDisplay->setStyleSheet("QLineEdit { border: none; }");

    dateDisplay->setFixedSize(200, 30);
    dateDisplay->setReadOnly(true);
    timestampToDate->addWidget(dateDisplay);

    this->dateDisplay = dateDisplay;


    ///////////////////////////////////////
    /// 日期转时间戳
    ///
    /////////////////////////////////////
    ///
    QHBoxLayout * dateToTimestamp = new QHBoxLayout;


    QLabel * label1 = new QLabel("时间（年-月-日 时:分:秒）");
    label1->setFixedSize(200, 30);
    dateToTimestamp->addWidget(label1);
    dateToTimestamp->setAlignment(Qt::AlignLeft);


    layout->addLayout(dateToTimestamp);

    QLineEdit * dateInput = new QLineEdit;

    this->dateInput = dateInput;


    // 去掉下划线
    dateInput->setStyleSheet("QLineEdit { border: none; }");

    dateInput->setFixedSize(200, 30);
    dateToTimestamp->addWidget(dateInput);



    QLabel * label3 = new QLabel("转换为UNIX时间戳");
    label3->setFixedSize(200, 30);
    dateToTimestamp->addWidget(label3);


    QLineEdit * timestampDisplay = new QLineEdit;

    // 去掉下划线
    timestampDisplay->setStyleSheet("QLineEdit { border: none; }");
    timestampDisplay->setFixedSize(200, 30);
    timestampDisplay->setReadOnly(true);
    dateToTimestamp->addWidget(timestampDisplay);

    this->timestampDisplay = timestampDisplay;

    // 创建一个 QComboBox 控件
    QComboBox * comboBox1 = new QComboBox;

    // 添加选项到下拉列表
    comboBox1->addItem("秒");
    comboBox1->addItem("毫秒");

    // 设置默认选中项
    comboBox1->setCurrentIndex(0);

    comboBox1->setFixedSize(50, 30);

    // 将 QComboBox 添加到布局
    dateToTimestamp->addWidget(comboBox1);

    this->dateToTimestampOption = comboBox1;

    connect(dateInput, SIGNAL(textChanged(QString)), this, SLOT(dateTextChangedSlot(QString)));

    layout->setAlignment(Qt::AlignTop);

    this->setLayout(layout);
}

void DateTimeUtil::timestampTextChangedSlot(const QString& currentTimestamp) {

    // 先清空
//    this->dateDisplay->setText("");

    qDebug() << "当前的时间戳: " << currentTimestamp;

    QString dateStr = secondTimestampToDate(currentTimestamp);
    this->dateDisplay->setText(dateStr);
}

void DateTimeUtil::dateTextChangedSlot(const QString& dateInput) {

    QString timestampString = dateToSecondTimestamp(dateInput);

    this->timestampDisplay->setText(timestampString);


}

void DateTimeUtil::updateTime()
{
    QDateTime currentDateTime = QDateTime::currentDateTime();

    // 获取当前时间戳（毫秒）
    qint64 timestampMillis = currentDateTime.toMSecsSinceEpoch();

    // 获取当前时间戳（秒）
    qint64 timestampSeconds = currentDateTime.toSecsSinceEpoch();

    // 格式化日期时间为年月日时分秒格式
    QString formattedDateTime = currentDateTime.toString("yyyy-MM-dd hh:mm:ss");

    // 输出时间戳
    qDebug() << "Timestamp in milliseconds:" << timestampMillis;
    qDebug() << "Timestamp in seconds:" << timestampSeconds;

    timeLabel->setText("当前时间戳: " + QString::number(timestampSeconds) + "    当前日期：" + formattedDateTime);
}

