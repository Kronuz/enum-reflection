// Compile cost of the lookups: the accessors of four enums (8, 32 and 128 members, and a sparse
// one of 32) in one translation unit. Compare with compile_base.cc, which only includes the
// header of the enums' names:
//   cmake --build build --target enum_reflection_ct_base enum_reflection_ct --clean-first
// or time `c++ -std=c++20 -O2 -c bench/compile_time.cc` with the include paths of the build.
#include "enums.h"

std::string_view name_of(E8 e) { return enum_name(e); }
E8 value_of_e8(std::string_view s) { return enum_type<E8>(s); }
std::string_view name_of(E32 e) { return enum_name(e); }
E32 value_of_e32(std::string_view s) { return enum_type<E32>(s); }
std::string_view name_of(E128 e) { return enum_name(e); }
E128 value_of_e128(std::string_view s) { return enum_type<E128>(s); }
std::string_view name_of(S32 e) { return enum_name(e); }
S32 value_of_s32(std::string_view s) { return enum_type<S32>(s); }
