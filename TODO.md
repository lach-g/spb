# TODO

Actionable task list for contributors. See [`ROADMAP.md`](ROADMAP.md) for the
high-level phases and rationale. This file is meant to be appended to over time.

Target specification: **Sparkplug 3.0.0** (`spBv1.0` namespace).

Severity tags: **[Critical]** can crash / corrupt memory · **[High]** important
correctness, API, or test-infra work · **[Medium]** quality / maintainability ·
**[Low]** cosmetic / housekeeping.

---

## Section 1 — Audit & stabilise the current implementation

Before any Sparkplug B work, harden and test the existing MQTT transport
wrapper. The findings below come from auditing `src/spb.c`, `src/spb.h`,
`CMakeLists.txt`, and the examples. Stabilise first, then build Sparkplug
(Section 2) on top.

### 1a. Correctness & safety

- [x] **[Critical]** Zero-initialise the client in `spb_init` (`src/spb.c:30`).
      Only the three Paho option structs are initialised; `client_callbacks`,
      `connect_options`, `response_options`, and `disconnect_options` are left
      as uninitialised heap memory. Use `calloc`/`memset`.
- [x] **[Critical]** NULL-check `response` before dereferencing in every
      success/failure wrapper callback (`src/spb.c:255`, `:271`, `:287`,
      `:301`, `:319`, `:333`). Paho can pass `NULL` here — the example code
      already guards with `response ? ... : 0` (`examples/simple_publisher.c:77`),
      so the library is the inconsistent party and risks a crash.
- [ ] **[High]** Give publish and subscribe independent response-option state.
      `spb_subscribe` and `spb_sendmessage` both overwrite
      `client->response_options` (`src/spb.c:156`), so concurrent/interleaved
      use can clobber each other's callbacks.
- [ ] **[High]** Add a lifecycle/state guard so `spb_connect` /
      `spb_sendmessage` / `spb_subscribe` called before `spb_create` cannot
      operate on an uninitialised Paho handle (currently UB).
- [ ] **[Medium]** Add an explicit `#include <stdio.h>` to `src/spb.c` (`printf`
      is used but the header is only pulled in transitively via `MQTTAsync.h`).
      Audit other implicit/transitive includes while here.

### 1b. API & design

- [ ] **[High]** Propagate underlying Paho error codes instead of collapsing
      every failure to `-1`. Define an `spb` error enum and return meaningful
      codes from the public API.
- [ ] **[Medium]** Fix the `spb_connect` signature drift: declared without
      `const` (`src/spb.c` ... `src/spb.h:68`) but defined with `const`
      (`src/spb.c:101`). Make declaration and definition consistent.
- [ ] **[Medium]** Replace the per-callback `printf` debug logging with a gated
      logging macro / configurable log level; also fix the missing newline in
      `connectionlost_wrapper_cb` (`src/spb.c:197`).
- [ ] **[Medium]** Expose connection options needed later instead of hardcoding
      `MQTTCLIENT_PERSISTENCE_NONE` in `spb_create` (`src/spb.c:55`): MQTT Will,
      TLS/SSL options, MQTT protocol version, and persistence mode. These are
      prerequisites for the Sparkplug session model in Section 2.

### 1c. Build & tooling

- [ ] **[Low]** Remove the leftover `message("testing.. 1 2 3")` debug line in
      `CMakeLists.txt`.
