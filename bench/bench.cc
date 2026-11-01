// Name and value lookups of enum-reflection:
//   1. the cost of a name lookup (enum_type, enum_try_type) on a hit, a lower-case hit, a miss
//      and a throwing miss, for two enums that look like HTTP methods and status codes;
//   2. the lookups against the usual alternatives for enums of 8, 32 and 128 members and a
//      sparse one of 32: an array indexed by value, a binary search by value, a linear scan
//      of the names (what magic_enum-style libraries do), a sorted array of names, and
//      absl::flat_hash_map when Abseil is found and magic_enum (magic_enum::enum_cast and
//      enum_name) when built with ENUM_REFLECTION_BENCH_MAGIC_ENUM; both are optional and only
//      this target uses them. Every comparator must return what enum-reflection returns.
// Build: cmake -B build -DENUM_REFLECTION_BUILD_BENCH=ON -DCMAKE_BUILD_TYPE=Release
//        cmake --build build --target enum_reflection_bench && build/enum_reflection_bench
// Time it on an otherwise idle machine; each row is the median of 9 runs.
// Abseil and magic_enum first: enum.h defines macros whose names their headers use.
#ifdef HAVE_ABSL
#include "absl/container/flat_hash_map.h"
#endif
#ifdef HAVE_MAGIC_ENUM
// magic_enum reflects the values in [MAGIC_ENUM_RANGE_MIN, MAGIC_ENUM_RANGE_MAX] (default -128..128);
// the sparse enum reaches 220.
#define MAGIC_ENUM_RANGE_MAX 256
#include "magic_enum/magic_enum.hpp"
#endif

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "enum.h"
#include "enums.h"

ENUM_CLASS(Method, int,
	GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS, TRACE, CONNECT, SEARCH, COUNT, INFO, DUMP, RESTORE
)

ENUM_CLASS(Status, int,
	Ok, Created, Accepted, NoContent, MovedPermanently, NotModified, BadRequest, Unauthorized, Forbidden,
	NotFound, Teapot, Internal_Server_Error, ServiceUnavailable, Gateway_Timeout
)

template <typename T>
static inline void use(const T& v) { asm volatile("" : : "r,m"(v) : "memory"); }

// Median ns per lookup over 9 runs of about `total` lookups cycling through `names`.
template <typename F>
static double measure(const std::vector<std::string>& names, F&& lookup, std::size_t total = 2000000) {
	const std::size_t rounds = std::max<std::size_t>(1, total / names.size());
	std::vector<double> runs;
	for (int r = 0; r < 9; ++r) {
		auto t0 = std::chrono::steady_clock::now();
		for (std::size_t i = 0; i < rounds; ++i) {
			for (const auto& n : names) { lookup(std::string_view(n)); }
		}
		double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count();
		runs.push_back(ns / double(rounds * names.size()));
	}
	std::sort(runs.begin(), runs.end());
	return runs[4];
}

// Median ns per operation over 9 runs of `f(n)`, which performs n operations.
template <typename F>
static double measure_ops(F&& f, std::size_t n = 1000000) {
	std::vector<double> runs;
	for (int r = 0; r < 9; ++r) {
		auto t0 = std::chrono::steady_clock::now();
		f(n);
		runs.push_back(std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / double(n));
	}
	std::sort(runs.begin(), runs.end());
	return runs[4];
}

static std::string lower(std::string s) {
	for (auto& c : s) { if (c >= 'A' && c <= 'Z') { c = char(c + 32); } }
	return s;
}

template <typename E>
static void rows(const char* label, const std::vector<std::string>& members, const std::vector<std::string>& misses) {
	std::vector<std::string> lowered;
	for (const auto& m : members) { lowered.push_back(lower(m)); }
	auto try_type = [](std::string_view n) { auto r = enum_try_type<E>(n); use(r); };
	auto type = [](std::string_view n) { try { auto r = enum_type<E>(n); use(r); } catch (const std::out_of_range&) { use(0); } };
	auto type_hit = [](std::string_view n) { auto r = enum_type<E>(n); use(r); };
	std::printf("%-10s hit  enum_type (exact case)  %7.2f ns\n", label, measure(members, type_hit));
	std::printf("%-10s hit  enum_try_type           %7.2f ns\n", label, measure(members, try_type));
	std::printf("%-10s hit  enum_try_type (lower)   %7.2f ns\n", label, measure(lowered, try_type));
	std::printf("%-10s miss enum_try_type           %7.2f ns\n", label, measure(misses, try_type));
	std::printf("%-10s miss enum_type (throws)      %7.2f ns\n", label, measure(misses, type, 20000));
}

