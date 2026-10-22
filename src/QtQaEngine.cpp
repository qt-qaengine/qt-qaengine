// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 Andrei Kozhevnikov <coderusinbox@gmail.com>

#include <qtqaengine/IEnginePlatform.h>
#include <qtqaengine/ITransportClient.h>
#include <qtqaengine/QtQaEngine.h>
#include <qtqaengine/QAEngineSocketClient.h>
#include <qtqaengine/TCPSocketServer.h>

#if defined(MO_USE_QUICK)
#include <QQuickWindow>
#include <qtqaengine/QuickEnginePlatform.h>
#endif

#if defined(MO_USE_QWIDGETS)
#include <QApplication>
#include <qtqaengine/WidgetsEnginePlatform.h>
#endif

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMetaMethod>
#include <QPointer>
#include <QProcessEnvironment>
#include <QTcpSocket>
#include <QTimer>
#include <QWindow>

#include <QLoggingCategory>

#include <private/qhooks_p.h>
#include <private/qmetaobject_p.h>
#include <private/qmetatype_p.h>

#include <array>

Q_LOGGING_CATEGORY(categoryEngine, "qtqaengine.engine", QtWarningMsg)

namespace
{

bool s_engineLoaded = false;
QtQaEngine* s_instance = nullptr;

QString s_processName = "qtqaengine";
bool s_exiting = false;

QHash<QWindow*, QPointer<IEnginePlatform>> s_windows;
QPointer<QWindow> s_lastFocusWindow;
// Sticky W3C target; null = follow focus (s_lastFocusWindow).
QPointer<QWindow> s_selectedWindow;

// MJSONWP / Appium status for NoSuchWindowError
constexpr int kStatusNoSuchWindow = 23;

void beginExit()
{
    if (s_exiting)
    {
        return;
    }
    s_exiting = true;
    // QFile/QObject created from dtors (JsonConfig::sync) must not hit AddQObject.
    qtHookData[QHooks::AddQObject] = 0;
    qtHookData[QHooks::RemoveQObject] = 0;
}

inline QGenericArgument qVariantToArgument(const QVariant& variant)
{
    if (variant.isValid() && !variant.isNull())
    {
        return QGenericArgument(variant.typeName(), variant.constData());
    }
    return QGenericArgument();
}

void clearWindowIfMatch(QWindow* window)
{
    s_windows.remove(window);
    if (s_lastFocusWindow == window)
    {
        s_lastFocusWindow = nullptr;
    }
    if (s_selectedWindow == window)
    {
        s_selectedWindow = nullptr;
    }
}

} // namespace

bool QtQaEngine::isLoaded()
{
    return s_instance;
}

void QtQaEngine::objectCreated(QObject* o)
{
    if (s_exiting || !s_instance)
    {
        return;
    }

    s_instance->addItem(o);
}

void QtQaEngine::objectRemoved(QObject* o)
{
    if (s_exiting || !s_instance)
    {
        return;
    }

    s_instance->removeItem(o);
}

QWindow* QtQaEngine::selectedWindow() const
{
    if (s_selectedWindow)
    {
        return s_selectedWindow;
    }
    return s_lastFocusWindow;
}

QString QtQaEngine::windowHandleFor(QWindow* window)
{
    if (!window)
    {
        return QString();
    }
    return QString::number(quintptr(window), 16);
}

QWindow* QtQaEngine::windowFromHandle(const QString& handle) const
{
    if (handle.isEmpty())
    {
        return nullptr;
    }
    bool ok = false;
    const quintptr ptr = handle.toULongLong(&ok, 16);
    if (!ok || !ptr)
    {
        return nullptr;
    }
    for (QWindow* w : qGuiApp->topLevelWindows())
    {
        if (quintptr(w) == ptr)
        {
            return w;
        }
    }
    return nullptr;
}

void QtQaEngine::socketReply(ITransportClient* socket, const QVariant& value, int status)
{
    QJsonObject reply;
    reply.insert(QStringLiteral("status"), status);
    reply.insert(QStringLiteral("value"), QJsonValue::fromVariant(value));
    const QByteArray data = QJsonDocument(reply).toJson(QJsonDocument::Compact);
    socket->write(data);
    socket->flush();
}

