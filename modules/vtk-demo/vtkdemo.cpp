#include "vtkdemo.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#ifdef WITH_VTK
// ── VTK includes ────────────────────────────────────────────────
#include <QVTKOpenGLNativeWidget.h>

#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkColorTransferFunction.h>
#include <vtkConeSource.h>
#include <vtkCubeAxesActor.h>
#include <vtkDataSetMapper.h>
#include <vtkDoubleArray.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkImageActor.h>
#include <vtkImageData.h>
#include <vtkImageMapToColors.h>
#include <vtkImageMapper3D.h>
#include <vtkLookupTable.h>
#include <vtkNamedColors.h>
#include <vtkParametricFunctionSource.h>
#include <vtkParametricKlein.h>
#include <vtkParametricTorus.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRendererCollection.h>
#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>
#include <vtkUnsignedCharArray.h>
#include <vtkVertexGlyphFilter.h>

#include <cmath>
#include <random>
#endif

REGISTER_DYNAMICOBJECT(VtkDemo);


#ifdef WITH_VTK

struct VtkDemo::Impl {
    QVTKOpenGLNativeWidget* view = nullptr;
    vtkSmartPointer<vtkRenderer> renderer;
    vtkSmartPointer<vtkRenderer> rendererRight;   // 模型对比时的第二视口
};

VtkDemo::VtkDemo() : QWidget(nullptr), DynamicObjectBase()
{
    m_impl = new Impl();
    setupUI();
    showPointCloud();   // 默认开个点云让用户立刻看到内容
}

VtkDemo::~VtkDemo()
{
    delete m_impl;
}

