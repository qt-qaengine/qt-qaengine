// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 Andrei Kozhevnikov <coderusinbox@gmail.com>

#pragma once

#include <QHash>
#include <QObject>

class ITransportClient;
class ITransportServer : public QObject
{
    Q_OBJECT
public:
    explicit ITransportServer(QObject* parent = nullptr);

    virtual void registerClient(ITransportClient* client);
    virtual void readData(ITransportClient* client);

signals:
    void commandReceived(ITransportClient* client, const QByteArray& cmd);
    void clientLost(ITransportClient* client);

public slots:
    virtual void start() = 0;

private:
    QHash<QObject*, ITransportClient*> m_clients;
};