IEnginePlatform* QtQaEngine::ensurePlatform(QWindow* window)
{
    if (s_exiting || !window)
    {
        return nullptr;
    }

    if (s_windows.contains(window))
    {
        return s_windows.value(window);
    }

    IEnginePlatform* platform = nullptr;
#if defined(MO_USE_QUICK)
    if (qobject_cast<QQuickWindow*>(window))
    {
        platform = new QuickEnginePlatform(window);
    }
#endif

#if defined(MO_USE_QWIDGETS)
    if (!platform)
    {
        platform = new WidgetsEnginePlatform(window);
    }
#endif
    if (!platform)
    {
        qWarning() << Q_FUNC_INFO << "Can't determine platform";
        return nullptr;
    }
    connect(platform, &IEnginePlatform::ready, this, &QtQaEngine::onPlatformReady);
    connect(qGuiApp, &QGuiApplication::focusWindowChanged, platform, &IEnginePlatform::focusWindowChanged);
    connect(window,
            &QWindow::destroyed,
            [window]()
            {
                clearWindowIfMatch(window);
            });
    QTimer::singleShot(0, platform, &IEnginePlatform::initialize);

    if (m_analyzeActive)
        platform->startAnalyze(m_analyzeSocket);

    qDebug() << "!!! new platform !!!" << platform << window;
    s_windows.insert(window, platform);
    return platform;
}

IEnginePlatform* QtQaEngine::getPlatform(bool silent)
{
    QWindow* window = selectedWindow();
    if (!window)
    {
        return nullptr;
    }
    const auto platform = s_windows.value(window);
    if (platform)
    {
        return platform;
    }
    else
    {
        if (!silent)
        {
            qCWarning(categoryEngine) << Q_FUNC_INFO << "No platform for window:" << window;
        }
    }
    return nullptr;
}

void QtQaEngine::initializeSocket()
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    qCDebug(categoryEngine) << Q_FUNC_INFO << Qt::endl
                            << "name:" << qApp->arguments().first() << Qt::endl
#else
    qCDebug(categoryEngine) << Q_FUNC_INFO << endl
                            << "name:" << qApp->arguments().first() << endl
#endif
                            << "pid:" << qApp->applicationPid();

    qCDebug(categoryEngine) << "QAPreload Version:"
#ifdef QAPRELOAD_VERSION
                            << QStringLiteral(QAPRELOAD_VERSION);
#else
                            << QStringLiteral("2.0.0-dev");
#endif

    m_socketServer->start();
}

void QtQaEngine::initializeEngine()
{
    disconnect(
        m_socketServer, &ITransportServer::commandReceived, this, &QtQaEngine::initializeEngine);

    if (s_engineLoaded)
    {
        return;
    }
    s_engineLoaded = true;

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    qCDebug(categoryEngine) << Q_FUNC_INFO << Qt::endl;
#else
    qCDebug(categoryEngine) << Q_FUNC_INFO << endl;
#endif

    qtHookData[QHooks::RemoveQObject] = reinterpret_cast<quintptr>(&QtQaEngine::objectRemoved);
    qtHookData[QHooks::AddQObject] = reinterpret_cast<quintptr>(&QtQaEngine::objectCreated);

    // Event filter sees QEvent::Quit before aboutToQuit. Chattti's aboutToQuit
    // (destroyQmlEngine) is often connected first, so uninstall hooks here in
    // beginExit — not only in aboutToQuit.
    qApp->installEventFilter(this);
    connect(qApp,
            &QCoreApplication::aboutToQuit,
            []()
            {
                qCDebug(categoryEngine) << Q_FUNC_INFO << "about to quit!";
                beginExit();
            });

    connect(qGuiApp, &QGuiApplication::focusWindowChanged, this, &QtQaEngine::onFocusWindowChanged);
    if (!qGuiApp->topLevelWindows().isEmpty())
    {
        onFocusWindowChanged(qGuiApp->topLevelWindows().first());
    }
    if (!qGuiApp->focusWindow())
    {
        QTimer* t = new QTimer();
        connect(t,
                &QTimer::timeout,
                [this, t]()
                {
                    if (s_exiting || !qGuiApp->focusWindow())
                    {
                        return;
                    }
                    onFocusWindowChanged(qGuiApp->focusWindow());
                    t->deleteLater();
                });
        t->start(1000);
    }
}

