#include <QMessageBox>
#include <QDateTime>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "global.h"
#include "publicfunc.h"
#include <QFileDialog>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
//    setFixedSize(800, 600);
    ui->editAddress->setMaxLength(8);
    ui->editAddress->setText("00000000");
    ui->labelRecvCnt->setText("R:       ");
    ui->labelSendCnt->setText("S:       ");
    this->setWindowTitle("fugu");
    this->setWindowIcon(QIcon(":/images/app.ico"));

    settingsDialog = new SettingsDialog;

    ui->actionQuit->setEnabled(true);
    ui->actionConfigure->setEnabled(true);    
    statusLabel = new QLabel;
    ui->statusBar->addWidget(statusLabel);
    ui->logTextEdit->setReadOnly(true);
    ui->ckLogShow->setChecked(true);

    ui->editDeviceId->setReadOnly(true);
    ui->editNandFlashSize->setReadOnly(true);
    ui->editSdcardSize->setReadOnly(true);
    ui->editSpiFlashSize->setReadOnly(true);
    ui->editSoftwareVersion->setReadOnly(true);

    ui->spiflashChk->setChecked(true);

    connect(ui->actionQuit, &QAction::triggered, this, &MainWindow::close);
    connect(ui->actionConfigure, &QAction::triggered, settingsDialog, &SettingsDialog::show);
    connect(ui->actionClear, &QAction::triggered, this, &MainWindow::clearLog);
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::about);
    connect(ui->buttonWriteSysDateTime, &QPushButton::clicked, this, &MainWindow::writeSystemDataTimeDataToTerm);
    connect(ui->readDeviceInfoPushButton, &QPushButton::clicked, this, &MainWindow::readDeviceInfo);
    connect(ui->btnBrowse, &QPushButton::clicked, this, &MainWindow::browseFile);
    connect(ui->btnSend, &QPushButton::clicked, this, &MainWindow::sendFile);
    connect(ui->btnSaveToFile, &QPushButton::clicked, this, &MainWindow::readSaveAsFile);
    connect(&thread, SIGNAL(showSendCnt(quint32)), this, SLOT(labelSendCnt(quint32)));
    connect(&thread, SIGNAL(showRecvCnt(quint32)), this, SLOT(labelRecvCnt(quint32)));
    connect(&thread, SIGNAL(setProcessValue(quint32)), this, SLOT(setProcessBarValue(quint32)));
    connect(&thread, SIGNAL(setProcessRange(int,int)), this, SLOT(setProcessBarRange(int,int)));
    connect(&thread, SIGNAL(showLog(quint8,QByteArray)), this, SLOT(logAppend(quint8,QByteArray)));
    connect(&thread, SIGNAL(showStatusMsg(QString)), this, SLOT(showStatusMessage(QString)));
    connect(&thread, SIGNAL(showMsgBox(QString,QString)), this, SLOT(warningBox(QString,QString)));
    connect(&thread, SIGNAL(saveErrInfoLog(QString)), this, SLOT(saveErrLog(QString)));
    connect(&thread, SIGNAL(showDevInfo(QString,QString,QString,QString,QString)), this, SLOT(showDeviceInformation(QString,QString,QString,QString,QString)));
}

MainWindow::~MainWindow()
{
    delete settingsDialog;
    delete ui;
}

void MainWindow::labelSendCnt(const quint32 cnt)
{
    ui->labelSendCnt->setText(QString("S: %1").arg(cnt));
}

void MainWindow::labelRecvCnt(const quint32 cnt)
{
    ui->labelRecvCnt->setText(QString("R: %1").arg(cnt));
}

void MainWindow::about()
{
    QMessageBox::about(this, tr("fugu"),
                       tr("fugu桌面配置工具"
                          /*"use the Qt Serial Port module in modern GUI applications "
                          "using Qt, with a menu bar, toolbars, and a status bar."*/));
}

void MainWindow::clearLog()
{
    ui->logTextEdit->setText("");
    G_set::sendCnt = 0;
    G_set::recvCnt = 0;
    ui->labelRecvCnt->setText("R:       ");
    ui->labelSendCnt->setText("S:       ");
}

void MainWindow::browseFile()
{
    QString binFileName;
    QFileInfo fileInfo;
    QDir dir;

    if (!ui->editBinPath->text().isEmpty())
    {
        dir = ui->editBinPath->text();
    }

    binFileName = QFileDialog::getOpenFileName( NULL, QString("加载bin文件"), dir.absolutePath() ,tr("Images (*.bin);; Hex files (*.hex);; All File (*.*)"));

    if (!binFileName.length())
    {
        return ;
    }

    fileInfo = QFileInfo(binFileName);
    binFileName = fileInfo.absoluteFilePath();
    binFileName = QDir::toNativeSeparators(binFileName);
    ui->editBinPath->setText(binFileName);
    ui->logTextEdit->append(tr("file size: %1").arg(fileInfo.size()));
    ui->progressBar->setValue(0);
}

