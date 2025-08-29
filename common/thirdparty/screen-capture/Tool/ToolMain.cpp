#include <QWindow>
#include <QCoreApplication>

#include "../App/App.h"
#include "../App/Util.h"
#include "../App/Logger.h"
#include "../Win/WinPin.h"
#include "../Win/WinFull.h"
#include "../Win/CutMask.h"
#include "../Win/Canvas.h"

#include "ToolMain.h"
#include "ToolSub.h"
#include "Btn.h"
#include "BtnCheck.h"

ToolMain::ToolMain(QWidget* parent) : ToolBase(parent) {
    LOG_INFO("ToolMain", "=== ToolMain构造函数开始 ===");
    LOG_DEBUG("ToolMain", QString("ToolMain: this指针: 0x%1, parent指针: 0x%2")
              .arg(reinterpret_cast<quintptr>(this), 0, 16)
              .arg(reinterpret_cast<quintptr>(parent), 0, 16));

    LOG_DEBUG("ToolMain", "调用initWindow()初始化窗口");
    qDebug() << "ToolMain: 开始调用initWindow()";
    initWindow();
    qDebug() << "ToolMain: initWindow()调用完成";

    qDebug() << "ToolMain: 开始创建布局";
    QHBoxLayout* layout = new QHBoxLayout(this);
    qDebug() << "ToolMain: 布局对象创建完成:" << layout;
    layout->setSpacing(0);
    layout->setContentsMargins(4, 2, 4, 2);
    qDebug() << "ToolMain: 布局属性设置完成";
    LOG_DEBUG("ToolMain", "布局创建完成，间距=0，边距=(4,2,4,2)");

    qDebug() << "ToolMain: 准备获取工具列表";
    auto tools = App::getTool();
    qDebug() << "ToolMain: 工具列表获取完成，大小:" << tools.size();
    LOG_DEBUG("ToolMain", QString("获取工具列表，共 %1 个工具").arg(tools.size()));

    qDebug() << "ToolMain: 检查工具列表是否为空";
    if (tools.isEmpty()) {
        LOG_INFO("ToolMain", "工具列表为空，使用默认工具");
        LOG_DEBUG("ToolMain", "开始调用initDefaultTool");
        initDefaultTool(layout);
        LOG_DEBUG("ToolMain", "initDefaultTool调用完成");
        setFixedSize(14 * btnW + 8, 32);
        LOG_DEBUG("ToolMain", QString("设置默认工具栏大小: %1x32").arg(14 * btnW + 8));
    } else {
        LOG_INFO("ToolMain", "使用自定义工具列表");
        int btnCount { 0 };
        for (size_t i = 0; i < tools.size(); i++) {
            if (tools[i] == "|") {
                splitters.push_back(i - (int) splitters.size());
                LOG_DEBUG("ToolMain", QString("添加分隔符在位置 %1").arg(i));
            } else {
                auto btn = getTool(tools[i]);
                if (btn) {
                    layout->addWidget(btn);
                    btnCount += 1;
                    LOG_DEBUG("ToolMain", QString("添加工具按钮: %1").arg(tools[i]));
                } else {
                    LOG_WARNING("ToolMain", QString("无法创建工具按钮: %1").arg(tools[i]));
                }
            }
        }
        setFixedSize(btnCount * btnW + 8, 32);
        LOG_DEBUG("ToolMain", QString("设置自定义工具栏大小: %1x32，按钮数量: %2").arg(btnCount * btnW + 8).arg(btnCount));
    }

    setLayout(layout);
    LOG_DEBUG("ToolMain", "布局设置完成");

    if (auto pin = qobject_cast<WinPin*>(parent)) {
        setBtnEnable("pin", false);
        LOG_DEBUG("ToolMain", "父窗口是WinPin，禁用pin按钮");
    } else {
        LOG_DEBUG("ToolMain", "父窗口不是WinPin，保持pin按钮启用");
    }

    // 验证所有按钮都正确创建并强制设置为可见
    auto allBtns = findChildren<BtnBase*>();
    LOG_DEBUG("ToolMain", QString("构造完成，共创建了 %1 个按钮").arg(allBtns.size()));
    for (auto btn : allBtns) {
        LOG_DEBUG("ToolMain", QString("  - 按钮: %1, 指针: 0x%2, 父对象: 0x%3, 可见: %4, 启用: %5")
                  .arg(btn->name)
                  .arg(reinterpret_cast<quintptr>(btn), 0, 16)
                  .arg(reinterpret_cast<quintptr>(btn->parent()), 0, 16)
                  .arg(btn->isVisible() ? "是" : "否")
                  .arg(btn->isEnabled() ? "是" : "否"));
        
        // 强制设置按钮为可见
        if (!btn->isVisible()) {
            LOG_DEBUG("ToolMain", QString("强制设置按钮 %1 为可见").arg(btn->name));
            btn->setVisible(true);
        }
    }
    
    // 再次验证可见性
    LOG_DEBUG("ToolMain", "=== 设置可见性后的状态 ===");
    LOG_DEBUG("ToolMain", QString("ToolMain自身可见性: %1").arg(this->isVisible() ? "是" : "否"));
    
    // 安全地检查父窗口可见性
    QObject* parentObj = this->parent();
    QWidget* parentWidget = qobject_cast<QWidget*>(parentObj);
    bool parentVisible = parentWidget && parentWidget->isVisible();
    LOG_DEBUG("ToolMain", QString("ToolMain父窗口可见性: %1").arg(parentVisible ? "是" : "否"));
    
    for (auto btn : allBtns) {
        QObject* btnParentObj = btn->parent();
        QWidget* btnParentWidget = qobject_cast<QWidget*>(btnParentObj);
        bool btnParentVisible = btnParentWidget && btnParentWidget->isVisible();
        LOG_DEBUG("ToolMain", QString("  - 按钮: %1, 可见: %2, 父对象可见: %3")
                  .arg(btn->name)
                  .arg(btn->isVisible() ? "是" : "否")
                  .arg(btnParentVisible ? "是" : "否"));
    }
    
    // 强制设置ToolMain本身为可见
    if (!this->isVisible()) {
        LOG_DEBUG("ToolMain", "强制设置ToolMain为可见");
        this->setVisible(true);
    }
    
    LOG_INFO("ToolMain", "=== ToolMain构造函数完成 ===");
}

