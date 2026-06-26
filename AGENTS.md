# AGENTS.md

Working notes for agents modifying this repository. For the design read
`ARCHITECTURE.md`; for usage read `README.md`. This file covers the repo layout,
how to build and test, the invariants you must not break, and the traps that are
easy to fall into.

## Repo map

```
enum.h                       ENUM_CLASS / ENUM macros + constexpr enum_name / enum_type / enum_find. Header.
test/test.cc                 Runnable smoke test: enum->string, string->enum, perfect-hash dispatch, unknown name, round-trip.
CMakeLists.txt               INTERFACE library `enum_reflection` (+ alias enum_reflection::enum_reflection); FetchContents perfect-hash + hashes; CTest test `enum_reflection`.
LICENSE                      MIT, Copyright (c) 2015-2019 Dubalu LLC (+ Anton Bachin, in the header).
README.md                    What it is, install, usage, the API.
ARCHITECTURE.md              How ENUM_CLASS works: the variadic map, the two reflected tables, the two perfect hashes.
```

`enum.h` is the only header here; everything else is header-only too (only the
test is compiled). The two dependencies —
[`perfect-hash`](https://github.com/Kronuz/perfect-hash) (the compile-time
perfect hash) and [`hashes`](https://github.com/Kronuz/hashes) (the
`hhl`/`hmix`/`fhhl`/`fhmix` hashing macros) — are pulled in by `FetchContent` at
pinned SHAs and provide `phf.hh` and `hashes.hh` on the include path. `hashes`
transitively brings `static-string` and `char-classify`.

## Build and run the test

```sh
cmake -B build && cmake --build build && ctest --test-dir build
```

The first `cmake -B build` clones `perfect-hash` and `hashes` (and, transitively,
`static-string` and `char-classify`) from GitHub at the pinned SHAs in
`CMakeLists.txt`. Expected output ends with `all enum-reflection tests passed`,
exit 0. The CMake `enum_reflection` target is an `INTERFACE` library that requests
`cxx_std_20`, links `perfect_hash` and `hashes::hashes`, and exposes the source
dir as an include. The test target is `enum_reflection_test`; the registered
CTest name is `enum_reflection`.

To build against local checkouts of the deps instead of cloning, pass:

```sh
cmake -B build \
  -DFETCHCONTENT_SOURCE_DIR_PERFECT_HASH=/path/to/perfect-hash \
  -DFETCHCONTENT_SOURCE_DIR_HASHES=/path/to/hashes
```

## Conventions

- **C++20.** Required. Don't drop the target below it. (The accessors are
  `constexpr`; the value/name perfect hashes and `string_view` comparisons assume
  a C++20 baseline.)
- **The only includes are the C++ standard library and the two deps.** `enum.h`
  does `#include <string_view>`, `#include "phf.hh"`, and `#include "hashes.hh"`;
  the latter two resolve via the FetchContent'd libraries. Do not add Xapiand
  headers (no `THROW`, MsgPack, `strings::`, log macros, `config.h`) back — there
  were none to begin with.
- **Filename is stable.** The header keeps its original Xapiand name (`enum.h`) so
  a consumer that already `#include`s it just needs this repo on the include path.
  Don't rename it. The dep headers also keep their original filenames
  (`phf.hh`, `hashes.hh`) for the same reason — don't rename them in the
  `#include` lines.
- Tabs for indentation, double quotes in code, no em dashes in prose.

## Load-bearing invariants

- **The two dep includes must keep resolving.** `enum.h` relies on `phf.hh` (the
  perfect hash that `enum_find` / `enum_name` / `enum_type` switch on) and
  `hashes.hh` (the `hhl`/`hmix`/`fhhl`/`fhmix` macros the `make_phf` initializers
  and `case` labels are written in terms of). The `CMakeLists.txt` FetchContents
  both and links their targets so the headers are on the include path. If you bump
  a dep SHA, keep both the `FetchContent_Declare` and the `target_link_libraries`
  line.
- **`std::out_of_range` arrives transitively.** `enum_type`'s `default` branch
  throws `std::out_of_range`, but `enum.h` does not `#include <stdexcept>` — it
  rides in through `phf.hh`. The test includes `<stdexcept>` itself so it never
  depends on that transitively. If you ever drop the `phf.hh` include, add
  `<stdexcept>` to `enum.h`.
- **Both directions are independent perfect hashes.** `enum_name` keys on a hash
  of the underlying *value* (`hmix`), `enum_type` on a hash of the *name* (`hhl`,
  case-insensitive FNV-1a). Don't collapse them or assume a slot from one is valid
  in the other — the test checks each direction's slots are distinct on their own.
- **The value dispatch keys on the value, not the index.** Members may carry
  explicit, sparse initializers (`Ok = 200`). `enum_name` must map by underlying
  value; the test covers a non-contiguous, non-zero-based enum for exactly this.

## How to extend

- **Always extend the smoke test.** `test/test.cc` is the only executable check.
  Prefer `static_assert` for anything that is `constexpr` (almost the whole API),
  so a regression is a compile error, not a silent runtime pass. When you add a
  shape the macros must handle (an empty member list, a member with an explicit
  value, a large enum), add an `ENUM_CLASS` for it and assert both directions.

## Traps

- **`enum_name` of an unknown value returns `""`, but `enum_type` of an unknown
  name throws.** The two `default` branches differ on purpose — don't "normalize"
  them. The test pins both behaviors.
- **The member-count ceiling is 999.** The `_ENUM_M*` / `ENUM_COUNT` chain tops
  out at 999 members. That is the upstream limit; an enum past it won't expand.
- **The `-Wgnu-zero-variadic-macro-arguments` pragma is load-bearing.** `enum.h`
  silences it at the top because the variadic-map machinery relies on the GNU
  zero-args extension. Don't remove it.
- **A consumer that also FetchContents the deps should declare them first.**
  `FetchContent` uses the first declaration of each name, so if Xapiand (or any
  consumer) declares `perfect_hash` / `hashes` (or `static_string` /
  `char_classify`) before pulling `enum_reflection`, those win and dedup — make
  sure the SHAs are compatible.

## Standalone vs. Xapiand

This is a standalone extraction from
[Xapiand](https://github.com/Kronuz/Xapiand). The delta from the original is pure
wiring: `enum.h` is byte-for-byte the Xapiand header. Its `#include "phf.hh"` and
`#include "hashes.hh"` now resolve to the two sibling libraries (FetchContent'd)
instead of Xapiand's in-tree headers. No macro or accessor logic changed.

Keep extraction hygiene separate from any future behavior changes so they can be
reconciled with upstream.