static bool check_only = false;

// Lookups of one enum against the alternatives. `tab` lists the members; `dense` says the values
// are 0..N-1, which the array row needs.
template <typename E, std::size_t N>
static void compare(const char* label, const std::pair<std::string_view, int> (&tab)[N], bool dense) {
	std::mt19937 rng(5);
	std::vector<std::string> names;
	std::vector<int> vals;
	for (const auto& [n, v] : tab) { names.emplace_back(n); vals.push_back(v); }
	std::vector<std::size_t> order(N);
	for (std::size_t i = 0; i < N; ++i) { order[i] = i; }
	std::shuffle(order.begin(), order.end(), rng);

	// Identity, and near misses (one letter changed) must all be rejected. A probe that equals a
	// member's name up to case is skipped: lookups ignore case.
	std::vector<std::string> lowered_names;
	for (const auto& n : names) { lowered_names.push_back(lower(n)); }
	std::size_t accepted = 0, probes = 0;
	for (std::size_t i = 0; i < N; ++i) {
		if (enum_name(E(vals[i])) != names[i] || enum_type<E>(names[i]) != E(vals[i])) { std::printf("MISMATCH %s %zu\n", label, i); std::exit(1); }
#ifdef HAVE_MAGIC_ENUM
		if (magic_enum::enum_name(E(vals[i])) != names[i] || magic_enum::enum_cast<E>(names[i]) != std::optional<E>(E(vals[i]))) { std::printf("MAGIC_ENUM MISMATCH %s %zu\n", label, i); std::exit(1); }
#endif
		for (int k = 0; k < 8; ++k) {
			std::string s = names[i];
			s[rng() % s.size()] = char('a' + rng() % 26);
			if (std::find(lowered_names.begin(), lowered_names.end(), lower(s)) != lowered_names.end()) { continue; }
			++probes;
			if (enum_try_type<E>(s)) { ++accepted; }
#ifdef HAVE_MAGIC_ENUM
			if (magic_enum::enum_cast<E>(s)) { std::printf("MAGIC_ENUM FALSE ACCEPT %s %s\n", label, s.c_str()); std::exit(1); }
#endif
		}
	}
	std::printf("%-10s near-miss probes %zu, accepted %zu\n", label, probes, accepted);
	if (accepted != 0) { std::printf("FALSE ACCEPT %s\n", label); std::exit(1); }
	if (check_only) { return; }

	std::vector<std::pair<std::string_view, int>> sorted(tab, tab + N);
	std::sort(sorted.begin(), sorted.end());
	std::vector<std::pair<int, std::string_view>> by_value;
	for (const auto& [n, v] : tab) { by_value.push_back({v, n}); }
	std::sort(by_value.begin(), by_value.end());
	std::vector<std::string_view> array(dense ? N : 0);
	if (dense) { for (const auto& [n, v] : tab) { array[v] = n; } }
	std::vector<std::string> misses;
	for (std::size_t i = 0; i < N; ++i) { misses.push_back("Nope" + std::to_string(i) + "zzz"); }
#ifdef HAVE_ABSL
	absl::flat_hash_map<std::string_view, int> map;
	for (const auto& [n, v] : tab) { map[n] = v; }
#endif
	auto row = [&](const char* what, const char* impl, double ns) { std::printf("%-10s %-22s %-28s %8.2f ns\n", label, what, impl, ns); };

	row("name(value)", "enum_name", measure_ops([&](std::size_t n) { std::size_t s = 0; for (std::size_t i = 0; i < n; ++i) { s += enum_name(E(vals[order[i % N]])).size(); } use(s); }));
	if (dense) { row("name(value)", "array by value", measure_ops([&](std::size_t n) { std::size_t s = 0; for (std::size_t i = 0; i < n; ++i) { s += array[vals[order[i % N]]].size(); } use(s); })); }
#ifdef HAVE_MAGIC_ENUM
	row("name(value)", "magic_enum::enum_name", measure_ops([&](std::size_t n) { std::size_t s = 0; for (std::size_t i = 0; i < n; ++i) { s += magic_enum::enum_name(E(vals[order[i % N]])).size(); } use(s); }));
#endif
	row("name(value)", "binary search by value", measure_ops([&](std::size_t n) {
		std::size_t s = 0;
		for (std::size_t i = 0; i < n; ++i) { auto it = std::lower_bound(by_value.begin(), by_value.end(), std::make_pair(vals[order[i % N]], std::string_view())); s += it->second.size(); }
		use(s);
	}));

	row("value(name) hit", "enum_type", measure_ops([&](std::size_t n) { int s = 0; for (std::size_t i = 0; i < n; ++i) { s += int(enum_type<E>(std::string_view(names[order[i % N]]))); } use(s); }));
	row("value(name) hit", "linear scan", measure_ops([&](std::size_t n) {
		int s = 0;
		for (std::size_t i = 0; i < n; ++i) { std::string_view q = names[order[i % N]]; for (const auto& [nm, v] : tab) { if (nm == q) { s += v; break; } } }
		use(s);
	}));
	row("value(name) hit", "sorted array", measure_ops([&](std::size_t n) {
		int s = 0;
		for (std::size_t i = 0; i < n; ++i) { std::string_view q = names[order[i % N]]; auto it = std::lower_bound(sorted.begin(), sorted.end(), q, [](const auto& a, std::string_view b) { return a.first < b; }); s += it->second; }
		use(s);
	}));
#ifdef HAVE_MAGIC_ENUM
	row("value(name) hit", "magic_enum::enum_cast", measure_ops([&](std::size_t n) { int s = 0; for (std::size_t i = 0; i < n; ++i) { s += int(*magic_enum::enum_cast<E>(std::string_view(names[order[i % N]]))); } use(s); }));
#endif
#ifdef HAVE_ABSL
	row("value(name) hit", "absl::flat_hash_map", measure_ops([&](std::size_t n) { int s = 0; for (std::size_t i = 0; i < n; ++i) { s += map.find(std::string_view(names[order[i % N]]))->second; } use(s); }));
#endif

	row("value(name) miss", "enum_try_type", measure_ops([&](std::size_t n) { int s = 0; for (std::size_t i = 0; i < n; ++i) { s += enum_try_type<E>(std::string_view(misses[i % N])).has_value(); } use(s); }));
	row("value(name) miss", "enum_type (throws)", measure_ops([&](std::size_t n) {
		int s = 0;
		for (std::size_t i = 0; i < n; ++i) { try { s += int(enum_type<E>(std::string_view(misses[i % N]))); } catch (const std::out_of_range&) { ++s; } }
		use(s);
	}, 20000));
#ifdef HAVE_MAGIC_ENUM
	row("value(name) miss", "magic_enum::enum_cast", measure_ops([&](std::size_t n) { int s = 0; for (std::size_t i = 0; i < n; ++i) { s += magic_enum::enum_cast<E>(std::string_view(misses[i % N])).has_value(); } use(s); }));
#endif
#ifdef HAVE_ABSL
	row("value(name) miss", "absl::flat_hash_map", measure_ops([&](std::size_t n) { int s = 0; for (std::size_t i = 0; i < n; ++i) { s += map.find(std::string_view(misses[i % N])) == map.end(); } use(s); }));
#endif
}

