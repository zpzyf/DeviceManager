#include "publicfunc.h"


PublicFunc::PublicFunc(QObject *parent) : QObject(parent)
{

}



quint8 PublicFunc::XORcheck(const QByteArray &msg)
{
    quint16 i;
    quint8 ret = 0;

    for (i=0; i<msg.size(); i++)
    {
        ret ^= msg.at(i);
    }

    return ret;
}

void PublicFunc::encodeFuguProtocolPacket(const QByteArray &infoUnit, quint8 cmd, QByteArray &sbuf)
{
    quint16 len;
    quint8 buf[0x200] = {0};
    quint16 tlen = 0;
    quint16 xorSize;

    len = infoUnit.size();
    buf[tlen++] = '@';
    buf[tlen++] = '@';
    buf[tlen++] = (quint8)(len >>8);
    buf[tlen++] = (quint8)len;
    buf[tlen++] = cmd;

    if (!infoUnit.isEmpty())
    {
        memcpy(&buf[tlen], infoUnit.data(), infoUnit.size());
    }

    tlen += infoUnit.size();
    xorSize = tlen - 2;
    QByteArray xorArray(QByteArray((const char *)&buf[2], xorSize));
    buf[tlen++] = XORcheck(xorArray);
    buf[tlen++] = '#';
    buf[tlen++] = '#';

    sbuf.resize(tlen);
    sbuf = QByteArray((const char *)buf, tlen);
}

bool PublicFunc::decodeFuguProtocolPacket(const QByteArray &rbuf, quint8 &tCmd, QByteArray &tbuf)
{
    quint8 buf[0x200] = {0};
    quint16 len = 0;
    quint16 tlen;

    len = rbuf.size();
    memcpy(buf, rbuf.data(), len);

    if (buf[0] == '@' && buf[1] == '@' && buf[len - 2] == '#' && buf[len - 1] == '#')
    {
        tlen = buf[2];
        tlen = (tlen << 8) | buf[3];
        tCmd = buf[4];

        if (XORcheck(QByteArray((const char *)&buf[2], tlen + 3)) == buf[len - 3])
        {
            if (tlen > 0)
            {
                tbuf.resize(tlen);
                memcpy(tbuf.data(), &buf[5], tlen);
            }

            return true;
        }
    }

    return false;
}

//Uint --> buf
void PublicFunc::tranDt_U16ToBuf(quint8 *buf,quint16 dt)
{
    *buf++ = (dt >>  8) & 0xff;
    *buf++ = dt & 0xff;
}


//Ulong --> buf
void PublicFunc::tranDt_U32ToBuf(quint8 *buf,quint32 dt)
{
    *buf++ = (dt >> 24) & 0xff;
    *buf++ = (dt >> 16) & 0xff;
    *buf++ = (dt >>  8) & 0xff;
    *buf++ = (dt      ) & 0xff;
}
