#include <QMessageBox>
#include <QFont>
#include <QFontDatabase>
#include <QFileDialog>
#include <QStandardPaths>
#include <ShellScalingApi.h>

#include "Util.h"
#include "Lang.h"
#include "Logger.h"
#include "App.h"

QFont* Util::getIconFont(const int& fontSize)
{
    static QFont font = []() {
        qDebug() << "尝试加载图标字体: :/Res/iconfont.ttf";
        
        // 首先检查资源文件是否存在
        QFile fontFile(":/Res/iconfont.ttf");
        if (!fontFile.exists()) {
            qDebug() << "字体资源文件不存在，尝试其他路径";
            // 尝试从主项目资源加载
            QFile altFontFile(":/common/thirdparty/screen-capture/Res/iconfont.ttf");
            if (altFontFile.exists()) {
                qDebug() << "找到字体文件在主项目资源中";
                int fontId = QFontDatabase::addApplicationFont(":/common/thirdparty/screen-capture/Res/iconfont.ttf");
                QStringList fontFamilies = QFontDatabase::applicationFontFamilies(fontId);
                if (!fontFamilies.isEmpty()) {
                    QString fontName = fontFamilies.at(0);
                    qDebug() << "成功从主项目资源加载图标字体:" << fontName;
                    QFont iconFont(fontName);
                    iconFont.setStyleStrategy(QFont::PreferAntialias);
                    iconFont.setHintingPreference(QFont::PreferNoHinting);
                    return iconFont;
                }
            }
        } else {
            qDebug() << "字体资源文件存在，尝试加载";
            int fontId = QFontDatabase::addApplicationFont(":/Res/iconfont.ttf");
            qDebug() << "字体ID:" << fontId;
            
            if (fontId != -1) {
                QStringList fontFamilies = QFontDatabase::applicationFontFamilies(fontId);
                qDebug() << "可用字体族:" << fontFamilies;
                
                if (!fontFamilies.isEmpty()) {
                    QString fontName = fontFamilies.at(0);
                    qDebug() << "成功加载图标字体:" << fontName;
                    LOG_DEBUG(MODULE_APP, QString("成功加载图标字体: %1").arg(fontName));
                    QFont iconFont(fontName);
                    iconFont.setStyleStrategy(QFont::PreferAntialias);
                    iconFont.setHintingPreference(QFont::PreferNoHinting);
                    return iconFont;
                }
            }
        }
        
        // 如果所有方法都失败，使用默认字体
        qDebug() << "图标字体加载失败，使用默认字体 Arial";
        LOG_WARNING(MODULE_APP, "图标字体加载失败，使用默认字体 Arial");
        QFont iconFont("Arial");
        iconFont.setStyleStrategy(QFont::PreferAntialias);
        iconFont.setHintingPreference(QFont::PreferNoHinting);
        return iconFont;
        }();
    font.setPixelSize(fontSize);
    return &font;
}

QFont* Util::getTextFont(const int& fontSize)
{
    static QFont font = []() {
        QFont font("Microsoft YaHei");
        font.setStyleStrategy(QFont::PreferAntialias);
        font.setHintingPreference(QFont::PreferNoHinting);
        return font;
        }();
    font.setPixelSize(fontSize);
    return &font;
}
/**
 * @brief 截取屏幕指定区域的图像
 * 
 * 此函数使用Windows GDI API来捕获屏幕上指定矩形区域的像素数据，
 * 并将其转换为Qt的QImage格式。这是截图功能的核心实现。
 * 
 * 工作流程：
 * 1. 获取屏幕设备上下文(HDC)
 * 2. 创建兼容的内存设备上下文和位图
 * 3. 使用BitBlt将屏幕内容复制到内存位图
 * 4. 将Windows位图数据转换为QImage格式
 * 5. 清理所有Windows GDI资源
 * 
 * @param x 截图区域左上角的X坐标（屏幕坐标系）
 * @param y 截图区域左上角的Y坐标（屏幕坐标系）
 * @param w 截图区域的宽度（像素）
 * @param h 截图区域的高度（像素）
 * @return QImage 截取的图像，格式为ARGB32_Premultiplied
 */
