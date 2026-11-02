# Architecture

The internal design of `enum-reflection`: a single header that turns an enum
declaration into a `constexpr` enum `<->` string mapping, built on two sibling
libraries. For usage see `README.md`; for the repo map and invariants see
`AGENTS.md`.

## Shape

One header, header-only, with two FetchContent'd dependencies (and two more
underneath them):

```
  phf.hh        compile-time minimal perfect hash   (dep: constexpr-phf)
  hashes.hh     hhl/hmix/fhhl/fhmix hashing macros   (dep: hashes
                  -> static-string, char-classify)
      ▲ (both included by)
  enum.h        ENUM_CLASS macro + constexpr accessors
```

`enum.h` includes `phf.hh` (the perfect hash that the generated lookups are built
on) and `hashes.hh` (the `hhl`/`hmix`/`fhhl`/`fhmix` macros that feed names and
values into that hash). Both come in through `FetchContent` at their branch tips;
`hashes` brings `static-string` and `char-classify` with it. Nothing else enters
except the C++ standard library (`<bit>`, `<cstddef>`, `<cstdint>`, `<cstring>`, `<optional>`, `<stdexcept>`, `<string_view>`, `<type_traits>`).

## How ENUM_CLASS works

`ENUM_CLASS(Enum, Underlying, ...)` does its work entirely in the preprocessor and
in `constexpr`, in three layers.

**The variadic map.** The bulk of `enum.h` (the long `_ENUM_M1` .. `_ENUM_M999`
chain and `ENUM_COUNT`) is a classic Better-Enums-style trick: count the
variadic members, then apply a per-member macro across all of them. This is what
lets one `ENUM_CLASS(...)` line expand into the enum body, a `values[]` array, a
`names[]` array, and the the `case` labels of the `enum_name` switch and the name table,
without the caller repeating the member list. `ENUM_FALLBACK` handles members
written with an explicit initializer (`Ok = 200`) so the name array stores `"Ok"`,
not `"Ok = 200"`; `_enum_name_length` trims at the first `= \t\n`.

**The two reflected tables.** Inside `namespace detail_Enum`, the macro
materializes two parallel `constexpr` arrays: `values[]` (each member as the
`Enum`) and `names[]` (each member's trimmed source name as a `string_view`).
Everything reflective is an index into these.

**The two perfect hashes.** Reflection needs two independent lookups, so the
macro bakes two `phf::make_phf` tables at compile time:

- a *value* hash, over `hmix(...)` of each member's underlying value, driving
  `enum_find(Enum)` and the `enum_name` switch (value -> name);
- a *name* hash, over `hhl(...)` (`wordwise_ci` from `hashes.hh`, a case-insensitive
  word-at-a-time hash) of each member name, driving `enum_find<Enum>(string_view)`
  and the name lookup (name -> value).

`enum_name` is a `switch` on the perfect-hash slot of the value, whose `case` labels
are the compile-time slots of the known members, returning the parallel `names[]`
entry. Because the hash is *perfect* over the known key set, each member gets a
distinct slot and the switch has no collisions; an unknown value falls to the
`default` and `enum_name` returns `""`. The value direction needs no comparison:
the integer mixer is a bijection, so two values never share a hash.

**The name lookup.** `enum_try_type` (which `enum_type` wraps, throwing on a miss)
hashes the input, asks the name `phf` for a slot, and compares the input with that
slot's entry before it answers. The slot is a candidate, not an answer: the name
hash is a chain of invertible steps, so a string that is not a member can be built
to share a member's hash, and the perfect hash sends every input, member or not, to
some slot. `detail_Enum::name_table` holds one `_enum_entry` per slot, built at
compile time: the member's value, the length and address of its name, and the name
lower-cased as words. An entry is 32 bytes, half a cache line.

The input is read once, with wide loads, lower-cased a word at a time (`A` to `Z`
become `a` to `z`, every other byte stays), and those words give both its hash and
the comparison. Which words depend on the length `n`:

| `n` | words of the input |
|---|---|
| 8 to 16 | the first eight bytes and the last eight (they overlap below 16) |
| 4 to 7 | the first four and the last four bytes, merged into the `n` bytes |
| 1 to 3 | the bytes at 0, `n / 2` and `n - 1`, merged into the `n` bytes |
| 17 to 24 | the first eight bytes, the next eight and the last eight, against the name itself |
| over 24 | eight bytes at a time against the name itself, the last eight overlapping, out of line |

Where the compiler has vector types, the two words of a name of 8 to 16 bytes are
lower-cased together in one vector register (`_enum_lower_pair`), and a name of 1 to 3
bytes byte by byte through `chars::tolower`'s table: a lookup is mostly integer
arithmetic, and these move work to the vector pipes and the load ports. All three
forms give the same words.

The lengths are compared first, then the words with the entry's, which are the
lower-cased name read the same way: two names are the same name exactly when their
lower-cased bytes are equal, and a byte that is not `A` to `Z` has to match as it
is. A slot that no member has maps to one more entry, which matches no input (its
first word is eight capitals, and an input's words are lower-cased), so the lookup
does not branch on whether the slot is a member's. A miss costs the hash, the slot
and one comparison.

`_enum_probe_input` computes `wordwise_ci::hash` from those words: the tail (the
last `n % 8` bytes) is already one of them, where `wordwise_ci` loops over it a
byte at a time through a table. `phf` was built from `wordwise_ci`, so the value
has to be the same for every name and for every other input except one kind:
`wordwise_ci` lower-cases a full eight-byte word with adds that carry from a byte of
`0xc1` or more into the next one, and the lookup lower-cases each byte alone. An
input with such a byte in a full word is not a member name, so it only has to be
rejected, and the comparison rejects it. In a constant expression the lookup uses
`wordwise_ci` and the byte loop `_enum_name_equal`, which is also the definition of a
match that the tests check against.

## Why this shape

The whole point is that the mapping is `constexpr`: `enum_name(Color::Red)` and
`enum_type<Color>("Red")` are usable in a `static_assert`, with no runtime
table-building and no per-call string scan. A perfect hash makes that affordable:
each lookup is one hash and one table row or `switch` case, where a linear scan or a runtime
`std::map` is neither `constexpr`-friendly nor branch-light. The two dependencies exist only
because the generated code reaches for them: `constexpr-phf` is the hash itself,
and `hashes` provides the hashing macros (`hmix` for the integer values, the
case-insensitive `hhl` for the names) plus the `find()`-composed `fhmix`/`fhhl`
that the `make_phf` initializers and `case` labels are written in terms of. Taking
them as sibling libraries keeps the dependency edges explicit: `enum-reflection`
declares what it uses and vendors no copies. Everything else is standard library and
`constexpr`.
