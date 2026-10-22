// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 Andrei Kozhevnikov <coderusinbox@gmail.com>

#pragma once

#include <QObject>
#include <QSet>
#include <QVariant>

class QAEngineSocketClient;
class ITransportClient;
class ITransportServer;
class IEnginePlatform;
class QWindow;
class QtQaEngine : public QObject
{
    Q_OBJECT
public:
    static QtQaEngine* instance();
    static bool isLoaded();
    static void objectCreated(QObject* o);
    static void objectRemoved(QObject* o);
    IEnginePlatform* getPlatform(bool silent = false);

    virtual ~QtQaEngine();
    bool eventFilter(QObject* watched, QEvent* event) override;

    static QString processName();
    static bool metaInvoke(ITransportClient* socket,
                           QObject* object,
                           const QString& methodName,
                           const QVariantList& params,
                           bool* implemented = nullptr);

    static bool metaInvokeRet(QObject* object,
                              const QString& methodName,
                              const QVariantList& params,
                              bool* implemented = nullptr,
                              QVariant *ret = nullptr);

    void addItem(QObject* o);
    void removeItem(QObject* o);

public slots:
    void initializeSocket();
    void initializeEngine();

private slots:
    void onFocusWindowChanged(QWindow* window);
    void processCommand(ITransportClient* socket, const QByteArray& cmd);
    bool processAppiumCommand(ITransportClient* socket,
                              const QString& action,
                              const QVariantList& params);
    void onPlatformReady();
    void clientLost(ITransportClient* client);

private:
    explicit QtQaEngine(QObject* parent = nullptr);
    IEnginePlatform* ensurePlatform(QWindow* window);
    QWindow* selectedWindow() const;
    static QString windowHandleFor(QWindow* window);
    QWindow* windowFromHandle(const QString& handle) const;
    static void socketReply(ITransportClient* socket, const QVariant& value, int status = 0);
    bool processWindowCommand(ITransportClient* socket,
                              const QString& action,
                              const QVariantList& params);

    ITransportServer* m_socketServer = nullptr;
    QSet<ITransportClient*> m_readyClients;

    bool m_analyzeActive = false;
    ITransportClient* m_analyzeSocket = nullptr;
};