ToolMain::~ToolMain() {
}

void ToolMain::setBtnEnable(const QString& name, bool flag) const {
    auto btns = findChildren<Btn*>();
    for (auto& b : btns) {
        if (b->name == name) {
            b->setEnable(flag);
            break;
        }
    }
}

void ToolMain::confirmPos() {
    LOG_DEBUG("ToolMain", "=== 开始确定工具栏位置 ===");

    auto full = static_cast<WinFull*>(parent());
    if (!full) {
        LOG_ERROR("ToolMain", "父窗口为空，无法确定位置");
        return;
    }
    LOG_DEBUG("ToolMain", QString("父窗口指针有效: 0x%1").arg(reinterpret_cast<quintptr>(full), 0, 16));

    if (!full->cutMask) {
        LOG_ERROR("ToolMain", "cutMask对象为空，无法获取截图区域");
        return;
    }
    LOG_DEBUG("ToolMain", QString("cutMask指针有效: 0x%1").arg(reinterpret_cast<quintptr>(full->cutMask), 0, 16));

    auto rectMask = full->cutMask->rectMask;
    LOG_DEBUG("ToolMain", QString("截图区域: (%1,%2,%3,%4)")
              .arg(rectMask.x()).arg(rectMask.y())
              .arg(rectMask.width()).arg(rectMask.height()));

    // 检查rectMask是否有效
    if (rectMask.isEmpty()) {
        LOG_ERROR("ToolMain", "截图区域为空，无法计算位置");
        return;
    }

    LOG_DEBUG("ToolMain", "开始坐标转换");
    QPoint br, tr, tl;

    try {
        br = full->mapToGlobal(rectMask.bottomRight());
        LOG_DEBUG("ToolMain", QString("右下角全局坐标: (%1,%2)").arg(br.x()).arg(br.y()));

        tr = full->mapToGlobal(rectMask.topRight());
        LOG_DEBUG("ToolMain", QString("右上角全局坐标: (%1,%2)").arg(tr.x()).arg(tr.y()));

        tl = full->mapToGlobal(rectMask.topLeft());
        LOG_DEBUG("ToolMain", QString("左上角全局坐标: (%1,%2)").arg(tl.x()).arg(tl.y()));
    } catch (const std::exception& e) {
        LOG_ERROR("ToolMain", QString("坐标转换异常: %1").arg(e.what()));
        return;
    } catch (...) {
        LOG_ERROR("ToolMain", "坐标转换发生未知异常");
        return;
    }

    LOG_DEBUG("ToolMain", QString("全局坐标 - 左上角:(%1,%2), 右上角:(%3,%4), 右下角:(%5,%6)")
              .arg(tl.x()).arg(tl.y()).arg(tr.x()).arg(tr.y()).arg(br.x()).arg(br.y()));

    auto left { br.x() - width() };
    auto top { br.y() + 6 };
    auto heightSpan { 6 * 3 + height() * 2 }; //三个缝隙，两个高度

    LOG_DEBUG("ToolMain", QString("工具栏尺寸: %1x%2, heightSpan: %3")
              .arg(width()).arg(height()).arg(heightSpan));
    LOG_DEBUG("ToolMain", QString("初始位置计算: left=%1, top=%2").arg(left).arg(top));

    if (auto screen = QGuiApplication::screenAt(QPoint(br.x(), br.y() + heightSpan))) {
        //工具条右下角在屏幕中
        LOG_DEBUG("ToolMain", "位置策略1: 工具栏在截图区域右下角");
        LOG_DEBUG("ToolMain", QString("屏幕几何: (%1,%2,%3,%4)")
                  .arg(screen->geometry().x()).arg(screen->geometry().y())
                  .arg(screen->geometry().width()).arg(screen->geometry().height()));

        if (!QGuiApplication::screenAt(QPoint(left, top))) {
            //工具条左上角不在屏幕中
            left = screen->geometry().left();
            LOG_DEBUG("ToolMain", QString("调整left到屏幕左边界: %1").arg(left));
        }
        posState = 0;
    } else {
        //工具条右下角不在屏幕中
        LOG_DEBUG("ToolMain", "位置策略1失败，尝试策略2");
        screen = QGuiApplication::screenAt(QPoint(tr.x(), tr.y() - heightSpan)); //2. 工具条在截图区域右上角
        if (screen) {
            LOG_DEBUG("ToolMain", "位置策略2: 工具栏在截图区域右上角");
            top = tr.y() - height() - 6;
            posState = 1;
            LOG_DEBUG("ToolMain", QString("调整top到上方: %1").arg(top));

            if (!QGuiApplication::screenAt(QPoint(left, tr.y() - heightSpan))) {
                //工具条左上角不在屏幕中
                left = screen->geometry().left();
                LOG_DEBUG("ToolMain", QString("调整left到屏幕左边界: %1").arg(left));
            }
        } else {
            //3. 屏幕顶部和屏幕底部都没有足够的空间，工具条只能显示在截图区域内
            LOG_DEBUG("ToolMain", "位置策略2失败，使用策略3: 工具栏在截图区域内");
            top = br.y() - height() - 6;
            posState = 2;
            screen = QGuiApplication::screenAt(QPoint(br.x(), top));
            LOG_DEBUG("ToolMain", QString("调整top到区域内: %1").arg(top));

            if (!QGuiApplication::screenAt(QPoint(left, top))) {
                //工具条左上角不在屏幕中
                if (screen) {
                    left = screen->geometry().left();
                    LOG_DEBUG("ToolMain", QString("调整left到屏幕左边界: %1").arg(left));
                }
            }
        }
    }

    LOG_INFO("ToolMain", QString("最终位置: (%1,%2), posState=%3").arg(left).arg(top).arg(posState));
    move(left, top);
    LOG_DEBUG("ToolMain", "=== 工具栏位置确定完成 ===");
}

