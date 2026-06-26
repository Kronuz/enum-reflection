// Smoke test for the standalone enum-reflection library.
//
// Exercises enum.h: ENUM_CLASS defines a real `enum class`, plus the reflective
// accessors it generates around the compile-time perfect hash --
//   - enum_name(e)            enum -> string_view  (the value dispatch)
//   - enum_type<Enum>(name)   string_view -> enum  (the name dispatch)
//   - enum_find(e) / enum_find<Enum>(name)  the raw perfect-hash slots
// both directions, the unknown-string case (throws std::out_of_range), and a
// full round-trip. Most of the surface is constexpr, so the checks are
// static_assert where possible -- a regression is a compile error, not a silent
// runtime pass.
//
// The perfect hash (phf.hh) and the hhl/hmix/fhhl/fhmix hashing macros
// (hashes.hh) are the two FetchContent'd dependencies; building the enum at all
// exercises both -- ENUM_CLASS bakes a phf::make_phf over the name hashes and a
// second over the value mixes.
//
// Build via CMake: cmake -B build && cmake --build build && ctest --test-dir build
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string_view>

#include "enum.h"


// A real reflected enum: a strongly-typed `enum class` over uint8_t with four
// members. ENUM_CLASS emits the enum plus enum_name / enum_type / enum_find for
// it, all constexpr.
ENUM_CLASS(Color, std::uint8_t,
	Red,
	Green,
	Blue,
	Yellow
)

// A second enum with explicit, non-contiguous values, to confirm the value
// dispatch keys on the underlying value (not the declaration index) and that a
// gap-y, non-zero-based value set still perfect-hashes.
ENUM_CLASS(Status, int,
	Ok = 200,
	NotFound = 404,
	Teapot = 418,
	Error = 500
)


// ---------------------------------------------------------------------------
// 1. enum -> string: enum_name maps each member to its source name.
// ---------------------------------------------------------------------------

static void test_enum_to_string() {
	// constexpr: resolved entirely at compile time.
	static_assert(enum_name(Color::Red) == "Red", "enum_name(Red)");
	static_assert(enum_name(Color::Green) == "Green", "enum_name(Green)");
	static_assert(enum_name(Color::Blue) == "Blue", "enum_name(Blue)");
	static_assert(enum_name(Color::Yellow) == "Yellow", "enum_name(Yellow)");

	// Explicit, non-contiguous underlying values dispatch by value correctly.
	static_assert(enum_name(Status::Ok) == "Ok", "enum_name(Ok=200)");
	static_assert(enum_name(Status::NotFound) == "NotFound", "enum_name(NotFound=404)");
	static_assert(enum_name(Status::Teapot) == "Teapot", "enum_name(Teapot=418)");
	static_assert(enum_name(Status::Error) == "Error", "enum_name(Error=500)");

	std::printf("enum->string OK: Color/Status members map to their names\n");
}


// ---------------------------------------------------------------------------
// 2. string -> enum: enum_type<Enum> looks a name back up to its member.
// ---------------------------------------------------------------------------

static void test_string_to_enum() {
	static_assert(enum_type<Color>("Red") == Color::Red, "enum_type(\"Red\")");
	static_assert(enum_type<Color>("Green") == Color::Green, "enum_type(\"Green\")");
	static_assert(enum_type<Color>("Blue") == Color::Blue, "enum_type(\"Blue\")");
	static_assert(enum_type<Color>("Yellow") == Color::Yellow, "enum_type(\"Yellow\")");

	static_assert(enum_type<Status>("Ok") == Status::Ok, "enum_type(\"Ok\")");
	static_assert(enum_type<Status>("NotFound") == Status::NotFound, "enum_type(\"NotFound\")");
	static_assert(enum_type<Status>("Teapot") == Status::Teapot, "enum_type(\"Teapot\")");
	static_assert(enum_type<Status>("Error") == Status::Error, "enum_type(\"Error\")");

	// Also works on a runtime string_view, not just literals.
	std::string_view name = "Blue";
	assert(enum_type<Color>(name) == Color::Blue);

	std::printf("string->enum OK: names resolve back to Color/Status members\n");
}


