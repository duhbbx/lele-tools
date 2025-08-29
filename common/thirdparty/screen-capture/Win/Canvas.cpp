/**
 * @file Canvas.cpp
 * @brief 截图编辑画布实现文件
 *
 * Canvas类是截图应用的核心绘制和编辑组件，负责：
 *
 * 🎨 **主要功能**：
 * 1. 管理所有绘制的形状对象（矩形、椭圆、箭头、文本、数字标号等）
 * 2. 处理鼠标事件，实现形状的创建、编辑和交互
 * 3. 维护画布图像（imgBoard）和形状列表（shapes）
 * 4. 提供撤销/重做功能
 * 5. 协调工具栏和形状之间的交互
 *
 * 📊 **主要数据结构**：
 * - imgBg: 背景图像（截图原图，只读）
 * - imgBoard: 绘制板图像（包含所有已完成的形状，持久化存储）
 * - imgCanvas: 画布图像（用于临时绘制和缓存）
 * - shapes: 形状对象列表（QList<ShapeBase*>，管理所有形状的生命周期）
 * - shapeCur: 当前正在编辑的形状指针
 * - shapeHover: 当前悬停的形状指针（用于显示拖拽器）
 *
 * 🔄 **工作流程**：
 * 1. 用户选择工具 → WinBase设置状态 → Canvas根据状态创建对应形状
 * 2. 鼠标按下 → mousePress() → 创建新形状或激活现有形状编辑
 * 3. 鼠标拖拽 → mouseDrag() → 实时更新形状几何属性
 * 4. 鼠标释放 → mouseRelease() → 完成形状，调用paintShapeOnBoard()添加到画板
 * 5. 形状管理 → 支持撤销(undo)、重做(redo)、删除等操作
 *
 * 🖼️ **绘制机制**：
 * - paint(): 主绘制方法，按顺序绘制背景→画板→当前形状→拖拽器
 * - paintShapeOnBoard(): 将完成的形状永久绘制到imgBoard上
 * - imgBoard作为形状的持久化存储，确保形状在工具切换后仍然可见
 *
 * 🛠️ **形状生命周期**：
 * 创建 → 编辑(temp/moving/sizing) → 完成(ready) → 添加到列表 → 可撤销(undo) → 删除
 *
 * @author ScreenCapture Team
 * @date 2025
 * @version 1.0
 */

#include <windows.h>
#include <QDateTime>

#include "Canvas.h"

#include "WinBase.h"
#include "CutMask.h"
#include "App/Util.h"
#include "App/Logger.h"
#include "Tool/ToolMain.h"
#include "Tool/ToolSub.h"
#include "Shape/ShapeBase.h"
#include "Shape/ShapeRect.h"
#include "Shape/ShapeEllipse.h"
#include "Shape/ShapeArrow.h"
#include "Shape/ShapeNumber.h"
#include "Shape/ShapeLine.h"
#include "Shape/ShapeText.h"
#include "Shape/ShapeEraserRect.h"
#include "Shape/ShapeEraserLine.h"
#include "Shape/ShapeMosaicRect.h"
#include "Shape/ShapeMosaicLine.h"

/**
 * @brief Canvas构造函数
 * @param img 截图背景图像
 * @param parent 父窗口对象（通常是WinFull或WinPin）
 *
 * 初始化画布的三个核心图像：
 * - imgBg: 背景图像（截图原图，不可修改）
 * - imgBoard: 绘制板图像（用于持久化存储已完成的形状）
 * - imgCanvas: 画布图像（用于临时绘制和缓存）
 *
 * 同时初始化形状指针为nullptr，确保内存安全。
 */
