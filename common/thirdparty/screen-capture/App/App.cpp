#include <QApplication>
#include <QClipboard>
#include <QTimer>
#include <QMimeData>
#include <iostream>
#include <fcntl.h>
#include <Windows.h>
#include <io.h>
#include <cstdio>
#include <cstdlib>
#include "App.h"
#include "Lang.h"
#include "Util.h"
#include "Logger.h"
#include "Win/WinFull.h"
#include "Win/WinPin.h"
#include "Win/WinLong.h"

namespace {
    std::unique_ptr<App> app;
    QString defaultSavePath;
    int compressSize { 100 };
    int compressQuality { -1 };
    int customCap { 2 }; // 默认显示工具栏进行编辑
    QStringList tools;
}

QMap<QString, QString> App::getCmd() {
    QStringList args = QCoreApplication::arguments();
    QMap<QString, QString> params;
    for (int i = 1; i < args.size(); ++i) {
        QString arg = args[i].trimmed();
        if (!arg.startsWith("--")) {
            continue;
        }
        arg = arg.mid(2);
        const int colonIndex = arg.indexOf(':');
        if (colonIndex == -1) {
            continue;
        }
        QString key = arg.left(colonIndex).toLower();
        QString value = arg.mid(colonIndex + 1);
        if (value.startsWith('"') && value.endsWith('"')) {
            value = value.mid(1, value.length() - 2);
        }
        params[key] = value;
    }
    return params;
}

bool App::parseCmd(const QMap<QString, QString>& params) {
    if (params.contains("lang")) {
        if (const QString lang = params["lang"]; lang == "en") {
            Lang::init("en");
        } else {
            Lang::init("zhcn");
        }
    } else {
        Lang::init("zhcn");
    }
    if (params.contains("tool")) {
        tools = params["tool"].split(",");
    }
    if (params.contains("comp")) {
        if (!setCompressVal(params["comp"]))
            return false;
    }
    if (params.contains("path")) {
        defaultSavePath = params["path"];
    }
    if (params.contains("pin")) {
        const auto pinVal = params["pin"];
        if (pinVal.startsWith("clipboard")) {
            pinClipboard(pinVal);
            return true;
        }
        if (pinVal.startsWith("file")) {
            pinFile(pinVal);
            return true;
        }
        if (pinVal.startsWith("area")) {
            pinArea(pinVal);
            return true;
        }
    }
    if (params.contains("cap")) {
        const auto cap = params["cap"];
        if (cap.startsWith("area")) {
            capArea(cap);
            return true;
        }
        if (cap.startsWith("fullscreen")) {
            capFullscreen(cap);
            return true;
        }
        if (cap.startsWith("long")) {
            new WinLong();
            return true;
        }
        if (cap.startsWith("custom")) {
            capCustom(cap);
            return false;
        }
    }
    return false;
}

void App::pinClipboard(const QString& cmd) {
    auto arr = cmd.split(",");
    int x { 100 }, y { 100 };
    bool ok;
    if (arr.size() == 3) {
        x = arr[1].toInt(&ok);
        if (!ok) {
            qDebug() << "pin clipboard param error.";
            return; // 不退出应用程序，直接返回
        }
        y = arr[2].toInt(&ok);
        if (!ok) {
            qDebug() << "pin clipboard param error.";
            return; // 不退出应用程序，直接返回
        }
    }
    const QClipboard* clipboard = QApplication::clipboard();
    const QMimeData* mimeData = clipboard->mimeData();
    QImage img;
    if (mimeData->hasImage()) {
        img = clipboard->image();
    } else if (mimeData->hasUrls()) {
        const auto url = mimeData->urls().first();
        if (const QString filePath = url.toLocalFile(); Util::isImagePath(filePath)) {
            img.load(filePath);
        }
    } else if (mimeData->hasText()) {
        if (const QString filePath = clipboard->text(); Util::isImagePath(filePath)) {
            img.load(filePath);
        }
    }
    if (img.isNull()) {
        qDebug() << "No image in clipboard.";
        return; // 不退出应用程序
    }
    new WinPin(QPoint(x, y), img);
}

void App::pinFile(const QString& cmd) {
    auto arr = cmd.split(",");
    if (arr.size() < 2) {
        qDebug() << "pin file param error.";
        return; // 不退出应用程序
    }
    QImage img(arr[1]);
    if (img.isNull()) {
        qDebug() << "Image Path error.";
        return; // 不退出应用程序
        return;
    }
    int x { 100 }, y { 100 };
    bool ok;
    if (arr.size() == 4) {
        x = arr[2].toInt(&ok);
        if (!ok) {
            qDebug() << "pin clipboard param error.";
            return; // 不退出应用程序，直接返回
        }
        y = arr[3].toInt(&ok);
        if (!ok) {
            qDebug() << "pin clipboard param error.";
            return; // 不退出应用程序，直接返回
        }
    }
    new WinPin(QPoint(x, y), img);
}