- [ ] **[Medium]** Add `enable_testing()`, a test target, and an install/export
      target with a proper public-header layout (so consumers and examples
      don't reach into `src/`).
- [ ] **[Low]** Update the examples to include the installed/public header path
      rather than the relative `"../src/spb.h"` (`examples/simple_publisher.c:1`,
      `examples/simple_subscriber.c:3`).

### 1d. Tests & CI

- [ ] **[High]** Adopt **Unity + CMock** as the test framework; wire it into
      CMake and Conan and create a `tests/` tree (unit + integration).
- [ ] **[High]** Introduce an **internal transport seam** — a thin interface
      over the `MQTTAsync_*` calls — so CMock can generate mocks for it. This
      decouples the wrapper logic from Paho for fast, broker-free unit tests and
      seeds the planned `spb_node_*`-over-`spb_*` layering used in Section 2.
- [ ] **[High]** Unit-test (against the mocked seam): callback translation
      (Paho → `spb_*` types), context propagation, NULL-response handling,
      `spb_init`/`spb_destroy` lifecycle, and argument validation/return codes.
- [ ] **[Medium]** Integration-test against a **local Mosquitto broker (Docker)**:
      connect → subscribe → publish → receive → disconnect round-trips.
      Deterministic and offline; scriptable so it runs the same locally and in
      CI.
- [ ] **[Medium]** Add a **GitHub Actions** workflow running on push/PR:
      Conan install → CMake build → unit tests → integration tests against a
      Mosquitto service container.
- [ ] **[Low]** Once the test infrastructure is in place, document the test
      setup, layout, and local integration-test commands (including the
      Mosquitto Docker workflow) in `CONTRIBUTING.md`.

---

## Section 2 — Baseline: EoN node session lifecycle + topic namespace

The first slice that turns this from "MQTT wrapper" into "Sparkplug B". Tasks
are roughly ordered by dependency. **Definition of done:** an edge node
connects, publishes a structurally- and sequence-correct `NBIRTH`, the broker
delivers the registered `NDEATH` on ungraceful disconnect, and `bdSeq`/`seq`
behave per spec across reconnects. Builds on the stabilised transport and
transport seam from Section 1.

### 2.1. Decide & integrate the protobuf approach *(prerequisite)*

- [ ] Confirm the payload encoding library using the tradeoff table in
      `ROADMAP.md` (recommendation: **nanopb**).
- [ ] Add the chosen library to `conanfile.txt` and `CMakeLists.txt`.
- [ ] Vendor / generate the Sparkplug `Payload` protobuf schema (from Eclipse
      Tahu) and wire codegen into the build.
- [ ] Verify a trivial encode/decode round-trip builds and runs.

### 2.2. Minimal payload encoding

- [ ] Encode a minimal Sparkplug `Payload` sufficient for `NBIRTH`/`NDEATH`
      (timestamp, `seq`, and the `bdSeq` metric).
- [ ] Decode a received `Payload` (enough to read `seq`/`bdSeq`).
- [ ] Define a payload abstraction so higher layers don't touch protobuf types
      directly.

### 2.3. Topic namespace

- [ ] Implement building of the namespace:
      `spBv1.0/{group_id}/{message_type}/{edge_node_id}[/{device_id}]`.
- [ ] Implement parsing/validation of an incoming Sparkplug topic into its
      components.
- [ ] Cover the node-level message types needed by the baseline: `NBIRTH`,
      `NDEATH`, `NDATA`.

### 2.4. EoN session state machine

- [ ] Register `NDEATH` as the MQTT **Will** message before connecting (with
      the session's `bdSeq`).
- [ ] Connect with Sparkplug-appropriate options (clean session per spec
      guidance, QoS, retain rules).
- [ ] Publish `NBIRTH` on successful connect.
- [ ] Define the session/online state transitions (offline → connecting →
      online → offline) and surface them to the application.

### 2.5. Sequence tracking

- [ ] Maintain `bdSeq` and increment it per new session; carry it into both the
      Will `NDEATH` and the `NBIRTH`.
- [ ] Maintain the per-message `seq` counter (0–255 with rollover) applied to
      all node messages after `NBIRTH`.

### 2.6. Public API surface

- [ ] Add the higher-level `spb_node_*` API atop the existing `spb_*` transport
      (see `ROADMAP.md` architectural decision).
- [ ] Ensure existing `spb_*` examples still build and run unchanged.
- [ ] Add a `examples/edge_node.c` demonstrating connect → `NBIRTH` → `NDATA`.

### 2.7. Verification

- [ ] Validate `NBIRTH`/`NDEATH`/`NDATA` against a broker and a Sparkplug-aware
      consumer or decoder.
- [ ] Confirm `NDEATH` is delivered on ungraceful disconnect (kill the process).
- [ ] Confirm `bdSeq`/`seq` correctness across a reconnect cycle.

---

## Backlog (later phases)

Stubs to expand as the baseline lands. See `ROADMAP.md` for context.

- [ ] **Payload metrics breadth** — full metric data types, timestamps,
      aliases, `Template`, `DataSet`.
- [ ] **Device messages** — `DBIRTH`, `DDEATH`, `DDATA`.
- [ ] **Host Application** — `STATE`, message ordering, `NCMD`/`DCMD`,
      rebirth requests.
- [ ] **Compliance & robustness** — conformance/TCK, TLS, reconnection &
      store-and-forward, broader test coverage.
