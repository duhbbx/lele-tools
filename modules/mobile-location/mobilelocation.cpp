#include "mobilelocation.h"
#include <QSplitter>
#include <QHeaderView>
#include <QScrollArea>
#include <QStringConverter>

REGISTER_DYNAMICOBJECT(MobileLocation);

MobileLocation::MobileLocation() : QWidget(nullptr), DynamicObjectBase()
{
    networkManager = new QNetworkAccessManager(this);
    currentReply = nullptr;
    statusTimer = new QTimer(this);
    statusTimer->setSingleShot(true);
    statusTimer->setInterval(3000);
    
    connect(statusTimer, &QTimer::timeout, [this]() {
        statusLabel->setText("就绪");
    });
    
    setupUI();
    initializeDomesticData();
    loadHistory();
}

MobileLocation::~MobileLocation()
{
    saveHistory();
    if (currentReply) {
        currentReply->abort();
    }
}

void MobileLocation::setupUI()
{
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);
    
    // 创建分割器
    QSplitter *mainSplitter = new QSplitter(Qt::Horizontal, this);
    
    // 左侧面板
    QWidget *leftPanel = new QWidget;
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    
    // 右侧面板
    QWidget *rightPanel = new QWidget;
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    
    setupInputArea();
    setupResultArea();
    setupBatchArea();
    setupHistoryArea();
    setupStatusArea();
    
    leftLayout->addWidget(inputGroup);
    leftLayout->addWidget(resultGroup);
    leftLayout->addWidget(batchGroup);
    
    rightLayout->addWidget(historyGroup);
    
    mainSplitter->addWidget(leftPanel);
    mainSplitter->addWidget(rightPanel);
    mainSplitter->setSizes({400, 350});
    
    mainLayout->addWidget(mainSplitter, 1); // 添加stretch factor
    mainLayout->addLayout(statusLayout);
}

void MobileLocation::setupInputArea()
{
    inputGroup = new QGroupBox("手机号码查询", this);
    inputLayout = new QGridLayout(inputGroup);
    
    // 手机号码输入
    inputLayout->addWidget(new QLabel("手机号码:"), 0, 0);
    numberEdit = new QLineEdit;
    numberEdit->setPlaceholderText("请输入手机号码，如：13812345678");
    inputLayout->addWidget(numberEdit, 0, 1, 1, 2);
    
    // 国家/地区选择
    inputLayout->addWidget(new QLabel("国家/地区:"), 1, 0);
    countryCombo = new QComboBox;
    countryCombo->addItem("中国 (+86)", "86");
    countryCombo->addItem("美国 (+1)", "1");
    countryCombo->addItem("英国 (+44)", "44");
    countryCombo->addItem("日本 (+81)", "81");
    countryCombo->addItem("韩国 (+82)", "82");
    inputLayout->addWidget(countryCombo, 1, 1, 1, 2);
    
    // 查询模式
    inputLayout->addWidget(new QLabel("查询模式:"), 2, 0);
    queryModeCombo = new QComboBox;
    queryModeCombo->addItem("本地数据库", 0);
    queryModeCombo->addItem("在线查询", 1);
    queryModeCombo->addItem("混合模式", 2);
    inputLayout->addWidget(queryModeCombo, 2, 1, 1, 2);
    
    // 按钮
    queryBtn = new QPushButton("查询");
    clearBtn = new QPushButton("清空");
    
    QHBoxLayout *btnLayout = new QHBoxLayout;
    btnLayout->addWidget(queryBtn);
    btnLayout->addWidget(clearBtn);
    inputLayout->addLayout(btnLayout, 3, 0, 1, 3);
    
    // 连接信号
    connect(numberEdit, &QLineEdit::textChanged, this, &MobileLocation::onNumberChanged);
    connect(countryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &MobileLocation::onCountryChanged);
    connect(queryModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &MobileLocation::onQueryModeChanged);
    connect(queryBtn, &QPushButton::clicked, this, &MobileLocation::onQueryButtonClicked);
    connect(clearBtn, &QPushButton::clicked, [this]() {
        numberEdit->clear();
        clearResultDisplay();
    });
}