Canvas::Canvas(const QImage& img, QObject* parent) : QObject(parent), imgBg { img }, timerDragger(nullptr) {
    LOG_INFO("Canvas", "=== Canvas构造函数开始 ===");

    auto win = static_cast<WinBase*>(parent);

    // 创建绘制板和画布图像的副本
    // imgBoard: 用于永久存储已完成的形状，确保形状持久化
    // imgCanvas: 用于临时绘制和缓存操作
    imgBoard = imgBg.copy();
    imgCanvas = imgBg.copy();

    // 重要：初始化形状指针为nullptr，避免野指针访问
    shapeCur = nullptr; // 当前正在编辑的形状
    shapeHover = nullptr; // 当前悬停的形状（用于显示拖拽器）

    LOG_DEBUG("Canvas", QString("背景图像大小: %1x%2").arg(imgBg.width()).arg(imgBg.height()));
    LOG_DEBUG("Canvas", QString("绘制板图像大小: %1x%2").arg(imgBoard.width()).arg(imgBoard.height()));
    LOG_DEBUG("Canvas", QString("画布图像大小: %1x%2").arg(imgCanvas.width()).arg(imgCanvas.height()));
    LOG_DEBUG("Canvas", "shapeCur和shapeHover已初始化为nullptr");

    LOG_INFO("Canvas", "=== Canvas构造函数完成 ===");
}

/**
 * @brief Canvas析构函数
 *
 * 负责清理Canvas对象的资源：
 * 1. 清理形状指针（shapeCur, shapeHover）
 * 2. 清空shapes列表（注意：形状对象的生命周期由Qt的父子关系管理）
 * 3. 图像资源会由QImage的析构函数自动清理
 *
 * 注意：shapes列表中的形状对象不在这里删除，它们的生命周期
 * 由Qt的父子对象关系管理（parent是WinBase）。
 */
Canvas::~Canvas() {
    LOG_INFO("Canvas", "=== Canvas析构函数开始 ===");

    // 安全清理当前形状指针（设为nullptr，避免悬空指针）
    if (shapeCur) {
        LOG_DEBUG("Canvas", "清理shapeCur指针");
        shapeCur = nullptr;
    }

    if (shapeHover) {
        LOG_DEBUG("Canvas", "清理shapeHover指针");
        shapeHover = nullptr;
    }

    LOG_DEBUG("Canvas", QString("清理shapes列表，共有 %1 个形状").arg(shapes.size()));
    // 注意：shapes列表中的对象生命周期由Qt的父子关系管理，这里只清空列表
    shapes.clear();

    LOG_INFO("Canvas", "=== Canvas析构函数完成 ===");
}

/**
 * @brief 撤销操作
 *
 * 将最后一个有效形状标记为撤销状态，并重新绘制imgBoard。
 * 撤销机制：
 * 1. 从后往前查找第一个非撤销状态的形状
 * 2. 将其状态设为ShapeState::undo
 * 3. 如果是数字形状，需要调整全局数字计数器
 * 4. 重新绘制imgBoard（排除撤销状态的形状）
 * 5. 更新工具栏按钮状态
 */
void Canvas::undo() {
    if (shapes.isEmpty()) return;

    // 从后往前查找第一个可撤销的形状（非undo状态）
    for (int i = shapes.size() - 1; i >= 0; i--) {
        if (shapes[i]->state != ShapeState::undo) {
            shapes[i]->state = ShapeState::undo;

            // 特殊处理：数字形状需要调整全局计数器
            if (qobject_cast<ShapeNumber*>(shapes[i])) {
                ShapeNumber::resetValBy(-1);
            }
            break; // 只撤销一个形状
        }
    }
    const auto win = static_cast<WinBase*>(parent());
    bool hasReady { false };
    for (auto& s : shapes) {
        if (s->state != ShapeState::undo) {
            hasReady = true;
            break;
        }
    }
    if (!hasReady) {
        win->toolMain->setBtnEnable("undo", false);
    }
    win->toolMain->setBtnEnable("redo", true);
    imgBoard.fill(Qt::transparent);
    QPainter p(&imgBoard);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    for (const auto& s : shapes) {
        if (s->state == ShapeState::undo) continue;
        s->paint(&p);
    }
    win->update();
}

void Canvas::redo() {
    if (shapes.isEmpty()) return;
    for (int i = 0; i < shapes.size(); i++) {
        if (shapes[i]->state == ShapeState::undo) {
            shapes[i]->state = ShapeState::ready;
            if (qobject_cast<ShapeNumber*>(shapes[i])) {
                ShapeNumber::resetValBy(1);
            }
            break;
        }
    }
    const auto win = static_cast<WinBase*>(parent());

    bool hasUndo { false };
    for (auto& s : shapes) {
        if (s->state == ShapeState::undo) {
            hasUndo = true;
            break;
        }
    }
    if (!hasUndo) {
        win->toolMain->setBtnEnable("redo", false);
    }
    win->toolMain->setBtnEnable("undo", true);
    imgBoard.fill(Qt::transparent);
    QPainter p(&imgBoard);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    for (auto& s : shapes) {
        if (s->state == ShapeState::undo) continue;
        s->paint(&p);
    }
    win->update();
}

