# enum-reflection

Part of [Kronuz C++ Libraries](https://github.com/Kronuz/cpp-libraries), a collection of small, reusable C++ libraries.

[![CI](https://github.com/Kronuz/enum-reflection/actions/workflows/ci.yml/badge.svg)](https://github.com/Kronuz/enum-reflection/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
![Header-only](https://img.shields.io/badge/header--only-yes-green.svg)

Header-only reflective enums for C++20: one macro defines an enum class and constexpr perfect-hash lookups both ways.
`ENUM_CLASS` declares a real `enum class` and, from the same member list, a
`switch` on a compile-time perfect hash of each value and of each name, so
`enum_name` and `enum_type` are `constexpr` and build no table at run time.
Reach for it when enums appear in text: configuration, protocols, logs and
command lines.

## ✨ Highlights

- **A real `enum class`.** With its underlying type and explicit values, dense or
  sparse.
- **Both directions.** `enum_name(Status::NotFound)` is `"NotFound"`;
  `enum_type<Status>("Teapot")` is `Status::Teapot`.
- **`constexpr` and allocation-free.** Lookups work in `static_assert`.
- **Perfect-hash dispatch.** Through [constexpr-phf](https://github.com/Kronuz/constexpr-phf)
  and [hashes](https://github.com/Kronuz/hashes); a hash hit is confirmed against
  the member's name, so a string that collides with a member's hash is rejected.
- **Misses your way.** `enum_type` throws `std::out_of_range`; `enum_try_type`
  returns `std::nullopt` from the same `switch`.
- **Case-insensitive names.** A name matches a member when the two are equal
  ignoring ASCII case.

## 🚀 Quick start

```cpp
#include "enum.h"

#include <cstdint>
#include <cstdio>

ENUM_CLASS(Status, std::uint16_t,
	Ok = 200,
	NotFound = 404,
	Teapot = 418
)

static_assert(enum_name(Status::NotFound) == "NotFound");       // value to name, at compile time
static_assert(enum_type<Status>("Teapot") == Status::Teapot);   // name to value

int main() {
	auto s = enum_try_type<Status>("notfound");                  // names match ignoring ASCII case
	std::printf("%d\n", s ? static_cast<int>(*s) : -1);          // 404
	std::printf("%d\n", enum_try_type<Status>("Gone").has_value());   // 0: a miss, no exception
	std::printf("[%.*s]\n", int(enum_name(static_cast<Status>(500)).size()),
		enum_name(static_cast<Status>(500)).data());             // []: no member has the value 500
}
```

[`examples/demo.cc`](examples/demo.cc) walks every member of an enum without
naming one.

## 📦 Installation

Requires a C++20 compiler and CMake 3.14 or newer; CMake fetches
[constexpr-phf](https://github.com/Kronuz/constexpr-phf) and
[hashes](https://github.com/Kronuz/hashes), which bring static-string and
char-classify. CI builds it on Linux (x86_64, arm64), macOS (arm64, x86_64), and
FreeBSD, plus ASan, UBSan, and TSan builds.

```cmake
include(FetchContent)
FetchContent_Declare(enum_reflection
  GIT_REPOSITORY https://github.com/Kronuz/enum-reflection.git
  GIT_TAG        90e837e9c6cac407aea406879cfa7ce0d2e33905)  # or main for the tip
FetchContent_MakeAvailable(enum_reflection)

target_link_libraries(my_app PRIVATE enum_reflection::enum_reflection)
```

```cpp
#include "enum.h"
```

## 🎯 When to use it

Use it for enums you print, parse or read from configuration, instead of a name
table kept by hand beside the enum.

Not for:

- **Enums declared elsewhere.** The macro declares the enum; it cannot reflect an
  existing one.
- **Very large enums.** Each name costs about 25 ms of compile time on an Apple M4
  Pro (75 ms on the i9) in every translation unit that uses the accessors, and the
  macros stop at 999 members.
- **Expected misses through `enum_type`.** A miss that throws costs several hundred
  times a hit; call `enum_try_type` for user input.
- **One answer for every miss.** An unknown value gives `enum_name` an empty
  string, while an unknown name makes `enum_type` throw.

## ⚡ Performance

`bench/bench.cc` checks every row's answer, then times lookups cycling through
the names in a shuffled order: the median of nine runs of about a million
lookups, ns per lookup, enum-reflection / the comparator. Apple clang 17, libc++,
Release, abseil from Homebrew (20260817.0 on the M4 Pro):

| lookup | comparator | Apple M4 Pro | Intel Core i9-9980HK |
|---|---|---:|---:|
| `enum_type` hit, 32 members | `absl::flat_hash_map` | 4.8 / 4.5 | 12.7 / 12.1 |
| `enum_type` hit, 32 members | linear scan | 4.8 / 17.6 | 12.7 / 43.0 |
| `enum_type` hit, 128 members | `absl::flat_hash_map` | 8.4 / 4.8 | 20.0 / 13.5 |
| `enum_try_type` miss, 128 members | `absl::flat_hash_map` | 3.2 / 2.4 | 9.9 / 6.0 |
| `enum_name`, 32 sparse members | binary search by value | 1.5 / 3.7 | 3.7 / 3.9 |

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DENUM_REFLECTION_BUILD_BENCH=ON
cmake --build build --target enum_reflection_bench
build/enum_reflection_bench
```

Up to 32 members a hit costs what the hash map's does; at 128 the map is 1.5x to
1.7x faster, and it answers misses faster at every size, but it allocates and is
built at run time. A linear scan wins among 8 names.
[docs/perf/lookup.md](docs/perf/lookup.md) has every size, `enum_name` against
an array by value, and the compile cost.

## 📚 Documentation

- [docs/API.md](docs/API.md): `ENUM_CLASS`, `ENUM`, and the accessors.
- [ARCHITECTURE.md](ARCHITECTURE.md): the preprocessor map and the two perfect
  hashes.
- [docs/perf/lookup.md](docs/perf/lookup.md): lookup and compile cost against the
  alternatives.
- [CHANGELOG.md](CHANGELOG.md): what changed, with numbers.
- [AGENTS.md](AGENTS.md): repository map, invariants, and traps.
- [Kronuz C++ Libraries](https://github.com/Kronuz/cpp-libraries/blob/main/AGENTS.md#enum-reflection): the catalog entry.

## 🛠️ Building and testing

```sh
cmake -B build
cmake --build build
ctest --test-dir build
```

`./build/enum_reflection_demo` runs the demo, and `enum_reflection_names` tests
forged hash collisions. Add `-DENUM_REFLECTION_BUILD_BENCH=ON` to build
`enum_reflection_bench`; `ctest` then runs its `--check` mode, and with abseil
installed it adds the hash map rows.

## 🤝 Contributing

Issues and pull requests are welcome. Read [AGENTS.md](AGENTS.md) and the
catalog's [rules of engagement](https://github.com/Kronuz/cpp-libraries/blob/main/AGENTS.md#rules-of-engagement)
first; CI must pass on every platform.

## 📄 License

MIT, Copyright (c) 2026 Germán Méndez Bravo and Copyright (c) 2015-2020 Dubalu
LLC; `enum.h` carries portions Copyright (c) 2012-2019 Anton Bachin. See
[LICENSE](LICENSE). Extracted from [Xapiand](https://github.com/Kronuz/Xapiand).
