#include "routetesttool.h"
#include "../ip-lookup-tool/iplookuptool.h"   // Ip2RegionLookup

#include <QApplication>
#include <QClipboard>
#include <QFrame>
#include <QHeaderView>
#include <QHostAddress>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSplitter>
#include <QTableWidgetItem>
#include <QTextStream>

REGISTER_DYNAMICOBJECT(RouteTestTool);

namespace {

// 内置节点（覆盖电信/联通/移动/教育网，做线路对比）
static const RouteTarget kPresetNodes[] = {
    {"上海电信",       "202.96.209.5",      "电信"},
    {"北京电信",       "219.141.140.10",    "电信"},
    {"广州电信",       "58.60.188.222",     "电信"},
    {"北京联通",       "123.125.81.6",      "联通"},
    {"上海联通",       "210.22.97.1",       "联通"},
    {"广州联通",       "210.21.196.6",      "联通"},
    {"北京移动",       "211.136.17.107",    "移动"},
    {"上海移动",       "211.136.150.66",    "移动"},
    {"广州移动",       "211.139.129.222",   "移动"},
    {"北京教育网",     "101.4.117.211",     "教育网"},
    {"Cloudflare DNS", "1.1.1.1",           "国际"},
    {"Google DNS",     "8.8.8.8",           "国际"},
};

QString isoLookupRegion(const QString& ip)
{
    auto& db = Ip2RegionLookup::instance();
    if (!db.isAvailable() || ip.isEmpty()) return QString();
    IpLookupResult r = db.lookup(ip);
    if (!r.error.isEmpty()) return QString();
    QStringList parts;
    if (!r.country.isEmpty() && r.country != "0") parts << r.country;
    if (!r.region.isEmpty()  && r.region  != "0") parts << r.region;
    if (!r.city.isEmpty()    && r.city    != "0") parts << r.city;
    if (!r.isp.isEmpty()     && r.isp     != "0") parts << r.isp;
    return parts.join(" · ");
}

} // namespace

// ============= ctor / dtor =============

RouteTestTool::RouteTestTool(QWidget* parent)
    : QWidget(parent), DynamicObjectBase()
{
    setupUI();
    populatePresets();
}

RouteTestTool::~RouteTestTool()
{
    if (m_proc && m_proc->state() != QProcess::NotRunning) {
        m_proc->kill();
        m_proc->waitForFinished(2000);
    }
}

// ============= UI =============

