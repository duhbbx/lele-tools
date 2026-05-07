#include "traceroutetool.h"
#include "../ip-lookup-tool/iplookuptool.h"   // 复用 Ip2RegionLookup

#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QFrame>
#include <QHeaderView>
#include <QHostAddress>
#include <QMessageBox>
#include <QRegularExpression>
#include <QTableWidgetItem>
#include <QTextStream>

REGISTER_DYNAMICOBJECT(TracerouteTool);

// ============= 构造 / 析构 =============

TracerouteTool::TracerouteTool(QWidget* parent)
    : QWidget(parent), DynamicObjectBase()
{
    setupUI();
}

TracerouteTool::~TracerouteTool()
{
    if (m_proc && m_proc->state() != QProcess::NotRunning) {
        m_proc->kill();
        m_proc->waitForFinished(2000);
    }
}

// ============= UI =============

void TracerouteTool::setupUI()
{
    setStyleSheet(R"(
        QLineEdit, QSpinBox {
            border: 1px solid #ced4da;
            border-radius: 4px;
            padding: 5px 8px;
            background: #fff;
            min-height: 22px;
            selection-background-color: #b8d4ff;
        }
        QLineEdit:focus, QSpinBox:focus { border-color: #228be6; }
        QPushButton {
            border: 1px solid #ced4da;
            border-radius: 4px;
            padding: 5px 14px;
            background: #ffffff;
            min-height: 22px;
            color: #343a40;
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
        QTableWidget {
            border: 1px solid #dee2e6; border-radius: 6px; background: #fff;
            gridline-color: #e9ecef;
        }
        QHeaderView::section {
            background: #f1f3f5; color: #495057; font-weight: bold;
            padding: 6px; border: none; border-right: 1px solid #dee2e6;
            border-bottom: 1px solid #dee2e6;
        }
    )");

    auto* main = new QVBoxLayout(this);
    main->setContentsMargins(12, 10, 12, 10);
    main->setSpacing(10);

    main->addWidget(buildTargetCard());
    main->addWidget(buildOptionsCard());

    // 操作按钮
    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(6);
    m_startBtn = new QPushButton(tr("🚀 开始追踪"));
    m_startBtn->setObjectName("primaryBtn");
    m_startBtn->setMinimumWidth(110);
    m_stopBtn = new QPushButton(tr("⏹ 停止"));
    m_stopBtn->setObjectName("dangerBtn");
    m_stopBtn->setMinimumWidth(80);
    m_stopBtn->setEnabled(false);
    m_clearBtn = new QPushButton(tr("🗑 清空"));
    m_clearBtn->setMinimumWidth(80);
    m_exportBtn = new QPushButton(tr("📋 复制结果"));
    m_exportBtn->setMinimumWidth(110);
    btnRow->addWidget(m_startBtn);
    btnRow->addWidget(m_stopBtn);
    btnRow->addWidget(m_clearBtn);
    btnRow->addWidget(m_exportBtn);
    btnRow->addStretch();
    main->addLayout(btnRow);

    // 状态条
    m_status = new QLabel(tr("就绪"));
    m_status->setStyleSheet(
        "color: #2e7d32; padding: 6px 10px; background: #e8f5e8;"
        " border-radius: 4px; font-weight: bold;");
    main->addWidget(m_status);

    main->addWidget(buildResultsArea(), 1);

    // 信号
    connect(m_startBtn, &QPushButton::clicked, this, &TracerouteTool::onStartTraceroute);
    connect(m_stopBtn, &QPushButton::clicked, this, &TracerouteTool::onStopTraceroute);
    connect(m_clearBtn, &QPushButton::clicked, this, &TracerouteTool::onClearResults);
    connect(m_exportBtn, &QPushButton::clicked, this, &TracerouteTool::onExportResults);
    connect(m_targetEdit, &QLineEdit::textChanged, this, &TracerouteTool::onTargetChanged);
    connect(m_targetEdit, &QLineEdit::returnPressed, this, &TracerouteTool::onStartTraceroute);

    m_targetEdit->setText("baidu.com");
}

QWidget* TracerouteTool::buildTargetCard()
{
    auto* card = new QFrame();
    card->setObjectName("card");
    auto* row = new QHBoxLayout(card);
    row->setContentsMargins(12, 10, 12, 10);
    row->setSpacing(8);

    auto* lbl = new QLabel(tr("🌐 目标"));
    lbl->setStyleSheet("font-weight: bold; color: #495057; background: transparent;");
    m_targetEdit = new QLineEdit();
    m_targetEdit->setPlaceholderText(tr("输入域名或 IP 地址"));
    m_targetEdit->setMinimumWidth(260);

    row->addWidget(lbl);
    row->addWidget(m_targetEdit, 1);
    return card;
}

QWidget* TracerouteTool::buildOptionsCard()
{
    auto* card = new QFrame();
    card->setObjectName("card");
    auto* row = new QHBoxLayout(card);
    row->setContentsMargins(12, 10, 12, 10);
    row->setSpacing(10);

    auto labelStyle = QStringLiteral("color:#495057; background: transparent;");

    auto* l1 = new QLabel(tr("⚙️ 最大跳数"));
    l1->setStyleSheet("font-weight: bold; color:#495057; background: transparent;");
    m_maxHopsSpin = new QSpinBox();
    m_maxHopsSpin->setRange(1, 64);
    m_maxHopsSpin->setValue(30);
    m_maxHopsSpin->setMinimumWidth(70);

    auto* l2 = new QLabel(tr("超时"));
    l2->setStyleSheet(labelStyle);
    m_timeoutSpin = new QSpinBox();
    m_timeoutSpin->setRange(1, 30);
    m_timeoutSpin->setValue(5);
    m_timeoutSpin->setSuffix(tr(" s"));
    m_timeoutSpin->setMinimumWidth(80);

    m_resolveNamesCheck = new QCheckBox(tr("解析主机名"));
    m_resolveNamesCheck->setChecked(true);
    m_resolveNamesCheck->setStyleSheet(labelStyle);

    row->addWidget(l1);
    row->addWidget(m_maxHopsSpin);
    row->addSpacing(10);
    row->addWidget(l2);
    row->addWidget(m_timeoutSpin);
    row->addSpacing(10);
    row->addWidget(m_resolveNamesCheck);
    row->addStretch();
    return card;
}

QWidget* TracerouteTool::buildResultsArea()
{
    auto* w = new QWidget();
    auto* col = new QVBoxLayout(w);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(6);

    m_progress = new QProgressBar();
    m_progress->setRange(0, 30);
    m_progress->setValue(0);
    m_progress->setTextVisible(true);
    m_progress->setFormat(tr("已经探测 %v / %m 跳"));
    col->addWidget(m_progress);

    m_table = new QTableWidget();
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels(QStringList()
        << tr("跳") << tr("IP") << tr("主机名")
        << tr("RTT 1") << tr("RTT 2") << tr("RTT 3")
        << tr("地区"));
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    auto* hdr = m_table->horizontalHeader();
    hdr->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(2, QHeaderView::Stretch);
    hdr->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(6, QHeaderView::Stretch);
    col->addWidget(m_table, 1);
    return w;
}

// ============= 启动 / 停止 =============

void TracerouteTool::onStartTraceroute()
{
    QString target = m_targetEdit->text().trimmed();
    if (target.isEmpty()) {
        QMessageBox::warning(this, tr("错误"), tr("请输入要追踪的目标"));
        return;
    }
    m_target = target;
    m_lastHopSeen = 0;
    m_pendingBuffer.clear();
    onClearResults();
    setBusyState(true);

    if (m_proc) { m_proc->deleteLater(); m_proc = nullptr; }
    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);  // 一起读 stdout/stderr
    connect(m_proc, &QProcess::readyReadStandardOutput,
            this, &TracerouteTool::onProcessOutput);
    connect(m_proc, &QProcess::errorOccurred,
            this, [this](QProcess::ProcessError){ onProcessError(); });
    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &TracerouteTool::onProcessFinished);

    int maxHops = m_maxHopsSpin->value();
    int timeoutS = m_timeoutSpin->value();
    bool resolveNames = m_resolveNamesCheck->isChecked();

    m_progress->setRange(0, maxHops);
    m_progress->setValue(0);

    QString program;
    QStringList args;
#ifdef Q_OS_WIN
    program = "tracert";
    if (!resolveNames) args << "-d";
    args << "-h" << QString::number(maxHops);
    args << "-w" << QString::number(timeoutS * 1000);     // ms
    args << target;
#else
    program = "traceroute";
    if (!resolveNames) args << "-n";
    args << "-m" << QString::number(maxHops);
    args << "-w" << QString::number(timeoutS);
    // macOS / Linux 默认走 UDP，无需 sudo
    args << target;
#endif

    updateStatus(tr("正在追踪 %1 ...").arg(target));
    m_proc->start(program, args);
    if (!m_proc->waitForStarted(3000)) {
        updateStatus(tr("无法启动 %1：请确认系统已安装该命令").arg(program), true);
        setBusyState(false);
    }
}