void VtkDemo::setupUI()
{
    setStyleSheet(R"(
        QPushButton {
            border: 1px solid #ced4da; border-radius: 4px;
            padding: 8px 14px; background: #ffffff; color: #343a40;
            text-align: left;
        }
        QPushButton:hover  { background: #f1f3f5; border-color: #adb5bd; }
        QPushButton:pressed{ background: #e9ecef; }
        QPushButton:checked {
            background: #228be6; border-color: #1c7ed6; color: white; font-weight: bold;
        }
        QLabel#titleLbl { font-size: 14pt; font-weight: bold; color: #1864ab; }
        QLabel#noteLbl  { color: #495057; font-size: 9pt; }
    )");

    auto* main = new QHBoxLayout(this);
    main->setContentsMargins(8, 8, 8, 8);
    main->setSpacing(8);

    // 左侧栏 — 5 个 demo 按钮
    auto* leftCol = new QVBoxLayout();
    leftCol->setSpacing(6);
    auto mkBtn = [](const QString& s) {
        auto* b = new QPushButton(s);
        b->setCheckable(true);
        b->setAutoExclusive(true);
        b->setMinimumWidth(140);
        return b;
    };
    m_btnPointCloud = mkBtn(tr("● 点云"));
    m_btnSurface    = mkBtn(tr("◆ 曲面"));
    m_btnHeatmap    = mkBtn(tr("▦ 热力图"));
    m_btnSlice      = mkBtn(tr("✂ 切片"));
    m_btnCompare    = mkBtn(tr("⇆ 模型对比"));
    m_btnPointCloud->setChecked(true);

    leftCol->addWidget(m_btnPointCloud);
    leftCol->addWidget(m_btnSurface);
    leftCol->addWidget(m_btnHeatmap);
    leftCol->addWidget(m_btnSlice);
    leftCol->addWidget(m_btnCompare);
    leftCol->addStretch();

    connect(m_btnPointCloud, &QPushButton::clicked, this, &VtkDemo::showPointCloud);
    connect(m_btnSurface,    &QPushButton::clicked, this, &VtkDemo::showSurface);
    connect(m_btnHeatmap,    &QPushButton::clicked, this, &VtkDemo::showHeatmap);
    connect(m_btnSlice,      &QPushButton::clicked, this, &VtkDemo::showSlice);
    connect(m_btnCompare,    &QPushButton::clicked, this, &VtkDemo::showModelCompare);
    main->addLayout(leftCol);

    // 右侧：标题 + VTK 视图 + 说明
    auto* rightCol = new QVBoxLayout();
    rightCol->setSpacing(4);
    m_titleLabel = new QLabel();
    m_titleLabel->setObjectName("titleLbl");
    rightCol->addWidget(m_titleLabel);

    m_impl->view = new QVTKOpenGLNativeWidget();
    auto renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    m_impl->view->setRenderWindow(renderWindow);
    m_impl->view->setMinimumSize(640, 480);
    rightCol->addWidget(m_impl->view, 1);

    m_noteLabel = new QLabel();
    m_noteLabel->setObjectName("noteLbl");
    m_noteLabel->setWordWrap(true);
    rightCol->addWidget(m_noteLabel);

    main->addLayout(rightCol, 1);
}

void VtkDemo::setTitle(const QString& title, const QString& note)
{
    m_titleLabel->setText(title);
    m_noteLabel->setText(note);
}

// 把当前所有 renderer 清空，重新建一个单视口 renderer
// 用 auto* 隐式得到 Impl 类型（Impl 在头里是 private，但同 cpp 内能从函数模板/lambda 推导出 — 这里改用 lambda 封进 cpp）
namespace {
vtkSmartPointer<vtkRenderer> resetSingleViewport(QVTKOpenGLNativeWidget* view,
                                                  vtkSmartPointer<vtkRenderer>& outRenderer,
                                                  vtkSmartPointer<vtkRenderer>& outRendererRight)
{
    auto rw = view->renderWindow();
    auto rcoll = rw->GetRenderers();
    rcoll->InitTraversal();
    while (auto* r = rcoll->GetNextItem()) rw->RemoveRenderer(r);
    outRenderer = vtkSmartPointer<vtkRenderer>::New();
    outRenderer->SetBackground(0.12, 0.13, 0.16);
    rw->AddRenderer(outRenderer);
    outRendererRight = nullptr;
    return outRenderer;
}
}   // namespace

void VtkDemo::showPointCloud()
{
    auto renderer = resetSingleViewport(m_impl->view, m_impl->renderer, m_impl->rendererRight);
    setTitle(tr("点云 · Point Cloud"),
             tr("5000 个随机点，按位置上色。鼠标左键旋转 / 中键平移 / 滚轮缩放。"));

    auto points = vtkSmartPointer<vtkPoints>::New();
    auto colors = vtkSmartPointer<vtkUnsignedCharArray>::New();
    colors->SetNumberOfComponents(3);
    colors->SetName("Colors");

    std::mt19937 rng(42);
    std::normal_distribution<double> nd(0.0, 0.5);
    for (int i = 0; i < 5000; ++i) {
        double x = nd(rng), y = nd(rng), z = nd(rng);
        points->InsertNextPoint(x, y, z);
        unsigned char c[3] = {
            (unsigned char)qBound(0, int((x + 1.5) * 90), 255),
            (unsigned char)qBound(0, int((y + 1.5) * 90), 255),
            (unsigned char)qBound(0, int((z + 1.5) * 90), 255),
        };
        colors->InsertNextTypedTuple(c);
    }

    auto poly = vtkSmartPointer<vtkPolyData>::New();
    poly->SetPoints(points);
    poly->GetPointData()->SetScalars(colors);

    auto glyph = vtkSmartPointer<vtkVertexGlyphFilter>::New();
    glyph->SetInputData(poly);

    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(glyph->GetOutputPort());

    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetPointSize(3);
    renderer->AddActor(actor);

    renderer->ResetCamera();
    m_impl->view->renderWindow()->Render();
}

void VtkDemo::showSurface()
{
    auto renderer = resetSingleViewport(m_impl->view, m_impl->renderer, m_impl->rendererRight);
    setTitle(tr("参数曲面 · Parametric Surface"),
             tr("克莱因瓶的参数化曲面，演示 vtkParametricFunctionSource。"));

    auto klein = vtkSmartPointer<vtkParametricKlein>::New();
    auto src = vtkSmartPointer<vtkParametricFunctionSource>::New();
    src->SetParametricFunction(klein);
    src->SetUResolution(60);
    src->SetVResolution(60);
    src->Update();

    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(src->GetOutputPort());

    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(0.30, 0.65, 0.95);
    actor->GetProperty()->SetSpecular(0.5);
    actor->GetProperty()->SetSpecularPower(30);
    renderer->AddActor(actor);

    renderer->ResetCamera();
    m_impl->view->renderWindow()->Render();
}

void VtkDemo::showHeatmap()
{
    auto renderer = resetSingleViewport(m_impl->view, m_impl->renderer, m_impl->rendererRight);
    setTitle(tr("热力图 · Heatmap"),
             tr("100×100 网格，高斯分布 + 噪声，蓝 → 绿 → 黄 → 红 色彩映射。"));

    const int W = 100, H = 100;
    auto img = vtkSmartPointer<vtkImageData>::New();
    img->SetDimensions(W, H, 1);
    img->AllocateScalars(VTK_DOUBLE, 1);
    double* p = static_cast<double*>(img->GetScalarPointer());
    std::mt19937 rng(7);
    std::uniform_real_distribution<double> noise(-0.05, 0.05);
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            double dx = (x - W * 0.5) / (W * 0.25);
            double dy = (y - H * 0.5) / (H * 0.25);
            double v = std::exp(-(dx * dx + dy * dy)) + noise(rng);
            p[y * W + x] = v;
        }
    }

    auto ctf = vtkSmartPointer<vtkColorTransferFunction>::New();
    ctf->AddRGBPoint(0.0, 0.05, 0.10, 0.50);
    ctf->AddRGBPoint(0.3, 0.10, 0.65, 0.40);
    ctf->AddRGBPoint(0.6, 0.95, 0.85, 0.10);
    ctf->AddRGBPoint(1.0, 0.90, 0.20, 0.10);

    auto m2c = vtkSmartPointer<vtkImageMapToColors>::New();
    m2c->SetInputData(img);
    m2c->SetLookupTable(ctf);
    m2c->Update();

    auto actor = vtkSmartPointer<vtkImageActor>::New();
    actor->GetMapper()->SetInputConnection(m2c->GetOutputPort());
    renderer->AddActor(actor);

    renderer->ResetCamera();
    // 让视图正对热力图（XY 平面）
    auto* cam = renderer->GetActiveCamera();
    cam->ParallelProjectionOn();
    cam->SetPosition(W * 0.5, H * 0.5, 200);
    cam->SetFocalPoint(W * 0.5, H * 0.5, 0);
    cam->SetViewUp(0, 1, 0);
    renderer->ResetCameraClippingRange();
    m_impl->view->renderWindow()->Render();
}

