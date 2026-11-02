# Changelog

Notable changes to enum-reflection, newest first. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); entries are dated because the
library has no releases.

## 2026-11-01

### Changed

- The name lookup (`enum_type<E>(name)`, `enum_try_type<E>(name)`) keeps its check against the
  member's name and costs far less. The `switch` that compared the name inside every `case` (a jump
  table and an indirect branch per lookup, 2.0x to 2.1x START's time on a 128-member enum) is a table
  with one 32-byte entry per hash slot, built at compile time. A name of up to 16 bytes is read once,
  with wide loads, and lower-cased a word at a time; the same words give its hash and are compared
  with the entry, which holds the member's name lower-cased the same way. Names of 17 to 24 bytes
  compare three words against the name itself, longer ones eight bytes at a time out of line. The
  lookup computes `wordwise_ci`'s hash from those words, so the tail of a name is not hashed byte by
  byte through a table any more. The hash value is the same for every name and for every input whose
  full eight-byte words have no byte of 0xc1 or more (an input with one hashes differently and is
  rejected by the comparison, as it was).
- A slot no member has maps to one more entry that matches no input, so the lookup does not branch on
  whether the slot is a member's (that branch is unpredictable on a stream of misses).
- The two words of a name of 8 to 16 bytes are lower-cased together in a vector register where the
  compiler has vector types, and on arm64 a name of 1 to 3 bytes is lower-cased through
  `chars::tolower`'s table: a lookup is mostly integer arithmetic, and these move work to the vector
  pipes and the load ports.
- `enum_type` and `enum_try_type` are forced inline, so a lookup is not a call with a prologue.
- Compile time is halved: the per-`case` `enum_find<E>(names[i])` rebuilt the name table for every
  member, and the table is now built once (four enums, 200 names, `-O2 -c`: 10.5 s to 5.0 s on the M4,
  14.5 s to 5.1 s on the i9; a 300-member enum on clang 12: 183 s to 50 s).
- `enum.h` includes `<bit>`, `<cstddef>`, `<cstdint>`, `<cstring>` and `<type_traits>`.
- The CMake option that builds the benchmark is `ENUM_REFLECTION_BUILD_BENCH` (it was
  `ENUM_REFLECTION_BENCH`). CI builds the benchmark target and runs the mutation check.

### Measured

`bench/bench.cc`, the gate's START (`79fe3fa`, the last commit before 2026-10-30) against the check as it
was (`610203f`) and as it is now, `-O3`, Apple clang 17, three interleaved rounds in one `quiet strict`
hold per machine, START / now (above 1 is faster than START; the A/A copy of START reads 0.96 to 1.03):

| row | M4 before | M4 now | i9 before | i9 now |
| --- | ---: | ---: | ---: | ---: |
| `Method` hit | 0.69 | 1.35 | 0.60 | 1.10 |
| `Status` hit | 0.65 | 1.09 | 0.62 | 1.09 |
| 8 dense hit | 0.74 | 0.94 | 0.73 | 1.09 |
| 32 dense hit | 0.76 | 1.02 | 0.79 | 1.01 |
| 128 dense hit | 0.48 | 1.13 | 0.50 | 1.28 |
| 32 sparse hit | 0.77 | 1.02 | 0.69 | 1.02 |
| geomean of the 16 gate rows | 0.86 | 1.14 | 0.85 | 1.12 |

The rows that are left below START: the 8-member hit on the M4 reads 0.94 (3.12 ns, 3.35 now;
the START binary turns the table into the identity there, so it loads nothing; issue #1). The two throwing
misses of the small enums (`Method`, `Status`) read 1.00 and 0.99 on the M4 and 0.93 and 0.94 on the
i9: a throw costs 2 to 4 microseconds, almost all of it the unwinder's, and the rows read 0.97
and 0.99 with the build before the vector lower-casing; the sized rows (8 to 128 members) are 1.1x to
1.8x faster. The i9's `enum_name` row at 128 members reads 0.90 in this binary and 0.93 and 0.89 in
the HEAD and unverified ones: that code is unchanged and the difference is the binary's layout. A miss
through `enum_try_type` costs what a hit does (2.8 to 3.0 ns on the M4 at 8 to 128 members, 5.9 to
6.3 ns on the i9), against 2.9 to 3.7 and 8.1 to 9.3 before.

### Added

- `test/names_test.cc` covers the pieces: every byte value in every lane of the lower-casing (the
  integer, the vector and the table forms), the hash against `wordwise_ci` for every size and byte
  value, the words of every entry, the entry no slot owns, and a lookup of inputs made to land on a
  chosen entry (`synthetic`), which reaches every comparison however the hash falls.
  `names_test SECTION` runs one section.
- `test/mutants.py` (CTest `enum_reflection_mutants`, in CI) edits a copy of `enum.h` 23 ways (the
  length check, each word, the lower-casing, the entry no slot owns, the hash, the verification
  itself) and requires the section meant to catch each to fail. With the verification removed the
  forged-collision section fails.

- `bench/bench.cc` times [magic_enum](https://github.com/Neargye/magic_enum) v0.9.8
  (`enum_cast`, `enum_name`) beside the lookups, fetched and built only with
  `-DENUM_REFLECTION_BENCH_MAGIC_ENUM=ON`, and checks its answers, near misses included, before
  timing.
- `docs/perf/lookup.md`: what a lookup costs against an array by value, a binary search, a
  linear scan, a sorted array and `absl::flat_hash_map` for enums of 8, 32 and 128 members and
  a sparse one of 32, on an Apple M4 Pro and an Intel i9-9980HK, and what the generated
  tables cost to compile. A hit costs the same as the hash map's up to 32 members, and 1.5x
  to 1.7x more at 128; a miss costs 1.5x to 2x more at every size.
- `bench/bench.cc` runs those comparisons, checks every answer, and exits non-zero when a near
  miss (a member's name with one letter changed) is accepted. With `--check` it runs the
  checks alone, which is CTest `enum_reflection_bench_check` when the benchmark is built. `bench/gen_enums.py` writes the
  enums it uses; `bench/compile_time.cc` and `bench/compile_base.cc` time the compile cost
  (CMake targets `enum_reflection_ct` and `enum_reflection_ct_base`, outside `all`).

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
