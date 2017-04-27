#ifndef GLOBAL_H
#define GLOBAL_H

#include <QtCore>

#define TIMEMS qPrintable (QTime::currentTime().toString("HH:mm:ss zzz"))
#define CURRENT_SYSTEM_DATETIME qPrintable (QDateTime::currentDateTime().toString("yyyy.MM.dd HH:mm:ss"))

#define IS_SEND 0
#define IS_RECV 1

#define THREAD_STA_NONE             0x00
#define THREAD_STA_SENDFILE         0x01
#define THREAD_STA_READ_DEV_INFO    0x02
#define THREAD_STA_SYNC_DATETIME    0x03

class G_set
{
public:
    static quint32 sendCnt; //发送数量
    static quint32 recvCnt; //接收数量
    static bool isCancelSend;    //取消发送
    quint8 XORcheck(const QByteArray &msg);         //异或校验和
    void encodeFuguProtocolPacket(const QByteArray &infoUnit, quint8 cmd, QByteArray &sbuf);    //协议打包
    quint16 decodeFuguProtocolPacket(const QByteArray &rbuf, quint8 &tCmd, QByteArray &tbuf);    //协议解包
};

/*
*协议相关命令
*/
#define SOH     0x01    //开始传输
#define ACK     0x06    //确认
#define NACK    0x15    //出现错误
#define POSI    0x02    //数据保存位置    spi flash、nand flash、SD卡
#define ADDR    0x03    //数据保存的开始地址
#define CAN     0x18    //取消传输
#define EOT     0x04    //传输结束
#define SDAT    0x05    //传输数据
//0x80至0x?为操作设备命令
#define DEV_SYNC_DATE_TIME  0x80    //同步系统日期时间到设备
#define DEV_READ_INFO       0x81    //读设备信息

#define TO_SPIFLASH     "SPI"
#define TO_NANDFLASH    "NAND"
#define TO_SDCARD       "SDCARD"

#endif // GLOBAL_H