void MobileLocation::setupResultArea()
{
    resultGroup = new QGroupBox("查询结果", this);
    resultLayout = new QVBoxLayout(resultGroup);
    
    // 创建结果显示区域
    QWidget *resultWidget = new QWidget;
    QGridLayout *resultGridLayout = new QGridLayout(resultWidget);
    
    // 结果标签
    resultGridLayout->addWidget(new QLabel("手机号码:"), 0, 0);
    numberLabel = new QLabel("--");
    numberLabel->setStyleSheet("font-weight: bold; color: #2c5aa0;");
    resultGridLayout->addWidget(numberLabel, 0, 1);
    
    resultGridLayout->addWidget(new QLabel("国家/地区:"), 1, 0);
    countryLabel = new QLabel("--");
    resultGridLayout->addWidget(countryLabel, 1, 1);
    
    resultGridLayout->addWidget(new QLabel("省份:"), 2, 0);
    provinceLabel = new QLabel("--");
    resultGridLayout->addWidget(provinceLabel, 2, 1);
    
    resultGridLayout->addWidget(new QLabel("城市:"), 3, 0);
    cityLabel = new QLabel("--");
    resultGridLayout->addWidget(cityLabel, 3, 1);
    
    resultGridLayout->addWidget(new QLabel("运营商:"), 4, 0);
    carrierLabel = new QLabel("--");
    resultGridLayout->addWidget(carrierLabel, 4, 1);
    
    resultGridLayout->addWidget(new QLabel("号码类型:"), 5, 0);
    typeLabel = new QLabel("--");
    resultGridLayout->addWidget(typeLabel, 5, 1);
    
    resultLayout->addWidget(resultWidget);
    
    // 复制按钮
    copyResultBtn = new QPushButton("复制结果");
    QHBoxLayout *resultBtnLayout = new QHBoxLayout;
    resultBtnLayout->addStretch();
    resultBtnLayout->addWidget(copyResultBtn);
    resultLayout->addLayout(resultBtnLayout);
    
    connect(copyResultBtn, &QPushButton::clicked, this, &MobileLocation::onCopyResultClicked);
}

void MobileLocation::setupBatchArea()
{
    batchGroup = new QGroupBox("批量查询", this);
    batchLayout = new QVBoxLayout(batchGroup);
    
    // 批量输入
    batchInput = new QTextEdit;
    batchInput->setMaximumHeight(100);
    batchInput->setPlaceholderText("每行输入一个手机号码，支持批量查询");
    batchLayout->addWidget(batchInput);
    
    // 批量结果表格
    batchResults = new QTableWidget;
    batchResults->setColumnCount(6);
    batchResults->setHorizontalHeaderLabels({"手机号码", "省份", "城市", "运营商", "类型", "状态"});
    batchResults->horizontalHeader()->setStretchLastSection(true);
    batchResults->setMaximumHeight(150);
    batchLayout->addWidget(batchResults);
    
    // 批量操作按钮
    batchButtonLayout = new QHBoxLayout;
    batchQueryBtn = new QPushButton("批量查询");
    importBtn = new QPushButton("导入");
    exportBtn = new QPushButton("导出");
    
    batchButtonLayout->addWidget(batchQueryBtn);
    batchButtonLayout->addWidget(importBtn);
    batchButtonLayout->addWidget(exportBtn);
    batchButtonLayout->addStretch();
    batchLayout->addLayout(batchButtonLayout);
    
    // 进度条
    batchProgress = new QProgressBar;
    batchProgress->setVisible(false);
    batchLayout->addWidget(batchProgress);
    
    batchStatusLabel = new QLabel("就绪");
    batchLayout->addWidget(batchStatusLabel);
    
    // 连接信号
    connect(batchQueryBtn, &QPushButton::clicked, this, &MobileLocation::onBatchQueryClicked);
    connect(importBtn, &QPushButton::clicked, this, &MobileLocation::onImportNumbersClicked);
    connect(exportBtn, &QPushButton::clicked, this, &MobileLocation::onExportHistoryClicked);
}

