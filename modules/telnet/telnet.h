#ifndef TELNET_H
#define TELNET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QGroupBox>
#include <QCheckBox>
#include <QTimer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QDateTime>
#include <QScrollBar>
#include <QFont>

#include "../../common/dynamicobjectbase.h"

class Telnet : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit Telnet();
    ~Telnet();

public slots:
    void onConnect();
    void onDisconnect();
    void onSendCommand();
    void onSendSelectedCommand();
    void onClearOutput();
    void onSaveLog();
    void onHostChanged();
    void onPortChanged();
    void onCommandHistoryUp();
    void onCommandHistoryDown();

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);
    void onSocketReadyRead();
    void onConnectionTimeout();

private:
    void setupUI();
    void setupConnectionArea();
    void setupCommandArea();
    void setupOutputArea();
    void setupStatusArea();
    void connectToHost();
    void disconnectFromHost();
    void sendCommand(const QString& command);
    void appendOutput(const QString& text, const QString& color = "#000000");
    void updateStatus(const QString& message, bool isError = false);
    void addToHistory(const QString& command);
    void updateConnectionButtons(bool connected);
    QString formatTimestamp();
    
    // UI组件
    QVBoxLayout* mainLayout;
    QSplitter* mainSplitter;
    
    // 连接区域
    QGroupBox* connectionGroup;
    QGridLayout* connectionLayout;
    QLabel* hostLabel;
    QLineEdit* hostEdit;
    QLabel* portLabel;
    QSpinBox* portSpinBox;
    QLabel* timeoutLabel;
    QSpinBox* timeoutSpinBox;
    QCheckBox* autoReconnectCheck;
    QHBoxLayout* connectionButtonLayout;
    QPushButton* connectBtn;
    QPushButton* disconnectBtn;
    
    // 命令区域
    QGroupBox* commandGroup;
    QVBoxLayout* commandLayout;
    QHBoxLayout* commandInputLayout;
    QTextEdit* commandEdit;
    QPushButton* sendBtn;

    QPushButton* sendSelectedBtn;
    QPushButton* clearBtn;
    QPushButton* saveLogBtn;
    
    // 输出区域
    QGroupBox* outputGroup;
    QVBoxLayout* outputLayout;
    QTextEdit* outputTextEdit;
    QCheckBox* autoScrollCheck;
    QCheckBox* timestampCheck;
    QCheckBox* wordWrapCheck;
    
    // 状态区域
    QHBoxLayout* statusLayout;
    QLabel* statusLabel;
    QLabel* connectionStatusLabel;
    QLabel* bytesLabel;
    
    // 网络和状态
    QTcpSocket* tcpSocket;
    QTimer* connectionTimer;
    bool isConnected;
    QString currentHost;
    int currentPort;
    qint64 bytesReceived;
    qint64 bytesSent;
    
    // 命令历史
    QStringList commandHistory;
    int historyIndex;
    QString currentCommand;
};

#endif // TELNET_H