#ifndef MYTHREAD_H
#define MYTHREAD_H

#include <QThread>
#include <QSerialPort>
#include <QMutex>
#include "settingsdialog.h"

class MyThread : public QThread
{
    Q_OBJECT
public:
    explicit MyThread();
    ~MyThread();

signals:
    void setProcessRange(int minimum, int maximum);
    void setProcessValue(quint32 value);
    void showSendCnt(quint32 cnt);
    void showRecvCnt(quint32 cnt);
    void showLog(quint8 type, const QByteArray msg);    //追加日志
    void showStatusMsg(const QString msg);
    void showMsgBox(const QString tital, const QString msg);
    void saveErrInfoLog(const QString errMsg);
    void showDevInfo(const QString ID, const QString nandSize,
                     const QString sdcardSize, const QString spiSize,
                     const QString softwareVersion);
private slots:
    void handleError(QSerialPort::SerialPortError);     //处理串口错误
protected:
    void run();

public:
    void stop(bool st);
    quint16 serialWriteRead(QSerialPort &serial, const QByteArray &send, QByteArray &recv, quint16 sendTimeout, quint16 recvTimeout);
    bool syncDataToTerminal(QSerialPort &serial, const QByteArray &sendBuf, const quint8 sCmd, QByteArray &recvBuf,const quint8 toCompareCmd);
    bool writeFileToFlash(QSerialPort &serial);
    bool writePcDateTimeToDev(QSerialPort &serial); //写PC当前时间到设备
    bool readInfoFromDev(QSerialPort &serial);
    bool readFlashToSaveAsFile(QSerialPort &serial);
    void closeSerialToIdle(QSerialPort &serial);
    SettingsDialog::Settings settingPara;
    QString filePath;
    volatile bool bootChk;
    volatile bool spiflashIsChk;
    quint32 setAddress;
    void setHandleStatus(quint8 sta);

private:
    volatile bool isStopped;
    volatile quint8 status;
    QMutex qm;
};

#endif // MYTHREAD_H
