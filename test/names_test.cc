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
#include <cstring>
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

// Names of every size from 1 to 40, and 47 and 64: each of the ways a name is read (one to three
// bytes, four to seven, eight to sixteen, and longer), a mix of upper and lower case, digits and
// underscores, and a 64-bit underlying type.
ENUM_CLASS(Sizes, std::uint64_t,
	J, Dr, lqc, ErSh, l_Mhe, JSvsGr,
	kgRKOh3, TK4CnkpV, jKlS14Vx9, cm_otbjhpo, R_8n_M_fvp2, ArseQ_Xy_Rmp,
	DgDbsd_C_ELPp, dJd4WaqwA4__IF, OYUz363pa_7wLX_, XdgpT7_uvm3FUz_7, F2_s_2T_Lra1EG3gh, Un0_oQQeA_9zeXbqP1,
	DidRcT_GOZHI__odkhG, yywLe_dpv28NklrWkJCz, Giyyn4R_WiwcaZt_dK_IB, P__fjQ4Vl4a8_H_uPM_g_w, IL_0x_Bm_JWoiIk_HjfmiGy, Q2bjhq_w9k7xEw_X8q9093bl,
	Y8aVizq_qxZiyxOMvbUtiWspi, GVWOYrc_oq_IgcqluD3PfpMeMk, wmg4csn0b4EiKl6_6__XxT4RpJx, pupMugZHK_EHkKi3_6xMyiLqZ_i_, pJ0n7_Cq__deVx7yzs_J_QW1q_itr, dkHHA_4gVCvLbWVa8p_ghjtF_Vtma_,
	D2w_1_fQ0xl_Din_yl46wLOl7nu0obx, mitx5_x1hw__i_2_z_w9kZ1M3UrF___c, aW_eT_r14si3FeschHDBp_o_jbscfI0t5, XbixG5Ljb8NmECMN_N_s_A__gmaFCLFbU6, n_2jf_p3b_BUwFHT7gqlh_3r0KtU6hVqApO, x26eloVuxxB6Zc_1pz_z3lItoq_SqlfU_Mzy,
	o__QDrM5lLcFBqu____0jNL_hBaJlNSlPa_eC, q60r_SQ3arfb_tGgUUTjux8n7uH_uk___brV8_, Nqi_3_5mhu__8QAXj6CFat_e0wBXs38_y__gbz1, oPd3kAjyk9JA0Yp8W_jaJ0pw2_Qfg_fu785m__6_, Nn_M_Ot9LQoXoYhoWgy9e_9Fgx_vme4GdmAzHUat06969_U, bu6m2nANV_uap_iF3D7WALN7vq19bmanVij8___FyzPXeu6J27hmNPx5jpUlj_s_
)

static const char* const role_names[] = {"User", "Admin", "Guest"};
static const char* const mixed_names[] = {
	"Red", "GREEN", "blue_2", "A", "Bb", "Teapot", "NotFound", "Internal_Server_Error", "x9", "ABCDEFGH", "abcdefghi", "Zz",
};

