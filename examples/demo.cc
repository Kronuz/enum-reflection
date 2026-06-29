// A runnable tour of enum-reflection.
//
// Build (when this repo is the top-level project):
//   cmake -B build && cmake --build build && ./build/enum_reflection_demo
//
// The one idea worth taking away: ENUM_CLASS gives you a real `enum class` plus
// a constexpr enum <-> string mapping in the same line, with no runtime table
// built and no per-call string scan. The name <-> value lookups go through a
// compile-time perfect hash, so they are usable in a static_assert. This demo
// turns members into their names, turns names back into members, walks the
// enumerators of an enum it was never told the shape of, and shows the two
// constexpr lookups resolving at compile time.
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string_view>

#include "enum.h"  // ENUM_CLASS, enum_name, enum_type, enum_find

static void rule(const char* title) {
	std::printf("\n\033[1m── %s ──\033[0m\n", title);
}

// A reflected enum class over uint8_t. ENUM_CLASS emits the `enum class` body
// plus enum_name / enum_type / enum_find for it, all constexpr.
ENUM_CLASS(Color, std::uint8_t,
	Red,
	Green,
	Blue,
	Yellow
)

// A second enum with explicit, gap-y values, to show the value dispatch keys on
// the underlying value (not the declaration index): an HTTP-status-shaped set.
ENUM_CLASS(Status, int,
	Ok = 200,
	NotFound = 404,
	Teapot = 418,
	Error = 500
)

int main() {
	std::puts("enum-reflection demo");

	// --- 1. enum -> string: a member becomes its source name ------------------
	rule("enum_name(): each member to the exact text you wrote");
	std::printf("  Color::Red    -> \"%.*s\"\n",
		(int)enum_name(Color::Red).size(), enum_name(Color::Red).data());
	std::printf("  Color::Green  -> \"%.*s\"\n",
		(int)enum_name(Color::Green).size(), enum_name(Color::Green).data());
	std::printf("  Color::Blue   -> \"%.*s\"\n",
		(int)enum_name(Color::Blue).size(), enum_name(Color::Blue).data());
	// Explicit, non-contiguous values resolve by value, not by position.
	std::printf("  Status::Teapot (=418) -> \"%.*s\"\n",
		(int)enum_name(Status::Teapot).size(), enum_name(Status::Teapot).data());

	// --- 2. string -> enum: a name resolves back to its member ----------------
	rule("enum_type<Enum>(): the name back to the typed member");
	// Print the underlying value so you can see the typed member came back.
	std::printf("  \"Blue\"     -> Color value %d\n",
		(int)static_cast<std::uint8_t>(enum_type<Color>("Blue")));
	std::printf("  \"Yellow\"   -> Color value %d\n",
		(int)static_cast<std::uint8_t>(enum_type<Color>("Yellow")));
	std::printf("  \"NotFound\" -> Status value %d\n",
		(int)static_cast<int>(enum_type<Status>("NotFound")));
	// Works on a runtime string_view, not just literals.
	std::string_view picked = "Error";
	std::printf("  string_view{\"%.*s\"} -> Status value %d\n",
		(int)picked.size(), picked.data(), (int)static_cast<int>(enum_type<Status>(picked)));

	// --- 3. iterate the enumerators -------------------------------------------
	rule("walk every enumerator (the reflected values[]/names[] tables)");
	// ENUM_CLASS materializes parallel constexpr tables in namespace detail_<Enum>:
	// values[] (each member as the enum) and names[] (each trimmed source name).
	// Walking them gives you the full member list without naming any member here.
	for (Color c : detail_Color::values) {
		std::string_view n = enum_name(c);
		std::printf("  Color #%d  value=%-3d name=\"%.*s\"\n",
			(int)static_cast<std::uint8_t>(c), (int)static_cast<std::uint8_t>(c),
			(int)n.size(), n.data());
	}
	std::puts("  ---");
	for (Status s : detail_Status::values) {
		std::string_view n = enum_name(s);
		std::printf("  Status    value=%-3d name=\"%.*s\"\n",
			(int)static_cast<int>(s), (int)n.size(), n.data());
	}

	// --- 4. the lookups are compile-time --------------------------------------
	rule("constexpr: the mapping resolves before main() runs");
	// These static_asserts are evaluated by the compiler. If any failed, this
	// demo would not have built. Printing them just makes the point visible.
	static_assert(enum_name(Color::Green) == "Green");
	static_assert(enum_type<Color>("Blue") == Color::Blue);
	static_assert(enum_type<Color>(enum_name(Color::Red)) == Color::Red);
	static_assert(enum_type<Status>(enum_name(Status::Teapot)) == Status::Teapot);
	std::puts("  static_assert(enum_name(Color::Green) == \"Green\")            held");
	std::puts("  static_assert(enum_type<Color>(\"Blue\") == Color::Blue)       held");
	std::puts("  static_assert(round-trip Color::Red and Status::Teapot)       held");
	std::puts("  (the compiler proved these; they are not checked at runtime)");

	// --- 5. round-trip both directions ----------------------------------------
	rule("round-trip: enum -> name -> enum, and name -> enum -> name");
	for (Color c : detail_Color::values) {
		std::string_view n = enum_name(c);
		Color back = enum_type<Color>(n);
		std::printf("  %-3d -> \"%.*s\" -> %-3d   %s\n",
			(int)static_cast<std::uint8_t>(c), (int)n.size(), n.data(),
			(int)static_cast<std::uint8_t>(back),
			back == c ? "(lossless)" : "(MISMATCH)");
	}
	std::printf("  \"Error\" -> %d -> \"%.*s\"  (name spelling preserved)\n",
		(int)static_cast<int>(enum_type<Status>("Error")),
		(int)enum_name(enum_type<Status>("Error")).size(),
		enum_name(enum_type<Status>("Error")).data());

	// --- 6. the unmatched cases -----------------------------------------------
	rule("unknown input: enum_name returns empty, enum_type throws");
	// enum_name of a value that is not a member falls through to "".
	std::string_view bad = enum_name(static_cast<Color>(99));
	std::printf("  enum_name(Color{99}) -> \"%.*s\"  (empty, %zu bytes)\n",
		(int)bad.size(), bad.data(), bad.size());
	// enum_type of a name that is not a member throws std::out_of_range.
	try {
		volatile auto c = enum_type<Color>("Purple");
		(void)c;
	} catch (const std::out_of_range&) {
		std::puts("  enum_type<Color>(\"Purple\") threw std::out_of_range");
	}

	std::puts("\ndone.");
	return 0;
}