void TracerouteTool::onStopTraceroute()
{
    if (m_proc && m_proc->state() != QProcess::NotRunning) {
        m_proc->kill();
        m_proc->waitForFinished(1000);
    }
    setBusyState(false);
    updateStatus(tr("已停止"));
}

void TracerouteTool::onClearResults()
{
    m_table->setRowCount(0);
    m_progress->setValue(0);
}

void TracerouteTool::onTargetChanged()
{
    // 占位，保留以便将来做"目标变化时重置 IP 标签"等
}

void TracerouteTool::onExportResults()
{
    if (m_table->rowCount() == 0) {
        QMessageBox::information(this, tr("提示"), tr("没有可复制的结果"));
        return;
    }
    QString out;
    QTextStream ts(&out);
    ts << QString("# Traceroute to %1").arg(m_target) << "\n";
    for (int r = 0; r < m_table->rowCount(); ++r) {
        QStringList cells;
        for (int c = 0; c < m_table->columnCount(); ++c) {
            auto* it = m_table->item(r, c);
            cells << (it ? it->text() : QString());
        }
        ts << cells.join('\t') << "\n";
    }
    QApplication::clipboard()->setText(out);
    updateStatus(tr("已复制 %1 行到剪贴板").arg(m_table->rowCount()));
}

