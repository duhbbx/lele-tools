#include "main-widget/mainwindow.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>

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

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "lele-tools_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }
    MainWindow w;
    w.show();
    return a.exec();
}