void VtkDemo::showSlice()
{
    auto renderer = resetSingleViewport(m_impl->view, m_impl->renderer, m_impl->rendererRight);
    setTitle(tr("体数据切片 · Volume Slice"),
             tr("50³ 合成体数据 sin(5x)·cos(5y)·sin(5z)，取中央 XY 切片显示。"));

    const int N = 50;
    auto img = vtkSmartPointer<vtkImageData>::New();
    img->SetDimensions(N, N, N);
    img->AllocateScalars(VTK_DOUBLE, 1);
    double* p = static_cast<double*>(img->GetScalarPointer());
    for (int z = 0; z < N; ++z) {
        for (int y = 0; y < N; ++y) {
            for (int x = 0; x < N; ++x) {
                double dx = (x - N * 0.5) / (N * 0.5);
                double dy = (y - N * 0.5) / (N * 0.5);
                double dz = (z - N * 0.5) / (N * 0.5);
                p[z * N * N + y * N + x] =
                    std::sin(dx * 5.0) * std::cos(dy * 5.0) * std::sin(dz * 5.0);
            }
        }
    }

    // 简单：直接抽中间 Z 切片显示（一个 ImageActor 设置切片索引）
    auto ctf = vtkSmartPointer<vtkColorTransferFunction>::New();
    ctf->AddRGBPoint(-1.0, 0.10, 0.30, 0.80);
    ctf->AddRGBPoint( 0.0, 0.95, 0.95, 0.95);
    ctf->AddRGBPoint( 1.0, 0.85, 0.15, 0.15);

    auto m2c = vtkSmartPointer<vtkImageMapToColors>::New();
    m2c->SetInputData(img);
    m2c->SetLookupTable(ctf);
    m2c->Update();

    auto actor = vtkSmartPointer<vtkImageActor>::New();
    actor->GetMapper()->SetInputConnection(m2c->GetOutputPort());
    actor->SetDisplayExtent(0, N - 1, 0, N - 1, N / 2, N / 2);
    renderer->AddActor(actor);

    renderer->ResetCamera();
    m_impl->view->renderWindow()->Render();
}

