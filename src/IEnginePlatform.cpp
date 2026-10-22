// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 Andrei Kozhevnikov <coderusinbox@gmail.com>

#include <qtqaengine/IEnginePlatform.h>

IEnginePlatform::IEnginePlatform(QWindow* window)
    : QObject(window)
{
}
