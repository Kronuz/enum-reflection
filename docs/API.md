# enum-reflection API reference

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