// ---------------------------------------------------------------------------
// 3. The perfect-hash dispatch: enum_find gives each key a distinct slot.
// ---------------------------------------------------------------------------

// enum_find(value) and enum_find<Enum>(name) are the raw perfect-hash lookups
// that enum_name / enum_type switch on. Distinct members must land in distinct
// slots (that is the whole point of a *perfect* hash) and the two directions are
// independent hashes, so we check each on its own.
static void test_perfect_hash_dispatch() {
	// Value-keyed phf: one slot per member, all distinct.
	constexpr auto r = enum_find(Color::Red);
	constexpr auto g = enum_find(Color::Green);
	constexpr auto b = enum_find(Color::Blue);
	constexpr auto y = enum_find(Color::Yellow);
	static_assert(r != g && r != b && r != y && g != b && g != y && b != y,
	              "value phf assigns four distinct slots");

	// Name-keyed phf: likewise distinct, and deterministic.
	constexpr auto nr = enum_find<Color>("Red");
	constexpr auto ng = enum_find<Color>("Green");
	constexpr auto nb = enum_find<Color>("Blue");
	constexpr auto ny = enum_find<Color>("Yellow");
	static_assert(nr != ng && nr != nb && nr != ny && ng != nb && ng != ny && nb != ny,
	              "name phf assigns four distinct slots");
	static_assert(enum_find<Color>("Red") == enum_find<Color>("Red"),
	              "name phf is deterministic");

	std::printf("perfect-hash OK: distinct slots for values and for names\n");
}


// ---------------------------------------------------------------------------
// 4. Unknown string: enum_type throws for a name that is not a member.
// ---------------------------------------------------------------------------

static void test_unknown_string() {
	bool threw = false;
	try {
		// "Purple" is not a Color; the name switch hits its default, which
		// throws std::out_of_range.
		volatile auto c = enum_type<Color>("Purple");
		(void)c;
	} catch (const std::out_of_range&) {
		threw = true;
	}
	assert(threw && "enum_type of an unknown name throws std::out_of_range");

	// enum_name of an out-of-range value falls through to the empty string
	// (it returns "", it does not throw).
	assert(enum_name(static_cast<Color>(99)).empty() &&
	       "enum_name of an unknown value returns empty");

	std::printf("unknown-string OK: enum_type throws, enum_name returns empty\n");
}


// ---------------------------------------------------------------------------
// 5. Round-trip: enum -> string -> enum and string -> enum -> string.
// ---------------------------------------------------------------------------

static void test_round_trip() {
	// enum -> name -> enum lands back on the original member, for every member.
	static_assert(enum_type<Color>(enum_name(Color::Red)) == Color::Red, "round-trip Red");
	static_assert(enum_type<Color>(enum_name(Color::Green)) == Color::Green, "round-trip Green");
	static_assert(enum_type<Color>(enum_name(Color::Blue)) == Color::Blue, "round-trip Blue");
	static_assert(enum_type<Color>(enum_name(Color::Yellow)) == Color::Yellow, "round-trip Yellow");

	static_assert(enum_type<Status>(enum_name(Status::Teapot)) == Status::Teapot,
	              "round-trip Teapot (explicit value)");

	// name -> enum -> name returns the same spelling.
	static_assert(enum_name(enum_type<Color>("Green")) == "Green", "round-trip \"Green\"");
	static_assert(enum_name(enum_type<Status>("Error")) == "Error", "round-trip \"Error\"");

	std::printf("round-trip OK: enum<->string is lossless both directions\n");
}


int main() {
	test_enum_to_string();
	test_string_to_enum();
	test_perfect_hash_dispatch();
	test_unknown_string();
	test_round_trip();
	std::printf("all enum-reflection tests passed\n");
	return 0;
}