void QtQaEngine::onFocusWindowChanged(QWindow* window)
{
    if (s_exiting)
    {
        return;
    }

    qCDebug(categoryEngine) << "focusWindowChanged" << window << "appName"
                            << qApp->arguments().first();

    if (!window)
    {
        for (auto w : qApp->topLevelWindows())
        {
            if (w->property("visible").toBool()) {
                s_lastFocusWindow = w;
                break;
            }
        }
        return;
    }

    if (!ensurePlatform(window))
    {
        return;
    }

    s_lastFocusWindow = window;
}

void QtQaEngine::onPlatformReady()
{

    IEnginePlatform* platform = qobject_cast<IEnginePlatform*>(sender());
    if (!platform)
    {
        return;
    }

    s_processName = QFileInfo(qApp->arguments().first()).baseName();

    qCDebug(categoryEngine) << Q_FUNC_INFO << "Process name:" << s_processName
                            << "platform:" << platform << platform->window()
                            << platform->rootObject();
}

void QtQaEngine::clientLost(ITransportClient* client)
{
    qCDebug(categoryEngine) << Q_FUNC_INFO << client;

    m_readyClients.remove(client);

    if (client == m_analyzeSocket) {
        for (auto platform : s_windows)
        {
            if (platform)
            {
                platform->stopAnalyze();
            }
        }

        m_analyzeActive = false;
        m_analyzeSocket = nullptr;
    }
}

QtQaEngine* QtQaEngine::instance()
{
    if (!s_instance)
    {
        s_instance = new QtQaEngine(qApp);
    }
    return s_instance;
}

QtQaEngine::QtQaEngine(QObject* parent)
    : QObject(parent)
{
    int port = QProcessEnvironment::systemEnvironment().value("QTQAENGINE_PORT", "8900").toInt();
    QString defaultRules = "qtqaengine.*.debug=false\n"
    //                        "qtqaengine.platform.generic=true\n"
    //                        "qtqaengine.platform.quick=true\n"
    //                        "qtqaengine.keymouse=true\n"
    //                        "qtqaengine.transport.server.debug=true\n";
                           "qtqaengine.engine.debug=true\n";
    QString filterRules = QProcessEnvironment::systemEnvironment().value("QTQAENGINE_FILTER_RULES", defaultRules);
    qDebug() << "QtQaEngine port:" << port;
    m_socketServer = new TCPSocketServer(port, this);

    qRegisterMetaType<QTcpSocket*>();
    qRegisterMetaType<ITransportClient*>();
    qRegisterMetaType<ITransportServer*>();
    QLoggingCategory::setFilterRules(filterRules);

    connect(m_socketServer, &ITransportServer::commandReceived, this, &QtQaEngine::initializeEngine);
    connect(m_socketServer, &ITransportServer::commandReceived, this, &QtQaEngine::processCommand);
    connect(m_socketServer, &ITransportServer::clientLost, this, &QtQaEngine::clientLost);
}

QtQaEngine::~QtQaEngine()
{
}

QString QtQaEngine::processName()
{
    return s_processName;
}

