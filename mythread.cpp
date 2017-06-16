#include "mythread.h"
#include "global.h"
#include "publicfunc.h"
#include "settingsdialog.h"
#include <QMessageBox>
#include <QMutexLocker>
#include <QTime>
#include <QJsonObject>

MyThread::MyThread()
{
    isStopped = true;
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

    msleep(30);
    PublicFunc::encodeFuguProtocolPacket(sendBuf, sCmd, sbuf);

    if (serialWriteRead(serial, sbuf, rbuf, 100, 80))
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

bool MyThread::readFlashToSaveAsFile(QSerialPort &serial)
{
    QByteArray tbuf;
    QByteArray currentFlashSize;

    //发送同步消息
    if (!syncDataToTerminal(serial, NULL, SOH, tbuf, ACK))
    {
        closeSerialToIdle(serial);
        return false;
    }

    if (spiflashIsChk)
    {
        if (!syncDataToTerminal(serial, tr(TO_SPIFLASH).toLatin1(), POSI, tbuf, ACK))
        {
            emit saveErrInfoLog(tr("%1 %2(%3): syncDataToTerminal error").arg(CURRENT_SYSTEM_DATETIME).arg(__func__).arg(__LINE__));
            return false;
        }
        //读取spi flash大小
        if (!syncDataToTerminal(serial, NULL, READ_DEV_SPIFLASH_SIZE, currentFlashSize, SDAT))
        {
            emit saveErrInfoLog(tr("%1 %2(%3): syncDataToTerminal error").arg(CURRENT_SYSTEM_DATETIME).arg(__func__).arg(__LINE__));
            return false;
        }
    }

    //发送读取标志
    if (!syncDataToTerminal(serial, NULL, READ, tbuf, ACK))
    {
        emit saveErrInfoLog(tr("%1 %2(%3): READ error").arg(CURRENT_SYSTEM_DATETIME).arg(__func__).arg(__LINE__));
        return false;
    }

    if (setAddress >= PublicFunc::bytesToInt(currentFlashSize))
    {
        emit showMsgBox("地址错误", "超过flash实际空间大小!!!");
        return false;
    }

    //接收数据  保存到文件
    //打开文件
    QFile file(filePath);

    if (setAddress < PublicFunc::bytesToInt(currentFlashSize) && file.open(QFile::WriteOnly))
    {
        file.resize(0);

        quint8 sbuf[6];
        quint16 readSize = 0;
        int totalSize = PublicFunc::bytesToInt(currentFlashSize);
        int readAddr = setAddress;
        emit setProcessRange(readAddr, totalSize);
        emit setProcessValue(readAddr);

        while (!isStopped)
        {
            if (readAddr + DATAPACK_MAX_SIZE < totalSize)
            {
                readSize = DATAPACK_MAX_SIZE;
            }
            else
            {
                readSize = totalSize - readAddr;
            }

            //填充sbuf=4byte地址+2byte大小
            PublicFunc::tranDt_U32ToBuf(sbuf, readAddr);
            PublicFunc::tranDt_U16ToBuf(&sbuf[4], readSize);

            if (!syncDataToTerminal(serial, QByteArray((const char *)sbuf, 6), RDAT, tbuf, SDAT))
            {
                emit saveErrInfoLog(tr("%1 %2(%3): syncDataToTerminal error").arg(CURRENT_SYSTEM_DATETIME).arg(__func__).arg(__LINE__));
                return false;
            }

            file.write(tbuf);

            readAddr += readSize;
            emit setProcessValue(readAddr);

            if (readAddr >= totalSize)
            {
                break;
            }
        }
    }

    file.close();

    //发送结束标志
    if (!syncDataToTerminal(serial, NULL, EOT, tbuf, ACK))
    {

        closeSerialToIdle(serial);
        return false;
    }

    return true;
}

bool MyThread::writeFileToFlash(QSerialPort &serial)
{
    QByteArray tbuf;
    QByteArray currentFlashSize;
    qint32 i = 0;
    int retry;

    //发送同步消息
    retry = 100;

    while (!isStopped)
    {
        if ((retry) && (--retry == 0))
        {
            break;
        }

        if (syncDataToTerminal(serial, NULL, SOH, tbuf, ACK))
        {
            break;
        }
        msleep(100);
    }

    if (retry == 0)
    {
        emit showMsgBox("同步", "同步超时");
        return false;
    }

    //发送保存位置
    if (bootChk)
    {
        retry = 4;

        while (!isStopped)
        {
            if ((retry) && (--retry == 0))
            {
                break;
            }

            if (syncDataToTerminal(serial, tr(TO_BOOT).toLatin1(), POSI, tbuf, ACK))
            {
                break;
            }
            msleep(500);
        }

        if (retry == 0)
        {
            emit showMsgBox("位置", "通信端口不对");
            return false;
        }

    }
    else if (spiflashIsChk)
    {
        if (!syncDataToTerminal(serial, tr(TO_SPIFLASH).toLatin1(), POSI, tbuf, ACK))
        {
            emit saveErrInfoLog(tr("%1 %2(%3): syncDataToTerminal error").arg(CURRENT_SYSTEM_DATETIME).arg(__func__).arg(__LINE__));
            return false;
        }

        //读取spi flash大小
        if (!syncDataToTerminal(serial, NULL, READ_DEV_SPIFLASH_SIZE, currentFlashSize, SDAT))
        {
            emit saveErrInfoLog(tr("%1 %2(%3): syncDataToTerminal error").arg(CURRENT_SYSTEM_DATETIME).arg(__func__).arg(__LINE__));
            return false;
        }
    }

    //发送写标志
    if (!syncDataToTerminal(serial, NULL, WRITE, tbuf, ACK))
    {
        emit saveErrInfoLog(tr("%1 %2(%3): syncDataToTerminal error").arg(CURRENT_SYSTEM_DATETIME).arg(__func__).arg(__LINE__));
        return false;
    }

    //发送文件
    //检查文件是否有内容
    if (!filePath.isEmpty())
    {
        QFile file(filePath);
        quint32 totalSize = QFileInfo(filePath).size();
        //打开文件
        if (totalSize > 0 && file.open(QFile::ReadOnly))
        {
            QByteArray fData;
            int addr = 0;
            quint32 cumulativeCnt = 0; //累计要发送的数目

            if (bootChk)
            {
                cumulativeCnt = 0x1000;
                file.seek(0x1000);
            }
            else if (spiflashIsChk)
            {
                addr = setAddress;

                if (addr+0x100 >= PublicFunc::bytesToInt(currentFlashSize))
                {
                    emit showMsgBox("地址错误", "超过flash实际空间大小!!!");
                    return false;
                }
                cumulativeCnt = 0;
                file.seek(0);
            }

            emit setProcessRange(cumulativeCnt, totalSize);
            emit setProcessValue(cumulativeCnt);

            i = 0;

            while (!file.atEnd())
            {
                if (isStopped)
                {
                    break;
                }

                if (i != 0)
                {
                    addr += DATAPACK_MAX_SIZE;
                }

                if (cumulativeCnt + totalSize%DATAPACK_MAX_SIZE == totalSize)
                {
                    fData = file.read(totalSize%DATAPACK_MAX_SIZE);
                    fData.prepend(PublicFunc::intToByte(addr));

                    if (!syncDataToTerminal(serial, fData, SDAT, tbuf, ACK))
                    {
                        emit saveErrInfoLog(tr("%1 %2(%3): syncDataToTerminal error").arg(CURRENT_SYSTEM_DATETIME).arg(__func__).arg(__LINE__));
                        return false;
                    }
                    cumulativeCnt += totalSize%DATAPACK_MAX_SIZE;
                }
                else
                {
                    fData = file.read(DATAPACK_MAX_SIZE);
                    fData.prepend(PublicFunc::intToByte(addr));

                    if (!syncDataToTerminal(serial, fData, SDAT, tbuf, ACK))
                    {
                        emit saveErrInfoLog(tr("%1 %2(%3): syncDataToTerminal error").arg(CURRENT_SYSTEM_DATETIME).arg(__func__).arg(__LINE__));
                        return false;
                    }
                    cumulativeCnt += DATAPACK_MAX_SIZE;
                }

                file.seek(cumulativeCnt);
                emit setProcessValue(cumulativeCnt);
                i++;
            }
        }

        file.close();
    }

    //发送结束标志
    if (!syncDataToTerminal(serial, NULL, EOT, tbuf, ACK))
    {

        closeSerialToIdle(serial);
        return false;
    }

    return true;
}

bool MyThread::writePcDateTimeToDev(QSerialPort &serial)
{
    QByteArray tbuf;

    if (!syncDataToTerminal(serial, QString(QDateTime::currentDateTime().toString("yyyy.MM.dd.HH.mm.ss")).toLatin1(),
                            SYNC_DATE_TIME_TO_DEV, tbuf, ACK))
    {
        emit saveErrInfoLog(tr("%1 %2(%3): syncDataToTerminal error").arg(CURRENT_SYSTEM_DATETIME).arg(__func__).arg(__LINE__));
        return false;
    }

    return true;
}

bool MyThread::readInfoFromDev(QSerialPort &serial)
{
    QByteArray devId,
            nandFlashSize,
            sdcardSize,
            spiFlashSize,
            softwareVersion;

    devId.clear();
    nandFlashSize.clear();
    sdcardSize.clear();
    spiFlashSize.clear();
    softwareVersion.clear();

    if (!syncDataToTerminal(serial, NULL, READ_DEV_ID, devId, SDAT))
    {
        emit saveErrInfoLog(tr("%1 %2(%3): syncDataToTerminal error").arg(CURRENT_SYSTEM_DATETIME).arg(__func__).arg(__LINE__));
        return false;
    }

    if (!syncDataToTerminal(serial, NULL, READ_DEV_NANDFLASH_SIZE, nandFlashSize, SDAT))
    {
        emit saveErrInfoLog(tr("%1 %2(%3): syncDataToTerminal error").arg(CURRENT_SYSTEM_DATETIME).arg(__func__).arg(__LINE__));
        return false;
    }

    if (!syncDataToTerminal(serial, NULL, READ_DEV_SDCARD_SIZE, sdcardSize, SDAT))
    {
        emit saveErrInfoLog(tr("%1 %2(%3): syncDataToTerminal error").arg(CURRENT_SYSTEM_DATETIME).arg(__func__).arg(__LINE__));
        return false;
    }

    if (!syncDataToTerminal(serial, NULL, READ_DEV_SPIFLASH_SIZE, spiFlashSize, SDAT))
    {
        emit saveErrInfoLog(tr("%1 %2(%3): syncDataToTerminal error").arg(CURRENT_SYSTEM_DATETIME).arg(__func__).arg(__LINE__));
        return false;
    }

    if (!syncDataToTerminal(serial, NULL, READ_DEV_VERSION_INFO, softwareVersion, SDAT))
    {
        emit saveErrInfoLog(tr("%1 %2(%3): syncDataToTerminal error").arg(CURRENT_SYSTEM_DATETIME).arg(__func__).arg(__LINE__));
        return false;
    }

    emit showDevInfo(tr(devId),
                     tr("%1MB").arg(PublicFunc::bytesToInt(nandFlashSize)/1024/1024),
                     tr(sdcardSize),
                     tr("%1MB").arg(PublicFunc::bytesToInt(spiFlashSize)/1024/1024),
                     tr(softwareVersion));
    return true;
}

void MyThread::closeSerialToIdle(QSerialPort &serial)
{
    serial.close();
    emit showStatusMsg(tr("空闲"));
}

void MyThread::setHandleStatus(quint8 sta)
{
    status = sta;
}
void MyThread::run()
{
    QSerialPort serial;
    QTime time;


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

        time.start();

        switch (status) {
        case THREAD_STA_SENDFILE:
            if (!writeFileToFlash(serial))
            {
                closeSerialToIdle(serial);
                return ;
            }
            break;
        case THREAD_STA_READ_DEV_INFO:
            if (!readInfoFromDev(serial))
            {
                closeSerialToIdle(serial);
                return ;
            }
            break;
        case THREAD_STA_SYNC_DATETIME:
            if (!writePcDateTimeToDev(serial))
            {
                closeSerialToIdle(serial);
                return ;
            }
            break;
        case THREAD_STA_FLASH_TO_FILE:
            if (!readFlashToSaveAsFile(serial))   //flash save as file
            {
                closeSerialToIdle(serial);
                return ;
            }
            break;
        default:
            break;
        }        

        switch (status) {
        case THREAD_STA_SENDFILE:
            emit showMsgBox(tr("发送文件状态"), tr("成功~使用%1秒").arg(time.elapsed()/1000.0));
            break;
        case THREAD_STA_READ_DEV_INFO:
            emit showMsgBox(tr("读取设备信息"), tr("成功~使用%1秒").arg(time.elapsed()/1000.0));
            break;
        case THREAD_STA_SYNC_DATETIME:
            emit showMsgBox(tr("同步系统时间到设备"), tr("成功~使用%1秒").arg(time.elapsed()/1000.0));
            break;
        case THREAD_STA_FLASH_TO_FILE:
            emit showMsgBox(tr("读取flash位置%1的内容").arg(setAddress), tr("完成~使用%1秒").arg(time.elapsed()/1000.0));
            break;
        default:
            break;
        }
    }

    closeSerialToIdle(serial);
    isStopped = true;
    status = THREAD_STA_NONE;
    qDebug("finish!!! %s", TIMEMS);
}

void MyThread::stop(bool st)
{
    isStopped = st;
}