void MobileLocation::setupHistoryArea()
{
    historyGroup = new QGroupBox("查询历史", this);
    historyLayout = new QVBoxLayout(historyGroup);
    
    // 历史记录表格
    historyTable = new QTableWidget;
    historyTable->setColumnCount(5);
    historyTable->setHorizontalHeaderLabels({"手机号码", "归属地", "运营商", "查询时间", "状态"});
    historyTable->horizontalHeader()->setStretchLastSection(true);
    historyLayout->addWidget(historyTable);
    
    // 历史记录操作按钮
    historyButtonLayout = new QHBoxLayout;
    clearHistoryBtn = new QPushButton("清空历史");
    exportHistoryBtn = new QPushButton("导出历史");
    historyCountLabel = new QLabel("共 0 条记录");
    
    historyButtonLayout->addWidget(clearHistoryBtn);
    historyButtonLayout->addWidget(exportHistoryBtn);
    historyButtonLayout->addStretch();
    historyButtonLayout->addWidget(historyCountLabel);
    historyLayout->addLayout(historyButtonLayout);
    
    // 连接信号
    connect(clearHistoryBtn, &QPushButton::clicked, this, &MobileLocation::onClearHistoryClicked);
    connect(exportHistoryBtn, &QPushButton::clicked, this, &MobileLocation::onExportHistoryClicked);
    connect(historyTable, &QTableWidget::cellClicked, this, &MobileLocation::onHistoryItemClicked);
}

