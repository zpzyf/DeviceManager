#-------------------------------------------------
#
# Project created by QtCreator 2017-01-16T15:29:27
#
#-------------------------------------------------

QT       += core gui serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = DeviceManager
TEMPLATE = app

QMAKE_CXXFLAGS += -std=c++11

SOURCES += main.cpp\
        mainwindow.cpp \
    settingsdialog.cpp \
    global.cpp \
    mythread.cpp \
    publicfunc.cpp

HEADERS  += mainwindow.h \
    settingsdialog.h \
    global.h \
    mythread.h \
    publicfunc.h

FORMS    += mainwindow.ui \
    settingsdialog.ui

RESOURCES += \
    images.qrc