// ============= 进程事件 =============

void TracerouteTool::onProcessOutput()
{
    if (!m_proc) return;
    m_pendingBuffer += m_proc->readAllStandardOutput();
    while (true) {
        int nl = m_pendingBuffer.indexOf('\n');
        if (nl < 0) break;
        QByteArray rawLine = m_pendingBuffer.left(nl);
        m_pendingBuffer.remove(0, nl + 1);
        QString line = QString::fromLocal8Bit(rawLine).trimmed();
        if (line.isEmpty()) continue;
        parseLine(line);
    }
}

void TracerouteTool::onProcessError()
{
    if (!m_proc) return;
    QString err = m_proc->errorString();
    updateStatus(tr("进程错误：%1").arg(err), true);
}

void TracerouteTool::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(exitCode);
    Q_UNUSED(status);
    if (!m_pendingBuffer.isEmpty()) {
        QString line = QString::fromLocal8Bit(m_pendingBuffer).trimmed();
        m_pendingBuffer.clear();
        if (!line.isEmpty()) parseLine(line);
    }
    setBusyState(false);
    updateStatus(tr("追踪完成（共 %1 跳）").arg(m_lastHopSeen));
}

// ============= 解析 =============

// 多平台 traceroute / tracert 输出格式不一，但都满足：
//   - 行首是跳数（整数）
//   - 包含若干 "<num> ms"（时间）
//   - 包含 IP（IPv4 / IPv6），可能括号或方括号包裹
//   - 失败时含 `*`
void TracerouteTool::parseLine(const QString& line)
{
    static const QRegularExpression rxHopStart(R"(^\s*(\d+)\s)");
    auto m = rxHopStart.match(line);
    if (!m.hasMatch()) return;
    int hopNum = m.captured(1).toInt();

    RouteHop hop;
    hop.hopNumber = hopNum;

    // IPv4 / IPv6 抓取
    static const QRegularExpression rxIp(
        R"((?:(\d{1,3}(?:\.\d{1,3}){3})|\b([0-9a-fA-F:]{2,39})\b))");
    QStringList ips;
    auto it = rxIp.globalMatch(line);
    while (it.hasNext()) {
        auto mi = it.next();
        QString s = !mi.captured(1).isEmpty() ? mi.captured(1) : mi.captured(2);
        if (!s.contains('.') && !s.contains(':')) continue;
        QHostAddress addr;
        if (addr.setAddress(s)) ips << s;
    }
    if (!ips.isEmpty()) hop.ipAddress = ips.first();

    // RTT 数字
    static const QRegularExpression rxTime(R"(([\d.]+)\s*ms)",
                                           QRegularExpression::CaseInsensitiveOption);
    QList<double> rtts;
    auto it2 = rxTime.globalMatch(line);
    while (it2.hasNext()) rtts << it2.next().captured(1).toDouble();
    if (rtts.size() >= 1) hop.rtt1 = rtts[0];
    if (rtts.size() >= 2) hop.rtt2 = rtts[1];
    if (rtts.size() >= 3) hop.rtt3 = rtts[2];

    // 主机名（如果有 — Unix `hostname (ip)` 或 Win `... hostname [ip]`）
    if (!hop.ipAddress.isEmpty()) {
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
        if (!candidate.isEmpty() && candidate != hop.ipAddress
            && QHostAddress(candidate).isNull()) {
            hop.hostname = candidate;
        }
    }

    // 失败检测
    if (hop.ipAddress.isEmpty() && rtts.isEmpty() && line.contains('*')) {
        hop.timeout = true;
    }

    // 离线 ip2region 地理位置
    if (!hop.ipAddress.isEmpty()) {
        auto& db = Ip2RegionLookup::instance();
        if (db.isAvailable()) {
            IpLookupResult r = db.lookup(hop.ipAddress);
            if (r.error.isEmpty()) {
                QStringList parts;
                if (!r.country.isEmpty() && r.country != "0") parts << r.country;
                if (!r.region.isEmpty() && r.region != "0")  parts << r.region;
                if (!r.city.isEmpty() && r.city != "0")      parts << r.city;
                if (!r.isp.isEmpty() && r.isp != "0")        parts << r.isp;
                hop.region = parts.join(" · ");
            }
        }
    }

    appendHop(hop);
    m_lastHopSeen = qMax(m_lastHopSeen, hopNum);
    m_progress->setValue(m_lastHopSeen);
}

