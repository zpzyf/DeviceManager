#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QtSerialPort/QSerialPort>

namespace Ui {
class SettingsDialog;
}

class QIntValidator;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    struct Settings {
        QString name;
        qint32 baudRate;
        QString stringBaudRate;
        QSerialPort::DataBits dataBits;
        QString stringDataBits;
        QSerialPort::Parity parity;
        QString stringParity;
        QSerialPort::StopBits stopBits;
        QString stringStopBits;
        QSerialPort::FlowControl flowControl;
        QString stringFlowControl;
    };

    explicit SettingsDialog(QWidget *parent = 0);
    ~SettingsDialog();

    Settings settings() const;                  //返回当前设置
private slots:
    void showPortInfo(int idx);                 //显示串口信息
    void apply();                               //应用
    void checkCustomBaudRatePolicy(int idx);    //检查自定义波特率策略
    void checkCustomDevicePathPolicy(int idx);  //检查自定义(串口)设备路径策略

private:
    void fillPortsParameters(); //填充串口参数
    void fillPortsInfo();       //填充串口信息
    void updateSettings();      //更新设置

private:
    Ui::SettingsDialog *ui;

    Settings currentSettings;   //当前设置
    QIntValidator *intValidator;//QIntValidator类提供了一个确保一个字符串包含一个在一定有效范围内的整数的验证器。
};

#endif // SETTINGSDIALOG_H