QImage Util::printScreen(const int& x, const int& y, const int& w, const int& h)
{
    LOG_INFO(MODULE_APP, QString("=== 开始截屏操作 ==="));
    LOG_INFO(MODULE_APP, QString("截屏区域: 位置=(%1,%2), 大小=%3x%4").arg(x).arg(y).arg(w).arg(h));
    
    // 第1步：获取屏幕设备上下文
    // GetDC(NULL) 获取整个屏幕的设备上下文
    HDC hScreen = GetDC(NULL);
    if (!hScreen) {
        LOG_ERROR(MODULE_APP, "获取屏幕设备上下文失败");
        return QImage();
    }
    LOG_DEBUG(MODULE_APP, "成功获取屏幕设备上下文");
    
    // 第2步：创建兼容的内存设备上下文
    // 用于在内存中进行绘图操作，而不是直接在屏幕上绘图
    HDC hDC = CreateCompatibleDC(hScreen);
    if (!hDC) {
        LOG_ERROR(MODULE_APP, "创建兼容设备上下文失败");
        ReleaseDC(NULL, hScreen);
        return QImage();
    }
    LOG_DEBUG(MODULE_APP, "成功创建兼容设备上下文");
    
    // 第3步：创建与屏幕兼容的位图
    // 这个位图将用来存储截取的屏幕内容
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, w, h);
    if (!hBitmap) {
        LOG_ERROR(MODULE_APP, "创建兼容位图失败");
        DeleteDC(hDC);
        ReleaseDC(NULL, hScreen);
        return QImage();
    }
    LOG_DEBUG(MODULE_APP, QString("成功创建 %1x%2 的兼容位图").arg(w).arg(h));
    
    // 第4步：将位图选入设备上下文
    // SelectObject返回之前选中的对象，我们立即删除它以避免内存泄漏
    HGDIOBJ oldBitmap = SelectObject(hDC, hBitmap);
    if (oldBitmap) {
        DeleteObject(oldBitmap);
    }
    LOG_DEBUG(MODULE_APP, "位图已选入设备上下文");
    
    // 第5步：执行位块传输（BitBlt）
    // 将屏幕上指定区域的像素数据复制到内存位图中
    // SRCCOPY表示直接复制源像素，不进行任何混合操作
    BOOL bRet = BitBlt(hDC, 0, 0, w, h, hScreen, x, y, SRCCOPY);
    if (!bRet) {
        LOG_ERROR(MODULE_APP, "位块传输(BitBlt)操作失败");
    } else {
        LOG_DEBUG(MODULE_APP, "位块传输操作成功完成");
    }
    
    // 第6步：创建QImage对象
    // Format_ARGB32_Premultiplied是Qt推荐的高性能格式
    auto img = QImage(w, h, QImage::Format_ARGB32_Premultiplied);
    LOG_DEBUG(MODULE_APP, QString("创建QImage对象: %1x%2, 格式=ARGB32_Premultiplied").arg(w).arg(h));
    
    // 第7步：设置位图信息结构
    // 这个结构告诉GetDIBits如何解释位图数据
    // 注意：biHeight为负值表示自顶向下的位图（与Qt的坐标系一致）
    BITMAPINFO bmi = { 
        sizeof(BITMAPINFOHEADER),  // biSize: 结构体大小
        (long)w,                   // biWidth: 位图宽度
        0 - (long)h,              // biHeight: 位图高度（负值=自顶向下）
        1,                        // biPlanes: 颜色平面数（必须为1）
        32,                       // biBitCount: 每像素位数（32位ARGB）
        BI_RGB,                   // biCompression: 压缩类型（无压缩）
        (DWORD)w * 4 * h,        // biSizeImage: 图像数据大小
        0, 0, 0, 0               // 其他字段（未使用）
    };
    LOG_DEBUG(MODULE_APP, "位图信息结构已设置");
    
    // 第8步：获取设备无关位图数据
    // 将Windows位图数据直接复制到QImage的内存缓冲区中
    int scanLines = GetDIBits(hDC, hBitmap, 0, h, img.bits(), &bmi, DIB_RGB_COLORS);
    if (scanLines == 0) {
        LOG_ERROR(MODULE_APP, "获取位图数据失败");
    } else {
        LOG_DEBUG(MODULE_APP, QString("成功获取 %1 行扫描线的位图数据").arg(scanLines));
    }
    
    // 第9步：清理Windows GDI资源
    // 按照创建的逆序释放所有资源，避免内存泄漏
    DeleteDC(hDC);              // 删除内存设备上下文
    DeleteObject(hBitmap);      // 删除位图对象
    ReleaseDC(NULL, hScreen);   // 释放屏幕设备上下文
    LOG_DEBUG(MODULE_APP, "所有GDI资源已清理完成");
    
    // 验证最终图像
    if (img.isNull()) {
        LOG_ERROR(MODULE_APP, "截屏失败：生成的图像为空");
    } else {
        LOG_INFO(MODULE_APP, QString("截屏成功完成: 图像大小=%1x%2, 字节数=%3")
                 .arg(img.width()).arg(img.height()).arg(img.sizeInBytes()));
    }
    
    LOG_INFO(MODULE_APP, QString("=== 截屏操作完成 ==="));
    return img;
}
void Util::imgToClipboard(const QImage& image)
{
    auto [_, compressSize] = App::getCompressVal();
    QImage img;
    if (compressSize != 1.0) {
        img = image.scaled(image.width() * compressSize/100, 
            image.height() * compressSize/100, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    else {
        img = image;
    }
    auto width = img.width();
    auto height = img.height();
    HDC screenDC = GetDC(nullptr);
    HDC memoryDC = CreateCompatibleDC(screenDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(screenDC, width, height);
    DeleteObject(SelectObject(memoryDC, hBitmap));
    BITMAPINFO bmi = { sizeof(BITMAPINFOHEADER), width, 0 - height, 1, 32, BI_RGB, width * 4 * height, 0, 0, 0, 0 };
    SetDIBitsToDevice(memoryDC, 0, 0, width, height, 0, 0, 0, height, img.bits(), &bmi, DIB_RGB_COLORS);
    if (!OpenClipboard(nullptr)) {
        QMessageBox::warning(NULL, "Error", "Failed to open clipboard when save to clipboard.", QMessageBox::StandardButton::Ok);
        return;
    }
    EmptyClipboard();
    SetClipboardData(CF_BITMAP, hBitmap);
    CloseClipboard();
    ReleaseDC(nullptr, screenDC);
    DeleteDC(memoryDC);
    DeleteObject(hBitmap);
}
bool Util::isImagePath(const QString& path)
{
    QFileInfo fileInfo(path);
    QString suffix = fileInfo.suffix().toLower();
    if (fileInfo.isFile() && (suffix == "png" || suffix == "jpg" || suffix == "jpeg" || suffix == "bmp")) {
        return true;
    }
    return false;
}
bool Util::posInScreen(const int& x, const int& y)
{
    QList<QScreen*> screens = QGuiApplication::screens();
    for (QScreen* screen : screens) {
        if (screen->geometry().contains(x, y)) {
            return true;
        }
    }
    return false;
}
QScreen* Util::getScreen(const int& x, const int& y)
{
    QList<QScreen*> screens = QGuiApplication::screens();
    for (QScreen* screen : screens) {
        if (screen->geometry().contains(x, y)) {
            return screen;
        }
    }
    return nullptr;
}
void Util::setClipboardText(const std::wstring& text) {
    OpenClipboard(NULL);
    EmptyClipboard();
    size_t len = (text.size() + 1) * sizeof(wchar_t);
    HANDLE copyHandle = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, len);
    if (copyHandle == NULL)
    {
        MessageBox(NULL, L"Failed to alloc clipboard memory.", L"Error", MB_OK | MB_ICONERROR);
        CloseClipboard();
        return; // 处理错误
    }
    byte* copyData = reinterpret_cast<byte*>(GlobalLock(copyHandle));
    if (copyData) {
        memcpy(copyData, text.data(), len);
    }
    GlobalUnlock(copyHandle);
    SetClipboardData(CF_UNICODETEXT, copyHandle);
    CloseClipboard();
}

void Util::copyColor(const int& key)
{
    std::wstring tarStr;
    HDC hdcScreen = GetDC(NULL);
    POINT pos;
    GetCursorPos(&pos);
    COLORREF colorNative = GetPixel(hdcScreen, pos.x, pos.y);
    ReleaseDC(NULL, hdcScreen);
    if (key == 0) {
        tarStr = QString("%1,%2,%3").arg(GetRValue(colorNative)).arg(GetGValue(colorNative)).arg(GetBValue(colorNative)).toStdWString();
    }
    else if (key == 1) {
        QColor color(GetRValue(colorNative), GetGValue(colorNative), GetBValue(colorNative));
        tarStr = color.name().toUpper().toStdWString();
    }
    else if (key == 2) {
        QColor cmyk(GetRValue(colorNative), GetGValue(colorNative), GetBValue(colorNative));
        tarStr = QString("%1,%2,%3,%4").arg(cmyk.cyan()).arg(cmyk.magenta()).arg(cmyk.yellow()).arg(cmyk.black()).toStdWString();
    }
    Util::setClipboardText(tarStr);
}

bool Util::saveToFile(const QImage& img)
{
    auto savePath = App::getSavePath();
	auto [compressQuality, compressSize] = App::getCompressVal();
    if (savePath.toUpper().endsWith(".PNG")) {
		if (compressSize != 100) {
			QImage image = img.scaled(img.width() * compressSize/100, 
                img.height() * compressSize/100,
                Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            image.save(savePath, "PNG", compressQuality);
            return true;
		}
        img.save(savePath, "PNG", compressQuality);
        return true;
    }
    auto fileName = "Img" + QDateTime::currentDateTime().toString("yyyyMMddhhmmsszzz") + ".png";    
    QString filePath;
	if (savePath.isEmpty()) {
        QFileDialog dialog(nullptr, Lang::get("saveFile"));
        dialog.setAcceptMode(QFileDialog::AcceptSave);
        dialog.setNameFilter("ScreenCapture (*.png)");
        dialog.setDefaultSuffix("png");
        dialog.setAttribute(Qt::WA_QuitOnClose, false);
        dialog.setDirectory(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation));
        dialog.selectFile(fileName);
        if (dialog.exec() == QDialog::Accepted) {
            filePath = dialog.selectedFiles().first();
        }
        else {
            return false;
        }
    }
    else {
		filePath = QDir::cleanPath(savePath + QDir::separator() + fileName);
    }
    if (compressSize != 1.0) {
        QImage image = img.scaled(img.width() * compressSize/100, 
            img.height() * compressSize/100, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        image.save(filePath, "PNG", compressQuality);
        return true;
    }
    img.save(filePath,"PNG", compressQuality);
    return true;
}
