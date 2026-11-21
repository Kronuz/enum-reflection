# Source layout

How the library's source is laid out, and why. The rules are cpp-libraries'
`docs/SPLITTING.md`.

## Files

| File | Lines | What it holds |
|---|---:|---|
| `enum.h` | 1,726 | The whole library: the variadic map, the `ENUM_CLASS` and `ENUM` macros, and the constexpr accessors with the name lookup behind them. |

## Kept whole

`enum.h` stays one file:

1. The library's contract is a single header. `ARCHITECTURE.md` describes it as "a single
   header" and "one header, header-only"; `AGENTS.md` keeps its name stable so a consumer that
   includes `enum.h` only needs this directory on the include path. `SPLITTING.md` keeps a
   single-file library single-file unless the owner decides otherwise.
2. Most of its length is one table. Lines 33 to 1195 (1,163 lines) are the variadic map:
   `_ENUM_M1` to `_ENUM_M999`, each applying a macro to one member and calling the one below, and
   the `ENUM_COUNT` argument counter of 999 slots. The table is edited only as a whole, to change
   the member limit, and a table stays whole at any size.
3. The rest is about 530 lines that read as one unit, in this order:
   - lines 1197-1259: the empty-member test (`ENUM_FALLBACK`) and the per-member macros that
     expand into the value, name and hash lists;
   - lines 1261-1634: the name lookup, `_enum_entry` and `_enum_table` built at compile time, the
     run-time hash from the lower-cased words, and `_enum_lookup` with its medium and long paths;
   - lines 1636-1726: `_enum_value`, the accessor declarations, `_ENUM_IMPL`, `ENUM_CLASS`, `ENUM`,
     and the C form of `ENUM`.
4. `test/mutants.py` edits a copy of `enum.h` alone in a scratch directory and builds
   `test/names_test.cc` against it. The lookup code it mutates has to be in that one file, and a
   piece included from the copy would not resolve there.

A split, if the owner wants one, has one cut a maintainer recognizes: the variadic map (lines 33
to 1195) into `enum/map.h`, leaving about 560 lines in `enum.h`. It needs `test/mutants.py` to
copy the piece beside the header.
