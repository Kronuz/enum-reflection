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

`enum.h` includes `phf.hh` (the perfect hash that the generated lookups switch
on) and `hashes.hh` (the `hhl`/`hmix`/`fhhl`/`fhmix` macros that feed names and
values into that hash). Both come in through `FetchContent` at their branch tips;
`hashes` brings `static-string` and `char-classify` with it. Nothing else enters
except the C++ standard library (`<optional>`, `<stdexcept>`, `<string_view>`).

## How ENUM_CLASS works

`ENUM_CLASS(Enum, Underlying, ...)` does its work entirely in the preprocessor and
in `constexpr`, in three layers.

**The variadic map.** The bulk of `enum.h` (the long `_ENUM_M1` .. `_ENUM_M999`
chain and `ENUM_COUNT`) is a classic Better-Enums-style trick: count the
variadic members, then apply a per-member macro across all of them. This is what
lets one `ENUM_CLASS(...)` line expand into the enum body, a `values[]` array, a
`names[]` array, and the per-member `case` labels of the lookup switches,
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
- a *name* hash, over `hhl(...)` (a case-insensitive FNV-1a from `hashes.hh`) of
  each member name, driving `enum_find<Enum>(string_view)` and the `enum_type`
  switch (name -> value).

`enum_name` and `enum_try_type` (which `enum_type` wraps, throwing on a miss) are
then just a `switch` on the perfect-hash slot
whose `case` labels are the compile-time hashes of the known members, returning
the parallel array entry. Because the hash is *perfect* over the known key set,
each member gets a distinct slot and the switch has no collisions; an unknown
input falls to the `default`: `enum_name` returns `""`, `enum_try_type` returns
`std::nullopt`, `enum_type` throws `std::out_of_range`.

The name direction compares before it answers. The slot comes from a 64-bit hash,
and a hash does not prove equality: the name hash is a chain of invertible steps,
so a string that is not a member can be built to share a member's hash. Each
`case` of the name switch therefore compares the input with the member's name
(`_enum_name_equal`: the exact bytes first, then with `A` to `Z` folded and every other byte compared as is) and
returns `std::nullopt` on a mismatch. A hit costs one extra comparison of
`length` bytes; a miss that never reaches a member's slot costs nothing extra.
The value direction needs no comparison: the integer mixer is a bijection, so two
values never share a hash.

## Why this shape

The whole point is that the mapping is `constexpr`: `enum_name(Color::Red)` and
`enum_type<Color>("Red")` are usable in a `static_assert`, with no runtime
table-building and no per-call string scan. A perfect hash makes that affordable:
each lookup is one hash and one `switch`, where a linear scan or a runtime
`std::map` is neither `constexpr`-friendly nor branch-light. The two dependencies exist only
because the generated code reaches for them: `constexpr-phf` is the hash itself,
and `hashes` provides the hashing macros (`hmix` for the integer values, the
case-insensitive `hhl` for the names) plus the `find()`-composed `fhmix`/`fhhl`
that the `make_phf` initializers and `case` labels are written in terms of. Taking
them as sibling libraries keeps the dependency edges explicit: `enum-reflection`
declares what it uses and vendors no copies. Everything else is standard library and
`constexpr`.
