TARGET = qaengine
TEMPLATE = lib

TARGETPATH = AutoQA/QAEngine
android {
    TARGET = $$qtLibraryTarget($$TARGET)
    installPath = $$[QT_INSTALL_QML]/$$TARGETPATH
    target.path = $$installPath
    INSTALLS += target
}

# TODO: uncomment xmlpatterns
QT += core core-private network
CONFIG += plugin
CONFIG += c++11

contains(DEFINES, MO_USE_QXMLPATTERNS) {
    QT += xmlpatterns
}

contains(DEFINES, MO_USE_QUICK) {
    QT += qml quick quick-private

    SOURCES += src/QuickEnginePlatform.cpp
    HEADERS += include/qt_qa_engine/QuickEnginePlatform.h
}

contains(DEFINES, MO_USE_QWIDGETS) {
    QT += widgets widgets-private

    SOURCES += src/WidgetsEnginePlatform.cpp
    HEADERS += include/qt_qa_engine/WidgetsEnginePlatform.h
}

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
    src/TCPSocketClient.cpp \
    src/TCPSocketServer.cpp \
    src/loader.cpp

HEADERS += \
    include/qt_qa_engine/GenericEnginePlatform.h \
    include/qt_qa_engine/IEnginePlatform.h \
    include/qt_qa_engine/ITransportClient.h \
    include/qt_qa_engine/ITransportServer.h \
    include/qt_qa_engine/QAEngine.h \
    include/qt_qa_engine/QAEngineSocketClient.h \
    include/qt_qa_engine/QAKeyEngine.h \
    include/qt_qa_engine/QAMouseEngine.h \
    include/qt_qa_engine/QAPendingEvent.h \
    include/qt_qa_engine/TCPSocketClient.h \
    include/qt_qa_engine/TCPSocketServer.h

INCLUDEPATH += include

RESOURCES += \
    qml/qtqaengine.qrc

CONFIG += qtquickcompiler