bool QtQaEngine::metaInvoke(ITransportClient* socket,
                          QObject* object,
                          const QString& methodName,
                          const QVariantList& params,
                          bool* implemented)
{
    auto mo = object->metaObject();
    do
    {
        for (int i = mo->methodOffset(); i < mo->methodOffset() + mo->methodCount(); i++)
        {
            if (mo->method(i).name() == methodName.toLatin1())
            {
                if (implemented)
                {
                    *implemented = true;
                }

                const QMetaMethod method = mo->method(i);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                QVariantList args = params;

                std::vector<const void*> data;
                std::vector<const char*> names;
                std::vector<const QtPrivate::QMetaTypeInterface*> types;

                QMetaMethodReturnArgument r = {};
                data.push_back(r.data);
                names.push_back(r.name);
                types.push_back(r.metaType);

                auto socketArg = Q_ARG(ITransportClient*, socket);
                data.push_back(socketArg.data);
                names.push_back(socketArg.name);
                types.push_back(socketArg.metaType);

                for (int i = 0; i < (method.parameterCount() - 1) && args.count() > i; i++) {
                    QMetaType paramType = method.parameterMetaType(i + 1);
                    if (args[i].metaType() != paramType) {
                        if (args[i].canConvert(paramType)) {
                            args[i].convert(paramType);
                        } else if (paramType == QMetaType(QMetaType::Type::QVariant)) {
                            args[i] = QVariant::fromValue(args[i]);
                        } else {
                            qWarning() << Q_FUNC_INFO << "Can't convert" << args[i].metaType() << args[i] << "to" << paramType;
                            return false;
                        }
                    }
                    data.push_back(args[i].data());
                    names.push_back(paramType.name());
                    types.push_back(paramType.iface());
                }

                QMetaMethodInvoker::InvokeFailReason reason =
                    QMetaMethodInvoker::invokeImpl(method, object, Qt::DirectConnection,
                                                   data.size(), &data[0], &names[0], &types[0]);

                if (int(reason) <= 0) {
                    return reason == QMetaMethodInvoker::InvokeFailReason::None;
                } else {
                    qWarning() << Q_FUNC_INFO << "method not found!";
                    return false;
                }
#else
                QGenericArgument arguments[9] = {QGenericArgument()};
                for (int i = 0; i < (method.parameterCount() - 1) && params.count() > i; i++)
                {
                    if (method.parameterType(i + 1) == QMetaType::QVariant)
                    {
                        arguments[i] = Q_ARG(QVariant, params[i]);
                    }
                    else
                    {
                        arguments[i] = qVariantToArgument(params[i]);
                    }
                }

                return QMetaObject::invokeMethod(object,
                                                 methodName.toLatin1().constData(),
                                                 Qt::DirectConnection,
                                                 Q_ARG(ITransportClient*, socket),
                                                 arguments[0],
                                                 arguments[1],
                                                 arguments[2],
                                                 arguments[3],
                                                 arguments[4],
                                                 arguments[5],
                                                 arguments[6],
                                                 arguments[7],
                                                 arguments[8]);
#endif
            }
        }
    } while ((mo = mo->superClass()));

    if (implemented)
    {
        *implemented = false;
    }
    return false;
}


