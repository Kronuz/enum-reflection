// Name lookup is exact: enum_try_type / enum_type accept a name only if it equals a member
// name, ASCII case-insensitively. The perfect hash picks a candidate slot; the name is then
// compared, so a string whose hash collides with a member's is a miss.
//
// The oracle is a brute-force reference written here (no hashing): the member a name denotes
// is the one whose source name matches it byte for byte after folding A-Z. The test runs it
// over every short string from an alphabet built from the member names, over every case
// variant of every member name, over random strings, and over strings forged to collide
// with each member's 64-bit hash (the hash is a chain of invertible steps, so a colliding
// 15-byte string takes a few hundred tries to find).
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "enum.h"

#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); std::abort(); } } while (0)

ENUM_CLASS(Role, int, User, Admin, Guest)

// Mixed case, digits and underscores, one-letter and long names, and names that differ only
// by case-folded letters elsewhere in the word.
ENUM_CLASS(Mixed, unsigned char,
	Red, GREEN, blue_2, A, Bb, Teapot, NotFound, Internal_Server_Error, x9, ABCDEFGH, abcdefghi, Zz
)

static const char* const role_names[] = {"User", "Admin", "Guest"};
static const char* const mixed_names[] = {
	"Red", "GREEN", "blue_2", "A", "Bb", "Teapot", "NotFound", "Internal_Server_Error", "x9", "ABCDEFGH", "abcdefghi", "Zz",
};

// The oracle: index of the member `s` denotes, or -1.
static int ref_member(const char* const* names, std::size_t count, std::string_view s) {
	auto fold = [](unsigned char c) { return (c >= 'A' && c <= 'Z') ? static_cast<unsigned char>(c + 32) : c; };
	for (std::size_t i = 0; i < count; ++i) {
		std::string_view n = names[i];
		if (n.size() != s.size()) { continue; }
		bool same = true;
		for (std::size_t k = 0; k < n.size() && same; ++k) { same = fold(static_cast<unsigned char>(n[k])) == fold(static_cast<unsigned char>(s[k])); }
		if (same) { return static_cast<int>(i); }
	}
	return -1;
}

template <typename E>
static void check_name(const char* const* names, std::size_t count, const E* values, std::string_view s) {
	int want = ref_member(names, count, s);
	std::optional<E> got = enum_try_type<E>(s);
	if (want < 0) {
		if (got) { std::fprintf(stderr, "accepted a non-member: [%.*s] -> %s\n", static_cast<int>(s.size()), s.data(), std::string(enum_name(*got)).c_str()); std::abort(); }
	} else {
		if (!got || *got != values[want]) { std::fprintf(stderr, "rejected a member name: [%.*s]\n", static_cast<int>(s.size()), s.data()); std::abort(); }
	}
	bool threw = false;
	try { (void)enum_type<E>(s); } catch (const std::out_of_range&) { threw = true; }
	CHECK(threw == (want < 0));
}

// Every string up to `max_len` over `alphabet`.
template <typename E>
static void exhaustive(const char* const* names, std::size_t count, const E* values, const std::string& alphabet, std::size_t max_len) {
	std::string s;
	std::vector<std::size_t> digit;
	for (std::size_t len = 0; len <= max_len; ++len) {
		digit.assign(len, 0);
		for (;;) {
			s.assign(len, '\0');
			for (std::size_t i = 0; i < len; ++i) { s[i] = alphabet[digit[i]]; }
			check_name(names, count, values, s);
			std::size_t i = 0;
			while (i < len && ++digit[i] == alphabet.size()) { digit[i++] = 0; }
			if (i == len) { break; }
		}
	}
}

// Every case variant of every member name (all 2^letters of them up to 12 letters, then random).
template <typename E>
static void case_variants(const char* const* names, std::size_t count, const E* values, std::mt19937& rng) {
	for (std::size_t m = 0; m < count; ++m) {
		std::string base = names[m];
		std::vector<std::size_t> letters;
		for (std::size_t i = 0; i < base.size(); ++i) { if ((base[i] | 0x20) >= 'a' && (base[i] | 0x20) <= 'z') { letters.push_back(i); } }
		std::size_t n = letters.size() <= 12 ? (std::size_t(1) << letters.size()) : 4096;
		for (std::size_t bits = 0; bits < n; ++bits) {
			std::string s = base;
			std::uint64_t mask = letters.size() <= 12 ? bits : rng();
			for (std::size_t j = 0; j < letters.size(); ++j) {
				char c = static_cast<char>(base[letters[j]] | 0x20);
				s[letters[j]] = (mask >> j & 1) ? static_cast<char>(c - 32) : c;
			}
			CHECK(ref_member(names, count, s) == static_cast<int>(m));
			check_name(names, count, values, s);
			// one byte changed (to the neighbours of the letter's case pair, or off by one)
			for (std::size_t i = 0; i < s.size(); ++i) {
				for (int d : {-32, -1, 1, 32, 128}) {
					std::string t = s;
					t[i] = static_cast<char>(t[i] + d);
					check_name(names, count, values, t);
				}
			}
			// a different length
			check_name(names, count, values, s + "x");
			if (!s.empty()) { check_name(names, count, values, s.substr(1)); check_name(names, count, values, s.substr(0, s.size() - 1)); }
		}
	}
}

