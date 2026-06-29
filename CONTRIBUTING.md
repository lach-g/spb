# Contributing to spb

Rules are stated explicitly and unambiguously to make the contribution process
as clear as possible.

---

## Table of contents

1. [Dev environment](#dev-environment)
2. [Code style](#code-style)
3. [Commit style — Conventional Commits 1.0.0](#commit-style--conventional-commits-100)
4. [Branch naming](#branch-naming)
5. [Pull requests](#pull-requests)
6. [Versioning](#versioning)
7. [Issue referencing](#issue-referencing)
8. [Contributor checklist](#contributor-checklist)

---

## Dev environment

**Requirements:** a C99-capable compiler, [Conan](https://conan.io/) ≥ 2, and
CMake ≥ 3.15.

```bash
# 1. Create a Conan profile for your toolchain
conan profile detect --force

# 2. Install dependencies
conan install . --build=missing -s build_type=Release

# 3. Configure and build
cmake -B build/Release -S . \
  -DCMAKE_TOOLCHAIN_FILE=build/Release/generators/conan_toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build/Release

# 4. Run tests (once the test target exists — see TODO.md §1d)
ctest --test-dir build/Release --output-on-failure
```

---

## Code style

- **Standard:** C99 strictly (`-std=c99 -pedantic`). No compiler extensions.
- **Warnings:** warnings are errors (`-Werror -Wall -Wextra -Wpedantic` on GCC/
  Clang; `/W4 /WX` on MSVC). All warnings must be resolved, not suppressed.
- **Naming:** `snake_case` for everything. Public symbols are prefixed `spb_`;
  internal/static symbols have no prefix.
- **Includes:** always include every header you use directly; do not rely on
  transitive includes.
- **Error handling:** public API functions return `0` on success. Failures
  return a negative error code (not just `-1`; use the `spb_err` enum once it
  exists — see TODO.md §1b).
- **Memory:** no hidden allocations in the public API. If a function allocates,
  document it and pair it with an explicit free.
- **Logging:** no bare `printf`/`fprintf` in library code. Use the logging
  macro/level mechanism once it exists (TODO.md §1b). Examples may use
  `printf`.
- **Line endings:** LF only.
- **Trailing whitespace:** none.

---

## Commit style — Conventional Commits 1.0.0

This project follows [Conventional Commits 1.0.0](https://www.conventionalcommits.org/en/v1.0.0/).
The format is:

```
<type>[optional scope]: <description>

[optional body]

[optional footer(s)]
```

### Types

| Type | Use for | SemVer impact |
| --- | --- | --- |
| `feat` | New public API function, new user-visible behaviour | MINOR |
| `fix` | Bug fix in existing behaviour | PATCH |
| `refactor` | Internal restructuring with no behaviour change | none |
| `test` | Adding or fixing tests only | none |
| `docs` | README, ROADMAP, TODO, CONTRIBUTING, code comments | none |
| `build` | CMakeLists.txt, conanfile.txt, generated build files | none |
| `ci` | GitHub Actions workflows | none |
| `chore` | Housekeeping that fits none of the above | none |
| `perf` | Performance improvement with no API change | PATCH |
| `style` | Whitespace, formatting, missing semicolons — no logic change | none |
| `revert` | Reverting a previous commit (see footer rules below) | varies |

Use `!` after the type/scope and/or a `BREAKING CHANGE:` footer for breaking
changes — see [Breaking changes](#breaking-changes) below.

### Scopes

Scope is optional but recommended when the change is clearly localised.

| Scope | Covers |
| --- | --- |
| `transport` | `src/spb.c`, `src/spb.h` — the `spb_*` MQTT wrapper layer |
| `api` | Public API surface (types, function signatures in `spb.h`) |
| `examples` | Files under `examples/` |
| `build` | Build system (`CMakeLists.txt`, `conanfile.txt`) |
| `ci` | `.github/` workflows |
| `test` | Files under `tests/` |
| `docs` | Documentation files |
| `node` | Future `spb_node_*` Sparkplug layer |
| `payload` | Future protobuf/payload encoding |
| `namespace` | Future topic namespace build/parse |

### Description

- Imperative mood, present tense: "add", "fix", "remove" — not "added",
  "fixes", "removed".
- No capital first letter (the type prefix covers that).
- No trailing full stop.
- Keep under 72 characters total for the first line (type + scope +
  description).

### Body

- Separate from the description with a blank line.
- Use the body to explain *why*, not *what* (the diff shows what).
- Wrap at 72 characters.

### Footers

- One blank line after the body (or description if no body).
- `Refs: #<issue>` to reference an issue without closing it.
- `Closes: #<issue>` to close an issue on merge.
- `Reviewed-by: Name` for attribution.
- `BREAKING CHANGE: <description>` for breaking changes (see below).

### Breaking changes

A change is **breaking** if it:
- Removes or renames a public function or type from `spb.h`.
- Changes the signature of a public function (parameter types, order, or
  return type).
- Changes the documented semantics of a public function in a way that would
  require callers to update their code.
- Changes the binary ABI in a way that requires relinking.

Mark breaking changes **two ways** (both required for unambiguous tooling):

```
feat(api)!: rename spb_sendmessage to spb_publish

BREAKING CHANGE: spb_sendmessage has been renamed to spb_publish to align
with MQTT terminology. Update all call sites.
```

### Revert commits

Use the `revert` type and reference the SHA(s) being reverted in a footer:

```
revert: zero-initialise client in spb_init

Refs: a1b2c3d
```

### Examples

```
fix(transport): null-check response before dereferencing in success callbacks

Paho can pass NULL as the response argument to success/failure callbacks.
All six wrapper callbacks now guard before accessing response->token,
->code, and ->message.

Refs: #12
```

```
refactor(transport): introduce internal transport seam for unit testing

Wrap all MQTTAsync_* calls behind a thin paho_transport interface so CMock
can generate mocks. No behaviour change; enables broker-free unit tests.
```

```
test(transport): add unit tests for callback translation and NULL responses
```

```
build: add enable_testing and ctest integration for Unity test runner
```

```
docs: add CONTRIBUTING.md with commit style and dev setup
```

```
feat(api)!: add spb_err error enum; change return type from int to spb_err

BREAKING CHANGE: all public spb_* functions now return spb_err instead of
int. The values SPB_OK (0) and SPB_ERR_* (negative) are defined in spb.h.
Update return-value checks in all callers.
```

---

## Branch naming

```
<type>/<short-slug>
```

Use the same type vocabulary as commits. Examples:

```
fix/null-response-crash
feat/transport-seam
refactor/zero-init-client
test/unity-cmock-setup
docs/contributing
ci/github-actions-workflow
```

- Use hyphens, not underscores.
- Keep slugs short and descriptive (≤ 40 characters after the prefix).
- Branch from `main`.

---

## Pull requests

- **One logical change per PR.** If a PR touches both a bug fix and an
  unrelated refactor, split it.
- **Title:** follow the same Conventional Commits format as the commit
  description (the merge commit will use it).
- **Description:** explain what the PR does and why; link to related issues or
  TODO items.
- **Size:** keep PRs small and reviewable. Large PRs take longer to review and
  are harder to revert. If a feature requires a lot of scaffolding, land the
  scaffolding first as a separate PR.
- **Tests:** all new or changed behaviour must have corresponding tests. CI must
  pass (build + unit + integration) before merge.
- **Docs:** if the PR adds or changes public API, update `README.md` and any
  relevant doc files in the same PR.

---

## Versioning

This project uses [Semantic Versioning 2.0.0](https://semver.org/).

The library is pre-1.0.0 (`0.x.y`) while the API is stabilising. In this
phase, MINOR bumps may include breaking changes. A 1.0.0 release will be
tagged once the Phase 2 (Sparkplug baseline) is complete and the API is
considered stable.

Commit types map to SemVer as follows:

| Commit | SemVer bump |
| --- | --- |
| Any commit with `BREAKING CHANGE` | MAJOR (or MINOR during pre-1.0) |
| `feat` | MINOR |
| `fix`, `perf` | PATCH |
| `refactor`, `test`, `docs`, `build`, `ci`, `chore`, `style` | none |

---

## Issue referencing

- Reference issues in commit footers with `Refs: #N` (does not close) or
  `Closes: #N` / `Fixes: #N` (closes on merge to default branch).
- Do not embed issue references in the description line — use the footer.

---

## Contributor checklist

A quick reference for the rules above, in order of operations.

**Before making any changes:**
1. Read [`ROADMAP.md`](ROADMAP.md) to understand the current phase.
2. Read [`TODO.md`](TODO.md) and work on items from the highest-priority
   uncompleted section (Section 1 before Section 2 before Backlog).
3. Read the file(s) you are about to change before editing them.

**Commits:**
- Follow Conventional Commits 1.0.0 exactly as described above.
- Use the correct type and scope from the tables above; do not invent new
  scopes without documenting them here first.
- One logical change per commit. If a task naturally produces multiple logical
  changes, make multiple commits.
- Never commit secrets, credentials, or local path references.
- Do not amend commits that have already been pushed.

**Code changes:**
- C99 only; do not use compiler extensions.
- All changes must compile with `-Werror` on GCC/Clang.
- Do not add `printf`/`fprintf` to library code — use the logging mechanism.
- Keep the transport (`spb_*`) and Sparkplug (`spb_node_*`) layers separate.
- Any change to `src/` requires corresponding unit tests.

**Tests:**
- Unit tests must not make network connections.
- Integration tests use the local Mosquitto Docker container (see Testing
  section); do not reference `test.mosquitto.org`.

**Documentation:**
- If a PR adds or changes public API, update `README.md` API reference in the
  same change.
- If a TODO item is completed, tick the checkbox in `TODO.md` in the same
  commit or a follow-up `docs` commit.
- If new scopes are needed that aren't in the scope table above, add them to
  this file in the same PR.