bool QtQaEngine::metaInvokeRet(QObject* object,
                             const QString& methodName,
                             const QVariantList& params,
                             bool* implemented,
                             QVariant *ret)
{
    auto mo = object->metaObject();
    do
    {
        for (int i = mo->methodOffset(); i < mo->methodOffset() + mo->methodCount(); i++)
        {
            if (mo->method(i).name() == methodName.toLatin1())
            {
                if (implemented)
                {
                    *implemented = true;
                }

                const QMetaMethod method = mo->method(i);
                QVariantList args = params;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)

                std::vector<const void*> data;
                std::vector<const char*> names;
                std::vector<const QtPrivate::QMetaTypeInterface*> types;

                QMetaMethodReturnArgument r = {};
                const QMetaType returnType = method.returnMetaType();
                if (!ret || returnType == QMetaType(QMetaType::Type::Void)) {
                    data.push_back(r.data);
                    names.push_back(r.name);
                    types.push_back(r.metaType);
                } else if (ret) {
                    ret->setValue(QVariant(returnType, ret->data()));

                    data.push_back(ret->data());
                    names.push_back(returnType.name());
                    types.push_back(returnType.iface());
                }

                for (int i = 0; i < method.parameterCount() && args.count() > i; i++) {
                    QMetaType paramType = method.parameterMetaType(i);
                    if (args[i].metaType() != paramType) {
                        if (args[i].canConvert(paramType)) {
                            args[i].convert(paramType);
                        } else if (paramType == QMetaType(QMetaType::Type::QVariant)) {
                            args[i] = QVariant::fromValue(args[i]);
                        } else {
                            qWarning() << Q_FUNC_INFO << "Can't convert" << args[i].metaType() << args[i] << "to" << paramType;
                            return false;
                        }
                    }
                    data.push_back(args[i].data());
                    names.push_back(paramType.name());
                    types.push_back(paramType.iface());
                }

                QMetaMethodInvoker::InvokeFailReason reason =
                    QMetaMethodInvoker::invokeImpl(method, object, Qt::DirectConnection,
                                                   data.size(), &data[0], &names[0], &types[0]);

                if (int(reason) <= 0) {
                    return reason == QMetaMethodInvoker::InvokeFailReason::None;
                } else {
                    qWarning() << Q_FUNC_INFO << "method not found!";
                    return false;
                }
#else
                QGenericArgument arguments[10] = {QGenericArgument()};
                for (int i = 0; i < method.parameterCount() && args.count() > i; i++)
                {
                    int paramType = method.parameterType(i);
                    if (paramType == QMetaType::QVariant)
                    {
                        arguments[i] = Q_ARG(QVariant, args[i]);
                    }
                    else
                    {
                        if (paramType != args[i].type() && args[i].canConvert(paramType)) {
                            args[i].convert(paramType);
                        }
                        arguments[i] = qVariantToArgument(args[i]);
                    }
                }

                int returnType =  method.returnType();
                if (returnType == QMetaType::Void) {
                    return QMetaObject::invokeMethod(object,
                                                     methodName.toLatin1().constData(),
                                                     Qt::DirectConnection,
                                                     arguments[0],
                                                     arguments[1],
                                                     arguments[2],
                                                     arguments[3],
                                                     arguments[4],
                                                     arguments[5],
                                                     arguments[6],
                                                     arguments[7],
                                                     arguments[8],
                                                     arguments[9]);
                } else {
                    ret->setValue(QVariant(QVariant::nameToType(QMetaType::typeName(returnType))));
                    return QMetaObject::invokeMethod(object,
                                                     methodName.toLatin1().constData(),
                                                     Qt::DirectConnection,
                                                     QGenericReturnArgument(ret->typeName(), ret->data()),
                                                     arguments[0],
                                                     arguments[1],
                                                     arguments[2],
                                                     arguments[3],
                                                     arguments[4],
                                                     arguments[5],
                                                     arguments[6],
                                                     arguments[7],
                                                     arguments[8],
                                                     arguments[9]);
                }
#endif
            }
        }
    } while ((mo = mo->superClass()));

    if (implemented)
    {
        *implemented = false;
    }
    return false;
}

void QtQaEngine::addItem(QObject* o)
{
    if (s_exiting)
    {
        return;
    }
    if (auto platform = getPlatform(true))
    {
        platform->addItem(o);
    }
}

bool QtQaEngine::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Quit)
    {
        beginExit();
    }
    return QObject::eventFilter(watched, event);
}

void QtQaEngine::removeItem(QObject* o)
{
    if (s_exiting)
    {
        return;
    }
    // Element ids live in the platform that found them (often Main), while
    // RemoveQObject may run when focus is on another window (Call/Notification).
    // Clear the pointer from every platform hash — not only the focused one.
    for (auto platform : s_windows)
    {
        if (platform)
        {
            platform->removeItem(o);
        }
    }
    if (s_selectedWindow == o)
    {
        s_selectedWindow = nullptr;
    }
    if (s_lastFocusWindow == o)
    {
        s_lastFocusWindow = nullptr;
        // ~QWindow already unregistered this window; the list can be empty
        // (last window / engine teardown). QList::first() asserts in Debug.
        const QWindowList windows = qGuiApp ? qGuiApp->topLevelWindows() : QWindowList{};
        for (QWindow* w : windows)
        {
            if (w && w != o)
            {
                s_lastFocusWindow = w;
                break;
            }
        }
    }
}