int main(int argc, char** argv) {
	// `--check`: only the identity and near-miss checks of `compare`, no timing (the CTest test).
	check_only = argc > 1 && std::string_view(argv[1]) == "--check";
	if (!check_only) {
		rows<Method>("Method",
			{"GET", "POST", "PUT", "DELETE", "PATCH", "HEAD", "OPTIONS", "TRACE", "CONNECT", "SEARCH", "COUNT", "INFO", "DUMP", "RESTORE"},
			{"GETS", "FETCH", "PU", "REMOVE", "PATC", "HEADS", "OPTION", "TRACER", "CONNEC", "SEARC", "COUNTS", "INF", "DUMPS", "RESTOR"});
		rows<Status>("Status",
			{"Ok", "Created", "Accepted", "NoContent", "MovedPermanently", "NotModified", "BadRequest", "Unauthorized", "Forbidden",
			 "NotFound", "Teapot", "Internal_Server_Error", "ServiceUnavailable", "Gateway_Timeout"},
			{"Okay", "Create", "Accept", "NoContents", "MovedPermanent", "NotModifie", "BadRequests", "Unauthorize", "Forbidde",
			 "NotFoun", "Teapots", "Internal_Server_Errors", "ServiceUnavailabl", "Gateway_Timeouts"});
	}

	std::printf("\n");
	compare<E8>("8 dense", tab_E8, true);
	compare<E32>("32 dense", tab_E32, true);
	compare<E128>("128 dense", tab_E128, true);
	compare<S32>("32 sparse", tab_S32, false);
}
