// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 Andrei Kozhevnikov <coderusinbox@gmail.com>

#pragma once

#include <QObject>
#include <QVariant>

class QAPendingEvent : public QObject
{
    Q_OBJECT
public:
    explicit QAPendingEvent(QObject *parent = nullptr);

signals:
    void completed(QAPendingEvent *pending);

public slots:
    void setCompleted();
};

