// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 Andrei Kozhevnikov <coderusinbox@gmail.com>

#pragma once
#include <qtqaengine/ITransportServer.h>

#include <QHash>
#include <QObject>

class QTcpServer;
class TCPSocketServer : public ITransportServer
{
    Q_OBJECT
public:
    explicit TCPSocketServer(quint16 port = 8900, QObject* parent = nullptr);

public slots:
    void start();

private slots:
    void newConnection();

private:
    quint16 m_port = 8900;
    QTcpServer* m_server = nullptr;
};
