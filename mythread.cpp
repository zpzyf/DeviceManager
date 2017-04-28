#include "mythread.h"
#include "global.h"
#include "publicfunc.h"
#include "settingsdialog.h"
#include <QMessageBox>
#include <QMutexLocker>
#include <QTime>

MyThread::MyThread()
{
    isStopped = true;    
    spiflashIsChk = false;
    nandflashIsChk = false;
    sdcardIsChk = false;
    status = THREAD_STA_NONE;
}

MyThread::~MyThread()
{

}

void MyThread::handleError(QSerialPort::SerialPortError error)
{
    error = error;
//    qDebug("error = %d\r\n", error);
}


quint16 MyThread::serialWriteRead(QSerialPort &serial, const QByteArray &send, QByteArray &recv, quint16 sendTimeout, quint16 recvTimeout)
{
    quint16 ret = 0;

    if (serial.isOpen())
    {

        G_set::sendCnt += send.size();
        serial.write(send);
        emit showSendCnt(G_set::sendCnt);
        emit showLog(IS_SEND, send);

        if (serial.waitForBytesWritten(sendTimeout))
        {
            if (serial.waitForReadyRead(recvTimeout))
            {
                recv = serial.readAll();

                while (serial.waitForReadyRead(recvTimeout))
                {
                    recv += serial.readAll();
                    qApp->processEvents();
                }

                ret = recv.size();
                G_set::recvCnt += recv.size();
                emit showRecvCnt(G_set::recvCnt);
                emit showLog(IS_RECV, recv);
                return ret;
            }
        }

    }

    return ret;
}

bool MyThread::syncDataToTerminal(QSerialPort &serial,const QByteArray &sendBuf, const quint8 sCmd, QByteArray &recvBuf,const quint8 toCompareCmd)
{
    QByteArray sbuf,rbuf, tbuf;
    quint8 rCmd;

    if (isStopped == true)
    {
        emit saveErrInfoLog(tr("%1 %2(%3): thread was stopped").arg(CURRENT_SYSTEM_DATETIME).arg(__func__).arg(__LINE__));
        return false;
    }
    sbuf.resize(0x200);
    rbuf.resize(0x200);
    tbuf.resize(0x200);

    PublicFunc::encodeFuguProtocolPacket(sendBuf, sCmd, sbuf);

    if (serialWriteRead(serial, sbuf, rbuf, 80, 80))
    {
        //解包,是否为ACK
        if (PublicFunc::decodeFuguProtocolPacket(rbuf, rCmd, tbuf))
        {
            if (rCmd == toCompareCmd)
            {
                recvBuf = tbuf;
            }
            else
            {
                emit saveErrInfoLog(tr("%1 %2(%3): rCmd != toCompareCmd").arg(CURRENT_SYSTEM_DATETIME).arg(__func__).arg(__LINE__));
                return false;
            }
        }
        else
        {
            emit saveErrInfoLog(tr("%1 %2(%3): is No Ack").arg(CURRENT_SYSTEM_DATETIME).arg(__func__).arg(__LINE__));
            return false;
        }

    }
    else
    {
        emit saveErrInfoLog(tr("%1 %2(%3): serialWriteRead error").arg(CURRENT_SYSTEM_DATETIME).arg(__func__).arg(__LINE__));
        return false;
    }

    return true;
}

bool MyThread::handleSendFile(QSerialPort &serial)
{
    QByteArray sbuf,rbuf, tbuf;
    qint32 i = 0;

    sbuf.resize(0x200);
    rbuf.resize(0x200);
    tbuf.resize(0x200);

    msleep(50);
    //发送保存位置
    if (spiflashIsChk)
    {
        if (!syncDataToTerminal(serial, tr("SPI").toLatin1(), POSI, tbuf, POSI))
        {
            emit saveErrInfoLog(tr("%1 %2(%3): syncDataToTerminal error").arg(CURRENT_SYSTEM_DATETIME).arg(__func__).arg(__LINE__));
            return false;
        }
    }
    else if (nandflashIsChk)
    {
        if (!syncDataToTerminal(serial, tr("NAND").toLatin1(), POSI, tbuf, POSI))
        {
            emit saveErrInfoLog(tr("%1 %2(%3): syncDataToTerminal error").arg(CURRENT_SYSTEM_DATETIME).arg(__func__).arg(__LINE__));
            return false;
        }
    }
    else if (sdcardIsChk)
    {
        if (!syncDataToTerminal(serial, tr("SDCARD_FAT").toLatin1(), POSI, tbuf, POSI))
        {
            emit saveErrInfoLog(tr("%1 %2(%3): syncDataToTerminal error").arg(CURRENT_SYSTEM_DATETIME).arg(__func__).arg(__LINE__));
            return false;
        }
    }

    msleep(50);
    //发送文件
    if (!filePath.isEmpty())
    {
        quint32 totalSize = QFileInfo(filePath).size();
        //检查文件是否有内容
        if (!filePath.isEmpty() && totalSize > 0)
        {
            //打开文件
            QFile file(filePath);

            if (file.open(QFile::ReadOnly | QFile::Text))
            {
                QByteArray fData;
                fData.resize(0x200);
                quint32 cumulativeCnt; //累计要发送的数目
                emit setProcessValue(0);
                cumulativeCnt = 0;
                file.seek(0);
                i = 0;

                while (!file.atEnd())
                {
                    if (isStopped)
                    {
                        break;
                    }

                    if (i != 0)
                    {
                        sendAddr += 256;
                    }

                    if (cumulativeCnt + totalSize%256 == totalSize)
                    {
                        fData = file.read(totalSize%256);
                        fData.prepend(PublicFunc::intToByte(sendAddr));

                        if (!syncDataToTerminal(serial, fData, SDAT, tbuf, SDAT))
                        {
                            emit saveErrInfoLog(tr("%1 %2(%3): syncDataToTerminal error").arg(CURRENT_SYSTEM_DATETIME).arg(__func__).arg(__LINE__));
                            return false;
                        }
                        cumulativeCnt += totalSize%256;
                    }
                    else
                    {
                        fData = file.read(256);
                        fData.prepend(PublicFunc::intToByte(sendAddr));

                        if (!syncDataToTerminal(serial, fData, SDAT, tbuf, SDAT))
                        {
                            emit saveErrInfoLog(tr("%1 %2(%3): syncDataToTerminal error").arg(CURRENT_SYSTEM_DATETIME).arg(__func__).arg(__LINE__));
                            return false;
                        }
                        cumulativeCnt += 256;
                    }

                    file.seek(cumulativeCnt);
                    emit setProcessValue(cumulativeCnt);
                    i++;
                    msleep(50);
                }
            }
        }
    }

    return true;
}

