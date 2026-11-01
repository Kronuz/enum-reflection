# AGENTS.md

Working notes for agents modifying this repository. For the design read
`ARCHITECTURE.md`; for usage read `README.md`. This file covers the repo layout,
how to build and test, the invariants you must not break, and the traps that are
easy to fall into.

## Repo map

```
enum.h                       ENUM_CLASS / ENUM macros + constexpr enum_name / enum_type / enum_find. Header.
test/test.cc                 Runnable smoke test: enum->string, string->enum, perfect-hash dispatch, unknown name, round-trip.
test/names_test.cc           Name lookup against a brute-force oracle: exhaustive short strings, case variants, random bytes, and strings forged to share a member's hash (CTest `enum_reflection_names`).
bench/bench.cc               Name lookups against an array by value, binary search, a linear scan, a sorted array and (when Abseil is found) absl::flat_hash_map; every answer is checked, near misses must be rejected (-DENUM_REFLECTION_BUILD_BENCH=ON; CTest `enum_reflection_bench_check` runs the checks with `--check`, without timing).
bench/gen_enums.py           Writes bench/enums.h: dense enums of 8, 32 and 128 members, a sparse one of 32, and their name tables.
bench/compile_time.cc        The accessors of those enums in one translation unit; compile_base.cc is the same without them (CMake targets enum_reflection_ct, enum_reflection_ct_base, excluded from `all`).
CMakeLists.txt               INTERFACE library `enum_reflection` (+ alias enum_reflection::enum_reflection); FetchContents constexpr-phf + hashes; CTest test `enum_reflection`.
LICENSE                      MIT, Copyright (c) 2026 Germán Méndez Bravo and (c) 2015-2020 Dubalu LLC (+ Anton Bachin, in the header).
README.md                    What it is, install, quick start.
docs/API.md                  ENUM_CLASS, ENUM, and the accessors.
docs/perf/lookup.md          Lookup cost against the alternatives and compile cost, with machines, compiler, commands and numbers.
ARCHITECTURE.md              How ENUM_CLASS works: the variadic map, the two reflected tables, the two perfect hashes.
```

`enum.h` is the only header here; everything else is header-only too (only the
test is compiled). The two dependencies,
[`constexpr-phf`](https://github.com/Kronuz/constexpr-phf) (the compile-time
perfect hash) and [`hashes`](https://github.com/Kronuz/hashes) (the
`hhl`/`hmix`/`fhhl`/`fhmix` hashing macros), are pulled in by `FetchContent` at
their branch tips and provide `phf.hh` and `hashes.hh` on the include path. `hashes`
transitively brings `static-string` and `char-classify`.

## Build and run the test

```sh
cmake -B build && cmake --build build && ctest --test-dir build
```

The first `cmake -B build` clones `constexpr-phf` and `hashes` (and, transitively,
`static-string` and `char-classify`) from GitHub at their branch tips. Expected output ends with `all enum-reflection tests passed`,
exit 0. The CMake `enum_reflection` target is an `INTERFACE` library that requests
`cxx_std_20`, links `constexpr_phf` and `hashes::hashes`, and exposes the source
dir as an include. The test target is `enum_reflection_test`; the registered
CTest name is `enum_reflection`.

To build against local checkouts of the deps instead of cloning, pass:

```sh
cmake -B build \
  -DFETCHCONTENT_SOURCE_DIR_CONSTEXPR_PHF=/path/to/constexpr-phf \
  -DFETCHCONTENT_SOURCE_DIR_HASHES=/path/to/hashes
```

## Conventions

- **C++20.** Required. Don't drop the target below it. (The accessors are
  `constexpr`; the value/name perfect hashes and `string_view` comparisons assume
  a C++20 baseline.)
- **The only includes are the C++ standard library and the two deps.** `enum.h`
  includes `<optional>`, `<stdexcept>`, `<string_view>`, `"phf.hh"`, and
  `"hashes.hh"`; the last two resolve via the FetchContent'd libraries. Do not add
  Xapiand headers (`THROW`, MsgPack, `strings::`, log macros, `config.h`).
- **Filename is stable.** The header keeps its original Xapiand name (`enum.h`) so
  a consumer that already `#include`s it just needs this repo on the include path.
  Don't rename it. The dep headers also keep their original filenames
  (`phf.hh`, `hashes.hh`) for the same reason; don't rename them in the
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
- **`enum_type` wraps `enum_try_type`.** The name switch is generated once, in
  `enum_try_type`, whose `default` returns `std::nullopt`; `enum_type` throws
  `std::out_of_range` on that `std::nullopt`. Keep one switch per enum.
- **A slot is a candidate, not an answer.** Each `case` of the name switch compares the
  input with the member's name (`_enum_name_equal`, ASCII case-insensitive) before it
  returns the value. The 64-bit name hash is invertible, so a string that is not a member
  can have a member's hash; without the comparison `enum_try_type` and `enum_type` accept
  it. `test/names_test.cc` forges such strings; keep the comparison and the fold in step
  with `wordwise_ci` (it folds `A` to `Z` only).
- **Both directions are independent perfect hashes.** `enum_name` keys on a hash
  of the underlying *value* (`hmix`), `enum_type` on a hash of the *name* (`hhl`,
  case-insensitive FNV-1a). Don't collapse them or assume a slot from one is valid
  in the other; the test checks each direction's slots are distinct on their own.
- **The value dispatch keys on the value, not the index.** Members may carry
  explicit, sparse initializers (`Ok = 200`). `enum_name` must map by underlying
  value; the test covers a non-contiguous, non-zero-based enum for exactly this.

## How to extend

- **Always extend the smoke test.** `test/test.cc` and `test/names_test.cc` are the
  executable checks. Prefer `static_assert` for anything that is `constexpr` (almost the whole API),
  so a regression is a compile error, not a silent runtime pass. When you add a
  shape the macros must handle (an empty member list, a member with an explicit
  value, a large enum), add an `ENUM_CLASS` for it and assert both directions.

## Traps

- **`enum_name` of an unknown value returns `""`; `enum_type` of an unknown name
  throws, and `enum_try_type` returns `std::nullopt`.** The `default` branches differ
  on purpose; don't "normalize" them. The test pins each behavior.
- **The member-count ceiling is 999.** The `_ENUM_M*` / `ENUM_COUNT` chain tops
  out at 999 members. That is the upstream limit; an enum past it won't expand.
- **The `-Wgnu-zero-variadic-macro-arguments` pragma is load-bearing.** `enum.h`
  silences it at the top because the variadic-map machinery relies on the GNU
  zero-args extension. Don't remove it.
- **A consumer that also FetchContents the deps should declare them first.**
  `FetchContent` uses the first declaration of each name, so if Xapiand (or any
  consumer) declares `constexpr_phf` / `hashes` (or `static_string` /
  `char_classify`) before pulling `enum_reflection`, those win and dedup; make
  sure the SHAs are compatible.

## Standalone vs. Xapiand

This is a standalone extraction from
[Xapiand](https://github.com/Kronuz/Xapiand). `enum.h` differs from the Xapiand
header in two ways: its `#include "phf.hh"` and `#include "hashes.hh"` resolve to
the two sibling libraries (FetchContent'd) instead of Xapiand's in-tree headers, and
it adds `enum_try_type`, with `enum_type` defined in terms of it.

Keep extraction wiring and behavior changes in separate commits so each can be
reconciled with upstream.
