# enum-reflection

A small, header-only **reflective enum** library for C++20, extracted from
[Xapiand](https://github.com/Kronuz/Xapiand).

## What it is

One header, `enum.h`: the `ENUM_CLASS` macro defines a strongly-typed
`enum class` and, alongside it, a set of `constexpr` accessors that reflect it —
turn a member into its source-code name, look a name back up to its member, and
do both through a compile-time perfect hash, so the lookups are branch-light and
heap-free with no runtime table-building. Everything is `constexpr`: an enum
`<->` string mapping you can use in a `static_assert`.

It builds on two already-extracted standalone libraries —
[`perfect-hash`](https://github.com/Kronuz/perfect-hash) (the compile-time
minimal perfect hash that powers the dispatch) and
[`hashes`](https://github.com/Kronuz/hashes) (the `hhl`/`hmix`/`fhhl`/`fhmix`
hashing macros the generated switch tables are built on) — pulled in by
`FetchContent` at pinned SHAs. `hashes` in turn pulls
[`static-string`](https://github.com/Kronuz/static-string) and
[`char-classify`](https://github.com/Kronuz/char-classify), so the whole stack
comes from one `FetchContent_Declare`.

## Install

CMake with `FetchContent`:

```cmake
include(FetchContent)
FetchContent_Declare(
  enum_reflection
  GIT_REPOSITORY https://github.com/Kronuz/enum-reflection.git
  GIT_TAG        main
)
FetchContent_MakeAvailable(enum_reflection)

target_link_libraries(your_target PRIVATE enum_reflection::enum_reflection)
```

`enum_reflection` itself `FetchContent`s `perfect-hash` and `hashes` (which pulls
`static-string` and `char-classify`) at the exact SHAs it was extracted against,
so you get the whole stack from one `FetchContent_Declare`. The
`enum_reflection` target is a pure `INTERFACE` library: it compiles nothing,
requests `cxx_std_20`, and puts `enum.h` (and the dependency headers) on your
include path. Then:

```cpp
#include "enum.h"  // ENUM_CLASS, enum_name, enum_type, enum_find
```

Requires C++20. On macOS it builds with AppleClang/libc++, the same toolchain
Xapiand uses. The header keeps its original filename, so a codebase that already
`#include "enum.h"` just needs this repo on its include path.

If you already pull `perfect-hash` / `hashes` (or their deps) into your own build
via `FetchContent`, declare them first — CMake's `FetchContent` uses the first
declaration of each name, so they dedup and `enum_reflection` reuses them rather
than fetching its pinned copies.

## Usage

Define a reflected enum with `ENUM_CLASS(Name, Underlying, members...)`:

```cpp
#include "enum.h"

ENUM_CLASS(Color, std::uint8_t,
    Red,
    Green,
    Blue
)

// enum -> string
static_assert(enum_name(Color::Green) == "Green");

// string -> enum
static_assert(enum_type<Color>("Blue") == Color::Blue);

// round-trips, all at compile time
static_assert(enum_type<Color>(enum_name(Color::Red)) == Color::Red);
```

`ENUM_CLASS(Enum, Underlying, ...)` expands to a real `enum class Enum :
Underlying` plus these free functions, all `constexpr`:

- **`enum_name(Enum e) -> std::string_view`** — the member's source name, or `""`
  if `e` is not a defined member.
- **`enum_type<Enum>(std::string_view name) -> Enum`** — the member with that
  name; throws `std::out_of_range` if no member matches.
- **`enum_find(Enum e)`** / **`enum_find<Enum>(std::string_view name)`** — the raw
  perfect-hash slot for a value / a name. These are what `enum_name` and
  `enum_type` switch on; rarely needed directly.

Members may carry explicit values (`Ok = 200, NotFound = 404, ...`); the value
dispatch keys on the underlying value, not the declaration order, and a sparse,
non-zero-based value set perfect-hashes fine. The companion `ENUM(Name, ...)`
macro defines a C-style `enum` with the same accessors.

## Build & test

```sh
cmake -B build && cmake --build build && ctest --test-dir build
```

The first `cmake -B build` clones `perfect-hash` and `hashes` (and, transitively,
`static-string` and `char-classify`) from GitHub at the pinned SHAs in
`CMakeLists.txt`. The test defines two `ENUM_CLASS`es and exercises `enum_name`
(enum->string), `enum_type` (string->enum), the perfect-hash dispatch, the
unknown-name case (throws), and a full round-trip — most of it as `static_assert`,
so a regression is a compile error. It prints `all enum-reflection tests passed`
and exits 0.

## Examples

[`examples/demo.cc`](examples/demo.cc) is a runnable tour. A top-level CMake build
produces it next to the test:

```sh
cmake -B build && cmake --build build && ./build/enum_reflection_demo
```

It defines a `Color` enum class and an HTTP-status-shaped `Status` (explicit,
gap-y values), then maps members to their source names with `enum_name`, resolves
names back to typed members with `enum_type` (including off a runtime
`string_view`), and walks every enumerator through the reflected
`detail_<Enum>::values[]` table without naming a member. It then shows the
mapping is `constexpr` (a handful of `static_assert`s the compiler already
proved), round-trips both directions to show the conversion is lossless, and
ends on the unmatched cases: `enum_name` of an undefined value returns `""`, and
`enum_type` of an unknown name throws `std::out_of_range`.

## Provenance

Extracted from [Xapiand](https://github.com/Kronuz/Xapiand). `enum.h` is verbatim:
its only includes are `<string_view>`, `"phf.hh"`, and `"hashes.hh"`, and the
standalone delta is pure wiring — `FetchContent` resolves those two includes to
the sibling libraries instead of Xapiand's in-tree headers. The macro and
accessor logic is unchanged. See [ARCHITECTURE.md](ARCHITECTURE.md) for the design
and [AGENTS.md](AGENTS.md) for the repo map and invariants.

## License

MIT, Copyright (c) 2015-2019 Dubalu LLC. `enum.h` carries portions Copyright (c)
2012-2019 Anton Bachin (the Better Enums-style preprocessor map). See
[LICENSE](LICENSE) and the per-file header.