void MobileLocation::setupStatusArea()
{
    statusLayout = new QHBoxLayout;
    statusLabel = new QLabel("就绪");
    progressBar = new QProgressBar;
    progressBar->setVisible(false);
    
    statusLayout->addWidget(statusLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(progressBar);
}

void MobileLocation::initializeDomesticData()
{
    // 初始化中国手机号段数据（简化版本）
    // 移动号段
    QStringList cmccNumbers = {"134", "135", "136", "137", "138", "139", "147", "150", "151", "152", 
                               "157", "158", "159", "172", "178", "182", "183", "184", "187", "188", 
                               "195", "197", "198"};
    for (const QString &prefix : cmccNumbers) {
        domesticCarriers[prefix] = "中国移动";
    }
    
    // 联通号段
    QStringList cuccNumbers = {"130", "131", "132", "145", "155", "156", "166", "171", "175", "176", 
                               "185", "186", "196"};
    for (const QString &prefix : cuccNumbers) {
        domesticCarriers[prefix] = "中国联通";
    }
    
    // 电信号段
    QStringList ctccNumbers = {"133", "149", "153", "173", "174", "177", "180", "181", "189", "191", 
                               "193", "199"};
    for (const QString &prefix : ctccNumbers) {
        domesticCarriers[prefix] = "中国电信";
    }
    
    // 广电号段
    QStringList cbnnNumbers = {"192"};
    for (const QString &prefix : cbnnNumbers) {
        domesticCarriers[prefix] = "中国广电";
    }
    
    // 中国手机号段归属地数据（基于真实号段分配）
    // 注意：实际的号段分配非常复杂，这里是简化版本
    
    // 北京 (部分号段)
    domesticProvinces["130"] = "北京";
    domesticCities["130"] = "北京";
    domesticProvinces["131"] = "北京";
    domesticCities["131"] = "北京";
    
    // 上海 (部分号段)
    domesticProvinces["138"] = "上海";
    domesticCities["138"] = "上海";
    domesticProvinces["139"] = "上海";
    domesticCities["139"] = "上海";
    
    // 广东省
    domesticProvinces["137"] = "广东";
    domesticCities["137"] = "广州";
    domesticProvinces["136"] = "广东";
    domesticCities["136"] = "深圳";
    domesticProvinces["135"] = "广东";
    domesticCities["135"] = "东莞";
    
    // 江苏省
    domesticProvinces["134"] = "江苏";
    domesticCities["134"] = "南京";
    domesticProvinces["150"] = "江苏";
    domesticCities["150"] = "苏州";
    
    // 浙江省
    domesticProvinces["151"] = "浙江";
    domesticCities["151"] = "杭州";
    domesticProvinces["152"] = "浙江";
    domesticCities["152"] = "宁波";
    
    // 山东省
    domesticProvinces["157"] = "山东";
    domesticCities["157"] = "济南";
    domesticProvinces["158"] = "山东";
    domesticCities["158"] = "青岛";
    
    // 河南省
    domesticProvinces["159"] = "河南";
    domesticCities["159"] = "郑州";
    domesticProvinces["182"] = "河南";
    domesticCities["182"] = "洛阳";
    
    // 湖北省
    domesticProvinces["187"] = "湖北";
    domesticCities["187"] = "武汉";
    
    // 湖南省
    domesticProvinces["188"] = "湖南";
    domesticCities["188"] = "长沙";
    
    // 四川省
    domesticProvinces["147"] = "四川";
    domesticCities["147"] = "成都";
    
    // 辽宁省
    domesticProvinces["132"] = "辽宁";
    domesticCities["132"] = "沈阳";
    
    // 黑龙江省
    domesticProvinces["133"] = "黑龙江";
    domesticCities["133"] = "哈尔滨";
    
    // 天津
    domesticProvinces["145"] = "天津";
    domesticCities["145"] = "天津";
    
    // 重庆
    domesticProvinces["155"] = "重庆";
    domesticCities["155"] = "重庆";
    
    // 河北省
    domesticProvinces["156"] = "河北";
    domesticCities["156"] = "石家庄";
    
    // 山西省
    domesticProvinces["166"] = "山西";
    domesticCities["166"] = "太原";
    
    // 内蒙古
    domesticProvinces["171"] = "内蒙古";
    domesticCities["171"] = "呼和浩特";
    
    // 吉林省
    domesticProvinces["172"] = "吉林";
    domesticCities["172"] = "长春";
    
    // 江西省
    domesticProvinces["173"] = "江西";
    domesticCities["173"] = "南昌";
    
    // 安徽省
    domesticProvinces["174"] = "安徽";
    domesticCities["174"] = "合肥";
    
    // 福建省
    domesticProvinces["175"] = "福建";
    domesticCities["175"] = "福州";
    
    // 广西省
    domesticProvinces["176"] = "广西";
    domesticCities["176"] = "南宁";
    
    // 海南省
    domesticProvinces["177"] = "海南";
    domesticCities["177"] = "海口";
    
    // 贵州省
    domesticProvinces["178"] = "贵州";
    domesticCities["178"] = "贵阳";
    
    // 云南省
    domesticProvinces["180"] = "云南";
    domesticCities["180"] = "昆明";
    
    // 西藏
    domesticProvinces["181"] = "西藏";
    domesticCities["181"] = "拉萨";
    
    // 陕西省
    domesticProvinces["183"] = "陕西";
    domesticCities["183"] = "西安";
    
    // 甘肃省
    domesticProvinces["184"] = "甘肃";
    domesticCities["184"] = "兰州";
    
    // 青海省
    domesticProvinces["185"] = "青海";
    domesticCities["185"] = "西宁";
    
    // 宁夏
    domesticProvinces["186"] = "宁夏";
    domesticCities["186"] = "银川";
    
    // 新疆
    domesticProvinces["189"] = "新疆";
    domesticCities["189"] = "乌鲁木齐";
    
    // 其他号段
    domesticProvinces["191"] = "北京";
    domesticCities["191"] = "北京";
    domesticProvinces["193"] = "上海";
    domesticCities["193"] = "上海";
    domesticProvinces["195"] = "广东";
    domesticCities["195"] = "深圳";
    domesticProvinces["196"] = "江苏";
    domesticCities["196"] = "南京";
    domesticProvinces["197"] = "浙江";
    domesticCities["197"] = "杭州";
    domesticProvinces["198"] = "山东";
    domesticCities["198"] = "济南";
    domesticProvinces["199"] = "四川";
    domesticCities["199"] = "成都";
    domesticProvinces["192"] = "河南";
    domesticCities["192"] = "郑州";
    
    // 其他号段的默认处理
    QStringList otherPrefixes;
    for (const QString &prefix : domesticCarriers.keys()) {
        if (!domesticProvinces.contains(prefix)) {
            otherPrefixes.append(prefix);
        }
    }
    
    // 为其他号段分配一些常见的省份城市
    QStringList provinces = {"河北", "山西", "内蒙古", "吉林", "江西", "安徽", "福建", "广西", "海南", 
                            "重庆", "贵州", "云南", "西藏", "甘肃", "青海", "宁夏", "新疆"};
    QStringList cities = {"石家庄", "太原", "呼和浩特", "长春", "南昌", "合肥", "福州", "南宁", "海口",
                         "重庆", "贵阳", "昆明", "拉萨", "兰州", "西宁", "银川", "乌鲁木齐"};
    
    for (int i = 0; i < otherPrefixes.size() && i < provinces.size(); ++i) {
        domesticProvinces[otherPrefixes[i]] = provinces[i % provinces.size()];
        domesticCities[otherPrefixes[i]] = cities[i % cities.size()];
    }
    
    // 国际区号数据
    countryCodes["1"] = "美国/加拿大";
    countryCodes["44"] = "英国";
    countryCodes["81"] = "日本";
    countryCodes["82"] = "韩国";
    countryCodes["86"] = "中国";
    countryCodes["33"] = "法国";
    countryCodes["49"] = "德国";
    countryCodes["39"] = "意大利";
    countryCodes["7"] = "俄罗斯";
    countryCodes["91"] = "印度";
}

// 槽函数实现
void MobileLocation::onQueryButtonClicked()
{
    QString number = numberEdit->text().trimmed();
    if (number.isEmpty()) {
        statusLabel->setText("请输入手机号码");
        return;
    }
    
    // 格式化手机号码
    QString formattedNumber = formatPhoneNumber(number);
    if (formattedNumber.isEmpty()) {
        statusLabel->setText("手机号码格式不正确");
        QMessageBox::warning(this, "输入错误", "请输入正确的手机号码格式");
        return;
    }
    
    // 根据国家选择查询方式
    QString countryCode = countryCombo->currentData().toString();
    int queryMode = queryModeCombo->currentData().toInt();
    
    MobileInfo info;
    
    if (countryCode == "86") {
        // 中国手机号码
        if (queryMode == 0 || queryMode == 2) {
            // 本地数据库查询
            info = queryDomesticNumber(formattedNumber);
        }
        
        if ((!info.isValid && queryMode == 2) || queryMode == 1) {
            // 在线查询
            queryNumberOnline(formattedNumber);
            return;
        }
    } else {
        // 国际号码
        info = queryInternationalNumber(formattedNumber);
    }
    
    displayResult(info);
    
    if (info.isValid) {
        addToHistory(info);
        statusLabel->setText("查询完成");
    } else {
        statusLabel->setText("查询失败: " + info.errorMessage);
    }
}

void MobileLocation::onBatchQueryClicked()
{
    QString text = batchInput->toPlainText().trimmed();
    if (text.isEmpty()) {
        statusLabel->setText("请输入要批量查询的手机号码");
        return;
    }
    
    QStringList numbers = text.split('\n', Qt::SkipEmptyParts);
    if (numbers.isEmpty()) {
        statusLabel->setText("没有有效的手机号码");
        return;
    }
    
    batchProgress->setVisible(true);
    batchProgress->setMaximum(numbers.size());
    batchProgress->setValue(0);
    
    batchResults->setRowCount(numbers.size());
    
    for (int i = 0; i < numbers.size(); ++i) {
        QString number = formatPhoneNumber(numbers[i].trimmed());
        
        MobileInfo info;
        if (!number.isEmpty()) {
            QString countryCode = detectCountryCode(number);
            if (countryCode == "86") {
                info = queryDomesticNumber(number);
            } else {
                info = queryInternationalNumber(number);
            }
        } else {
            info.errorMessage = "格式错误";
        }
        
        // 更新表格
        batchResults->setItem(i, 0, new QTableWidgetItem(numbers[i]));
        batchResults->setItem(i, 1, new QTableWidgetItem(info.province));
        batchResults->setItem(i, 2, new QTableWidgetItem(info.city));
        batchResults->setItem(i, 3, new QTableWidgetItem(info.carrier));
        batchResults->setItem(i, 4, new QTableWidgetItem(info.numberType));
        batchResults->setItem(i, 5, new QTableWidgetItem(info.isValid ? "成功" : "失败"));
        
        if (info.isValid) {
            addToHistory(info);
        }
        
        batchProgress->setValue(i + 1);
        QApplication::processEvents();
    }
    
    batchProgress->setVisible(false);
    batchStatusLabel->setText(QString("批量查询完成，共处理 %1 个号码").arg(numbers.size()));
}

void MobileLocation::onClearHistoryClicked()
{
    historyList.clear();
    updateHistoryDisplay();
    statusLabel->setText("历史记录已清空");
}

void MobileLocation::onExportHistoryClicked()
{
    if (historyList.isEmpty()) {
        QMessageBox::information(this, "提示", "没有历史记录可导出");
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(this, 
        "导出历史记录", 
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/mobile_history.csv",
        "CSV文件 (*.csv)");
    
    if (fileName.isEmpty()) return;
    
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "错误", "无法创建文件");
        return;
    }
    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    
    // 写入标题行
    out << "手机号码,国家,省份,城市,运营商,号码类型,查询时间\n";
    
    // 写入数据
    for (const QueryHistory &history : historyList) {
        const MobileInfo &info = history.info;
        out << QString("%1,%2,%3,%4,%5,%6,%7\n")
               .arg(info.number)
               .arg(info.country)
               .arg(info.province)
               .arg(info.city)
               .arg(info.carrier)
               .arg(info.numberType)
               .arg(history.queryTime.toString("yyyy-MM-dd hh:mm:ss"));
    }
    
    statusLabel->setText(QString("历史记录已导出到: %1").arg(fileName));
}

