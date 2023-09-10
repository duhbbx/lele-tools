#include "ping.h"

#include <QLabel>

#include <QHBoxLayout>

#include <QLineEdit>
#include <QPushButton>
#include <Winsock2.h>
#include <WS2tcpip.h>

REGISTER_DYNAMICOBJECT(Ping);

namespace PING_PRIVATE {

// 计算ICMP包的校验和(发送前要用)
unsigned short checkSum(unsigned short *buffer, int size)
{
    unsigned long ckSum = 0;

    while(size > 1)
    {
        ckSum += *buffer++;
        size -= sizeof(unsigned short);
    }

    if(size)
    {
        ckSum += *(unsigned char*)buffer;
    }

    ckSum = (ckSum >> 16) + (ckSum & 0xffff);
    ckSum += (ckSum >>16);
    return (unsigned short)(~ckSum);
}

// 填充ICMP请求包的具体参数
void fillIcmpData(char *icmpData, int dataSize)
{
    IcmpHeader *icmpHead = (IcmpHeader*)icmpData;
    icmpHead->iType = 8;  // 8表示请求包
    icmpHead->iCode = 0;
    icmpHead->iID = (unsigned short)GetCurrentThreadId();
    icmpHead->iSeq = 0;
    icmpHead->timeStamp = GetTickCount();
    char *datapart = icmpData + sizeof(IcmpHeader);
    memset(datapart, 'x', dataSize - sizeof(IcmpHeader)); // 数据部分为xxx..., 实际上有32个x
    icmpHead->iCheckSum = checkSum((unsigned short*)icmpData, dataSize); // 千万要注意，这个一定要放到最后
}

// 对返回的IP数据包进行解析，定位到ICMP数据
int decodeResponse(char *buf, int bytes, struct sockaddr_in *from, int tid)
{
    IpHeader *ipHead = (IpHeader *)buf;
    unsigned short ipHeadLen = ipHead->headLen * 4 ;
    if (bytes < ipHeadLen + 8) // ICMP数据不完整, 或者不包含ICMP数据
    {
        return -1;
    }

    IcmpHeader *icmpHead = (IcmpHeader*)(buf + ipHeadLen);  // 定位到ICMP包头的起始位置
    if (icmpHead->iType != 0)   // 0表示回应包
    {
        return -2;
    }

    if (icmpHead->iID != (unsigned short)tid) // 理应相等
    {
        return -3;
    }

    int time = GetTickCount() - (icmpHead->timeStamp); // 返回时间与发送时间的差值
    if(time >= 0)
    {
        return time;
    }

    return -4; // 时间错误
}

// ping操作
int do_ping(const char *ip, unsigned int timeout)
{
    // 网络初始化
    WSADATA wsaData;
    WSAStartup(MAKEWORD(1, 1), &wsaData);
    unsigned int sockRaw = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);  // 注意，第三个参数非常重要，指定了是icmp
    setsockopt(sockRaw, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));  // 设置套接字的接收超时选项
    setsockopt(sockRaw, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));  // 设置套接字的发送超时选项

    // 准备要发送的数据
    int  dataSize = sizeof(IcmpHeader) + 32; // 待会儿会有32个x
    char icmpData[1024] = {0};
    fillIcmpData(icmpData, dataSize);
    unsigned long startTime = ((IcmpHeader *)icmpData)->timeStamp;

    // 远程通信端
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));

    struct addrinfo hints, *result = nullptr;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    // 支持IPv4和IPv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // 使用getaddrinfo获取主机信息
    int iResult = getaddrinfo(ip, nullptr, &hints, &result);
    if (iResult != 0) {
        qDebug() << "getaddrinfo failed: " << gai_strerror(iResult);
        WSACleanup();
        return 2;
    }

    int iRet = -1;

    // 打印主机地址信息
    struct addrinfo* ptr = result;
    while (ptr != nullptr) {
        void* addr;
        if (ptr->ai_family == AF_INET) {
            struct sockaddr_in* ipv4 = (struct sockaddr_in*)ptr->ai_addr;
            addr = &(ipv4->sin_addr);


            // 发送数据
            sendto(sockRaw, icmpData, dataSize, 0, (struct sockaddr*)ipv4, sizeof(dest));

            struct sockaddr_in from;
            int fromLen = sizeof(from);
            while(1)
            {
                // 接收数据
                char recvBuf[1024] = {0};
                int iRecv = recvfrom(sockRaw, recvBuf, 1024, 0, (struct sockaddr*)&from, &fromLen);
                int time  = decodeResponse(recvBuf, iRecv, &from, GetCurrentThreadId());
                if(time >= 0)
                {
                    iRet = 0;   // ping ok
                    break;
                }
                else if( GetTickCount() - startTime >= timeout || GetTickCount() < startTime)
                {
                    iRet = -1;  // ping超时
                    break;
                }
            }

            return iRet;



        } else {
            struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)ptr->ai_addr;
            addr = &(ipv6->sin6_addr);
        }

        char ipstr[INET6_ADDRSTRLEN];
        inet_ntop(ptr->ai_family, addr, ipstr, sizeof(ipstr));
        qDebug() << "IP Address: " << ipstr;

        ptr = ptr->ai_next;
    }



    // 释放
    closesocket(sockRaw);
    WSACleanup();


    return iRet;
}

}


