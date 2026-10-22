# qt-qaengine socket

JSON objects, one per line, UTF-8. Default port is 8900 (`QTQAENGINE_PORT`).

The first object from the client must be:

```json
{"cmd":"hello","protocol":1}
```

The server answers:

```json
{"cmd":"hello","protocol":1,"product":"qt-qaengine"}
```

Any other first object closes the connection.

Later objects use `cmd` `action`:

```json
{"cmd":"action","action":"<name>","params":[]}
```

Replies use `status` and `value`. `status` 0 is success.

`qt-qaengine-inspector` and `qt-qaengine-driver` are the clients of this revision. The driver package also runs an MCP stdio server (`build/mcp.js`) that speaks the same hello and actions. Build and install for the driver and that server are in the driver repository README.
