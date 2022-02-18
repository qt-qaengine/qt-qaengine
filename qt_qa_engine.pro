TARGET = qaengine

TEMPLATE = lib
CONFIG += plugin
TARGET = $$qtLibraryTarget($$TARGET)
TARGETPATH = $$[QT_INSTALL_LIBS]
target.path = $$TARGETPATH

INSTALLS = target

QT += qml core network quick quick-private core-private #xmlpatterns
CONFIG += plugin
CONFIG += c++11

SOURCES += \
    src/GenericEnginePlatform.cpp \
    src/IEnginePlatform.cpp \
    src/ITransportClient.cpp \
    src/ITransportServer.cpp \
    src/QAEngine.cpp \
    src/QAEngineSocketClient.cpp \
    src/QAKeyEngine.cpp \
    src/QAMouseEngine.cpp \
    src/QAPendingEvent.cpp \
    src/QuickEnginePlatform.cpp \
    src/TCPSocketClient.cpp \
    src/TCPSocketServer.cpp

HEADERS += \
    src/GenericEnginePlatform.h \
    src/IEnginePlatform.h \
    src/ITransportClient.h \
    src/ITransportServer.h \
    src/QAEngine.h \
    src/QAEngineSocketClient.h \
    src/QAKeyEngine.h \
    src/QAMouseEngine.h \
    src/QAPendingEvent.h \
    src/QuickEnginePlatform.h \
    src/TCPSocketClient.h \
    src/TCPSocketServer.h

RESOURCES += \
    qml/qtqaengine.qrc

CONFIG += qtquickcompiler
