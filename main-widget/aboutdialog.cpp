#include "aboutdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPixmap>

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("关于乐乐的工具箱"));
    setMinimumWidth(680);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
    setupUI();
}

void AboutDialog::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->setContentsMargins(24, 20, 24, 20);

    // 标题和版本
    auto* titleLabel = new QLabel(QString("<h2 style='margin:0;color:#212529;'>%1</h2>"
                                          "<p style='color:#868e96;margin:4px 0;'>v%2</p>")
                                  .arg(tr("乐乐的工具箱"), APP_VERSION));
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(titleLabel);

    // 简介
    auto* descLabel = new QLabel(tr("一个集成了多种实用工具的跨平台桌面应用程序。"));
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setStyleSheet("color:#495057; font-size:10pt;");
    descLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(descLabel);

    // 分割线
    auto* line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("color:#e9ecef;");
    layout->addWidget(line);

    // 开发者信息
    auto* devInfo = new QLabel(
        "<p style='margin:2px 0;'><b>" + tr("开发者") + ":</b> " + tr("武汉斯凯勒网络科技有限公司") + "</p>"
        "<p style='margin:2px 0;'><b>" + tr("官网") + ":</b> <a href='https://www.skyler.uno' style='color:#228be6;text-decoration:none;'>www.skyler.uno</a></p>"
        "<p style='margin:2px 0;'><b>" + tr("邮箱") + ":</b> duhbbx@gmail.com</p>"
        "<p style='margin:2px 0;'><b>" + tr("微信") + ":</b> tuhoooo</p>"
    );
    devInfo->setOpenExternalLinks(true);
    devInfo->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
    devInfo->setStyleSheet("font-size:12pt; color:#495057;");
    layout->addWidget(devInfo);

    // 业务推广
    auto* bizLabel = new QLabel(
        "<p style='color:#868e96;font-size:12pt;text-align:center;'>"
        + tr("我们提供专业的软件开发服务：小程序 / App / 桌面PC应用 开发") + "</p>"
    );
    bizLabel->setAlignment(Qt::AlignCenter);
    bizLabel->setWordWrap(true);
    bizLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(bizLabel);

    // 二维码区域（微信 + 打赏并排）
    auto* qrRow = new QHBoxLayout();
    qrRow->setSpacing(16);

    // 获取屏幕 DPR（Retina 屏为 2.0）
    qreal dpr = this->devicePixelRatioF();
    int displaySize = 260;
    int renderSize = static_cast<int>(displaySize * dpr);

    // 微信二维码
    auto* wechatBox = new QVBoxLayout();
    auto* wechatQr = new QLabel();
    QPixmap wechatPx(":/resources/wechat-qr.jpg");
    if (!wechatPx.isNull()) {
        QPixmap scaled = wechatPx.scaled(renderSize, renderSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        scaled.setDevicePixelRatio(dpr);
        wechatQr->setPixmap(scaled);
    }
    wechatQr->setAlignment(Qt::AlignCenter);
    auto* wechatTip = new QLabel(tr("添加微信"));
    wechatTip->setAlignment(Qt::AlignCenter);
    wechatTip->setStyleSheet("color:#868e96; font-size:8pt;");
    wechatBox->addWidget(wechatQr);
    wechatBox->addWidget(wechatTip);

    // 打赏二维码
    auto* donateBox = new QVBoxLayout();
    auto* donateQr = new QLabel();
    QPixmap donatePx(":/resources/donate-qr.jpg");
    if (!donatePx.isNull()) {
        QPixmap scaled = donatePx.scaled(renderSize, renderSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        scaled.setDevicePixelRatio(dpr);
        donateQr->setPixmap(scaled);
    }
    donateQr->setAlignment(Qt::AlignCenter);
    auto* donateTip = new QLabel(tr("请作者喝杯咖啡"));
    donateTip->setAlignment(Qt::AlignCenter);
    donateTip->setStyleSheet("color:#868e96; font-size:8pt;");
    donateBox->addWidget(donateQr);
    donateBox->addWidget(donateTip);

    qrRow->addStretch();
    qrRow->addLayout(wechatBox);
    qrRow->addLayout(donateBox);
    qrRow->addStretch();
    layout->addLayout(qrRow);

    // 让对话框自适应内容高度
    adjustSize();
    setFixedSize(size());
}