void VtkDemo::showModelCompare()
{
    // 左右两个视口对比，左：高分辨率球，右：低分辨率球
    auto rw = m_impl->view->renderWindow();
    auto rcoll = rw->GetRenderers();
    rcoll->InitTraversal();
    while (auto* r = rcoll->GetNextItem()) rw->RemoveRenderer(r);

    auto rL = vtkSmartPointer<vtkRenderer>::New();
    auto rR = vtkSmartPointer<vtkRenderer>::New();
    rL->SetViewport(0.0, 0.0, 0.5, 1.0);
    rR->SetViewport(0.5, 0.0, 1.0, 1.0);
    rL->SetBackground(0.10, 0.14, 0.18);
    rR->SetBackground(0.18, 0.14, 0.10);
    rw->AddRenderer(rL);
    rw->AddRenderer(rR);
    m_impl->renderer = rL;
    m_impl->rendererRight = rR;

    setTitle(tr("模型对比 · Side-by-Side"),
             tr("左视口高分辨率球（60×60），右视口低分辨率球（10×10），共享交互。"));

    auto sphHi = vtkSmartPointer<vtkSphereSource>::New();
    sphHi->SetThetaResolution(60); sphHi->SetPhiResolution(60);
    auto sphLo = vtkSmartPointer<vtkSphereSource>::New();
    sphLo->SetThetaResolution(10); sphLo->SetPhiResolution(10);

    auto mL = vtkSmartPointer<vtkPolyDataMapper>::New();
    mL->SetInputConnection(sphHi->GetOutputPort());
    auto aL = vtkSmartPointer<vtkActor>::New();
    aL->SetMapper(mL);
    aL->GetProperty()->SetColor(0.30, 0.65, 0.95);
    rL->AddActor(aL);

    auto mR = vtkSmartPointer<vtkPolyDataMapper>::New();
    mR->SetInputConnection(sphLo->GetOutputPort());
    auto aR = vtkSmartPointer<vtkActor>::New();
    aR->SetMapper(mR);
    aR->GetProperty()->SetColor(0.95, 0.55, 0.20);
    aR->GetProperty()->EdgeVisibilityOn();
    rR->AddActor(aR);

    rL->ResetCamera();
    rR->ResetCamera();
    // 两视口共享同一个相机方位（不共享 camera 对象，但同步 view up）
    rw->Render();
}

#else   // !WITH_VTK ─────────────────────────────────────────────

struct VtkDemo::Impl {};

VtkDemo::VtkDemo() : QWidget(nullptr), DynamicObjectBase()
{
    auto* lay = new QVBoxLayout(this);
    lay->setAlignment(Qt::AlignCenter);
    auto* lbl = new QLabel(tr("⚠️ VTK 未编译\n\n"
                              "需要先安装 VTK 9.x（macOS: brew install vtk）\n"
                              "然后重新构建：cmake -DWITH_VTK=ON ..."));
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet("color:#c92a2a; font-size:11pt;");
    lay->addWidget(lbl);
}
VtkDemo::~VtkDemo() = default;
void VtkDemo::setupUI()           {}
void VtkDemo::setTitle(const QString&, const QString&) {}
void VtkDemo::showPointCloud()    {}
void VtkDemo::showSurface()       {}
void VtkDemo::showHeatmap()       {}
void VtkDemo::showSlice()         {}
void VtkDemo::showModelCompare()  {}

#endif // WITH_VTK