Ping::Ping() : QWidget(nullptr), DynamicObjectBase()
{
    QVBoxLayout * layout = new QVBoxLayout;

    QLabel * label = new QLabel;

    QHBoxLayout * hostInputLayout = new QHBoxLayout;
    hostInputLayout->setAlignment(Qt::AlignLeft);
    QLabel * hostInputTip = new QLabel("请输入host");
    hostInputTip->setFixedSize(80, 30);
    QLineEdit * hostInputEdit = new QLineEdit;
    hostInputEdit->setFixedSize(400, 30);
    hostInputLayout->addWidget(hostInputTip);
    hostInputLayout->addWidget(hostInputEdit);

    this->hostInputEdit = hostInputEdit;


    layout->addLayout(hostInputLayout);


    QHBoxLayout * buttonLayout = new QHBoxLayout;
    buttonLayout->setAlignment(Qt::AlignLeft);

    QPushButton * startButton = new QPushButton("开始");
    startButton->setFixedSize(50, 30);
    buttonLayout->addWidget(startButton);


    connect(startButton, SIGNAL(clicked()), this, SLOT(startPing()));

    layout->addLayout(buttonLayout);

    this->startButton = startButton;

    QPlainTextEdit * response = new QPlainTextEdit;

    response->appendPlainText("这里显示响应内容...........");

    layout->addWidget(response);

    this->response = response;

    this->flag = false;
    layout->addWidget(label);
    layout->setAlignment(Qt::AlignTop);
    this->setLayout(layout);
}

void Ping::appendResponse(int value) {

    if (value == 0) {
        this->response->appendPlainText("Ping ok ........................");
    } else if (value == 2) {
        this->response->appendPlainText("找不到主机......................");
    } else {
        this->response->appendPlainText("Ping error ........................");
    }
}

void Ping::startPing() {

    if (flag) {

        this->flag = false;
        this->startButton->setText("开始");

        this->timer->stop();
        delete this->timer;
        this->thread->terminate();
        delete this->thread;
        this->response->clear();

    } else {

        this->flag = true;
        this->response->clear();

        this->startButton->setText("停止");

        QString targetHost = this->hostInputEdit->text(); // 设置目标主机的IP地址
        PingWorker* pingWorker = new PingWorker(targetHost);
        QThread* thread = new QThread;
        pingWorker->moveToThread(thread);

        // 每隔1秒执行一次Ping
        QTimer* timer = new QTimer;
        QObject::connect(timer, &QTimer::timeout, pingWorker, &PingWorker::ping);

        QObject::connect(pingWorker, &PingWorker::returnCode, this, &Ping::appendResponse);
        timer->start(1000);
        thread->start();

        this->pingWorker = pingWorker;
        this->thread = thread;
        this->timer = timer;
    }


}