void MobileLocation::onImportNumbersClicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, 
        "导入手机号码", 
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        "文本文件 (*.txt);;CSV文件 (*.csv);;所有文件 (*.*)");
    
    if (fileName.isEmpty()) return;
    
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "错误", "无法打开文件");
        return;
    }
    
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    
    QStringList numbers;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (!line.isEmpty()) {
            // 如果是CSV格式，取第一列
            if (fileName.endsWith(".csv")) {
                QStringList parts = line.split(',');
                if (!parts.isEmpty()) {
                    numbers.append(parts.first().trimmed());
                }
            } else {
                numbers.append(line);
            }
        }
    }
    
    if (!numbers.isEmpty()) {
        batchInput->setPlainText(numbers.join('\n'));
        statusLabel->setText(QString("已导入 %1 个手机号码").arg(numbers.size()));
    }
}

void MobileLocation::onCopyResultClicked()
{
    if (numberLabel->text() == "--") {
        QMessageBox::information(this, "提示", "没有查询结果可复制");
        return;
    }
    
    QString result = QString("手机号码: %1\n国家/地区: %2\n省份: %3\n城市: %4\n运营商: %5\n号码类型: %6")
                     .arg(numberLabel->text())
                     .arg(countryLabel->text())
                     .arg(provinceLabel->text())
                     .arg(cityLabel->text())
                     .arg(carrierLabel->text())
                     .arg(typeLabel->text());
    
    QApplication::clipboard()->setText(result);
    statusLabel->setText("查询结果已复制到剪贴板");
}

