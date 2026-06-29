# Roadmap

This document is for **contributors**. It maps the high-level path from the
current MQTT transport wrapper to a usable Sparkplug B implementation, records
the key architectural decisions, and points to the concrete, actionable task
list in [`TODO.md`](TODO.md).

Target specification: **Sparkplug 3.0.0** (topic namespace `spBv1.0`).

## Vision

Provide a C library that lets developers stand up Sparkplug B
**Edge-of-Network (EoN) nodes** — and eventually **Host Applications** — on top
of MQTT without re-implementing the Sparkplug session model, topic namespace,
or payload encoding. The existing `spb_*` MQTT layer becomes the transport;
Sparkplug semantics are built as a distinct layer above it.

## Phased roadmap

### Phase 0 — MQTT transport layer *(current state)*

The implemented baseline: an opaque client over Paho `MQTTAsync` with
async connect/publish/subscribe/disconnect and `spb_*` callback translation.
No Sparkplug semantics. See the README for the current API.

### Phase 1 — Audit & stabilise

Before building Sparkplug semantics, harden the existing transport wrapper and
stand up a test foundation. This phase fixes the correctness/safety issues
found in the audit (uninitialised client memory, NULL-response dereferences,
shared response-option state, missing state guards), tightens the API and
build, and establishes testing: **Unity + CMock** unit tests, a local
**Mosquitto (Docker)** integration suite, and **GitHub Actions** CI.

Crucially, this phase introduces an **internal transport seam** — a thin
interface over the `MQTTAsync_*` calls — so the wrapper logic can be mocked and
unit-tested without a broker. That same seam is what the Sparkplug `spb_node_*`
layer sits on in Phase 2, so the testing investment and the Sparkplug
architecture reinforce each other. Detailed, severity-tagged tasks live in
**Section 1** of [`TODO.md`](TODO.md).

### Phase 2 — Baseline: EoN node session lifecycle + topic namespace

The first slice of *actual* Sparkplug B functionality and the project's
near-term focus after stabilisation. It delivers a single edge node that can
establish a spec-correct Sparkplug session: build/parse the `spBv1.0` topic
namespace, register `NDEATH` as the MQTT Will, publish `NBIRTH` on connect, and
maintain `bdSeq` and the per-message `seq` counter. Detailed, checkable tasks
live in **Section 2** of [`TODO.md`](TODO.md).

Definition of done for the baseline: an edge node connects, emits a
structurally- and sequence-correct `NBIRTH`, the broker delivers the registered
`NDEATH` on ungraceful disconnect, and `bdSeq`/`seq` behave per spec across
reconnects.

### Phase 3 — Payload metrics breadth

Build out the Sparkplug payload/metric model: the full set of metric data
types, timestamps, metric aliases, and the more advanced `Template` and
`DataSet` types. Encode/decode round-trip coverage.

### Phase 4 — Device-level messages

Add device support beneath the edge node: `DBIRTH`, `DDEATH`, `DDATA`, and the
relationship between node and device birth/death ordering.

### Phase 5 — Host Application side

Implement the primary Host Application role: `STATE` birth/death, consuming and
ordering node/device messages, issuing `NCMD`/`DCMD` commands, and handling
rebirth requests.

### Phase 6 — Compliance, security, robustness

Conformance against the Sparkplug test suite / TCK where possible, TLS
transport, reconnection and store-and-forward semantics, and a real test
harness (unit + broker integration).

## Key architectural decisions

### Sparkplug layered atop the existing MQTT wrapper

The current `spb_*` functions remain the **transport layer**. Sparkplug
functionality is added as a distinct **higher-level API** (working name
`spb_node_*`) that uses the transport internally. This keeps transport concerns
and Sparkplug semantics separated, and lets the existing examples keep working
unchanged. The exact shape of the `spb_node_*` API is settled during the
baseline.

### Payload encoding — tradeoffs documented, choice made in the baseline

Sparkplug B payloads are **Google Protobuf**-encoded (the `Payload` schema from
Eclipse Tahu). C has no built-in protobuf, so an approach must be chosen. The
decision itself is the **first task of the baseline** (see [`TODO.md`](TODO.md))
because `NBIRTH`/`NDEATH` cannot be spec-compliant without a real payload (they
carry the `bdSeq` metric). The candidates:

| Option | Pros | Cons |
| --- | --- | --- |
| **nanopb** *(recommended)* | Designed for embedded/constrained targets; pure C; very small code + RAM footprint; static allocation friendly; available via Conan. | Codegen + `.options` tuning has a learning curve; some dynamic/repeated fields need care. |
| **protobuf-c** | Mature; simpler generated API. | Larger footprint; heap-heavy allocation; less suited to constrained edge devices. |
| **Hand-rolled encoder** | Zero external dependency; full control over just the Sparkplug schema. | High effort; error-prone; must track spec changes manually. |

Recommendation: **nanopb**, given the likely edge/industrial deployment
targets. The decision is confirmed (and the chosen library integrated) as
baseline task #1.

## How to contribute

1. Read this roadmap and [`TODO.md`](TODO.md).
2. Start with **Section 1 — Audit & stabilise**; until that lands it is the
   project's primary focus. Section 2 (the Sparkplug baseline) builds on it.
3. Pick an unchecked task (they are roughly ordered by dependency/severity).
4. Keep transport (`spb_*`) and Sparkplug (`spb_node_*`) concerns separated.
5. Add or update tests and an example demonstrating the new behaviour.