static const char* const sizes_names[] = {
	"J", "Dr", "lqc",
	"ErSh", "l_Mhe", "JSvsGr",
	"kgRKOh3", "TK4CnkpV", "jKlS14Vx9",
	"cm_otbjhpo", "R_8n_M_fvp2", "ArseQ_Xy_Rmp",
	"DgDbsd_C_ELPp", "dJd4WaqwA4__IF", "OYUz363pa_7wLX_",
	"XdgpT7_uvm3FUz_7", "F2_s_2T_Lra1EG3gh", "Un0_oQQeA_9zeXbqP1",
	"DidRcT_GOZHI__odkhG", "yywLe_dpv28NklrWkJCz", "Giyyn4R_WiwcaZt_dK_IB",
	"P__fjQ4Vl4a8_H_uPM_g_w", "IL_0x_Bm_JWoiIk_HjfmiGy", "Q2bjhq_w9k7xEw_X8q9093bl",
	"Y8aVizq_qxZiyxOMvbUtiWspi", "GVWOYrc_oq_IgcqluD3PfpMeMk", "wmg4csn0b4EiKl6_6__XxT4RpJx",
	"pupMugZHK_EHkKi3_6xMyiLqZ_i_", "pJ0n7_Cq__deVx7yzs_J_QW1q_itr", "dkHHA_4gVCvLbWVa8p_ghjtF_Vtma_",
	"D2w_1_fQ0xl_Din_yl46wLOl7nu0obx", "mitx5_x1hw__i_2_z_w9kZ1M3UrF___c", "aW_eT_r14si3FeschHDBp_o_jbscfI0t5",
	"XbixG5Ljb8NmECMN_N_s_A__gmaFCLFbU6", "n_2jf_p3b_BUwFHT7gqlh_3r0KtU6hVqApO", "x26eloVuxxB6Zc_1pz_z3lItoq_SqlfU_Mzy",
	"o__QDrM5lLcFBqu____0jNL_hBaJlNSlPa_eC", "q60r_SQ3arfb_tGgUUTjux8n7uH_uk___brV8_", "Nqi_3_5mhu__8QAXj6CFat_e0wBXs38_y__gbz1",
	"oPd3kAjyk9JA0Yp8W_jaJ0pw2_Qfg_fu785m__6_", "Nn_M_Ot9LQoXoYhoWgy9e_9Fgx_vme4GdmAzHUat06969_U", "bu6m2nANV_uap_iF3D7WALN7vq19bmanVij8___FyzPXeu6J27hmNPx5jpUlj_s_",
};
static const Sizes sizes_values[] = {
	Sizes::J, Sizes::Dr, Sizes::lqc, Sizes::ErSh,
	Sizes::l_Mhe, Sizes::JSvsGr, Sizes::kgRKOh3, Sizes::TK4CnkpV,
	Sizes::jKlS14Vx9, Sizes::cm_otbjhpo, Sizes::R_8n_M_fvp2, Sizes::ArseQ_Xy_Rmp,
	Sizes::DgDbsd_C_ELPp, Sizes::dJd4WaqwA4__IF, Sizes::OYUz363pa_7wLX_, Sizes::XdgpT7_uvm3FUz_7,
	Sizes::F2_s_2T_Lra1EG3gh, Sizes::Un0_oQQeA_9zeXbqP1, Sizes::DidRcT_GOZHI__odkhG, Sizes::yywLe_dpv28NklrWkJCz,
	Sizes::Giyyn4R_WiwcaZt_dK_IB, Sizes::P__fjQ4Vl4a8_H_uPM_g_w, Sizes::IL_0x_Bm_JWoiIk_HjfmiGy, Sizes::Q2bjhq_w9k7xEw_X8q9093bl,
	Sizes::Y8aVizq_qxZiyxOMvbUtiWspi, Sizes::GVWOYrc_oq_IgcqluD3PfpMeMk, Sizes::wmg4csn0b4EiKl6_6__XxT4RpJx, Sizes::pupMugZHK_EHkKi3_6xMyiLqZ_i_,
	Sizes::pJ0n7_Cq__deVx7yzs_J_QW1q_itr, Sizes::dkHHA_4gVCvLbWVa8p_ghjtF_Vtma_, Sizes::D2w_1_fQ0xl_Din_yl46wLOl7nu0obx, Sizes::mitx5_x1hw__i_2_z_w9kZ1M3UrF___c,
	Sizes::aW_eT_r14si3FeschHDBp_o_jbscfI0t5, Sizes::XbixG5Ljb8NmECMN_N_s_A__gmaFCLFbU6, Sizes::n_2jf_p3b_BUwFHT7gqlh_3r0KtU6hVqApO, Sizes::x26eloVuxxB6Zc_1pz_z3lItoq_SqlfU_Mzy,
	Sizes::o__QDrM5lLcFBqu____0jNL_hBaJlNSlPa_eC, Sizes::q60r_SQ3arfb_tGgUUTjux8n7uH_uk___brV8_, Sizes::Nqi_3_5mhu__8QAXj6CFat_e0wBXs38_y__gbz1, Sizes::oPd3kAjyk9JA0Yp8W_jaJ0pw2_Qfg_fu785m__6_,
	Sizes::Nn_M_Ot9LQoXoYhoWgy9e_9Fgx_vme4GdmAzHUat06969_U, Sizes::bu6m2nANV_uap_iF3D7WALN7vq19bmanVij8___FyzPXeu6J27hmNPx5jpUlj_s_,
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
static void case_variants(const char* const* names, std::size_t count, const E* values, std::mt19937& rng, std::size_t random_variants = 4096) {
	for (std::size_t m = 0; m < count; ++m) {
		std::string base = names[m];
		std::vector<std::size_t> letters;
		for (std::size_t i = 0; i < base.size(); ++i) { if ((base[i] | 0x20) >= 'a' && (base[i] | 0x20) <= 'z') { letters.push_back(i); } }
		std::size_t n = letters.size() <= 12 ? (std::size_t(1) << letters.size()) : random_variants;
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


// Every byte value at every position of every member name: a name with one byte replaced is a
// member only if the byte is the other case of a letter.
template <typename E>
static void every_byte(const char* const* names, std::size_t count, const E* values) {
	for (std::size_t m = 0; m < count; ++m) {
		const std::string base = names[m];
		for (std::size_t i = 0; i < base.size(); ++i) {
			for (int v = 0; v < 256; ++v) {
				std::string s = base;
				s[i] = static_cast<char>(v);
				check_name(names, count, values, s);
			}
		}
	}
}

// ---- the pieces enum.h builds a lookup from ----

constexpr bool is_letter(unsigned b) { return (b >= 'A' && b <= 'Z') || (b >= 'a' && b <= 'z'); }

constexpr unsigned lower_byte(unsigned b) { return (b >= 'A' && b <= 'Z') ? b + 32 : b; }

// Every byte of a word lower-cased on its own: 'A'..'Z' change and no other value does, whatever its neighbours.
constexpr bool lower_ok() {
	for (unsigned b = 0; b < 256; ++b) {
		for (int lane = 0; lane < 8; ++lane) {
			const std::uint64_t w = static_cast<std::uint64_t>(b) << (8 * lane);
			if (_enum_lower(w) != static_cast<std::uint64_t>(lower_byte(b)) << (8 * lane)) { return false; }
		}
	}
	return true;
}
static_assert(lower_ok(), "_enum_lower maps exactly A-Z to a-z");

// The table the lookup lower-cases a short name with agrees with it on every byte value.
constexpr bool table_lower_ok() {
	for (unsigned b = 0; b < 256; ++b) {
		if (static_cast<unsigned char>(chars::tolower(static_cast<char>(b))) != lower_byte(b)) { return false; }
	}
	return true;
}
static_assert(table_lower_ok(), "chars::tolower maps exactly A-Z to a-z");

// The two ways to read a name of 1 to 3 bytes (the table on arm64, the word elsewhere) give the lower-cased n bytes.
static void short_names_lower() {
	for (std::size_t n = 1; n <= 3; ++n) {
		for (unsigned a = 0; a < 256; a += 5) {
			for (unsigned b = 0; b < 256; b += 3) {
				for (unsigned c = 0; c < 256; c += 7) {
					const char s[3] = {static_cast<char>(a), static_cast<char>(b), static_cast<char>(c)};
					std::uint64_t want = 0;
					for (std::size_t i = 0; i < n; ++i) { want |= static_cast<std::uint64_t>(lower_byte(static_cast<unsigned char>(s[i]))) << (8 * i); }
					CHECK(_enum_short_lower_table(s, n) == want);
					CHECK(_enum_short_lower_word(s, n) == want);
					CHECK(_enum_short_lower(s, n) == want);
				}
			}
		}
	}
}

// The byte swap a big-endian host applies to what it loads (nothing here runs on one).
static_assert(_enum_bswap(0x0102030405060708ULL) == 0x0807060504030201ULL, "_enum_bswap reverses eight bytes");
static_assert((_enum_bswap(0x01020304ULL) >> 32) == 0x04030201ULL, "a four byte load swaps to the low half");
static_assert(_enum_pack("\x01\x02\x03\x04\x05\x06\x07\x08", 8) == 0x0807060504030201ULL, "words are little-endian");

static void lower_in_words(std::mt19937_64& rng) {
	for (int iter = 0; iter < 1000000; ++iter) {
		std::uint64_t w = rng(), want = 0;
		if (iter & 1) { w &= 0x7f7f7f7f7f7f7f7fULL; }
		if (iter & 2) { w |= 0x4040404040404040ULL; }
		if (iter & 4) { w |= 0xc1c1c1c1c1c1c1c1ULL; }
		for (int lane = 0; lane < 8; ++lane) { want |= static_cast<std::uint64_t>(lower_byte((w >> (8 * lane)) & 0xff)) << (8 * lane); }
		CHECK(_enum_lower(w) == want);
		std::uint64_t a = w, b = ~w;
		_enum_lower_pair(a, b);
		CHECK(a == want);
		std::uint64_t want_b = 0;
		for (int lane = 0; lane < 8; ++lane) { want_b |= static_cast<std::uint64_t>(lower_byte((~w >> (8 * lane)) & 0xff)) << (8 * lane); }
		CHECK(b == want_b);
	}
	for (unsigned b = 0; b < 256; ++b) {  // every byte value in every lane of either word
		for (int lane = 0; lane < 8; ++lane) {
			std::uint64_t x = static_cast<std::uint64_t>(b) << (8 * lane), y = x ^ 0x4141414141414141ULL;
			const std::uint64_t want_x = _enum_lower(x), want_y = _enum_lower(y);
			_enum_lower_pair(x, y);
			CHECK(x == want_x && y == want_y);
		}
	}
}

// The hash a lookup computes from the words it loads is wordwise_ci's, for every size and every
// byte value (the perfect hash was built from wordwise_ci).
static void hash_agrees(std::mt19937_64& rng) {
	auto agrees = [](const std::string& s) {
		// A byte of 0xc1 or more in a full eight-byte word makes wordwise_ci's whole-word lower-casing carry
		// into the next byte; the lookup lower-cases each byte alone, so only these inputs may hash differently.
		bool carries = false;
		for (std::size_t i = 0; i < s.size() / 8 * 8; ++i) { carries |= static_cast<unsigned char>(s[i]) >= 0xc1; }
		const std::uint64_t want = wordwise_ci::hash(s.data(), s.size());
		if (!carries) {
			if (s.size() <= 16) { CHECK(_enum_probe_input(s.data(), s.size()).hash == want); }
			if (s.size() >= 8) { CHECK(_enum_hash_long(s.data(), s.size()) == want); }
			if (s.size() >= 17 && s.size() <= 24) { CHECK(_enum_hash_medium(_enum_pack(s.data(), 8), _enum_pack(s.data() + 8, 8), _enum_pack(s.data() + s.size() - 8, 8), s.size()) == want); }
		}
		// Either way the two ways of hashing one input agree with each other.
		if (s.size() >= 8 && s.size() <= 16) { CHECK(_enum_probe_input(s.data(), s.size()).hash == _enum_hash_long(s.data(), s.size())); }
		if (s.size() >= 17 && s.size() <= 24) { CHECK(_enum_hash_medium(_enum_pack(s.data(), 8), _enum_pack(s.data() + 8, 8), _enum_pack(s.data() + s.size() - 8, 8), s.size()) == _enum_hash_long(s.data(), s.size())); }
	};
	for (std::size_t n = 0; n <= 70; ++n) {
		for (int iter = 0; iter < 2000; ++iter) {
			std::string s(n, '\0');
			for (auto& c : s) {
				switch (iter % 4) {
					case 0: c = static_cast<char>(rng()); break;
					case 1: c = "AZaz@[`{ \xc1\xff\xbf\x80_09"[rng() % 17]; break;
					case 2: c = static_cast<char>('A' + rng() % 60); break;
					default: c = static_cast<char>(0xb0 + rng() % 0x50); break;
				}
			}
			agrees(s);
			if (n > 0) {  // every byte value at one position
				const std::size_t at = rng() % n;
				for (int v = 0; v < 256; ++v) { s[at] = static_cast<char>(v); agrees(s); }
			}
		}
	}
}

// A name and an input of another length can give the same words (a run of one byte does, in every
// way a name is read), so the length is what tells them apart.
static void probe_lengths() {
	for (const char unit : {'a', 'Q', '7', '_'}) {
		for (std::size_t size = 1; size <= 16; ++size) {
			const _enum_entry<int> entry = _enum_make_entry<int>(0, std::string(size, unit));
			for (std::size_t n = 0; n <= 16; ++n) {
				const std::string input(n, unit);
				CHECK(_enum_probe_equal(entry, _enum_probe_input(input.data(), n), n) == (n == size));
				if (is_letter(static_cast<unsigned char>(unit))) {  // the other case of the same letter
					const std::string other(n, static_cast<char>(unit ^ 0x20));
					CHECK(_enum_probe_equal(entry, _enum_probe_input(other.data(), n), n) == (n == size));
				}
			}
		}
	}
}

// The perfect hash sends every input to some slot, and a forged input to a member's own. Here the
// slot is chosen: `input` is made to collide with a one-member enum named `name`, so the lookup has
// to decide on the bytes alone. It must answer as the oracle does, whatever the two look like.
static bool collides_into(std::string_view name, std::string_view input, bool* found) {
	const std::uint64_t hash = input.size() <= 16 ? _enum_probe_input(input.data(), input.size()).hash : _enum_hash_long(input.data(), input.size());
	const std::uint64_t hashes[2] = {hash, hash ^ 0x5555aaaa5555aaaaULL};
	const auto phf_names = phf::make_phf(hashes);
	_enum_table<int, 2> table{};
	table.entry[2] = _enum_no_entry<int>();
	table.entry[phf_names.lookup(hashes[0])] = _enum_make_entry<int>(7, name);
	const std::optional<int> got = _enum_lookup(phf_names, table, input);
	*found = got.has_value();
	if (got) { CHECK(*got == 7); }
	std::string folded_name(name), folded_input(input);
	for (auto& c : folded_name) { if (c >= 'A' && c <= 'Z') { c = static_cast<char>(c + 32); } }
	for (auto& c : folded_input) { if (c >= 'A' && c <= 'Z') { c = static_cast<char>(c + 32); } }
	return folded_name == folded_input;
}

static void synthetic_collisions(std::mt19937& rng) {
	auto check = [](std::string_view name, std::string_view input) {
		bool found;
		const bool want = collides_into(name, input, &found);
		if (found != want) {
			std::fprintf(stderr, "collision: name [%.*s] input [%.*s]: %s\n", static_cast<int>(name.size()), name.data(),
				static_cast<int>(input.size()), input.data(), found ? "accepted" : "rejected");
			std::abort();
		}
	};
	// Names and inputs that repeat a unit, of every length: equal words in different lengths.
	for (const std::string unit : {"a", "Q", "7", "_", "ab", "aB", "xyz", "Ab_", "abcd", "AbCd9_ef"}) {
		for (std::size_t size = 1; size <= 40; ++size) {
			std::string name;
			while (name.size() < size) { name += unit; }
			name.resize(size);
			for (std::size_t n = 0; n <= 42; ++n) {
				std::string input;
				while (input.size() < n) { input += unit; }
				input.resize(n);
				check(name, input);
				for (auto& c : input) { if (c >= 'a' && c <= 'z') { c = static_cast<char>(c - 32); } }
				check(name, input);
			}
		}
	}
	// Random names, and their case variants, with one byte of every value changed, a byte inserted or dropped.
	for (int iter = 0; iter < 400; ++iter) {
		const std::size_t size = 1 + rng() % 40;
		std::string name(size, 'a');
		for (auto& c : name) { c = "abcXYZ019_"[rng() % 10]; }
		std::string input = name;
		for (auto& c : input) { if (rng() & 1) { c = static_cast<char>(c ^ (is_letter(static_cast<unsigned char>(c)) ? 0x20 : 0)); } }
		check(name, input);
		const std::size_t at = rng() % size;
		for (int v = 0; v < 256; ++v) { std::string t = input; t[at] = static_cast<char>(v); check(name, t); }
		check(name, input + "z");
		check(name, input.substr(1));
		input.insert(input.begin() + at, 'a');
		check(name, input);
	}
}

// The entry of a slot that no member has matches no input: not the empty one, not eight capitals,
// not any string.
static void no_entry_matches_nothing(std::mt19937_64& rng) {
	const _enum_entry<int> none = _enum_no_entry<int>();
	for (std::size_t n = 0; n <= 16; ++n) {
		for (int iter = 0; iter < 20000; ++iter) {
			std::string s(n, '\0');
			for (auto& c : s) { c = iter % 3 == 0 ? static_cast<char>(rng()) : iter % 3 == 1 ? 'A' : static_cast<char>('A' + rng() % 2 * 32); }
			CHECK(!_enum_probe_equal(none, _enum_probe_input(s.data(), n), n));
		}
	}
}

// The words an entry holds for a member are the words the same bytes give when they are read as input.
template <typename E, std::size_t N>
static void entries_match_probes(const char* const* names, std::size_t count, const _enum_table<E, N>& table) {
	for (std::size_t m = 0; m < count; ++m) {
		const std::string_view name = names[m];
		const std::size_t slot = enum_find<E>(name);
		CHECK(slot < N);
		const auto& e = table.entry[slot];
		CHECK(e.size == name.size());
		CHECK(std::string_view(e.data, e.size) == name);
		if (name.size() <= 16) {
			// The words of the name lower-cased, from the definition: the whole name, or its first eight bytes and last eight.
			auto window = [&](std::size_t from, std::size_t count) {
				std::uint64_t w = 0;
				for (std::size_t i = 0; i < count; ++i) { w |= static_cast<std::uint64_t>(lower_byte(static_cast<unsigned char>(name[from + i]))) << (8 * i); }
				return w;
			};
			const std::size_t n = name.size();
			CHECK(e.word[0] == (n >= 8 ? window(0, 8) : window(0, n)));
			CHECK(e.word[1] == (n >= 8 ? window(n - 8, 8) : 0));
			const _enum_probe probe = _enum_probe_input(name.data(), n);
			CHECK(probe.word[0] == e.word[0] && probe.word[1] == e.word[1]);
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
		// ASCII only: wordwise_ci folds a whole word with adds, and a byte of 0xc1 or more carries into the next one.
		for (int k = 0; k < 8; ++k) { ok &= fold_fixed(static_cast<unsigned char>(w >> (8 * k))) && (w >> (8 * k) & 0xff) < 0x80; }
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

int main(int argc, char** argv) {
	// constexpr use keeps working.
	static_assert(enum_try_type<Role>("admin") == Role::Admin);
	static_assert(enum_try_type<Role>("ADMIN") == Role::Admin);
	static_assert(!enum_try_type<Role>("Admins").has_value());
	static_assert(!enum_try_type<Role>("").has_value());
	static_assert(enum_type<Role>("gUeSt") == Role::Guest);
	static_assert(enum_try_type<Mixed>("RED") == Mixed::Red);
	static_assert(enum_try_type<Mixed>("internal_server_error") == Mixed::Internal_Server_Error);
	static_assert(!enum_try_type<Mixed>("Internal_Server_Errors").has_value());
	static_assert(enum_try_type<Sizes>("j") == Sizes::J);
	static_assert(enum_try_type<Sizes>("LQC") == Sizes::lqc);
	static_assert(enum_try_type<Sizes>("kgrkoh3") == Sizes::kgRKOh3);
	static_assert(enum_try_type<Sizes>("UN0_OQQEA_9ZEXBQP1") == Sizes::Un0_oQQeA_9zeXbqP1);
	static_assert(!enum_try_type<Sizes>("Un0_oQQeA_9zeXbqP2").has_value());
	static_assert(enum_try_type<Sizes>("BU6M2NANV_UAP_IF3D7WALN7VQ19BMANVIJ8___FYZPXEU6J27HMNPX5JPULJ_S_") == Sizes::bu6m2nANV_uap_iF3D7WALN7vq19bmanVij8___FyzPXeu6J27hmNPx5jpUlj_s_);

	const Role role_values[] = {Role::User, Role::Admin, Role::Guest};
	const Mixed mixed_values[] = {Mixed::Red, Mixed::GREEN, Mixed::blue_2, Mixed::A, Mixed::Bb, Mixed::Teapot, Mixed::NotFound,
		Mixed::Internal_Server_Error, Mixed::x9, Mixed::ABCDEFGH, Mixed::abcdefghi, Mixed::Zz};
	std::mt19937 rng(2026);
	std::mt19937_64 rng64(2027);

	// With a name as the argument, only that section runs (tests/mutants.py runs the section that must catch a mutant).
	const char* only = argc > 1 ? argv[1] : nullptr;
	auto section = [&](const char* name) { return !only || std::strcmp(only, name) == 0; };

	if (section("exhaustive")) {
		exhaustive<Role>(role_names, 3, role_values, "UuSsEeRrAaDdMmIiNnGgTt@`[{", 4);
		exhaustive<Mixed>(mixed_names, 12, mixed_values, "ArRgGbBzZ29_@`[{\xff", 4);
		exhaustive<Sizes>(sizes_names, 42, sizes_values, "JjDdRrLlQqCcEeSsHh@`", 4);
	}
	if (section("variants")) {
		case_variants<Role>(role_names, 3, role_values, rng);
		case_variants<Mixed>(mixed_names, 12, mixed_values, rng);
		case_variants<Sizes>(sizes_names, 42, sizes_values, rng, 24);
	}
	if (section("bytes")) {
		every_byte<Role>(role_names, 3, role_values);
		every_byte<Mixed>(mixed_names, 12, mixed_values);
		every_byte<Sizes>(sizes_names, 42, sizes_values);
	}
	if (section("pieces")) {
		lower_in_words(rng64);
		short_names_lower();
		hash_agrees(rng64);
		probe_lengths();
		no_entry_matches_nothing(rng64);
		entries_match_probes<Role>(role_names, 3, detail_Role::name_table);
		entries_match_probes<Mixed>(mixed_names, 12, detail_Mixed::name_table);
		entries_match_probes<Sizes>(sizes_names, 42, detail_Sizes::name_table);
	}
	if (section("synthetic")) {
		synthetic_collisions(rng);
	}
	if (section("random")) {
		// Random strings, random bytes, and random strings built from member-name pieces.
		for (int iter = 0; iter < 200000; ++iter) {
			std::string s(rng() % 24, '\0');
			for (auto& c : s) { c = static_cast<char>(rng()); }
			check_name<Mixed>(mixed_names, 12, mixed_values, s);
			check_name<Role>(role_names, 3, role_values, s);
			check_name<Sizes>(sizes_names, 42, sizes_values, s);
			std::string t;
			while (t.size() < 24 && rng() % 5) { t += mixed_names[rng() % 12]; }
			check_name<Mixed>(mixed_names, 12, mixed_values, t);
		}
	}
	if (section("forged")) {
		forged<Role>(role_names, 3, role_values);
		forged<Mixed>(mixed_names, 12, mixed_values);
		forged<Sizes>(sizes_names, 42, sizes_values);
	}

	std::printf("enum names OK: exhaustive, case variants, every byte, synthetic and forged collisions, random\n");
	return 0;
}