void MainWindow::setProcessBarValue(quint32 value)
{
    ui->progressBar->setValue(value);
}

void MainWindow::setProcessBarRange(int min, int max)
{
    ui->progressBar->setRange(min, max);
}

void MainWindow::readSaveAsFile()
{
    thread.settingPara = settingsDialog->settings();

    if (thread.isRunning())
    {
        ui->btnSaveToFile->setText(tr("读出"));
        thread.stop(true);
    }
    else
    {
        if (ui->spiflashChk->isChecked())
        {
            thread.spiflashIsChk = true;
            thread.bootChk = false;
        }        
        else
        {
            return;
        }

        QString filename = QFileDialog::getSaveFileName(this,
                                                        QString::fromLocal8Bit("文件另存为"),
                                                        "",
                                                        tr("Images (*.bin);; Text files (*.txt);; All File (*.*)"));

        if (!filename.isEmpty())
        {
            QFile file(filename);

            if (!file.open(QIODevice::WriteOnly | QIODevice::Append))
            {
                QMessageBox msgBox;
                msgBox.setText("保存文件失败");
                msgBox.exec();
            }
            else
            {
                thread.stop(false);
                QFileInfo fi(file);
                QString filePath = QDir::toNativeSeparators(fi.absoluteFilePath());
                thread.filePath = filePath;
                file.close();
                logAppend(0x00, filePath.toLatin1());
                //计算地址
                //检查地址格式是否合法
                if (!ui->editAddress->text().isEmpty())
                {
                    QString addressStr = ui->editAddress->text();
                    logAppend(0x00, tr("len = %1").arg(addressStr.length()).toLatin1());
                    //判断长度是否超过8位数
                    if (addressStr.length() > 8)
                    {
                        warningBox("错误", "长度超过8位数");
                        return ;
                    }

                    //判断是否为奇数
                    if (addressStr.length() % 2)
                    {
                        addressStr.prepend('0');
                    }

                    for (quint16 i=0; i<addressStr.length(); i++)
                    {
                        if (!PublicFunc::isHex(addressStr.at(i)))
                        {
                            warningBox(tr("错误"), tr("地址格式非法"));
                            return;
                        }
                    }


                    thread.setAddress = PublicFunc::bytesToInt(PublicFunc::hexStringToByteArray(addressStr));
                    logAppend(0x00, PublicFunc::byteArrayToHexStr(PublicFunc::hexStringToByteArray(addressStr)).toLatin1());
                }
                else
                {
                    thread.setAddress = 0;
                }

                QEventLoop evLoop;
                thread.setHandleStatus(THREAD_STA_FLASH_TO_FILE);
                ui->btnSaveToFile->setText(tr("取消"));
                thread.start(QThread::HighPriority);
                connect(&thread, SIGNAL(finished()), &evLoop, SLOT(quit()));
                evLoop.exec();

                if (thread.isFinished())
                {
                    ui->btnSaveToFile->setText(tr("读出"));
                }
            }
        }
    }
}

