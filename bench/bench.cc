// Cost of a name lookup: enum_type / enum_try_type on a hit, a miss, and a throwing miss.
// Build: cmake -B build -DENUM_REFLECTION_BENCH=ON -DCMAKE_BUILD_TYPE=Release
//        cmake --build build --target enum_reflection_bench && build/enum_reflection_bench
// Time it on an otherwise idle machine; each row is the median of 9 runs.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#include "enum.h"

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

int main() {
	rows<Method>("Method",
		{"GET", "POST", "PUT", "DELETE", "PATCH", "HEAD", "OPTIONS", "TRACE", "CONNECT", "SEARCH", "COUNT", "INFO", "DUMP", "RESTORE"},
		{"GETS", "FETCH", "PU", "REMOVE", "PATC", "HEADS", "OPTION", "TRACER", "CONNEC", "SEARC", "COUNTS", "INF", "DUMPS", "RESTOR"});
	rows<Status>("Status",
		{"Ok", "Created", "Accepted", "NoContent", "MovedPermanently", "NotModified", "BadRequest", "Unauthorized", "Forbidden",
		 "NotFound", "Teapot", "Internal_Server_Error", "ServiceUnavailable", "Gateway_Timeout"},
		{"Okay", "Create", "Accept", "NoContents", "MovedPermanent", "NotModifie", "BadRequests", "Unauthorize", "Forbidde",
		 "NotFoun", "Teapots", "Internal_Server_Errors", "ServiceUnavailabl", "Gateway_Timeouts"});
}