void App::pinArea(const QString& cmd) {
    auto arr = cmd.split(",");
    if (arr.size() < 5) {
        qDebug() << "pin area param error.";
        return; // 不退出应用程序
        return;
    }
    bool ok;
    auto x = arr[1].toInt(&ok);
    if (!ok) {
        qDebug() << "pin area param error.";
        return; // 不退出应用程序
        return;
    }
    auto y = arr[2].toInt(&ok);
    if (!ok) {
        qDebug() << "pin area param error.";
        return; // 不退出应用程序
        return;
    }
    auto w = arr[3].toInt(&ok);
    if (!ok) {
        qDebug() << "pin area param error.";
        return; // 不退出应用程序
        return;
    }
    auto h = arr[4].toInt(&ok);
    if (!ok) {
        qDebug() << "pin area param error.";
        return; // 不退出应用程序
        return;
    }
    int x1 { 100 }, y1 { 100 };
    if (arr.size() == 7) {
        x1 = arr[5].toInt(&ok);
        if (!ok) {
            qDebug() << "pin area param error.";
            return; // 不退出应用程序
        }
        y1 = arr[6].toInt(&ok);
        if (!ok) {
            qDebug() << "pin area param error.";
            return; // 不退出应用程序
        }
    }
    QImage img = Util::printScreen(x, y, w, h);
    new WinPin(QPoint(x1, y1), img);
}

bool App::setCompressVal(const QString& cmd) {
    auto arr = cmd.split(",");
    bool ok;
    if (arr.size() >= 1) {
        compressQuality = arr[0].toInt(&ok);
        if (!ok) {
            qDebug() << "comp param error.";
            return false; // 返回false表示失败
        }
    }
    if (arr.size() >= 2) {
        compressSize = arr[1].toInt(&ok);
        if (!ok) {
            qDebug() << "comp param error.";
            return false; // 返回false表示失败
        }
    }
    return true;
}

void App::capArea(const QString& cmd) {
    auto arr = cmd.split(",");
    if (arr.size() < 5) {
        qDebug() << "cap area param error.";
        return; // 不退出应用程序
    }
    bool ok;
    auto x = arr[1].toInt(&ok);
    if (!ok) {
        qDebug() << "cap area param error.";
        return; // 不退出应用程序
    }
    auto y = arr[2].toInt(&ok);
    if (!ok) {
        qDebug() << "cap area param error.";
        return; // 不退出应用程序
    }
    auto w = arr[3].toInt(&ok);
    if (!ok) {
        qDebug() << "cap area param error.";
        return; // 不退出应用程序
    }
    auto h = arr[4].toInt(&ok);
    if (!ok) {
        qDebug() << "cap area param error.";
        return; // 不退出应用程序
    }
    QImage img = Util::printScreen(x, y, w, h);
    if (arr.size() == 6) {
        if (arr[5] == "clipboard") {
            Util::imgToClipboard(img);
            return; // 不退出应用程序
        }
    } else {
        Util::saveToFile(img);
        return; // 不退出应用程序
    }
}

void App::capFullscreen(const QString& cmd) {
    const auto x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const auto y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const auto w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const auto h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    const auto img = Util::printScreen(x, y, w, h);
    if (auto arr = cmd.split(","); arr.size() == 2) {
        if (arr[1] == "clipboard") {
            Util::imgToClipboard(img);
            return; // 不退出应用程序
        }
    } else {
        Util::saveToFile(img);
        return; // 不退出应用程序
    }
}

bool App::capCustom(const QString& cmd) {
    if (auto arr = cmd.split(","); arr.size() == 2) {
        if (arr[1] == "clipboard") {
            customCap = 1;
        }
    } else {
        customCap = 0;
    }
    return true;
}

void App::exit(const int& code) {
    // 在集成模式下不退出应用程序，只记录日志
    LOG_INFO(MODULE_APP, QString("App::exit() 被调用，代码: %1（已禁用退出）").arg(code));
    qDebug() << "App::exit() 调用被忽略，代码:" << code;
}

void App::attachConsole() {
    if (::AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE* unused;
        if (freopen_s(&unused, "CONOUT$", "w", stdout)) {
            _dup2(_fileno(stdout), 1);
        }
        if (freopen_s(&unused, "CONOUT$", "w", stderr)) {
            _dup2(_fileno(stdout), 2);
        }
        std::ios::sync_with_stdio();
        qDebug() << "Hello ScreenCapture";
    }
}

void App::init() {
    LOG_INFO(MODULE_APP, "App::init() 开始");

    const QFont font("Microsoft YaHei", 9);
    qApp->setFont(font);
    LOG_DEBUG(MODULE_APP, "设置应用程序字体: Microsoft YaHei, 9pt");

    const auto cmds = getCmd();
    LOG_INFO(MODULE_APP, QString("解析到 %1 个命令行参数").arg(cmds.size()));

    if (cmds.size() > 0) {
        LOG_DEBUG(MODULE_APP, "附加控制台输出");
        attachConsole();
    }

    if (const auto flag = parseCmd(cmds); !flag) {
        LOG_INFO(MODULE_APP, "创建全屏截图窗口");
        new WinFull();
    } else {
        LOG_INFO(MODULE_APP, "命令行模式，不创建窗口");
    }

    LOG_INFO(MODULE_APP, "App::init() 完成");
}

void App::dispose() {
    LOG_INFO(MODULE_APP, "App::dispose() 开始");
    app.reset();
    LOG_INFO(MODULE_APP, "App::dispose() 完成");
}

QString App::getSavePath() {
    return defaultSavePath;
}

std::tuple<int, int> App::getCompressVal() {
    return { compressQuality, compressSize };
}

int App::getCustomCap() {
    qDebug() << "App::getCustomCap() 返回值:" << customCap;
    return customCap;
}

void App::setCustomCap(const int cap) {
    qDebug() << "App::setCustomCap() 设置值:" << cap;
    customCap = cap;
    qDebug() << "App::setCustomCap() 设置后的值:" << customCap;
}

QStringList App::getTool() {
    return tools;
}