void MainWindow::sendFile()
{        
    thread.setAddress = 0;
    thread.settingPara = settingsDialog->settings();

    if (ui->bootChk->isChecked())
    {
        thread.spiflashIsChk = false;
        thread.bootChk = true;
    }
    else if (ui->spiflashChk->isChecked())
    {
        thread.spiflashIsChk = true;
        thread.bootChk = false;
    }

    if (thread.isRunning())
    {
        ui->btnSend->setText(tr("写入"));
        thread.stop(true);
    }
    else
    {

        if (!ui->editBinPath->text().isEmpty())
        {
            thread.stop(false);
            //判断文件是否存在
            QFile mFile(ui->editBinPath->text());
            if (!mFile.exists())
            {
                warningBox(tr("错误"), tr("文件不存在,或路径不对!!!"));
                return ;
            }

            thread.filePath = ui->editBinPath->text().toLatin1();
            //计算文件总大小，根据包大小，设定进度条值范围
            quint32 totalSize = QFileInfo(thread.filePath).size();
            ui->logTextEdit->append(thread.filePath);
            //计算地址
            //检查地址格式是否合法
            if (!ui->editAddress->text().isEmpty())
            {
                QString addressStr = ui->editAddress->text();

                ui->logTextEdit->append(tr("len = %1").arg(addressStr.length()));
                //判断长度是否超过8位数
                if (addressStr.length() > 8)
                {
                    warningBox("错误", "长度超过8位数");
                    return ;
                }

                //判断是否为奇数
                if (addressStr.length() % 2)
                {
                    addressStr.prepend('0');
                }

                for (quint16 i=0; i<addressStr.length(); i++)
                {
                    if (!PublicFunc::isHex(addressStr.at(i)))
                    {
                        warningBox(tr("错误"), tr("地址格式非法"));
                        return;
                    }
                }


                thread.setAddress = PublicFunc::bytesToInt(PublicFunc::hexStringToByteArray(addressStr));
                ui->logTextEdit->append(PublicFunc::byteArrayToHexStr(PublicFunc::hexStringToByteArray(addressStr)));
            }
            else
            {
                thread.setAddress = 0;
            }

            QEventLoop evLoop;            
            thread.setHandleStatus(THREAD_STA_SENDFILE);
            ui->btnSend->setText(tr("取消"));                        
            thread.start(QThread::HighPriority);
            connect(&thread, SIGNAL(finished()), &evLoop, SLOT(quit()));
            evLoop.exec();

            if (thread.isFinished())
            {
                ui->btnSend->setText(tr("写入"));
            }
        }
    }

}

void MainWindow::writeSystemDataTimeDataToTerm()
{
    thread.settingPara = settingsDialog->settings();

    if (!thread.isRunning())
    {
        QEventLoop evLoop;
        thread.stop(false);
        thread.setHandleStatus(THREAD_STA_SYNC_DATETIME);
        thread.start(QThread::HighPriority);
        connect(&thread, SIGNAL(finished()), &evLoop, SLOT(quit()));
        evLoop.exec();
    }
}


void MainWindow::readDeviceInfo()
{
    thread.settingPara = settingsDialog->settings();

    if (!thread.isRunning())
    {
        QEventLoop evLoop;
        thread.stop(false);
        thread.setHandleStatus(THREAD_STA_READ_DEV_INFO);
        thread.start(QThread::HighPriority);
        connect(&thread, SIGNAL(finished()), &evLoop, SLOT(quit()));
        evLoop.exec();
    }
}

void MainWindow::logAppend(quint8 type,const QByteArray msg)
{
    QString str;
    QString buffer;

    if (type == IS_SEND)
    {

        ui->logTextEdit->setTextColor(QColor("dodgerblue"));
        str = QString(">> SEND: %1 byte\r\n").arg(msg.size());
    }
    else if (type == IS_RECV)
    {
        ui->logTextEdit->setTextColor(QColor("red"));
        str = QString("<< RECV: %1 byte\r\n").arg(msg.size());
    }

    if (ui->ckHexShow->isChecked())
    {
        buffer = PublicFunc::byteArrayToHexStr(msg);
    }
    else
    {
        buffer = tr(msg);
    }

    if (ui->ckLogShow->isChecked())
    {
        ui->logTextEdit->append(QString("[%1] %2%3").arg(TIMEMS).arg(str).arg(buffer));        
    }

}

void MainWindow::showStatusMessage(const QString msg)
{
    statusLabel->setText(msg);
}

void MainWindow::warningBox(const QString tital, const QString msg)
{
    QMessageBox::information(this, tital, msg, QMessageBox::Ok, QMessageBox::Ok);
}

void MainWindow::saveErrLog(const QString errMsg)
{
    //保存错误信息

    QFile logFile(QString(tr("%1_%2").arg(QDateTime::currentDateTime().toString("yyyy.MM.dd")).arg("error.log")));

    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append))
    {
        QMessageBox::critical(NULL, "提示", "无法创建日志文件");
        return ;
    }

    QTextStream out(&logFile);
    out<<errMsg<<endl;
    out.flush();
    logFile.close();
}

void MainWindow::showDeviceInformation(const QString ID, const QString nandSize, const QString sdcardSize, const QString spiSize, const QString softwareVersion)
{
    ui->editDeviceId->clear();
    ui->editDeviceId->setText(ID);
    ui->editNandFlashSize->clear();
    ui->editNandFlashSize->setText(nandSize);
    ui->editSdcardSize->clear();
    ui->editSdcardSize->setText(sdcardSize);
    ui->editSpiFlashSize->clear();
    ui->editSpiFlashSize->setText(spiSize);
    ui->editSoftwareVersion->clear();
    ui->editSoftwareVersion->setText(softwareVersion);
}


