QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    AVPlayer.cpp \
    MainWindow.cpp \
    MediaUse.cpp \
    main.cpp

HEADERS += \
    AVPlayer.h \
    MainWindow.h \
    MediaUse.h

FORMS +=


QT += opengl


#INCLUDEPATH += $$PWD/ffmpeg/include
#LIBS += -L$$PWD/ffmpeg/lib -lavcodec -lavutil -lavformat -lavdevice -lavfilter -lpostproc -lswresample -lswscale



INCLUDEPATH += $$PWD/ffmpeg/include
LIBS += -L$$PWD/ffmpeg/lib -lavcodec -lavutil -lavformat -lavdevice -lavfilter -lpostproc -lswresample -lswscale

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resource.qrc





unix|win32: LIBS += -lopenal

#unix|win32: LIBS += -lprofiler