// ---- forging: a string that is not a member name but has a member's hash ----

static constexpr std::uint64_t P = 0x9e3779b97f4a7c15ULL;  // wordwise_ci's multiplier

static std::uint64_t inv_mul(std::uint64_t a) {  // modular inverse of an odd number mod 2^64
	std::uint64_t x = a;
	for (int i = 0; i < 6; ++i) { x *= 2 - a * x; }
	return x;
}
static std::uint64_t inv_xorshift(std::uint64_t h, int s) {  // inverse of h ^= h >> s
	std::uint64_t x = h;
	for (int i = 0; i < 64 / s + 1; ++i) { x = h ^ (x >> s); }
	return x;
}
static bool fold_fixed(unsigned char b) { return !(b >= 'A' && b <= 'Z'); }

// A 15-byte string (an 8-byte word and a 7-byte tail) whose wordwise_ci hash equals `target`'s.
static std::string forge(std::string_view target, std::uint64_t seed) {
	std::mt19937_64 rng(seed);
	const std::uint64_t T = wordwise_ci::hash(target.data(), target.size());
	const std::size_t L = 15;
	const std::uint64_t h0 = 0x100000001b3ULL ^ (static_cast<std::uint64_t>(L) * P);
	const std::uint64_t before_mul = inv_xorshift(T, 32) * inv_mul(P);  // the state after `h ^= tail`
	for (;;) {
		std::uint64_t w = rng();
		bool ok = true;
		for (int k = 0; k < 8; ++k) { ok &= fold_fixed(static_cast<unsigned char>(w >> (8 * k))); }
		if (!ok) { continue; }
		std::uint64_t h = (h0 ^ w) * P;
		h ^= h >> 29;
		std::uint64_t tail = h ^ before_mul;
		if (tail >> 56) { continue; }
		bool tok = true;
		for (int k = 0; k < 7; ++k) { tok &= fold_fixed(static_cast<unsigned char>(tail >> (8 * k))); }
		if (!tok) { continue; }
		char buf[16];
		for (int k = 0; k < 8; ++k) { buf[k] = static_cast<char>(w >> (8 * k)); }
		for (int k = 0; k < 7; ++k) { buf[8 + k] = static_cast<char>(tail >> (8 * k)); }
		return std::string(buf, L);
	}
}

// A string of length 8k+7 that starts with `prefix` (any case) and has the hash of `target`: k full
// words, at least six of whose bytes after the prefix are free, and a 7-byte tail that the hash
// determines. The free bytes are random ASCII that is not A-Z, so the fold leaves them alone.
static std::string forge_with_prefix(std::string_view target, std::string_view prefix, std::uint64_t seed) {
	std::mt19937_64 rng(seed);
	const std::uint64_t T = wordwise_ci::hash(target.data(), target.size());
	std::size_t k = 1;
	while (8 * k < prefix.size() + 6) { ++k; }
	const std::size_t L = 8 * k + 7;
	const std::uint64_t h0 = 0x100000001b3ULL ^ (static_cast<std::uint64_t>(L) * P);
	const std::uint64_t before_mul = inv_xorshift(T, 32) * inv_mul(P);
	auto folded = [](unsigned char c) { return (c >= 'A' && c <= 'Z') ? static_cast<unsigned char>(c + 32) : c; };
	for (;;) {
		std::string s(L, '\0');
		for (std::size_t i = 0; i < 8 * k; ++i) {
			if (i < prefix.size()) { s[i] = prefix[i]; continue; }
			unsigned char c;
			do { c = static_cast<unsigned char>(rng() & 0x7f); } while (!fold_fixed(c));
			s[i] = static_cast<char>(c);
		}
		std::uint64_t h = h0;
		for (std::size_t w = 0; w < k; ++w) {
			std::uint64_t word = 0;
			for (int b = 0; b < 8; ++b) { word |= static_cast<std::uint64_t>(folded(static_cast<unsigned char>(s[8 * w + b]))) << (8 * b); }
			h ^= word;
			h *= P;
			h ^= h >> 29;
		}
		const std::uint64_t tail = h ^ before_mul;
		if (tail >> 56) { continue; }
		bool ok = true;
		for (int b = 0; b < 7; ++b) { ok &= fold_fixed(static_cast<unsigned char>(tail >> (8 * b))); }
		if (!ok) { continue; }
		for (int b = 0; b < 7; ++b) { s[8 * k + b] = static_cast<char>(tail >> (8 * b)); }
		return s;
	}
}

