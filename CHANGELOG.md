# Changelog

Notable changes to enum-reflection, newest first. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); entries are dated because the
library has no releases.

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