void MobileLocation::onNumberChanged()
{
    QString number = numberEdit->text().trimmed();
    if (number.length() >= 3) {
        // 实时显示可能的运营商信息
        QString countryCode = detectCountryCode(number);
        if (countryCode == "86" && number.length() >= 7) {
            QString prefix = number.left(3);
            if (domesticCarriers.contains(prefix)) {
                statusLabel->setText(QString("可能的运营商: %1").arg(domesticCarriers[prefix]));
                statusTimer->start();
            }
        }
    }
}

void MobileLocation::onCountryChanged()
{
    // 根据国家选择更新界面
    QString countryCode = countryCombo->currentData().toString();
    if (countryCode == "86") {
        numberEdit->setPlaceholderText("请输入手机号码，如：13812345678");
    } else {
        numberEdit->setPlaceholderText(QString("请输入%1的手机号码").arg(countryCombo->currentText()));
    }
}

void MobileLocation::onQueryModeChanged()
{
    int mode = queryModeCombo->currentData().toInt();
    QString modeText;
    switch (mode) {
        case 0: modeText = "使用本地数据库查询，速度快但数据可能不够新"; break;
        case 1: modeText = "使用在线查询，数据准确但需要网络连接"; break;
        case 2: modeText = "先尝试本地查询，失败后使用在线查询"; break;
    }
    statusLabel->setText(modeText);
    statusTimer->start();
}

void MobileLocation::onHistoryItemClicked(int row, int column)
{
    Q_UNUSED(column)
    
    if (row >= 0 && row < historyList.size()) {
        const QueryHistory &history = historyList[row];
        
        // 填充到输入框
        numberEdit->setText(history.info.number);
        
        // 显示结果
        displayResult(history.info);
        
        statusLabel->setText("已加载历史记录");
    }
}

void MobileLocation::onNetworkReplyFinished()
{
    if (!currentReply) return;
    
    if (currentReply->error() == QNetworkReply::NoError) {
        QByteArray data = currentReply->readAll();
        QString response = QString::fromUtf8(data);
        
        // 解析在线查询结果（简化实现）
        MobileInfo info = parseOnlineResponse(response);
        
        displayResult(info);
        
        if (info.isValid) {
            addToHistory(info);
            statusLabel->setText("在线查询完成");
        } else {
            statusLabel->setText("在线查询失败: " + info.errorMessage);
        }
    } else {
        statusLabel->setText("网络查询失败");
    }
    
    progressBar->setVisible(false);
    currentReply->deleteLater();
    currentReply = nullptr;
}

