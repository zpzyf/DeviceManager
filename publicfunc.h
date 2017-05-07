#ifndef PUBLICFUNC_H
#define PUBLICFUNC_H

#include <QtCore>

class PublicFunc : public QObject
{
    Q_OBJECT
public:
    explicit PublicFunc(QObject *parent = 0);

    //十六进制编码转成带空格的十六进制字符串............1
    static QString byteArrayToHexStr(QByteArray data)
    {
        QString temp = "";
        QString hex = data.toHex();

        for (int i = 0,j = 0, k = 0; i < hex.length();i += 2,j++,k++)
        {
            if (k>15)
            {
                k = 0;
//                temp += "    ";
            }
            if (j>31)
            {
                j = 0;
//                temp += "\r\n";
            }

            temp += hex.mid(i, 2) + " ";
        }

        return temp.trimmed().toUpper();
    }
    // 将十六进制字串每字节中间加空格分隔
    static void formatString(QString &org, int n = 2, const QChar &ch=QChar(' '))
    {
        int size = org.size();
        int space = qRound(size*1.0/n+0.5) - 1;

        if (space < 0)
        {
            return ;
        }

        for (int i=0, pos = n; i < space; ++i, pos += (n+1))
        {
            org.insert(pos, ch);
        }
    }
    //十六进制字串转化为十六进制编码..............2
    static QByteArray hexStringToByteArray(QString hex)
    {
        QByteArray ret;

        hex = hex.trimmed();
        formatString(hex, 2, ' ');
        QStringList sl = hex.split(" ");

        foreach (QString s, sl) {
            if (!s.isEmpty())
            {
                ret.append((char)s.toInt(0, 16) & 0xFF);
            }
        }

        return ret;
    }
    //判断字符ch是否为16进制字符
    static int isHex(QChar ch)
    {
        if ( ch >='0' && ch <='9' ) //属于0-9集合，返回是
                return 1;
        if ( ch >='A' && ch <='F' ) //属于A-F集合，返回是
            return 1;
        if ( ch >='a' && ch <='f' ) //属于a-f集合，返回是
            return 1;

        return 0; //否则，返回不是
    }
    //int转ByteArray高位牌前面
    static QByteArray  intToByte(int number)
    {
        QByteArray abyte0;
        abyte0.resize(4);
        abyte0[3] = (uchar)  (0x000000ff & number);
        abyte0[2] = (uchar) ((0x0000ff00 & number) >> 8);
        abyte0[1] = (uchar) ((0x00ff0000 & number) >> 16);
        abyte0[0] = (uchar) ((0xff000000 & number) >> 24);
        return abyte0;
    }
    //ByteArray转int
    static int bytesToInt(QByteArray bytes) {
        int addr = bytes[3] & 0x000000FF;
        addr |= ((bytes[2] << 8) & 0x0000FF00);
        addr |= ((bytes[1] << 16) & 0x00FF0000);
        addr |= ((bytes[0] << 24) & 0xFF000000);
        return addr;
    }

    static quint8 XORcheck(const QByteArray &msg);
    static void encodeFuguProtocolPacket(const QByteArray &infoUnit, quint8 cmd, QByteArray &sbuf);
    static bool decodeFuguProtocolPacket(const QByteArray &rbuf, quint8 &tCmd, QByteArray &tbuf);

private:

};

#endif // PUBLICFUNC_H