bool MyThread::writePcDateTimeToDev(QSerialPort &serial)
{
    QByteArray tbuf;

    if (!syncDataToTerminal(serial, QString(QDateTime::currentDateTime().toString("yyyy.MM.dd.HH.mm.ss")).toLatin1(),
                            DEV_SYNC_DATE_TIME, tbuf, ACK))
    {
        emit saveErrInfoLog(tr("%1 %2(%3): syncDataToTerminal error").arg(CURRENT_SYSTEM_DATETIME).arg(__func__).arg(__LINE__));
        return false;
    }

//    emit showMsgBox(tr("当前系统时间"), QString(QDateTime::currentDateTime().toString("yyyy.MM.dd.HH.mm.ss")));
    return true;
}

bool MyThread::readInfoFromDev(QSerialPort &serial)
{
    QByteArray tbuf;
    tbuf.resize(0x100);

    if (!syncDataToTerminal(serial, NULL, DEV_READ_INFO, tbuf, DEV_READ_INFO))
    {
        emit saveErrInfoLog(tr("%1 %2(%3): syncDataToTerminal error").arg(CURRENT_SYSTEM_DATETIME).arg(__func__).arg(__LINE__));
        return false;
    }
    //解析tbuf，

    return true;
}

void MyThread::setHandleStatus(quint8 sta)
{
    status = sta;
}
void MyThread::run()
{
    QSerialPort serial;
    QByteArray tbuf;
    tbuf.resize(0x200);

    QMutexLocker locker(&qm);

    if (!isStopped)
    {

        //打开串口
        serial.setPortName(settingPara.name);
        serial.setBaudRate(settingPara.baudRate);
        serial.setDataBits(settingPara.dataBits);
        serial.setParity(settingPara.parity);
        serial.setStopBits(settingPara.stopBits);
        serial.setFlowControl(settingPara.flowControl);

        if (serial.open(QIODevice::ReadWrite))
        {
            emit showStatusMsg(tr("Connected to %1 : %2, %3, %4, %5, %6")
                               .arg(settingPara.name)
                               .arg(settingPara.stringBaudRate)
                               .arg(settingPara.stringDataBits)
                               .arg(settingPara.stringParity)
                               .arg(settingPara.stringStopBits)
                               .arg(settingPara.stringFlowControl));
        }
        else
        {
            emit showStatusMsg(tr("Open %1 error").arg(settingPara.name));
            return;
        }
        //发送同步消息
        if (!syncDataToTerminal(serial, NULL, SOH, tbuf, ACK))
        {
            serial.close();
            emit showStatusMsg(tr("空闲"));
            return;
        }

        switch (status) {
        case THREAD_STA_SENDFILE:
            if (!handleSendFile(serial))
            {
                serial.close();
                emit showStatusMsg(tr("空闲"));
                return ;
            }
            break;
        case THREAD_STA_READ_DEV_INFO:
            break;
        case THREAD_STA_SYNC_DATETIME:
            if (!writePcDateTimeToDev(serial))
            {
                serial.close();
                emit showStatusMsg(tr("空闲"));
                return ;
            }
            break;
        default:
            break;
        }

        msleep(50);
        //发送结束标志
        if (!syncDataToTerminal(serial, NULL, EOT, tbuf, ACK))
        {

            serial.close();
            emit showStatusMsg(tr("空闲"));
            return;
        }

        serial.close();
        emit showStatusMsg(tr("空闲"));
        isStopped = true;

        switch (status) {
        case THREAD_STA_SENDFILE:
            emit showMsgBox(tr("发送文件状态"), tr("成功!!!"));
            break;
        case THREAD_STA_READ_DEV_INFO:
            emit showMsgBox(tr("读取设备信息"), tr("成功!!!"));
            break;
        case THREAD_STA_SYNC_DATETIME:
            emit showMsgBox(tr("同步系统时间到设备"), tr("成功!!!"));
            break;
        default:
            break;
        }
    }

    qDebug("finish!!! %s", TIMEMS);

    status = THREAD_STA_NONE;
}

void MyThread::stop(bool st)
{
    isStopped = st;
}