/**
 * @brief 处理鼠标按下事件
 * @param event 鼠标事件对象
 *
 * 鼠标按下事件的处理逻辑：
 * 1. 首先检查现有形状是否能处理该事件（形状编辑模式）
 * 2. 如果没有形状处理，则创建新形状（形状创建模式）
 * 3. 根据当前工具状态（win->state）创建对应类型的形状
 * 4. 调用新形状的mousePress方法进行初始化
 *
 * 形状重用机制：
 * - 已完成的形状（ready状态）可以被重新激活进行编辑
 * - 这允许用户修改已绘制的形状（移动、调整大小等）
 *
 * 形状创建流程：
 * - 调用addShape()根据工具状态创建对应形状
 * - 设置shapeCur指向新创建的形状
 * - 调用形状的mousePress进行初始化
 */
void Canvas::mousePress(QMouseEvent* event) {
    LOG_DEBUG("Canvas", "=== Canvas鼠标按下事件开始 ===");
    LOG_DEBUG("Canvas", QString("鼠标位置: (%1,%2), 按钮: %3")
              .arg(event->pos().x()).arg(event->pos().y()).arg(event->buttons()));

    if (auto win = static_cast<WinBase*>(parent())) {
        LOG_DEBUG("Canvas", QString("当前窗口状态: %1").arg(static_cast<int>(win->state)));
    }

    LOG_DEBUG("Canvas", QString("检查现有形状，共 %1 个").arg(shapes.size()));

    for (int i = shapes.size() - 1; i >= 0; i--) {
        if (shapes[i]->state == ShapeState::undo) {
            LOG_DEBUG("Canvas", QString("跳过撤销状态的形状 %1").arg(i));
            continue;
        }

        LOG_DEBUG("Canvas", QString("检查形状 %1 的鼠标按下，指针: 0x%2")
                  .arg(i).arg(reinterpret_cast<quintptr>(shapes[i]), 0, 16));

        try {
            auto flag = shapes[i]->mousePress(event);
            LOG_DEBUG("Canvas", QString("形状 %1 mousePress返回: %2").arg(i).arg(flag ? "true" : "false"));

            if (flag) {
                LOG_INFO("Canvas", QString("重用现有形状 %1 (指针: 0x%2)")
                         .arg(i).arg(reinterpret_cast<quintptr>(shapes[i]), 0, 16));
                // 设置shapeCur为被重用的形状
                shapeCur = shapes[i];
                LOG_DEBUG("Canvas", QString("设置shapeCur为重用的形状: 0x%1")
                          .arg(reinterpret_cast<quintptr>(shapeCur), 0, 16));
                return;
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Canvas", QString("形状 %1 mousePress异常: %2").arg(i).arg(e.what()));
            continue;
        } catch (...) {
            LOG_ERROR("Canvas", QString("形状 %1 mousePress未知异常").arg(i));
            continue;
        }
    }

    LOG_DEBUG("Canvas", "没有现有形状处理鼠标事件，创建新形状");

    try {
        addShape();
        LOG_DEBUG("Canvas", "新形状创建成功");

        if (shapeCur) {
            LOG_DEBUG("Canvas", QString("调用新形状的mousePress，指针: 0x%1")
                      .arg(reinterpret_cast<quintptr>(shapeCur), 0, 16));
            shapeCur->mousePress(event);
            LOG_DEBUG("Canvas", "新形状mousePress调用完成");
        } else {
            LOG_ERROR("Canvas", "shapeCur为空，无法调用mousePress");
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Canvas", QString("创建或处理新形状时发生异常: %1").arg(e.what()));
    } catch (...) {
        LOG_ERROR("Canvas", "创建或处理新形状时发生未知异常");
    }

    LOG_DEBUG("Canvas", "=== Canvas鼠标按下事件完成 ===");
}

void Canvas::mouseDrag(QMouseEvent* event) const {
    if (shapeCur) {
        shapeCur->mouseDrag(event);
    }
}

void Canvas::mouseMove(QMouseEvent* event) {
    auto flag { false };
    for (int i = shapes.size() - 1; i >= 0; i--) {
        if (shapes[i]->state == ShapeState::undo) continue;
        flag = shapes[i]->mouseMove(event);
        if (flag) {
            return;
        }
    }
    if (!flag) {
        auto win = (WinBase*) parent();
        if (win->state == State::text) {
            win->setCursor(Qt::IBeamCursor);
        } else {
            win->setCursor(Qt::CrossCursor);
        }
    }
}

void Canvas::mouseRelease(QMouseEvent* event) {
    if (shapeCur) {
        if (shapeCur->mouseRelease(event)) {
            if (const auto win = static_cast<WinBase*>(parent()); win->state == State::text) {
                return;
            }
            paintShapeOnBoard(shapeCur);
        } else {
            removeShapeCur();
        }
        shapeCur = nullptr;
    }
}

/**
 * @brief Canvas主绘制方法
 * @param p QPainter对象，用于绘制到目标设备
 *
 * Canvas的绘制层次结构（从底到顶）：
 * 1. 背景图像（imgBg）- 截图原图，作为底层
 * 2. 绘制板图像（imgBoard）- 包含所有已完成的形状，持久化层
 * 3. 当前形状（shapeCur）- 正在编辑的形状，临时层
 * 4. 拖拽器（shapeHover->paintDragger）- 形状编辑控制点，交互层
 *
 * 绘制原理：
 * - imgBoard作为形状的持久化存储，确保形状在工具切换后仍可见
 * - shapeCur用于实时显示正在绘制的形状
 * - 拖拽器提供形状编辑的视觉反馈
 *
 * 性能考虑：
 * - 只有shapeCur和拖拽器需要实时重绘
 * - imgBoard只在形状完成时更新，减少重绘开销
 */
void Canvas::paint(QPainter& p) {
    LOG_DEBUG("Canvas", "=== Canvas::paint 开始 ===");

    // 检查shapes列表状态，用于诊断形状持久化问题
    LOG_DEBUG("Canvas", QString("当前shapes列表大小: %1").arg(shapes.size()));
    int readyShapes = 0;
    int undoShapes = 0;
    for (int i = 0; i < shapes.size(); i++) {
        if (shapes[i]->state == ShapeState::undo) {
            undoShapes++;
        } else {
            readyShapes++;
        }
    }
    LOG_INFO("Canvas", QString("形状状态统计 - 有效形状: %1, 撤销形状: %2").arg(readyShapes).arg(undoShapes));

    const auto win = static_cast<WinBase*>(parent());
    if (!win) {
        LOG_ERROR("Canvas", "父窗口为空，无法绘制");
        return;
    }

    auto rect = win->rect();
    LOG_DEBUG("Canvas", QString("窗口矩形: (%1,%2,%3,%4)")
              .arg(rect.x()).arg(rect.y())
              .arg(rect.width()).arg(rect.height()));
    LOG_DEBUG("Canvas", QString("背景图像大小: %1x%2").arg(imgBg.width()).arg(imgBg.height()));
    LOG_DEBUG("Canvas", QString("绘制板图像大小: %1x%2").arg(imgBoard.width()).arg(imgBoard.height()));

    // 绘制背景图像
    p.drawImage(rect, imgBg);
    LOG_DEBUG("Canvas", "背景图像绘制完成");

    // 绘制绘制板图像
    p.drawImage(rect, imgBoard);
    LOG_DEBUG("Canvas", "绘制板图像绘制完成");

    // 绘制当前形状
    if (shapeCur) {
        LOG_DEBUG("Canvas", QString("准备绘制当前形状，指针地址: 0x%1").arg(reinterpret_cast<quintptr>(shapeCur), 0, 16));

        // 检查shapeCur指针的有效性
        try {
            // 尝试访问shapeCur的基本信息（如果有的话）
            LOG_DEBUG("Canvas", "shapeCur指针似乎有效，开始调用paint方法");

            // 检查QPainter的有效性
            if (!p.isActive()) {
                LOG_ERROR("Canvas", "QPainter未激活，无法绘制形状");
                return;
            }

            LOG_DEBUG("Canvas", "QPainter状态正常，调用shapeCur->paint()");
            shapeCur->paint(&p);
            LOG_DEBUG("Canvas", "shapeCur->paint() 调用成功完成");
        } catch (const std::exception& e) {
            LOG_ERROR("Canvas", QString("绘制当前形状时发生标准异常: %1").arg(e.what()));
        } catch (...) {
            LOG_ERROR("Canvas", "绘制当前形状时发生未知异常");
        }
    } else {
        LOG_DEBUG("Canvas", "没有当前形状需要绘制 (shapeCur为nullptr)");
    }

    // 绘制悬停形状的拖拽器
    if (shapeHover) {
        LOG_DEBUG("Canvas", QString("准备绘制悬停形状拖拽器，指针地址: 0x%1").arg(reinterpret_cast<quintptr>(shapeHover), 0, 16));

        try {
            LOG_DEBUG("Canvas", "shapeHover指针似乎有效，开始调用paintDragger方法");

            // 检查QPainter的有效性
            if (!p.isActive()) {
                LOG_ERROR("Canvas", "QPainter未激活，无法绘制拖拽器");
                return;
            }

            LOG_DEBUG("Canvas", "QPainter状态正常，调用shapeHover->paintDragger()");
            shapeHover->paintDragger(&p);
            LOG_DEBUG("Canvas", "shapeHover->paintDragger() 调用成功完成");
        } catch (const std::exception& e) {
            LOG_ERROR("Canvas", QString("绘制悬停形状拖拽器时发生标准异常: %1").arg(e.what()));
        } catch (...) {
            LOG_ERROR("Canvas", "绘制悬停形状拖拽器时发生未知异常");
        }
    } else {
        LOG_DEBUG("Canvas", "没有悬停形状需要绘制拖拽器 (shapeHover为nullptr)");
    }

    LOG_DEBUG("Canvas", "=== Canvas::paint 完成 ===");
}

/**
 * @brief 根据当前工具状态创建新形状
 *
 * 形状工厂方法，根据窗口状态（win->state）创建对应类型的形状：
 * - State::rect → ShapeRect（矩形）
 * - State::ellipse → ShapeEllipse（椭圆）
 * - State::arrow → ShapeArrow（箭头）
 * - State::number → ShapeNumber（数字标号）
 * - State::line → ShapeLine（直线）
 * - State::text → ShapeText（文本）
 * - State::mosaic → ShapeMosaicRect/ShapeMosaicLine（马赛克）
 * - State::eraser → ShapeEraserRect/ShapeEraserLine（橡皮擦）
 *
 * 创建流程：
 * 1. 检查窗口状态和toolSub有效性
 * 2. 根据状态创建对应的形状对象
 * 3. 将新形状设置为shapeCur（当前编辑形状）
 * 4. 形状构造函数会从toolSub获取工具设置（颜色、线宽等）
 *
 * 依赖关系：
 * - 需要有效的win->state（工具状态）
 * - 需要有效的win->toolSub（工具设置）
 * - 形状对象的parent设为win，由Qt管理生命周期
 *
 * 错误处理：
 * - 如果toolSub为空，无法获取工具设置，创建失败
 * - 如果状态未知，不会创建任何形状
 */
void Canvas::addShape() {
    LOG_DEBUG("Canvas", "=== 开始创建新形状 ===");

    const auto win = static_cast<WinBase*>(parent());
    if (!win) {
        LOG_ERROR("Canvas", "父窗口为空，无法创建形状");
        return;
    }

    LOG_DEBUG("Canvas", QString("当前窗口状态: %1").arg(static_cast<int>(win->state)));

    if (!win->toolSub) {
        LOG_ERROR("Canvas", "toolSub为空，无法获取工具设置");
        return;
    }

    try {
        if (win->state == State::rect) {
            LOG_DEBUG("Canvas", "创建矩形形状");
            shapeCur = new ShapeRect(win);
        } else if (win->state == State::ellipse) {
            LOG_DEBUG("Canvas", "创建椭圆形状");
            shapeCur = new ShapeEllipse(win);
        } else if (win->state == State::arrow) {
            LOG_DEBUG("Canvas", "创建箭头形状");
            shapeCur = new ShapeArrow(win);
        } else if (win->state == State::number) {
            LOG_DEBUG("Canvas", "创建数字形状");
            shapeCur = new ShapeNumber(win);
        } else if (win->state == State::line) {
            LOG_DEBUG("Canvas", "创建线条形状");
            shapeCur = new ShapeLine(win);
        } else if (win->state == State::text) {
            LOG_DEBUG("Canvas", "创建文本形状");
            shapeCur = new ShapeText(win);
            LOG_DEBUG("Canvas", QString("文本形状创建完成，指针: 0x%1")
                      .arg(reinterpret_cast<quintptr>(shapeCur), 0, 16));
        } else if (win->state == State::mosaic) {
            LOG_DEBUG("Canvas", "创建马赛克形状");
            if (win->toolSub->getSelectState("mosaicFill")) {
                LOG_DEBUG("Canvas", "创建矩形马赛克");
                shapeCur = new ShapeMosaicRect(win);
            } else {
                LOG_DEBUG("Canvas", "创建线条马赛克");
                shapeCur = new ShapeMosaicLine(win);
            }
        } else if (win->state == State::eraser) {
            LOG_DEBUG("Canvas", "创建橡皮擦形状");
            if (win->toolSub->getSelectState("eraserFill")) {
                LOG_DEBUG("Canvas", "创建矩形橡皮擦");
                shapeCur = new ShapeEraserRect(win);
            } else {
                LOG_DEBUG("Canvas", "创建线条橡皮擦");
                shapeCur = new ShapeEraserLine(win);
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Canvas", QString("创建形状时发生异常: %1").arg(e.what()));
        shapeCur = nullptr;
    } catch (...) {
        LOG_ERROR("Canvas", "创建形状时发生未知异常");
        shapeCur = nullptr;
    }

    LOG_DEBUG("Canvas", "=== 形状创建完成 ===");
    return;
}

void Canvas::setHoverShape(ShapeBase* shape) {
    if (!timerDragger) {
        timerDragger = new QTimer(this);
        timerDragger->setInterval(800);
        timerDragger->setSingleShot(true);
        connect(timerDragger, &QTimer::timeout, [this]() {
            if (shapeHover && shapeHover->hoverDraggerIndex != -1) {
                timerDragger->start();
            } else {
                shapeHover = nullptr;
                const auto win = static_cast<WinBase*>(parent());
                win->update();
            }
        });
    }
    if (shape != shapeHover) {
        shapeHover = shape;
    }
    const auto win = static_cast<WinBase*>(parent());
    win->update();
    timerDragger->start();
}

void Canvas::removeShapeFromBoard(ShapeBase* shape) {
    imgBoard.fill(Qt::transparent);
    QPainter p(&imgBoard);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    for (auto& s : shapes) {
        if (shape == s || s->state == ShapeState::undo) continue;
        s->paint(&p);
    }
    shapeCur = shape; //这行主要是为了显示Dragger
}

/**
 * @brief 将完成的形状永久绘制到画板上
 * @param shape 要添加到画板的形状对象
 *
 * 这是形状持久化的核心方法，负责：
 * 1. 将形状绘制到imgBoard（画板图像）上，实现持久化存储
 * 2. 将形状添加到shapes列表中，用于管理和后续操作
 * 3. 清理撤销状态的形状，保持列表整洁
 * 4. 更新工具栏按钮状态（启用撤销按钮）
 *
 * 持久化机制：
 * - imgBoard作为"画布"，所有完成的形状都绘制在上面
 * - 即使工具切换或shapeCur变化，imgBoard中的内容仍然可见
 * - 这确保了用户绘制的形状不会因为界面操作而丢失
 *
 * 形状管理：
 * - 检查形状是否已在列表中，避免重复添加
 * - 自动清理undo状态的形状，释放内存
 * - 维护shapes列表的一致性
 *
 * 注意：这个方法是形状可见性的关键，如果这里出问题，
 * 形状会被添加到列表但不会显示在界面上。
 */
void Canvas::paintShapeOnBoard(ShapeBase* shape) {
    LOG_DEBUG("Canvas", "=== paintShapeOnBoard 开始 ===");
    LOG_DEBUG("Canvas", QString("要添加的形状指针: 0x%1")
              .arg(reinterpret_cast<quintptr>(shape), 0, 16));

    auto win = (WinBase*) parent();
    if (!win) {
        LOG_ERROR("Canvas", "父窗口为空");
        return;
    }

    LOG_DEBUG("Canvas", "创建画板画笔并绘制形状");
    QPainter p(&imgBoard);
    p.setRenderHint(QPainter::Antialiasing, true);

    try {
        shape->paint(&p);
        LOG_DEBUG("Canvas", "形状绘制到画板成功");
    } catch (const std::exception& e) {
        LOG_ERROR("Canvas", QString("绘制形状到画板时发生异常: %1").arg(e.what()));
        return;
    } catch (...) {
        LOG_ERROR("Canvas", "绘制形状到画板时发生未知异常");
        return;
    }

    LOG_DEBUG("Canvas", "调用win->update()刷新显示");
    win->update();

    LOG_DEBUG("Canvas", QString("当前shapes列表大小: %1").arg(shapes.size()));
    LOG_DEBUG("Canvas", QString("检查形状是否已在列表中: %1")
              .arg(shapes.contains(shape) ? "是" : "否"));

    if (!shapes.contains(shape)) {
        LOG_DEBUG("Canvas", "形状不在列表中，需要添加");

        int removedCount = 0;
        for (int i = shapes.size() - 1; i >= 0; i--) //undo之后画新元素，undo的元素都得删掉
        {
            if (shapes[i]->state == ShapeState::undo) {
                LOG_DEBUG("Canvas", QString("移除undo状态的形状，索引: %1").arg(i));
                auto s = shapes.takeAt(i);
                s->deleteLater();
                removedCount++;
            }
        }

        if (removedCount > 0) {
            LOG_DEBUG("Canvas", QString("移除了 %1 个undo状态的形状").arg(removedCount));
        }

        LOG_DEBUG("Canvas", "将形状添加到shapes列表");
        shapes.push_back(shape);
        LOG_INFO("Canvas", QString("形状成功添加到画板，当前列表大小: %1").arg(shapes.size()));

        if (win->toolMain) {
            LOG_DEBUG("Canvas", "启用undo按钮");
            win->toolMain->setBtnEnable("undo", true);
        }
    } else {
        LOG_DEBUG("Canvas", "形状已在列表中，无需重复添加");
    }

    LOG_DEBUG("Canvas", "=== paintShapeOnBoard 完成 ===");
}

void Canvas::removeShapeCur() {
    shapes.removeOne(shapeCur);
    shapeCur->deleteLater();
    shapeCur = nullptr;
}

void Canvas::removeShapeHover() {
    if (shapeHover == nullptr) return;
    shapeHover->state = ShapeState::undo;
    auto win = (WinBase*) parent();
    imgBoard.fill(Qt::transparent);
    QPainter p(&imgBoard);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    for (auto& s : shapes) {
        if (s->state == ShapeState::undo) continue;
        s->paint(&p);
    }
    shapeHover = nullptr;
    bool hasReady { false };
    for (auto& s : shapes) {
        if (s->state != ShapeState::undo) {
            hasReady = true;
            break;
        }
    }
    if (!hasReady) {
        win->toolMain->setBtnEnable("undo", false);
    }
    win->toolMain->setBtnEnable("redo", true);
    win->update();
}

void Canvas::copyColor(const int& key) {
    if (key == 3) {
        POINT pos;
        GetCursorPos(&pos);
        std::wstring tarStr = QString("%1,%2").arg(pos.x).arg(pos.y).toStdWString();
        Util::setClipboardText(tarStr);
        return;
    }
    Util::copyColor(key);
    // 移除qApp->exit()调用，颜色复制不应退出应用程序
}

void Canvas::resize(const QSize& size) {
    auto win = (WinBase*) parent();
    auto dpr = win->devicePixelRatio();
    imgBg = imgBg.scaled(size * dpr);
    imgBoard = imgBoard.scaled(size * dpr, Qt::KeepAspectRatio);
    imgCanvas = imgCanvas.scaled(size * dpr, Qt::KeepAspectRatio);
}