// ============= 显示 =============

void TracerouteTool::appendHop(const RouteHop& hop)
{
    int row = -1;
    for (int r = 0; r < m_table->rowCount(); ++r) {
        auto* it = m_table->item(r, 0);
        if (it && it->text().toInt() == hop.hopNumber) { row = r; break; }
    }
    if (row < 0) {
        row = m_table->rowCount();
        m_table->insertRow(row);
    }

    auto setCell = [&](int col, const QString& text,
                       Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter) {
        auto* it = new QTableWidgetItem(text);
        it->setTextAlignment(align);
        m_table->setItem(row, col, it);
    };

    setCell(0, QString::number(hop.hopNumber), Qt::AlignCenter);
    setCell(1, hop.ipAddress.isEmpty() ? "*" : hop.ipAddress);
    setCell(2, hop.hostname);

    auto fmt = [](double v) {
        return v < 0 ? QStringLiteral("*") : QString("%1 ms").arg(v, 0, 'f', 2);
    };
    setCell(3, fmt(hop.rtt1), Qt::AlignRight | Qt::AlignVCenter);
    setCell(4, fmt(hop.rtt2), Qt::AlignRight | Qt::AlignVCenter);
    setCell(5, fmt(hop.rtt3), Qt::AlignRight | Qt::AlignVCenter);
    setCell(6, hop.region);

    QColor bg;
    if (hop.timeout || hop.ipAddress.isEmpty()) {
        bg = QColor(255, 235, 238);
    } else if (hop.rtt1 > 0 || hop.rtt2 > 0 || hop.rtt3 > 0) {
        bg = QColor(232, 245, 233);
    }
    if (bg.isValid()) {
        for (int c = 0; c < m_table->columnCount(); ++c) {
            if (auto* it = m_table->item(row, c))
                it->setBackground(QBrush(bg));
        }
    }
    m_table->scrollToBottom();
}

// ============= 状态切换 =============

void TracerouteTool::setBusyState(bool busy)
{
    m_startBtn->setEnabled(!busy);
    m_stopBtn->setEnabled(busy);
    m_targetEdit->setEnabled(!busy);
    m_maxHopsSpin->setEnabled(!busy);
    m_timeoutSpin->setEnabled(!busy);
    m_resolveNamesCheck->setEnabled(!busy);
}

void TracerouteTool::updateStatus(const QString& msg, bool err)
{
    m_status->setText(msg);
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
