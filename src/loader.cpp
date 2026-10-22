// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 Andrei Kozhevnikov <coderusinbox@gmail.com>

#include <QGuiApplication>
#include <QTimer>

#include <qtqaengine/QtQaEngine.h>

#ifdef Q_OS_UNIX
#include <private/qhooks_p.h>
#endif

#ifdef __cplusplus
#define INITIALIZER(f)   \
    static void f(void); \
    struct f##_t_        \
    {                    \
        f##_t_(void)     \
        {                \
            f();         \
        }                \
    };                   \
    static f##_t_ f##_;  \
    static void f(void)
#elif defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
#define INITIALIZER2_(f, p)                                  \
    static void f(void);                                     \
    __declspec(allocate(".CRT$XCU")) void (*f##_)(void) = f; \
    __pragma(comment(linker, "/include:" p #f "_")) static void f(void)
#ifdef _WIN64
#define INITIALIZER(f) INITIALIZER2_(f, "")
#else
#define INITIALIZER(f) INITIALIZER2_(f, "_")
#endif
#else
#define INITIALIZER(f)                                \
    static void f(void) __attribute__((constructor)); \
    static void f(void)
#endif

static void qtStartup()
{
    QGuiApplication* gui = qobject_cast<QGuiApplication*>(qApp);
    if (!gui)
    {
        return;
    }

    QTimer::singleShot(0,
                       qApp,
                       []()
                       {
                           QtQaEngine* engine = QtQaEngine::instance();
                           QTimer::singleShot(0, engine, &QtQaEngine::initializeSocket);
                           QTimer::singleShot(0, engine, &QtQaEngine::initializeEngine);
                       });
}

INITIALIZER(libConstructor)
{
#ifdef Q_OS_UNIX
    qtHookData[QHooks::Startup] = reinterpret_cast<quintptr>(&qtStartup);
#else
    qtStartup();
#endif
}