template <typename E>
static void forged(const char* const* names, std::size_t count, const E* values) {
	for (std::size_t m = 0; m < count; ++m) {
		for (std::uint64_t seed = 1; seed <= 3; ++seed) {
			std::string f = forge(names[m], seed);
			// The premise: the forged string has the member's hash, so the perfect hash alone cannot tell it apart.
			CHECK(wordwise_ci::hash(f) == wordwise_ci::hash(std::string_view(names[m])));
			CHECK(enum_find<E>(f) == enum_find<E>(std::string_view(names[m])));
			CHECK(enum_find<E>(f) != phf::npos);
			CHECK(ref_member(names, count, f) < 0);
			// The lookup rejects it.
			CHECK(!enum_try_type<E>(f).has_value());
			bool threw = false;
			try { (void)enum_type<E>(f); } catch (const std::out_of_range&) { threw = true; }
			CHECK(threw);
			// And the member itself, in any case, is still found.
			CHECK(enum_try_type<E>(std::string_view(names[m])) == values[m]);
		}
		// A longer string that begins with the member's name (any case) and has its hash: a
		// comparison that stops at the shorter length would accept it.
		std::string name = names[m], low = name, up = name;
		for (auto& c : low) { if (c >= 'A' && c <= 'Z') { c = static_cast<char>(c + 32); } }
		for (auto& c : up) { if (c >= 'a' && c <= 'z') { c = static_cast<char>(c - 32); } }
		for (const std::string& prefix : {name, low, up}) {
			std::string f = forge_with_prefix(name, prefix, 7 + m);
			CHECK(f.size() > name.size() && ref_member(names, count, std::string_view(f).substr(0, name.size())) == static_cast<int>(m));
			CHECK(wordwise_ci::hash(f) == wordwise_ci::hash(std::string_view(name)));
			CHECK(enum_find<E>(f) == enum_find<E>(std::string_view(name)));
			CHECK(ref_member(names, count, f) < 0);
			CHECK(!enum_try_type<E>(f).has_value());
		}
	}
}

int main() {
	// constexpr use keeps working.
	static_assert(enum_try_type<Role>("admin") == Role::Admin);
	static_assert(enum_try_type<Role>("ADMIN") == Role::Admin);
	static_assert(!enum_try_type<Role>("Admins").has_value());
	static_assert(!enum_try_type<Role>("").has_value());
	static_assert(enum_type<Role>("gUeSt") == Role::Guest);

	const Role role_values[] = {Role::User, Role::Admin, Role::Guest};
	const Mixed mixed_values[] = {Mixed::Red, Mixed::GREEN, Mixed::blue_2, Mixed::A, Mixed::Bb, Mixed::Teapot, Mixed::NotFound,
		Mixed::Internal_Server_Error, Mixed::x9, Mixed::ABCDEFGH, Mixed::abcdefghi, Mixed::Zz};
	std::mt19937 rng(2026);

	exhaustive<Role>(role_names, 3, role_values, "UuSsEeRrAaDdMmIiNnGgTt@`[{", 4);
	exhaustive<Mixed>(mixed_names, 12, mixed_values, "ArRgGbBzZ29_@`[{\xff", 4);
	case_variants<Role>(role_names, 3, role_values, rng);
	case_variants<Mixed>(mixed_names, 12, mixed_values, rng);

	// Random strings, random bytes, and random strings built from member-name pieces.
	for (int iter = 0; iter < 200000; ++iter) {
		std::string s(rng() % 24, '\0');
		for (auto& c : s) { c = static_cast<char>(rng()); }
		check_name<Mixed>(mixed_names, 12, mixed_values, s);
		check_name<Role>(role_names, 3, role_values, s);
		std::string t;
		while (t.size() < 24 && rng() % 5) { t += mixed_names[rng() % 12]; }
		check_name<Mixed>(mixed_names, 12, mixed_values, t);
	}

	forged<Role>(role_names, 3, role_values);
	forged<Mixed>(mixed_names, 12, mixed_values);

	std::printf("enum names OK: exhaustive, case variants, random and forged collisions\n");
	return 0;
}
