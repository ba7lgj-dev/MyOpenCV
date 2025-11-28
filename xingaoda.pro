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

INCLUDEPATH += /usr/include/opencv4
LIBS += -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_videoio

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
