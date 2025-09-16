#include <iostream>

#include "main-widget/mainwindow.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QSettings>
#include <QDebug>

#ifdef Q_OS_WIN
#include <io.h>
#include <fcntl.h>
#include <windows.h>
#endif

int main(int argc, char* argv[]) {


#ifdef Q_OS_WIN
    // 尝试附加到父进程的控制台（如果从命令行启动）
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        // 重定向标准输出到现有控制台
        FILE* pCout;
        freopen_s(&pCout, "CONOUT$", "w", stdout);
        FILE* pCerr;
        freopen_s(&pCerr, "CONOUT$", "w", stderr);
        
        // 同步C++流
        std::ios::sync_with_stdio(true);
        std::cout.clear();
        std::cerr.clear();
        
        // 输出一个换行，让输出从新行开始
        std::cout << std::endl;
    }
#endif

    // 设置Qt消息输出格式，让CLion能更好地显示
    qSetMessagePattern("[%{time hh:mm:ss.zzz}] %{type}: %{message}");
    
    qDebug() << "=== Lele Tools Starting ===";
    qDebug() << "Hello from qDebug!";
    std::cout << "Hello from std::cout" << std::endl;
    
    // 输出到stderr，这样CLion更容易捕获
    std::cerr << "Debug: Application initializing..." << std::endl;

    QApplication a(argc, argv);

    // Qt 6.9中这些属性已经默认启用，不再需要设置
    // QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);  // Qt 6.9中已废弃
    // QApplication::setAttribute(Qt::AA_EnableHighDpiScaling); // Qt 6.9中已废弃

    // 设置全局字体渲染质量
    QFont defaultFont = QApplication::font();
    defaultFont.setStyleStrategy(QFont::PreferAntialias);
    defaultFont.setHintingPreference(QFont::PreferDefaultHinting);
    QApplication::setFont(defaultFont);

    // 加载翻译文件
    QTranslator translator;
    const QSettings settings(QSettings::IniFormat, QSettings::UserScope, "LeleTools", "Settings");
    const QString savedLanguage = settings.value("language", "").toString();

    QString languageToLoad;
    if (!savedLanguage.isEmpty()) {
        // 使用保存的语言设置
        languageToLoad = savedLanguage;
    } else {
        // 使用系统语言作为默认
        const QStringList uiLanguages = QLocale::system().uiLanguages();
        for (const QString& locale : uiLanguages) {
            QString localeCode = QLocale(locale).name();
            if (localeCode.startsWith("zh_CN") || localeCode.startsWith("zh-CN")) {
                languageToLoad = "zh_CN";
                break;
            }
            if (localeCode.startsWith("en")) {
                languageToLoad = "en_US";
                break;
            }
        }
        // 如果没有匹配的语言，默认使用中文
        if (languageToLoad.isEmpty()) {
            languageToLoad = "zh_CN";
        }
    }

    // 尝试加载翻译文件，Qt6的qt_add_translations可能使用不同的资源路径
    bool translationLoaded = false;

    // 调试输出
    qDebug() << "Trying to load language:" << languageToLoad;

    // 尝试Qt6 qt_add_translations的默认路径
    QString qmFile = QString(":/qt/qml/lele-tools/lele-tools_%1.qm").arg(languageToLoad);
    qDebug() << "Trying path 1:" << qmFile;
    if (translator.load(qmFile)) {
        translationLoaded = true;
        qDebug() << "Translation loaded from path 1";
    } else {
        // 尝试标准的i18n路径
        qmFile = QString(":/i18n/lele-tools_%1.qm").arg(languageToLoad);
        qDebug() << "Trying path 2:" << qmFile;
        if (translator.load(qmFile)) {
            translationLoaded = true;
            qDebug() << "Translation loaded from path 2";
        } else {
            qDebug() << "Failed to load translation files";
        }
    }

    translator.load("qtbase_en.qm", ":/translations"); // Qt 自带的 qm


    if (translationLoaded) {
        QApplication::installTranslator(&translator);
        qDebug() << "Translator installed successfully";
    } else {
        qDebug() << "No translator installed, using default language";
    }
    MainWindow w;
    w.show();

    const int result = QApplication::exec();
    
    return result;
}
