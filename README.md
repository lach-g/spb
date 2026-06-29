# spb

> **Status: Pre-alpha / Work in progress.** Today this library provides an MQTT
> client abstraction over [Eclipse Paho](https://github.com/eclipse/paho.mqtt.c).
> The Sparkplug B features implied by the name are **not implemented yet** —
> see [Status](#status-read-this-first) and [`ROADMAP.md`](ROADMAP.md).

`spb` is a C library that wraps the Eclipse Paho asynchronous MQTT client
(`MQTTAsync`) behind a small, Paho-free API. Its goal is to grow into a
[**Sparkplug B**](https://sparkplug.eclipse.org/) implementation (specification
**3.0.0**, `spBv1.0` topic namespace) that other C developers can build
industrial / IIoT applications on without re-implementing the Sparkplug session
model, topic namespace, and payload encoding themselves.

## Status

What works **today**:

- An opaque MQTT client (`spb_client_t`) layered over Paho `MQTTAsync`.
- A public API that does **not** require consumers to include Paho headers.
- Asynchronous connect, publish, subscribe, and disconnect.
- Callback indirection so application callbacks receive `spb_*` types rather
  than Paho types.

What does **not** exist yet (the actual Sparkplug B layer):

- The `spBv1.0` topic namespace (group / message-type / edge-node / device).
- Protobuf-encoded Sparkplug payloads and metrics.
- The Edge-of-Network (EoN) node session lifecycle: `NBIRTH`, `NDEATH`,
  `NDATA`, `bdSeq`, and the per-message `seq` counter.
- Device-level messages, Host Application / `STATE` handling, and commands.

If you need a working Sparkplug B stack right now, this is not yet it. If you
want to follow or contribute to one being built, see [`ROADMAP.md`](ROADMAP.md)
and [`TODO.md`](TODO.md).

## Current features

The implemented surface is a transport abstraction:

- **Opaque handle** — `spb_client_t` hides the Paho client and its option
  structs.
- **No Paho leakage** — application code includes only `spb.h`.
- **Async lifecycle** — non-blocking connect/publish/subscribe/disconnect with
  success/failure callbacks.

## Architecture

```
your application
      │  includes spb.h only
      ▼
┌───────────────────────────────┐
│ spb (this library)            │
│  • spb_client_t (opaque)      │
│  • *_wrapper_cb translators   │  ← Sparkplug B layer will sit here later
└───────────────────────────────┘
      │  uses
      ▼
Eclipse Paho MQTTAsync
```

Internally, `struct spb_client` (`src/spb.c`) holds a `paho_lib` containing the
Paho `MQTTAsync` handle and its connect/response/disconnect option structs,
alongside the application's `spb_*` callbacks and options. Each Paho callback
(e.g. `messagearrived_wrapper_cb`, `connect_onsuccess_wrapper_cb`) is a static
wrapper that translates Paho structures into `spb_*` structures before invoking
the application callback with the application's `context`.

## API reference

All functions return `0` on success and `-1` on failure unless noted.

| Function | Purpose |
| --- | --- |
| `spb_client_t* spb_init(void)` | Allocate a client and initialise Paho option structs. Returns `NULL` on allocation failure. |
| `int spb_create(client, uri, client_id)` | Create the underlying Paho client for the given broker `uri` and `client_id`. |
| `int spb_setcallbacks(client, context, connection_lost, message_arrived, delivery_complete)` | Register connection-lost, message-arrived, and delivery-complete callbacks. `message_arrived` is required. |
| `int spb_connect(client, connect_options)` | Begin an async connect. Success/failure reported via the options' callbacks. |
| `int spb_sendmessage(client, topic, message, response_options)` | Publish `message` to `topic`. |
| `int spb_subscribe(client, topic, qos, response_options)` | Subscribe to `topic` at `qos`. |
| `int spb_disconnect(client, disconnect_options)` | Begin an async disconnect. |
| `void spb_destroy(client)` | Destroy the Paho client and free the handle. |

### Option and callback types

- `spb_message_t` — `payload`, `payloadlen`, `retained`, `qos`.
- `spb_connect_options_t` — `keep_alive_interval`, `clean_session`,
  `onsuccess_cb`, `onfailure_cb`, `context`.
- `spb_response_options_t` / `spb_disconnect_options_t` — `onsuccess_cb`,
  `onfailure_cb`, `context`.
- `spb_successdata_t` — `token`.
- `spb_failuredata_t` — `token`, `code`, `message`.
- Callback typedefs: `spb_connectionlost`, `spb_messagearrived` (return `1` if
  the message was successfully processed, else `0`; copy any payload/topic you
  need to retain, as the provided pointers are freed on return),
  `spb_deliverycomplete`, `spb_onsuccess`, `spb_onfailure`.

## Usage

A minimal publisher (see [`examples/simple_publisher.c`](examples/simple_publisher.c)
for the full version, and [`examples/simple_subscriber.c`](examples/simple_subscriber.c)
for the subscribe side):

```c
#include "spb.h"

void on_connect(void* context, spb_successdata_t* response) {
    spb_client_t* client = context;
    spb_response_options_t opts = { .onsuccess_cb = on_send, .context = client };
    spb_message_t msg = { .payload = "Hello there",
                          .payloadlen = 11, .qos = 1, .retained = 0 };
    spb_sendmessage(client, "spb", &msg, opts);
}

int main(void) {
    spb_client_t* client = spb_init();
    spb_create(client, "tcp://test.mosquitto.org:1883", "spb-c-publisher");
    spb_setcallbacks(client, client, conn_lost, msg_arrived, delivery_complete);

    spb_connect_options_t conn = { .keep_alive_interval = 20, .clean_session = 1,
                                   .onsuccess_cb = on_connect, .context = client };
    spb_connect(client, conn);
    /* pump until done... */
    spb_destroy(client);
}
```

## Building

This project uses [Conan](https://conan.io/) for dependencies and CMake to
build. The only dependency is `paho-mqtt-c/1.3.13` (see `conanfile.txt`).

### First time

1. Create a Conan profile for your compiler/architecture:

   ```bash
   conan profile detect --force
   ```

2. Install dependencies:

   ```bash
   conan install . --build=missing -s build_type=Release
   ```

3. Configure, build, and run an example:

   ```bash
   cmake -B build/Release -S . \
     -DCMAKE_TOOLCHAIN_FILE=build/Release/generators/conan_toolchain.cmake \
     -DCMAKE_BUILD_TYPE=Release
   cmake --build build/Release
   ./build/Release/simple_publisher
   ./build/Release/simple_subscriber
   ```

The CMake project builds a static library target `spb_lib` (from `src/spb.c`)
and the two example executables, linking against
`eclipse-paho-mqtt-c::paho-mqtt3as-static`. The library is compiled as C99 with
warnings-as-errors.

## Known limitations

This is an early transport-only layer with rough edges (debug logging in
callbacks, shared response-option state, partial NULL-checking) and no
Sparkplug B semantics. These are tracked in [`TODO.md`](TODO.md).

## License

MIT — see [`LICENSE`](LICENSE).
