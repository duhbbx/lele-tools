#include "main-widget/mainwindow.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QSettings>

int main(int argc, char *argv[])
{
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
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "LeleTools", "Settings");
    QString savedLanguage = settings.value("language", "").toString();
    
    QString languageToLoad;
    if (!savedLanguage.isEmpty()) {
        // 使用保存的语言设置
        languageToLoad = savedLanguage;
    } else {
        // 使用系统语言作为默认
        const QStringList uiLanguages = QLocale::system().uiLanguages();
        for (const QString &locale : uiLanguages) {
            QString localeCode = QLocale(locale).name();
            if (localeCode.startsWith("zh_CN") || localeCode.startsWith("zh-CN")) {
                languageToLoad = "zh_CN";
                break;
            } else if (localeCode.startsWith("en")) {
                languageToLoad = "en_US";
                break;
            }
        }
        // 如果没有匹配的语言，默认使用中文
        if (languageToLoad.isEmpty()) {
            languageToLoad = "zh_CN";
        }
    }
    
    const QString qmFile = QString(":/i18n/lele-tools_%1.qm").arg(languageToLoad);
    if (translator.load(qmFile)) {
        a.installTranslator(&translator);
    }
    MainWindow w;
    w.show();
    return a.exec();
}