void QtQaEngine::processCommand(ITransportClient* socket, const QByteArray& cmd)
{
    qCDebug(categoryEngine) << Q_FUNC_INFO << socket;
    qCDebug(categoryEngine).noquote() << cmd;

    QJsonParseError error;
    QJsonDocument json = QJsonDocument::fromJson(cmd, &error);
    if (error.error != QJsonParseError::NoError)
    {
        return;
    }

    const QJsonObject object = json.object();
    const QString command = object.value(QStringLiteral("cmd")).toString();

    if (!m_readyClients.contains(socket))
    {
        if (command == QLatin1String("hello")
            && object.value(QStringLiteral("protocol")).toInt() == 1)
        {
            m_readyClients.insert(socket);
            QJsonObject reply;
            reply.insert(QStringLiteral("cmd"), QStringLiteral("hello"));
            reply.insert(QStringLiteral("protocol"), 1);
            reply.insert(QStringLiteral("product"), QStringLiteral("qt-qaengine"));
            socket->write(QJsonDocument(reply).toJson(QJsonDocument::Compact) + '\n');
            socket->flush();
            return;
        }
        socket->close();
        return;
    }

    if (command != QLatin1String("action"))
    {
        return;
    }

    if (!object.contains(QStringLiteral("action")))
    {
        return;
    }

    const auto action = object.value(QStringLiteral("action")).toVariant().toString();
    const auto params = object.value(QStringLiteral("params")).toVariant().toList();

    if (action == "startAnalyze") {
        m_analyzeActive = true;
        m_analyzeSocket = socket;
    } else if (action == "stopAnalyze") {
        m_analyzeActive = false;
        m_analyzeSocket = nullptr;
    }

    processAppiumCommand(socket, action, params);
}

bool QtQaEngine::processWindowCommand(ITransportClient* socket,
                                    const QString& action,
                                    const QVariantList& params)
{
    if (action == QLatin1String("getWindowHandles"))
    {
        QStringList handles;
        for (QWindow* w : qGuiApp->topLevelWindows())
        {
            if (!w)
            {
                continue;
            }
            ensurePlatform(w);
            handles.append(windowHandleFor(w));
        }
        socketReply(socket, handles);
        return true;
    }

    if (action == QLatin1String("getWindowHandle"))
    {
        QWindow* window = selectedWindow();
        if (!window)
        {
            socketReply(socket, QStringLiteral("no such window"), kStatusNoSuchWindow);
            return true;
        }
        ensurePlatform(window);
        socketReply(socket, windowHandleFor(window));
        return true;
    }

    if (action == QLatin1String("setWindow"))
    {
        // Appium makeArgs yields [name, handle]; either may be empty/duplicate.
        QString handle;
        if (params.size() >= 2 && !params.at(1).toString().isEmpty())
        {
            handle = params.at(1).toString();
        }
        else if (!params.isEmpty())
        {
            handle = params.at(0).toString();
        }

        // Empty handle drops the sticky target so the next getWindowHandle follows focus.
        if (handle.isEmpty())
        {
            s_selectedWindow = nullptr;
            QWindow* window = selectedWindow();
            if (window)
            {
                ensurePlatform(window);
            }
            socketReply(socket, window ? windowHandleFor(window) : QString());
            return true;
        }

        QWindow* window = windowFromHandle(handle);
        if (!window)
        {
            socketReply(socket, QStringLiteral("no such window"), kStatusNoSuchWindow);
            return true;
        }

        if (!ensurePlatform(window))
        {
            socketReply(socket, QStringLiteral("no such window"), kStatusNoSuchWindow);
            return true;
        }

        s_selectedWindow = window;
        s_lastFocusWindow = window;
        window->raise();
        window->requestActivate();

        socketReply(socket, QString());
        return true;
    }

    return false;
}

bool QtQaEngine::processAppiumCommand(ITransportClient* socket,
                                    const QString& action,
                                    const QVariantList& params)
{
    if (processWindowCommand(socket, action, params))
    {
        return true;
    }

    const QString methodName = QStringLiteral("%1Command").arg(action);
    qCDebug(categoryEngine) << Q_FUNC_INFO << socket << methodName << params;

    bool result = false;
    if (auto platform = getPlatform())
    {
        qCDebug(categoryEngine) << Q_FUNC_INFO << platform << platform->window()
                                << platform->rootObject();
        bool implemented = true;
        result = metaInvoke(socket, platform, methodName, params, &implemented);

        if (!implemented)
        {
            platform->socketReply(socket, QStringLiteral("not_implemented"), 405);
        }
    }
    else
    {
        socketReply(socket, QStringLiteral("no platform!"), 1);
    }

    return result;
}