void ToolMain::btnCheckChange(BtnCheck* btn) {
    qDebug() << "=== ToolMain::btnCheckChange 开始 ===";
    qDebug() << "ToolMain: this指针:" << this;
    qDebug() << "ToolMain: btn指针:" << btn;
    qDebug() << "ToolMain: parent()指针:" << parent();
    
    LOG_DEBUG("ToolMain", "=== 按钮状态变化开始 ===");

    if (!btn) {
        qDebug() << "ToolMain: 按钮指针为空！";
        LOG_ERROR("ToolMain", "按钮指针为空");
        return;
    }

    qDebug() << "ToolMain: 按钮名称:" << btn->name << "状态:" << (btn->isChecked ? "选中" : "未选中");
    LOG_DEBUG("ToolMain", QString("按钮名称: %1, 状态: %2").arg(btn->name).arg(btn->isChecked ? "选中" : "未选中"));

    qDebug() << "ToolMain: 准备转换父窗口指针";
    auto win = dynamic_cast<WinBase*>(parent());
    qDebug() << "ToolMain: dynamic_cast结果:" << win;
    
    if (!win) {
        qDebug() << "ToolMain: 父窗口指针转换失败！parent()类型:" << (parent() ? parent()->metaObject()->className() : "nullptr");
        LOG_ERROR("ToolMain", "父窗口指针为空或类型转换失败");
        return;
    }

    LOG_DEBUG("ToolMain", QString("当前窗口状态: %1").arg((int)win->state));

    if (win->toolSub) {
        LOG_DEBUG("ToolMain", QString("检查现有的ToolSub，指针: 0x%1")
                  .arg(reinterpret_cast<quintptr>(win->toolSub), 0, 16));

        // 检查指针是否为已释放内存的标记值
        quintptr ptrValue = reinterpret_cast<quintptr>(win->toolSub);
        if (ptrValue == 0xcdcdcdcdcdcdcdcd || ptrValue == 0xdddddddddddddddd ||
            ptrValue == 0xfeeefeeefeeefeee || ptrValue < 0x10000) {
            LOG_ERROR("ToolMain", QString("检测到悬空指针，值: 0x%1，直接清空")
                      .arg(ptrValue, 0, 16));
            win->toolSub = nullptr;
        } else {
            try {
                LOG_DEBUG("ToolMain", "指针看起来有效，尝试安全清理");
                auto oldToolSub = win->toolSub;
                win->toolSub = nullptr; // 先设置为nullptr，避免递归调用

                // 尝试检查对象是否仍然有效
                if (oldToolSub && oldToolSub->isWidgetType()) {
                    LOG_DEBUG("ToolMain", "对象类型检查通过，调用hide()");
                    oldToolSub->hide();

                    LOG_DEBUG("ToolMain", "调用deleteLater()删除ToolSub");
                    oldToolSub->deleteLater();

                    LOG_DEBUG("ToolMain", "ToolSub清理完成");
                } else {
                    LOG_WARNING("ToolMain", "对象类型检查失败，跳过清理操作");
                }
            } catch (const std::exception& e) {
                LOG_ERROR("ToolMain", QString("关闭ToolSub时发生异常: %1").arg(e.what()));
                win->toolSub = nullptr; // 确保指针被清理
            } catch (...) {
                LOG_ERROR("ToolMain", "关闭ToolSub时发生未知异常");
                win->toolSub = nullptr; // 确保指针被清理
            }
        }

        LOG_DEBUG("ToolMain", "ToolSub关闭操作完成");
    } else {
        if (posState == 1 || posState == 2) {
            LOG_DEBUG("ToolMain", QString("调整工具栏位置，posState=%1").arg(posState));
            move(x(), y() - height() - 12);
        }
    }

    if (!btn->isChecked) {
        LOG_DEBUG("ToolMain", "按钮未选中，恢复到工具状态");
        win->state = State::tool;
        win->toolSub = nullptr;
        if (posState == 1 || posState == 2) {
            move(x(), y() + height() + 12);
        }
        LOG_DEBUG("ToolMain", "=== 按钮状态变化完成（未选中） ===");
        return;
    }

    LOG_DEBUG("ToolMain", "=== 开始工具切换逻辑 ===");
    LOG_DEBUG("ToolMain", QString("新选中的工具: %1").arg(btn->name));

    auto btns = findChildren<BtnCheck*>();
    LOG_DEBUG("ToolMain", QString("找到 %1 个按钮").arg(btns.size()));

    QString previousTool = "无";
    int deactivatedCount = 0;

    for (auto& b : btns) {
        if (b == btn) continue;
        if (b->isChecked) {
            previousTool = b->name;
            LOG_INFO("ToolMain", QString("从工具 '%1' 切换到工具 '%2'").arg(b->name).arg(btn->name));
            LOG_DEBUG("ToolMain", QString("取消按钮 %1 的选中状态").arg(b->name));
            b->isChecked = false;
            b->update();
            deactivatedCount++;
        }
    }

    if (deactivatedCount == 0) {
        LOG_INFO("ToolMain", QString("首次激活工具: %1").arg(btn->name));
    } else {
        LOG_INFO("ToolMain", QString("工具切换完成: %1 → %2, 取消了 %3 个按钮")
                 .arg(previousTool).arg(btn->name).arg(deactivatedCount));
    }

    LOG_DEBUG("ToolMain", "=== 工具切换逻辑完成 ===");

    LOG_INFO("ToolMain", QString("设置窗口状态为: %1").arg((int)btn->state));
    win->state = btn->state;

    if (btn->state == State::text) {
        LOG_DEBUG("ToolMain", "设置文本工具光标");
        win->setCursor(Qt::IBeamCursor);
    } else {
        LOG_DEBUG("ToolMain", "设置十字光标");
        win->setCursor(Qt::CrossCursor);
    }

    btnCheckedCenterX = btn->x() + btn->width() / 2;
    LOG_DEBUG("ToolMain", QString("按钮中心X坐标: %1").arg(btnCheckedCenterX));

    LOG_DEBUG("ToolMain", "=== 开始创建新的ToolSub ===");
    LOG_DEBUG("ToolMain", QString("为工具 '%1' (状态=%2) 创建ToolSub").arg(btn->name).arg((int)btn->state));

    // 确保之前的ToolSub已经被清理
    if (win->toolSub != nullptr) {
        LOG_WARNING("ToolMain", QString("警告: win->toolSub不为空，值: 0x%1")
                    .arg(reinterpret_cast<quintptr>(win->toolSub), 0, 16));
        win->toolSub = nullptr;
    }

    try {
        LOG_DEBUG("ToolMain", "调用 new ToolSub(win)");
        qDebug() << "ToolMain: 即将创建ToolSub，按钮状态:" << (int)btn->state;
        
        win->toolSub = new ToolSub(win);
        qDebug() << "ToolMain: ToolSub创建完成";

        if (win->toolSub) {
            LOG_INFO("ToolMain", QString("ToolSub创建成功，指针: 0x%1")
                     .arg(reinterpret_cast<quintptr>(win->toolSub), 0, 16));
            LOG_DEBUG("ToolMain", "验证ToolSub对象有效性");

            // 验证ToolSub是否正确创建
            if (win->toolSub->isWidgetType()) {
                LOG_DEBUG("ToolMain", "ToolSub对象类型验证通过");
                qDebug() << "ToolMain: ToolSub对象验证成功";
            } else {
                LOG_ERROR("ToolMain", "ToolSub对象类型验证失败");
                qDebug() << "ToolMain: ToolSub对象验证失败";
            }
        } else {
            LOG_ERROR("ToolMain", "ToolSub创建后为nullptr");
            qDebug() << "ToolMain: ToolSub创建失败，为nullptr";
            return;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("ToolMain", QString("创建ToolSub时发生异常: %1").arg(e.what()));
        qDebug() << "ToolMain: ToolSub创建异常:" << e.what();
        win->toolSub = nullptr;
        return;
    } catch (...) {
        LOG_ERROR("ToolMain", "创建ToolSub时发生未知异常");
        qDebug() << "ToolMain: ToolSub创建发生未知异常";
        win->toolSub = nullptr;
        return;
    }

    LOG_DEBUG("ToolMain", "=== ToolSub创建完成 ===");

    LOG_DEBUG("ToolMain", "调用update()更新界面");
    update();

    LOG_DEBUG("ToolMain", "=== 按钮状态变化完成 ===");
}

void ToolMain::btnClick(Btn* btn) {
    qDebug() << "ToolMain: btnClick开始，按钮:" << btn->name;
    
    auto win = dynamic_cast<WinBase*>(parent());
    if (!win) {
        qDebug() << "ToolMain: 父窗口转换失败";
        return;
    }
    
    qDebug() << "ToolMain: 父窗口转换成功，执行按钮操作";
    
    if (btn->name == "clipboard") {
        qDebug() << "ToolMain: 保存到剪贴板";
        win->saveToClipboard();
    } else if (btn->name == "save") {
        qDebug() << "ToolMain: 保存到文件";
        win->saveToFile();
    } else if (btn->name == "undo") {
        qDebug() << "ToolMain: 撤销操作";
        if (win->canvas) {
            win->canvas->undo();
        } else {
            qDebug() << "ToolMain: canvas为空，无法撤销";
        }
    } else if (btn->name == "redo") {
        qDebug() << "ToolMain: 重做操作";
        if (win->canvas) {
            win->canvas->redo();
        } else {
            qDebug() << "ToolMain: canvas为空，无法重做";
        }
    } else if (btn->name == "pin") {
        qDebug() << "ToolMain: 固定截图";
        if (auto winFull = dynamic_cast<WinFull*>(parent())) {
            winFull->pin();
        } else {
            qDebug() << "ToolMain: 父窗口不是WinFull类型，无法固定";
        }
    } else if (btn->name == "close") {
        qDebug() << "ToolMain: 关闭截图窗口";
        win->close();
        // 不退出应用程序，让截图窗口正常关闭
    } else {
        qDebug() << "ToolMain: 未知按钮名称:" << btn->name;
    }
    
    qDebug() << "ToolMain: btnClick完成";
}

void ToolMain::paintEvent(QPaintEvent* event) {
    LOG_DEBUG("ToolMain", "=== ToolMain::paintEvent 开始 ===");
    
    try {
        QPainter painter(this);
        LOG_DEBUG("ToolMain", "QPainter创建成功");
        
        LOG_DEBUG("ToolMain", "开始获取图标字体");
        auto font = Util::getIconFont(15);
        if (font) {
            LOG_DEBUG("ToolMain", "字体获取成功");
            font->setPixelSize(15);
            painter.setFont(*font);
        } else {
            LOG_WARNING("ToolMain", "字体获取失败，使用默认字体");
            QFont defaultFont;
            defaultFont.setPixelSize(15);
            painter.setFont(defaultFont);
        }
        
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        LOG_DEBUG("ToolMain", "画笔设置完成");
        
        QPen pen;
        pen.setColor(QColor(22, 118, 255));
        pen.setWidthF(border);
        painter.setPen(pen);
        painter.setBrush(Qt::white);
        painter.drawRect(rect().toRectF().adjusted(border, border, -border, -border));
        painter.setPen(QColor(190, 190, 190));
        auto y1 { height() - 9 };
        
        LOG_DEBUG("ToolMain", "开始绘制分隔线");
        const auto tools = App::getTool();
        if (tools.isEmpty()) {
            auto x { 4 + 8 * btnW + 0.5 };
            auto y1 { height() - 9 };
            painter.drawLine(x, 9, x, y1);
            x = 4 + 10 * btnW + 0.5;
            painter.drawLine(x, 9, x, y1);
        } else {
            for (size_t i = 0; i < splitters.size(); i++) {
                auto x { 4 + splitters[i] * btnW + 0.5 };
                painter.drawLine(x, 9, x, y1);
            }
        }
        
        LOG_DEBUG("ToolMain", "=== ToolMain::paintEvent 完成 ===");
    } catch (const std::exception& e) {
        LOG_ERROR("ToolMain", QString("paintEvent异常: %1").arg(e.what()));
    } catch (...) {
        LOG_ERROR("ToolMain", "paintEvent发生未知异常");
    }
}

void ToolMain::closeEvent(QCloseEvent* event) {
    auto win = static_cast<WinBase*>(parent());
    if (win->toolSub) {
        win->toolSub->close();
    }
    deleteLater();
    win->toolMain = nullptr;
}

QWidget* ToolMain::getTool(const QString& toolName) {
    if (toolName == "rect") {
        return new BtnCheck("rect", QChar(0xe8e8), this, State::rect);
    } else if (toolName == "ellipse") {
        return new BtnCheck("ellipse", QChar(0xe6bc), this, State::ellipse);
    } else if (toolName == "arrow") {
        return new BtnCheck("arrow", QChar(0xe603), this, State::arrow);
    } else if (toolName == "number") {
        return new BtnCheck("number", QChar(0xe776), this, State::number);
    } else if (toolName == "line") {
        return new BtnCheck("line", QChar(0xe601), this, State::line);
    } else if (toolName == "text") {
        return new BtnCheck("text", QChar(0xe6ec), this, State::text);
    } else if (toolName == "mosaic") {
        return new BtnCheck("mosaic", QChar(0xe82e), this, State::mosaic);
    } else if (toolName == "eraser") {
        return new BtnCheck("eraser", QChar(0xe6be), this, State::eraser);
    } else if (toolName == "undo") {
        return new Btn("undo", QChar(0xed85), false, this);
    } else if (toolName == "redo") {
        return new Btn("redo", QChar(0xed8a), false, this);
    } else if (toolName == "pin") {
        return new Btn("pin", QChar(0xe6a3), this);
    } else if (toolName == "clipboard") {
        return new Btn("clipboard", QChar(0xe87f), this);
    } else if (toolName == "save") {
        return new Btn("save", QChar(0xe6c0), this);
    } else if (toolName == "close") {
        return new Btn("close", QChar(0xe6e7), this);
    }
    return nullptr;
}

void ToolMain::initDefaultTool(QHBoxLayout* layout) {
    LOG_DEBUG("ToolMain", "initDefaultTool: 开始创建默认按钮");
    LOG_DEBUG("ToolMain", QString("initDefaultTool: layout指针: 0x%1, this指针: 0x%2")
              .arg(reinterpret_cast<quintptr>(layout), 0, 16)
              .arg(reinterpret_cast<quintptr>(this), 0, 16));
    
    try {
        LOG_DEBUG("ToolMain", "initDefaultTool: 创建rect按钮");
        auto rectBtn = new BtnCheck("rect", QChar(0xe8e8), this, State::rect);
        LOG_DEBUG("ToolMain", QString("initDefaultTool: rect按钮创建完成: 0x%1")
                  .arg(reinterpret_cast<quintptr>(rectBtn), 0, 16));
        layout->addWidget(rectBtn);
        LOG_DEBUG("ToolMain", "initDefaultTool: rect按钮添加到布局");
        
        qDebug() << "initDefaultTool: 创建ellipse按钮";
        auto ellipseBtn = new BtnCheck("ellipse", QChar(0xe6bc), this, State::ellipse);
        layout->addWidget(ellipseBtn);
        
        qDebug() << "initDefaultTool: 创建arrow按钮";
        layout->addWidget(new BtnCheck("arrow", QChar(0xe603), this, State::arrow));
        
        qDebug() << "initDefaultTool: 创建number按钮";
        layout->addWidget(new BtnCheck("number", QChar(0xe776), this, State::number));
        
        qDebug() << "initDefaultTool: 创建line按钮";
        layout->addWidget(new BtnCheck("line", QChar(0xe601), this, State::line));
        
        qDebug() << "initDefaultTool: 创建text按钮";
        layout->addWidget(new BtnCheck("text", QChar(0xe6ec), this, State::text));
        
        qDebug() << "initDefaultTool: 创建mosaic按钮";
        layout->addWidget(new BtnCheck("mosaic", QChar(0xe82e), this, State::mosaic));
        
        qDebug() << "initDefaultTool: 创建eraser按钮";
        layout->addWidget(new BtnCheck("eraser", QChar(0xe6be), this, State::eraser));
        
        qDebug() << "initDefaultTool: 创建undo按钮";
        layout->addWidget(new Btn("undo", QChar(0xed85), false, this));
        
        qDebug() << "initDefaultTool: 创建redo按钮";
        layout->addWidget(new Btn("redo", QChar(0xed8a), false, this));
        
        qDebug() << "initDefaultTool: 创建pin按钮";
        layout->addWidget(new Btn("pin", QChar(0xe6a3), this));
        
        qDebug() << "initDefaultTool: 创建clipboard按钮";
        layout->addWidget(new Btn("clipboard", QChar(0xe87f), this));
        
        qDebug() << "initDefaultTool: 创建save按钮";
        layout->addWidget(new Btn("save", QChar(0xe6c0), this));
        
        qDebug() << "initDefaultTool: 创建close按钮";
        layout->addWidget(new Btn("close", QChar(0xe6e7), this));
        
        qDebug() << "initDefaultTool: 所有按钮创建完成";
    } catch (const std::exception& e) {
        qDebug() << "initDefaultTool: 异常:" << e.what();
    } catch (...) {
        qDebug() << "initDefaultTool: 未知异常";
    }
}