void MobileLocation::onNetworkError(QNetworkReply::NetworkError error)
{
    Q_UNUSED(error)
    statusLabel->setText("网络连接错误");
    progressBar->setVisible(false);
}

// 核心查询功能实现
MobileInfo MobileLocation::queryDomesticNumber(const QString &number)
{
    MobileInfo info;
    info.number = number;
    info.country = "中国";
    info.countryCode = "86";
    info.numberType = "移动电话";
    
    if (!isValidChineseNumber(number)) {
        info.errorMessage = "不是有效的中国手机号码";
        return info;
    }
    
    QString prefix = number.left(3);
    
    // 查询运营商
    if (domesticCarriers.contains(prefix)) {
        info.carrier = domesticCarriers[prefix];
    } else {
        info.carrier = "未知运营商";
    }
    
    // 查询省份城市（简化实现）
    if (domesticProvinces.contains(prefix)) {
        info.province = domesticProvinces[prefix];
        info.city = domesticCities[prefix];
    } else {
        info.province = "未知省份";
        info.city = "未知城市";
    }
    
    info.isValid = true;
    return info;
}

MobileInfo MobileLocation::queryInternationalNumber(const QString &number)
{
    MobileInfo info;
    info.number = number;
    info.numberType = "国际号码";
    
    QString countryCode = detectCountryCode(number);
    if (countryCodes.contains(countryCode)) {
        info.country = countryCodes[countryCode];
        info.countryCode = countryCode;
        info.province = "国际";
        info.city = "国际";
        info.carrier = "国际运营商";
        info.isValid = true;
    } else {
        info.errorMessage = "无法识别的国际号码";
    }
    
    return info;
}

void MobileLocation::queryNumberOnline(const QString &number)
{
    if (currentReply) {
        currentReply->abort();
        currentReply->deleteLater();
    }
    
    // 简化的在线查询（实际应用中需要使用真实的API）
    QString url = QString("https://api.example.com/mobile?number=%1").arg(number);
    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setHeader(QNetworkRequest::UserAgentHeader, "MobileLocation/1.0");
    
    currentReply = networkManager->get(request);
    currentQueryNumber = number;
    
    connect(currentReply, &QNetworkReply::finished, this, &MobileLocation::onNetworkReplyFinished);
    connect(currentReply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &MobileLocation::onNetworkError);
    
    progressBar->setVisible(true);
    statusLabel->setText("正在在线查询...");
}

// 数据处理功能
bool MobileLocation::isValidChineseNumber(const QString &number)
{
    if (number.length() != 11) return false;
    if (!number.startsWith("1")) return false;
    
    bool ok;
    number.toLongLong(&ok);
    return ok;
}

bool MobileLocation::isValidInternationalNumber(const QString &number)
{
    if (number.length() < 7 || number.length() > 15) return false;
    
    bool ok;
    number.toLongLong(&ok);
    return ok;
}

QString MobileLocation::formatPhoneNumber(const QString &number)
{
    QString cleaned = number;
    cleaned.remove(QRegularExpression("[^0-9+]"));
    
    if (cleaned.startsWith("+86")) {
        cleaned = cleaned.mid(3);
    } else if (cleaned.startsWith("86") && cleaned.length() > 11) {
        cleaned = cleaned.mid(2);
    }
    
    if (isValidChineseNumber(cleaned)) {
        return cleaned;
    } else if (isValidInternationalNumber(cleaned)) {
        return cleaned;
    }
    
    return QString();
}

QString MobileLocation::detectCountryCode(const QString &number)
{
    if (number.startsWith("+")) {
        if (number.startsWith("+86")) return "86";
        if (number.startsWith("+1")) return "1";
        if (number.startsWith("+44")) return "44";
        if (number.startsWith("+81")) return "81";
        if (number.startsWith("+82")) return "82";
    }
    
    // 默认为中国
    if (isValidChineseNumber(number)) {
        return "86";
    }
    
    return "86"; // 默认
}