void RouteTestTool::setupUI()
{
    setStyleSheet(R"(
        QLineEdit, QSpinBox {
            border: 1px solid #ced4da; border-radius: 4px;
            padding: 5px 8px; background: #fff; min-height: 22px;
            selection-background-color: #b8d4ff;
        }
        QLineEdit:focus, QSpinBox:focus { border-color: #228be6; }
        QPushButton {
            border: 1px solid #ced4da; border-radius: 4px;
            padding: 5px 14px; background: #ffffff; min-height: 22px; color: #343a40;
        }
        QPushButton:hover  { background: #f1f3f5; border-color: #adb5bd; }
        QPushButton:pressed{ background: #e9ecef; }
        QPushButton:disabled { color: #adb5bd; background: #f8f9fa; }
        QPushButton#primaryBtn {
            background: #228be6; border: 1px solid #1c7ed6; color: white; font-weight: bold;
        }
        QPushButton#primaryBtn:hover  { background: #1c7ed6; }
        QPushButton#primaryBtn:disabled { background: #adb5bd; border-color: #adb5bd; color: #f1f3f5; }
        QPushButton#dangerBtn {
            background: #fa5252; border: 1px solid #f03e3e; color: white; font-weight: bold;
        }
        QPushButton#dangerBtn:hover  { background: #f03e3e; }
        QPushButton#dangerBtn:disabled { background: #adb5bd; border-color: #adb5bd; color: #f1f3f5; }
        QFrame#card {
            background: #f8f9fa; border: 1px solid #dee2e6; border-radius: 6px;
        }
        QListWidget {
            border: 1px solid #dee2e6; border-radius: 4px; background: #ffffff;
            outline: none;
        }
        QListWidget::item {
            padding: 5px 6px; border-bottom: 1px solid #f1f3f5;
        }
        QListWidget::item:hover { background: #f1f3f5; }
        QListWidget::item:selected { background: #e7f5ff; color: #1864ab; }
        QTabWidget::pane {
            border: 1px solid #dee2e6; border-radius: 6px;
            background: #fff; top: -1px;
        }
        QTabBar::tab {
            background: #f1f3f5; border: 1px solid #dee2e6;
            border-bottom: none; border-top-left-radius: 5px; border-top-right-radius: 5px;
            padding: 6px 14px; margin-right: 2px; color: #495057;
        }
        QTabBar::tab:selected { background: #fff; color: #1864ab; font-weight: bold; }
        QTabBar::tab:hover:!selected { background: #e9ecef; }
        QTableWidget {
            border: none; background: #fff; gridline-color: #e9ecef;
        }
        QHeaderView::section {
            background: #f8f9fa; color: #495057; font-weight: bold;
            padding: 6px; border: none; border-right: 1px solid #dee2e6;
            border-bottom: 1px solid #dee2e6;
        }
    )");

    auto* main = new QVBoxLayout(this);
    main->setContentsMargins(12, 10, 12, 10);
    main->setSpacing(10);

    // 左右分栏
    auto* split = new QSplitter(Qt::Horizontal);
    split->setHandleWidth(6);

    // 左：节点选择 + 自定义
    auto* left = new QWidget();
    auto* leftCol = new QVBoxLayout(left);
    leftCol->setContentsMargins(0, 0, 0, 0);
    leftCol->setSpacing(8);
    leftCol->addWidget(buildPresetCard(), 1);
    leftCol->addWidget(buildCustomCard());

    // 操作按钮（左下）
    auto* btnRow = new QHBoxLayout();
    m_startBtn = new QPushButton(tr("🚀 测试已选"));
    m_startBtn->setObjectName("primaryBtn");
    m_stopBtn = new QPushButton(tr("⏹ 停止"));
    m_stopBtn->setObjectName("dangerBtn");
    m_stopBtn->setEnabled(false);
    btnRow->addWidget(m_startBtn);
    btnRow->addWidget(m_stopBtn);
    btnRow->addStretch();
    leftCol->addLayout(btnRow);
    split->addWidget(left);

    // 右：进度 + 状态条 + Tab 结果
    auto* right = new QWidget();
    auto* rightCol = new QVBoxLayout(right);
    rightCol->setContentsMargins(0, 0, 0, 0);
    rightCol->setSpacing(8);

    auto* topBar = new QHBoxLayout();
    m_progress = new QProgressBar();
    m_progress->setRange(0, 1);
    m_progress->setValue(0);
    m_progress->setTextVisible(true);
    m_progress->setFormat(tr("已完成 %v / %m"));
    m_clearBtn = new QPushButton(tr("🗑 清空"));
    m_copyBtn = new QPushButton(tr("📋 复制全部"));
    topBar->addWidget(m_progress, 1);
    topBar->addWidget(m_clearBtn);
    topBar->addWidget(m_copyBtn);
    rightCol->addLayout(topBar);

    m_status = new QLabel(tr("就绪"));
    m_status->setStyleSheet(
        "color: #2e7d32; padding: 6px 10px; background: #e8f5e8;"
        " border-radius: 4px; font-weight: bold;");
    rightCol->addWidget(m_status);

    rightCol->addWidget(buildResultsArea(), 1);
    split->addWidget(right);

    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setSizes({320, 700});
    main->addWidget(split);

    connect(m_startBtn, &QPushButton::clicked, this, &RouteTestTool::onStartSelected);
    connect(m_stopBtn, &QPushButton::clicked, this, &RouteTestTool::onStop);
    connect(m_clearBtn, &QPushButton::clicked, this, &RouteTestTool::onClearTabs);
    connect(m_copyBtn, &QPushButton::clicked, this, &RouteTestTool::onCopyAll);
    connect(m_addCustomBtn, &QPushButton::clicked, this, &RouteTestTool::onAddCustom);
}

QWidget* RouteTestTool::buildPresetCard()
{
    auto* card = new QFrame();
    card->setObjectName("card");
    auto* col = new QVBoxLayout(card);
    col->setContentsMargins(12, 10, 12, 10);
    col->setSpacing(6);

    auto* hdr = new QLabel(tr("🌐 内置节点（按 ISP 分组，勾选后批量测试）"));
    hdr->setStyleSheet("font-weight: bold; color: #495057; background: transparent;");
    col->addWidget(hdr);

    m_presetList = new QListWidget();
    m_presetList->setSelectionMode(QAbstractItemView::NoSelection);
    col->addWidget(m_presetList, 1);

    auto* btns = new QHBoxLayout();
    auto* selAll = new QPushButton(tr("全选"));
    auto* selNone = new QPushButton(tr("全不选"));
    btns->addWidget(selAll);
    btns->addWidget(selNone);
    btns->addStretch();
    col->addLayout(btns);

    connect(selAll, &QPushButton::clicked, this, [this](){
        for (int i = 0; i < m_presetList->count(); ++i)
            m_presetList->item(i)->setCheckState(Qt::Checked);
    });
    connect(selNone, &QPushButton::clicked, this, [this](){
        for (int i = 0; i < m_presetList->count(); ++i)
            m_presetList->item(i)->setCheckState(Qt::Unchecked);
    });

    return card;
}

QWidget* RouteTestTool::buildCustomCard()
{
    auto* card = new QFrame();
    card->setObjectName("card");
    auto* col = new QVBoxLayout(card);
    col->setContentsMargins(12, 10, 12, 10);
    col->setSpacing(6);

    auto* hdr = new QLabel(tr("🎯 自定义节点"));
    hdr->setStyleSheet("font-weight: bold; color: #495057; background: transparent;");
    col->addWidget(hdr);

    auto* row1 = new QHBoxLayout();
    m_customName = new QLineEdit();
    m_customName->setPlaceholderText(tr("名称（可选）"));
    row1->addWidget(m_customName);
    col->addLayout(row1);

    auto* row2 = new QHBoxLayout();
    m_customIp = new QLineEdit();
    m_customIp->setPlaceholderText(tr("IP 或域名"));
    m_addCustomBtn = new QPushButton(tr("+ 添加"));
    row2->addWidget(m_customIp, 1);
    row2->addWidget(m_addCustomBtn);
    col->addLayout(row2);

    return card;
}

QWidget* RouteTestTool::buildResultsArea()
{
    m_tabs = new QTabWidget();
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, [this](int idx){
        QWidget* w = m_tabs->widget(idx);
        m_tabs->removeTab(idx);
        if (w) w->deleteLater();
    });
    return m_tabs;
}

QTableWidget* RouteTestTool::makeHopTable()
{
    auto* tbl = new QTableWidget();
    tbl->setColumnCount(7);
    tbl->setHorizontalHeaderLabels(QStringList()
        << tr("跳") << tr("IP") << tr("主机名")
        << tr("RTT 1") << tr("RTT 2") << tr("RTT 3")
        << tr("地区"));
    tbl->verticalHeader()->setVisible(false);
    tbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tbl->setSelectionBehavior(QAbstractItemView::SelectRows);
    tbl->setAlternatingRowColors(true);
    auto* hdr = tbl->horizontalHeader();
    hdr->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(2, QHeaderView::Stretch);
    hdr->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(6, QHeaderView::Stretch);
    return tbl;
}

void RouteTestTool::populatePresets()
{
    QString lastIsp;
    for (const auto& n : kPresetNodes) {
        if (n.isp != lastIsp) {
            // 分组分隔行
            auto* group = new QListWidgetItem(QString("── %1 ──").arg(n.isp));
            group->setFlags(Qt::ItemIsEnabled);
            group->setForeground(QColor(0x86, 0x8e, 0x96));
            QFont f = group->font(); f.setBold(true); group->setFont(f);
            m_presetList->addItem(group);
            lastIsp = n.isp;
        }
        auto* it = new QListWidgetItem(QString("%1   %2").arg(n.name, n.ip));
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(Qt::Unchecked);
        QVariant v; v.setValue(n);
        it->setData(Qt::UserRole, v);
        m_presetList->addItem(it);
    }
}

// ============= 启动 / 停止 =============

void RouteTestTool::onStartSelected()
{
    m_queue.clear();
    for (int i = 0; i < m_presetList->count(); ++i) {
        auto* it = m_presetList->item(i);
        if (!(it->flags() & Qt::ItemIsUserCheckable)) continue;  // 跳过分组行
        if (it->checkState() == Qt::Checked) {
            m_queue.append(it->data(Qt::UserRole).value<RouteTarget>());
        }
    }

    if (m_queue.isEmpty()) {
        QMessageBox::information(this, tr("提示"),
            tr("请先勾选要测试的节点，或在右侧自定义添加。"));
        return;
    }

    setBusy(true);
    m_progress->setRange(0, m_queue.size());
    m_progress->setValue(0);
    m_currentIdx = -1;
    runNextTarget();
}

void RouteTestTool::onAddCustom()
{
    QString ip = m_customIp->text().trimmed();
    if (ip.isEmpty()) {
        QMessageBox::warning(this, tr("错误"), tr("请输入 IP 或域名"));
        return;
    }
    QString name = m_customName->text().trimmed();
    if (name.isEmpty()) name = ip;

    RouteTarget t;
    t.name = name;
    t.ip = ip;
    t.isp = tr("自定义");

    m_queue = {t};

    setBusy(true);
    m_progress->setRange(0, 1);
    m_progress->setValue(0);
    m_currentIdx = -1;
    runNextTarget();
}

void RouteTestTool::onStop()
{
    if (m_proc && m_proc->state() != QProcess::NotRunning) {
        m_proc->kill();
        m_proc->waitForFinished(1000);
    }
    m_queue.clear();
    setBusy(false);
    setStatus(tr("已停止"));
}

void RouteTestTool::onClearTabs()
{
    while (m_tabs->count() > 0) {
        QWidget* w = m_tabs->widget(0);
        m_tabs->removeTab(0);
        if (w) w->deleteLater();
    }
    m_progress->setValue(0);
    setStatus(tr("已清空"));
}

void RouteTestTool::onCopyAll()
{
    if (m_tabs->count() == 0) {
        QMessageBox::information(this, tr("提示"), tr("没有可复制的内容"));
        return;
    }
    QString out;
    QTextStream ts(&out);
    for (int t = 0; t < m_tabs->count(); ++t) {
        QString tabName = m_tabs->tabText(t);
        ts << "\n# " << tabName << "\n";
        auto* tbl = qobject_cast<QTableWidget*>(m_tabs->widget(t));
        if (!tbl) continue;
        for (int r = 0; r < tbl->rowCount(); ++r) {
            QStringList cells;
            for (int c = 0; c < tbl->columnCount(); ++c) {
                auto* it = tbl->item(r, c);
                cells << (it ? it->text() : QString());
            }
            ts << cells.join('\t') << "\n";
        }
    }
    QApplication::clipboard()->setText(out);
    setStatus(tr("已复制 %1 个节点的结果到剪贴板").arg(m_tabs->count()));
}

// ============= 队列 =============

void RouteTestTool::runNextTarget()
{
    m_currentIdx++;
    if (m_currentIdx >= m_queue.size()) {
        // 队列结束
        setBusy(false);
        setStatus(tr("全部完成（共测试 %1 个节点）").arg(m_queue.size()));
        return;
    }

    const RouteTarget& tgt = m_queue.at(m_currentIdx);
    m_pendingBuffer.clear();
    m_currentLastHop = 0;

    // 创建该 target 的 tab + table
    m_currentTable = makeHopTable();
    int tabIdx = m_tabs->addTab(m_currentTable,
        QString("%1 (%2)").arg(tgt.name, tgt.ip));
    m_tabs->setCurrentIndex(tabIdx);

    setStatus(tr("正在测试 %1 (%2) ... [%3 / %4]")
                  .arg(tgt.name, tgt.ip)
                  .arg(m_currentIdx + 1).arg(m_queue.size()));

    if (m_proc) { m_proc->deleteLater(); m_proc = nullptr; }
    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_proc, &QProcess::readyReadStandardOutput, this, &RouteTestTool::onProcOutput);
    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &RouteTestTool::onProcFinished);

    QString program;
    QStringList args;
#ifdef Q_OS_WIN
    program = "tracert";
    args << "-d" << "-h" << "30" << "-w" << "5000" << tgt.ip;
#else
    program = "traceroute";
    args << "-n" << "-m" << "30" << "-w" << "5" << tgt.ip;
#endif
    m_proc->start(program, args);
    if (!m_proc->waitForStarted(3000)) {
        setStatus(tr("无法启动 %1").arg(program), true);
        runNextTarget();   // 跳过当前，继续下一个
    }
}

// ============= 进程事件 =============

void RouteTestTool::onProcOutput()
{
    if (!m_proc) return;
    m_pendingBuffer += m_proc->readAllStandardOutput();
    while (true) {
        int nl = m_pendingBuffer.indexOf('\n');
        if (nl < 0) break;
        QByteArray rawLine = m_pendingBuffer.left(nl);
        m_pendingBuffer.remove(0, nl + 1);
        QString line = QString::fromLocal8Bit(rawLine).trimmed();
        if (!line.isEmpty()) parseLine(line);
    }
}

void RouteTestTool::onProcFinished(int code, QProcess::ExitStatus status)
{
    Q_UNUSED(code);
    Q_UNUSED(status);
    if (!m_pendingBuffer.isEmpty()) {
        QString line = QString::fromLocal8Bit(m_pendingBuffer).trimmed();
        m_pendingBuffer.clear();
        if (!line.isEmpty()) parseLine(line);
    }
    m_progress->setValue(m_currentIdx + 1);
    runNextTarget();
}

// ============= 解析（与 TracerouteTool 一致的通用规则）=============

void RouteTestTool::parseLine(const QString& line)
{
    static const QRegularExpression rxHopStart(R"(^\s*(\d+)\s)");
    auto m = rxHopStart.match(line);
    if (!m.hasMatch()) return;
    int hopNum = m.captured(1).toInt();

    QString ipAddr;
    static const QRegularExpression rxIp(
        R"((?:(\d{1,3}(?:\.\d{1,3}){3})|\b([0-9a-fA-F:]{2,39})\b))");
    auto it = rxIp.globalMatch(line);
    while (it.hasNext()) {
        auto mi = it.next();
        QString s = !mi.captured(1).isEmpty() ? mi.captured(1) : mi.captured(2);
        if (!s.contains('.') && !s.contains(':')) continue;
        QHostAddress addr;
        if (addr.setAddress(s)) { ipAddr = s; break; }
    }

    static const QRegularExpression rxTime(R"(([\d.]+)\s*ms)",
                                           QRegularExpression::CaseInsensitiveOption);
    QList<double> rtts;
    auto it2 = rxTime.globalMatch(line);
    while (it2.hasNext()) rtts << it2.next().captured(1).toDouble();
    double r1 = rtts.size() >= 1 ? rtts[0] : -1;
    double r2 = rtts.size() >= 2 ? rtts[1] : -1;
    double r3 = rtts.size() >= 3 ? rtts[2] : -1;

    QString hostname;
    if (!ipAddr.isEmpty()) {
        QString rest = line.mid(m.capturedEnd());
        int parenPos = rest.indexOf('(');
        int bracketPos = rest.indexOf('[');
        QString candidate;
        if (parenPos > 0) {
            candidate = rest.left(parenPos).trimmed();
        } else if (bracketPos > 0) {
            QString before = rest.left(bracketPos).trimmed();
            static const QRegularExpression rxTail(
                R"((?:(?:\d+\s*ms\s*)+|\*\s*)+)",
                QRegularExpression::CaseInsensitiveOption);
            QString hostPart = before;
            hostPart.remove(rxTail);
            candidate = hostPart.trimmed();
        }
        if (!candidate.isEmpty() && candidate != ipAddr
            && QHostAddress(candidate).isNull()) {
            hostname = candidate;
        }
    }

    bool timeout = (ipAddr.isEmpty() && rtts.isEmpty() && line.contains('*'));
    QString region = isoLookupRegion(ipAddr);

    appendHopToCurrent(hopNum, ipAddr, hostname, r1, r2, r3, timeout, region);
}

// ============= 显示 =============

void RouteTestTool::appendHopToCurrent(int hop, const QString& ip, const QString& host,
                                       double r1, double r2, double r3,
                                       bool timeout, const QString& region)
{
    if (!m_currentTable) return;

    int row = -1;
    for (int r = 0; r < m_currentTable->rowCount(); ++r) {
        auto* it = m_currentTable->item(r, 0);
        if (it && it->text().toInt() == hop) { row = r; break; }
    }
    if (row < 0) {
        row = m_currentTable->rowCount();
        m_currentTable->insertRow(row);
    }

    auto setCell = [&](int col, const QString& text,
                       Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter) {
        auto* it = new QTableWidgetItem(text);
        it->setTextAlignment(align);
        m_currentTable->setItem(row, col, it);
    };

    setCell(0, QString::number(hop), Qt::AlignCenter);
    setCell(1, ip.isEmpty() ? "*" : ip);
    setCell(2, host);
    auto fmt = [](double v) {
        return v < 0 ? QStringLiteral("*") : QString("%1 ms").arg(v, 0, 'f', 2);
    };
    setCell(3, fmt(r1), Qt::AlignRight | Qt::AlignVCenter);
    setCell(4, fmt(r2), Qt::AlignRight | Qt::AlignVCenter);
    setCell(5, fmt(r3), Qt::AlignRight | Qt::AlignVCenter);
    setCell(6, region);

    QColor bg;
    if (timeout || ip.isEmpty()) bg = QColor(255, 235, 238);
    else if (r1 > 0 || r2 > 0 || r3 > 0) bg = QColor(232, 245, 233);
    if (bg.isValid()) {
        for (int c = 0; c < m_currentTable->columnCount(); ++c) {
            if (auto* it = m_currentTable->item(row, c))
                it->setBackground(QBrush(bg));
        }
    }
    m_currentTable->scrollToBottom();
    m_currentLastHop = qMax(m_currentLastHop, hop);
}

// ============= 状态切换 =============

void RouteTestTool::setBusy(bool busy)
{
    m_startBtn->setEnabled(!busy);
    m_addCustomBtn->setEnabled(!busy);
    m_stopBtn->setEnabled(busy);
    m_presetList->setEnabled(!busy);
    m_customName->setEnabled(!busy);
    m_customIp->setEnabled(!busy);
}

void RouteTestTool::setStatus(const QString& s, bool err)
{
    m_status->setText(s);
    if (err) {
        m_status->setStyleSheet(
            "color: #c62828; padding: 6px 10px; background: #ffebee;"
            " border-radius: 4px; font-weight: bold;");
    } else {
        m_status->setStyleSheet(
            "color: #2e7d32; padding: 6px 10px; background: #e8f5e8;"
            " border-radius: 4px; font-weight: bold;");
    }
}
