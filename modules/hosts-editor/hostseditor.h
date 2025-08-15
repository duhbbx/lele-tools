#ifndef HOSTSEDITOR_H
#define HOSTSEDITOR_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QFile>
#include <QTextStream>
#include <QIODevice>
#include <QDir>
#include <QStandardPaths>
#include <QProcess>
#include <QThread>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <windows.h>
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")
#endif

#ifdef Q_OS_LINUX
#include <unistd.h>
#endif

#include "../../common/dynamicobjectbase.h"

class HostsEditor : public QWidget, public DynamicObjectBase {
    Q_OBJECT

public:
    explicit HostsEditor();

private slots:
    void loadHostsFile();
    void saveHostsFile();
    void flushDns();

private:
    // UI 组件
    QPushButton *refreshButton;
    QPushButton *saveButton;
    QPushButton *flushDnsButton;
    QPlainTextEdit *hostsTextEdit;
    QLabel *statusLabel;

    // 私有方法
    QString getHostsFilePath();
    bool isRunningAsAdmin();
    void showMessage(const QString &message, bool isError = false);
};

#endif // HOSTSEDITOR_H
