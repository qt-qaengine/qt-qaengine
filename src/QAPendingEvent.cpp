// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 Andrei Kozhevnikov <coderusinbox@gmail.com>

#include "include/qtqaengine/QAPendingEvent.h"

QAPendingEvent::QAPendingEvent(QObject* parent)
    : QObject(parent)
{
}

void QAPendingEvent::setCompleted()
{
    emit completed(this);
}
