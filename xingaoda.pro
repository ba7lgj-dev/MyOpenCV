QT       += core gui widgets serialport charts network

CONFIG += c++11

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    main.cpp \
    xingaodaapp.cpp \
    camera.cpp \
    widthestimator.cpp \
    configmanager.cpp \
    calibrationmanager.cpp \
    logmanager.cpp \
    pumpcontroller.cpp \
    applicationcore.cpp \
    pushmanager.cpp

HEADERS += \
    xingaodaapp.h \
    camera.h \
    widthestimator.h \
    configmanager.h \
    calibrationmanager.h \
    logmanager.h \
    pumpcontroller.h \
    applicationcore.h \
    pushmanager.h

FORMS += \
    xingaodaapp.ui

# OpenCV include path
INCLUDEPATH += C:\Software\opencv\opencv-build\install\include

# OpenCV lib path
LIBS += C:\Software\opencv\opencv-build\install\x64\mingw\lib\libopencv_*.a



qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