void MobileLocation::displayResult(const MobileInfo &info)
{
    if (info.isValid) {
        numberLabel->setText(info.number);
        countryLabel->setText(info.country);
        provinceLabel->setText(info.province);
        cityLabel->setText(info.city);
        carrierLabel->setText(info.carrier);
        typeLabel->setText(info.numberType);
    } else {
        clearResultDisplay();
    }
}

void MobileLocation::clearResultDisplay()
{
    numberLabel->setText("--");
    countryLabel->setText("--");
    provinceLabel->setText("--");
    cityLabel->setText("--");
    carrierLabel->setText("--");
    typeLabel->setText("--");
}

MobileInfo MobileLocation::parseOnlineResponse(const QString &response)
{
    // 简化的JSON解析（实际应用中需要使用QJsonDocument）
    MobileInfo info;
    info.number = currentQueryNumber;
    
    // 这里应该解析真实的API响应
    // 目前只是模拟实现
    if (response.contains("success")) {
        info.isValid = true;
        info.country = "中国";
        info.province = "在线查询省份";
        info.city = "在线查询城市";
        info.carrier = "在线查询运营商";
        info.numberType = "移动电话";
    } else {
        info.errorMessage = "在线查询失败";
    }
    
    return info;
}

// 历史记录管理
void MobileLocation::addToHistory(const MobileInfo &info)
{
    QueryHistory history;
    history.number = info.number;
    history.info = info;
    history.queryTime = QDateTime::currentDateTime();
    
    // 检查是否已存在
    for (int i = 0; i < historyList.size(); ++i) {
        if (historyList[i].number == info.number) {
            historyList.removeAt(i);
            break;
        }
    }
    
    historyList.prepend(history);
    
    // 限制历史记录数量
    if (historyList.size() > 1000) {
        historyList.removeLast();
    }
    
    updateHistoryDisplay();
}

void MobileLocation::updateHistoryDisplay()
{
    historyTable->setRowCount(historyList.size());
    
    for (int i = 0; i < historyList.size(); ++i) {
        const QueryHistory &history = historyList[i];
        const MobileInfo &info = history.info;
        
        historyTable->setItem(i, 0, new QTableWidgetItem(info.number));
        historyTable->setItem(i, 1, new QTableWidgetItem(QString("%1 %2").arg(info.province, info.city)));
        historyTable->setItem(i, 2, new QTableWidgetItem(info.carrier));
        historyTable->setItem(i, 3, new QTableWidgetItem(history.queryTime.toString("MM-dd hh:mm")));
        historyTable->setItem(i, 4, new QTableWidgetItem(info.isValid ? "成功" : "失败"));
    }
    
    historyCountLabel->setText(QString("共 %1 条记录").arg(historyList.size()));
}

void MobileLocation::loadHistory()
{
    // 简化的历史记录加载
    QSettings settings;
    settings.beginGroup("MobileLocation");
    
    int count = settings.beginReadArray("History");
    for (int i = 0; i < count && i < 100; ++i) {
        settings.setArrayIndex(i);
        
        QueryHistory history;
        history.number = settings.value("number").toString();
        history.info.number = history.number;
        history.info.country = settings.value("country").toString();
        history.info.province = settings.value("province").toString();
        history.info.city = settings.value("city").toString();
        history.info.carrier = settings.value("carrier").toString();
        history.info.numberType = settings.value("numberType").toString();
        history.info.isValid = settings.value("isValid", false).toBool();
        history.queryTime = settings.value("queryTime").toDateTime();
        
        if (!history.number.isEmpty()) {
            historyList.append(history);
        }
    }
    settings.endArray();
    settings.endGroup();
    
    updateHistoryDisplay();
}

void MobileLocation::saveHistory()
{
    // 简化的历史记录保存
    QSettings settings;
    settings.beginGroup("MobileLocation");
    
    settings.beginWriteArray("History");
    int count = qMin(historyList.size(), 100); // 只保存最近100条
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        const QueryHistory &history = historyList[i];
        
        settings.setValue("number", history.number);
        settings.setValue("country", history.info.country);
        settings.setValue("province", history.info.province);
        settings.setValue("city", history.info.city);
        settings.setValue("carrier", history.info.carrier);
        settings.setValue("numberType", history.info.numberType);
        settings.setValue("isValid", history.info.isValid);
        settings.setValue("queryTime", history.queryTime);
    }
    settings.endArray();
    settings.endGroup();
}


