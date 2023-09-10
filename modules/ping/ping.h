
#ifndef PING_H
#define PING_H


#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>

#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QLabel>
#include <QRadioButton>
#include <QButtonGroup>
#include <QLineEdit>
#include <QtNetwork>
#include "../../common/dynamicobjectbase.h"

#include <stdio.h>
#include <winsock2.h>

namespace PING_PRIVATE {



// IP数据包头结构
typedef struct iphdr
{
    unsigned int headLen:4;
    unsigned int version:4;
    unsigned char tos;
    unsigned short totalLen;
    unsigned short ident;
    unsigned short fragAndFlags;
    unsigned char ttl;
    unsigned char proto;
    unsigned short checkSum;
    unsigned int sourceIP;
    unsigned int destIP;
}IpHeader;

// ICMP数据头结构
typedef struct ihdr
{
    unsigned char iType;
    unsigned char iCode;
    unsigned short iCheckSum;
    unsigned short iID;
    unsigned short iSeq;
    unsigned long  timeStamp;
}IcmpHeader;

// 计算ICMP包的校验和(发送前要用)
unsigned short checkSum(unsigned short *buffer, int size);

// 填充ICMP请求包的具体参数
void fillIcmpData(char *icmpData, int dataSize);

// 对返回的IP数据包进行解析，定位到ICMP数据
int decodeResponse(char *buf, int bytes, struct sockaddr_in *from, int tid);

// ping操作
int do_ping(const char *ip, unsigned int timeout);

}

class PingWorker : public QObject
{
    Q_OBJECT

public:
    PingWorker(const QString& host) : host_(host) {}

signals:
    void returnCode(int value);

public slots:
    void ping()
    {

        // 将QString转换为QByteArray
        QByteArray byteArray = host_.toUtf8();

        // 使用data()函数获取char*指针
        const char *charArray = byteArray.constData();

        int code = PING_PRIVATE::do_ping(charArray, 3000);


        emit returnCode(code);

    }

private:
    QString host_;
};



class Ping : public QWidget, public DynamicObjectBase {

    Q_OBJECT
public:
    explicit Ping();

private:
    QLineEdit * hostInputEdit;
    QPushButton * startButton;
    QPlainTextEdit * response;
    PingWorker* pingWorker;
    QThread* thread;
    QTimer * timer;
    bool flag;

public slots:
    void startPing();
    void appendResponse(int value);
};

#endif // PING_H
