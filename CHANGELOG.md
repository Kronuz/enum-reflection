# Changelog

Notable changes to enum-reflection, newest first. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); entries are dated because the
library has no releases.

## 2026-10-31

### Fixed

- `enum_type<E>(name)` and `enum_try_type<E>(name)` verify the string before they answer.
  The name switch keyed on a 64-bit hash alone, the hash is a chain of invertible steps,
  and a string that is not a member could be built to share a member's hash (a 15-byte
  string was enough), so the lookup returned that enumerator. Each `case` now compares the
  input with the member's name, ASCII case-insensitively as before, exact bytes first; a
  forged name misses. `test/names_test.cc` (CTest `enum_reflection_names`) checks every
  lookup against a brute-force oracle: all short strings, case variants, random bytes, and
  strings forged to collide with each member, with and without a prefix.
- The documentation said a throwing miss costs about a thousand times a hit. Measured it
  is 620 to 830 times (2.0 us against 2.6 ns on the Apple M4 Pro, 3.7 us against 5.9 ns on
  the Intel i9-9980HK), and about 500 to 770 times after the check above.

### Measured

The cost of the check, `bench/bench.cc` (`-DENUM_REFLECTION_BENCH=ON`, `-O3`, `quiet strict`,
before and after on the same machine):

| Median per lookup, before to after | Apple M4 Pro | Intel i9-9980HK |
|---|---:|---:|
| Hit, exact case | 2.5-2.6 to 3.9-4.0 ns | 5.9-7.0 to 9.3-10.1 ns |
| Hit, lower case | 2.7-2.8 to 4.6-5.7 ns | 6.4-6.6 to 12.1-17.2 ns |
| Miss through `enum_try_type` | 2.7-2.9 to 3.4-3.8 ns | 6.4-6.9 to 7.9-8.8 ns |
| Miss through `enum_type` (throws) | 2.0-2.1 us, unchanged | 3.7-4.0 us, unchanged |
| Compile time, 64 members, `-O2 -c` | 1.25-1.29 s to 1.25-1.27 s | 3.77-3.82 s to 3.77-3.79 s |
| Object size, 64 members | 17.5 KB to 22.8 KB | 16.3 KB to 20.5 KB |

### Added

- `bench/bench.cc`, built with `-DENUM_REFLECTION_BENCH=ON`: hit, lower-case hit and miss
  through `enum_type` and `enum_try_type`, and the throwing miss.

## 2026-10-30

### Added

- `enum_try_type<E>(name)` returns `std::optional<E>`, `std::nullopt` on a miss, from
  the same perfect-hash switch as `enum_type`. It is `constexpr` like `enum_type`.
  A miss through `enum_type` throws `std::out_of_range`, which costs about a thousand
  times a hit. (`7d394c5`)

### Changed

- `enum_type` is defined in terms of `enum_try_type`, so no extra switch is generated
  per enum; its behavior is unchanged. `enum.h` includes `<optional>` and
  `<stdexcept>` directly. Before, `<stdexcept>` arrived transitively through
  `phf.hh`. (`7d394c5`)
