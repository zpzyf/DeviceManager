#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "settingsdialog.h"
#include "mythread.h"

#include <QMainWindow>
#include <QtSerialPort/QSerialPort>
#include <QLabel>
#include <QTimer>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void about();               //关于
    void clearLog();           //清除日志
    void browseFile();          //浏览文件
    void sendFile();            //发送文件    
    void writeSystemDataTimeDataToTerm(); //写PC系统当前日期之间到终端
    void readDeviceInfo();      //读设备信息    
    void processBar(quint32 value);        
    void labelSendCnt(const quint32 cnt);
    void labelRecvCnt(const quint32 cnt);
    void logAppend(quint8 type, const QByteArray msg);    //追加日志
    void showStatusMessage(const QString msg);     //显示状态信息
    void warningBox(const QString tital, const QString msg);
    void saveErrLog(const QString errMsg);   //保存错误信息
    void showDeviceInformation(const QString ID, const QString nandSize, const QString sdcardSize, const QString spiSize, const QString softwareVersion);
public:        

private:
    Ui::MainWindow *ui;

    QLabel *statusLabel;    //状态栏标签
    SettingsDialog *settingsDialog;
    MyThread thread;
};

#endif // MAINWINDOW_H
