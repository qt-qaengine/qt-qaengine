# qt-qaengine

In-process automation library for Qt applications. It runs inside the application under test and exposes the live object tree over a TCP socket. It is not an official Qt module. The name is not related to [qt/qtqa](https://github.com/qt/qtqa).

Use it when a test or a tool must drive a Qt Quick or Qt Widgets interface from outside the process. The library stays in the process under test. It is not a component to ship inside a product.

## Projects

| Project | Role |
| --- | --- |
| [qt-qaengine](https://github.com/CODeRUS/qt-qaengine) | This library. Listens on the socket. |
| [qt-qaengine-inspector](https://github.com/CODeRUS/qt-qaengine-inspector) | Desktop inspector for the live object tree. |
| [qt-qaengine-driver](https://github.com/CODeRUS/qt-qaengine-driver) | Appium 2 driver and MCP server. |

These three speak one protocol.

## How it runs

`src/loader.cpp` starts the engine when the shared library is loaded into a `QGuiApplication`. On Unix the library registers a Qt startup hook, so it can be loaded before the application object exists. On Windows the static initializer starts the engine only if a `QGuiApplication` already exists when the DLL is loaded.

Once started, the library listens for newline-delimited JSON. The default port is **8900**. The first client frame must be a protocol hello. Any other first frame closes the socket. The wire format is [docs/protocol.md](docs/protocol.md).

```json
{"cmd":"hello","protocol":1}
```

```json
{"cmd":"hello","protocol":1,"product":"qt-qaengine"}
```

Later frames are actions:

```json
{"cmd":"action","action":"initialize","params":[]}
```

A reply uses `status` and `value`. `status` 0 means success. `initialize` needs a Qt Quick or Qt Widgets window. Without one the engine answers `no platform!`.

Qt Quick support is compiled in when Qt Quick is found. Qt Widgets support is off unless you pass `-DQA_ENGINE_WITH_WIDGETS=ON`.

## Features

After `initialize`, clients send `{"cmd":"action","action":"<name>","params":[...]}`. [qt-qaengine-inspector](https://github.com/CODeRUS/qt-qaengine-inspector) and [qt-qaengine-driver](https://github.com/CODeRUS/qt-qaengine-driver) use these actions. The socket is plain TCP, so a client on another machine uses the host and `QTQAENGINE_PORT`.

**Find a Qt object.** `findElement` and `findElements` take a strategy and a selector. Built-in strategies are `objectname`, `objectid`, `classname`, `id`, `name`, `xpath`, `parent`, and `root`. `name` matches visible text. A selector wrapped in `*` is a partial text match. `parent` takes a depth. `root` with an empty selector or `window` is the window. Any other strategy is the name of a `QObject` property. `findElementFromElement` searches under an object you already have.

**Read and change it.** `getText` reads the text. `getAttribute` reads a property such as `objectName`, `enabled`, or `visible`. `click` activates the object. `setValue` and `clear` edit a text field.

**Take a picture.** `getScreenshot` returns a PNG of the window, base64-encoded. `getElementScreenshot` does the same for one object. `getWindowRect` returns the window geometry.

**Read the tree.** `getPageSource` returns the object tree as XML, including `objectName`. This is the same tree the inspector shows after Dump tree.

**Pick a window.** `getWindowHandles` lists the windows. `setWindow` selects the one later actions use.

## Requirements

- CMake 3.16 or newer
- Qt 5 or Qt 6 (Core, Gui, Network). Qt 6 uses C++17. Qt 5 uses C++11.
- Qt Quick, Qml, and Quick Controls 2 when you automate Qt Quick
- A C++ compiler for the same ABI as that Qt build

## Build

Single-config generators:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Visual Studio and other multi-config generators:

```bash
cmake -S . -B build
cmake --build build --config Release
```

The DLL has to use the same Qt build and the same Debug or Release runtime as the application that loads it. A Debug DLL imports `Qt6Cored.dll` and will not load into a Release process. Pass that Qt prefix on the first configure. If `build` was already configured against another Qt, delete `build` and configure again.

Windows, Visual Studio 2022 (toolset v143), Qt 6.11.1, Debug:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -T v143 -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/msvc2022_64"
cmake --build build --config Debug
```

Release is the same configure and `cmake --build build --config Release`. The products are `build/Debug/qtqaengine.dll` and `build/Release/qtqaengine.dll`, each with a matching `qtqaengine.lib`.

Set `QTQAENGINE_PORT` in the application process before the DLL is loaded. The default is 8900.

Widgets backend:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DQA_ENGINE_WITH_WIDGETS=ON
```

The shared library is `libqtqaengine.so` on Unix and `qtqaengine.dll` (with `qtqaengine.lib`) on Windows. Public headers are under `include/qtqaengine/`. The installed CMake package is `qtqaengine`, target `QtQaEngine::QtQaEngineLib`.

```cmake
find_package(qtqaengine REQUIRED)
target_link_libraries(myapp PRIVATE QtQaEngine::QtQaEngineLib)
```

```cpp
#include <qtqaengine/QtQaEngine.h>
```

You do not need to call `QtQaEngine::instance()` yourself when the loader runs. The public entry points are `QtQaEngine::instance()`, `initializeSocket()`, and `initializeEngine()`.

## Configuration

| Variable | Default | Meaning |
| --- | --- | --- |
| `QTQAENGINE_PORT` | `8900` | TCP port the application listens on. |
| `QTQAENGINE_FILTER_RULES` | engine debug on, other `qtqaengine.*` categories off | Qt logging rules. Categories use the `qtqaengine` prefix. |

## Clients

Point [qt-qaengine-inspector](https://github.com/CODeRUS/qt-qaengine-inspector) or [qt-qaengine-driver](https://github.com/CODeRUS/qt-qaengine-driver) at the host and port of the process that has this library loaded. Both clients send the hello frame themselves. They do not inject this DLL and they do not start the application.

The driver repository builds the Appium 2 driver and the MCP server. Install steps, including `appium driver install --source=local` and a Cursor `mcp.json` entry for `node build/mcp.js`, are in that repository's README. The MCP `QTQAENGINE_PORT` and the Appium `qtqaenginePort` capability must be the port this process is listening on.

## License

Noncommercial use is under [PolyForm Noncommercial 1.0.0](LICENSE).

[OPEN_SOURCE_GRANT](OPEN_SOURCE_GRANT) is a separate permission. It lets you install, copy, and run this software to develop and test a program that is publicly available under an OSI-approved license or an FSF free-software license. A company may use that permission. It does not cover a proprietary program, including a proprietary program in the same repository or the same CI job. It does not allow shipping this software inside a product, and it does not allow relicensing it under the other program's license.

Copyright (c) 2026 Andrei Kozhevnikov. The required notice is in [NOTICE](NOTICE).

## Commercial Licensing

Using this library in commercial products requires a separate license. Contact me at [coderus@duck.com](mailto:coderus@duck.com) and on Telegram at [https://t.me/icoderus](https://t.me/icoderus) to discuss pricing and terms. I also provide paid services for integrating automated tests into your infrastructure.
