cmake_minimum_required(VERSION 3.14)
project(enum_reflection LANGUAGES CXX)

include(FetchContent)

# --- Dependencies ---
# enum.h does #include "phf.hh" (for the compile-time perfect hash that powers
# the enum <-> string dispatch) and #include "hashes.hh" (for the hhl/hmix/fhhl/
# fhmix hashing macros the generated switch tables are built on). Both are
# already-extracted standalone libraries; FetchContent pulls them in at the SHAs
# enum-reflection was extracted against and puts their headers on the include
# path so those includes resolve. hashes itself transitively FetchContents
# static-string + char-classify, so the whole stack comes from these two
# declarations.
FetchContent_Declare(
  constexpr_phf
  GIT_REPOSITORY https://github.com/Kronuz/constexpr-phf.git
  GIT_TAG        183081ea89f030f214348374c2cc3e5efb741933
)
FetchContent_Declare(
  hashes
  GIT_REPOSITORY https://github.com/Kronuz/hashes.git
  GIT_TAG        ffa2f5c7d5fa3ab576f1c32b43733ef3633199cd
)
FetchContent_MakeAvailable(constexpr_phf hashes)

# --- Library target ---
# enum-reflection is header-only: one header (enum.h) of preprocessor machinery
# and constexpr accessors. The target is a pure INTERFACE library that puts the
# source dir on the consumer's include path, requests cxx_std_20, and links the
# two deps so phf.hh / hashes.hh resolve and ride onto the consumer's include
# path too.
add_library(enum_reflection INTERFACE)
target_include_directories(enum_reflection INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(enum_reflection INTERFACE cxx_std_20)
target_link_libraries(enum_reflection INTERFACE constexpr_phf hashes::hashes)
# Alias so consumers can link `enum_reflection::enum_reflection` whether they add
# this via add_subdirectory or FetchContent.
add_library(enum_reflection::enum_reflection ALIAS enum_reflection)

# Only build the test when this repo is the top-level project, so consumers
# (e.g. via FetchContent) don't build our tests.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  enable_testing()
  add_executable(enum_reflection_test test/test.cc)
  target_link_libraries(enum_reflection_test PRIVATE enum_reflection)
  add_test(NAME enum_reflection COMMAND enum_reflection_test)

  # Name lookup against a brute-force oracle, including names forged to collide with a
  # member's hash (they must be a miss).
  add_executable(enum_reflection_names test/names_test.cc)
  target_link_libraries(enum_reflection_names PRIVATE enum_reflection)
  add_test(NAME enum_reflection_names COMMAND enum_reflection_names)

  # Mutation check of the name lookup: edits a copy of enum.h one way at a time (the length check,
  # the lower-casing, the verification itself, ...) and requires test/names_test.cc to fail for each.
  # It compiles the test once per edit, so it is off by default:
  #   cmake -B build -DENUM_REFLECTION_MUTANTS=ON && ctest --test-dir build -R mutants
  option(ENUM_REFLECTION_MUTANTS "Register the mutation check of the name lookup" OFF)
  if(ENUM_REFLECTION_MUTANTS)
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_FOUND)
      add_test(NAME enum_reflection_mutants
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test/mutants.py
                ${CMAKE_CXX_COMPILER} ${CMAKE_CURRENT_SOURCE_DIR} ${FETCHCONTENT_BASE_DIR} -std=c++20 -O1)
      set_tests_properties(enum_reflection_mutants PROPERTIES TIMEOUT 3600)
    else()
      message(STATUS "enum-reflection: no Python 3, the mutation check is not registered")
    endif()
  endif()

  # Lookup cost and comparison with the usual alternatives, and compile cost:
  #   cmake -B build -DENUM_REFLECTION_BUILD_BENCH=ON -DCMAKE_BUILD_TYPE=Release
  #   cmake --build build --target enum_reflection_bench && build/enum_reflection_bench
  # Abseil is only looked for here (find_package); without it the flat_hash_map rows are left out.
  option(ENUM_REFLECTION_BUILD_BENCH "Build the lookup benchmark" OFF)
  if(ENUM_REFLECTION_BUILD_BENCH)
    add_executable(enum_reflection_bench bench/bench.cc)
    target_include_directories(enum_reflection_bench PRIVATE bench)
    target_link_libraries(enum_reflection_bench PRIVATE enum_reflection)
    find_package(absl QUIET)
    if(absl_FOUND)
      target_link_libraries(enum_reflection_bench PRIVATE absl::flat_hash_map)
      target_compile_definitions(enum_reflection_bench PRIVATE HAVE_ABSL=1)
    endif()
    # magic_enum, the usual reflection library, as a comparator: fetched for this target only.
    option(ENUM_REFLECTION_BENCH_MAGIC_ENUM "Fetch magic_enum v0.9.8 as a benchmark-only comparator" OFF)
    if(ENUM_REFLECTION_BENCH_MAGIC_ENUM)
      include(FetchContent)
      set(MAGIC_ENUM_OPT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
      set(MAGIC_ENUM_OPT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
      set(MAGIC_ENUM_OPT_INSTALL OFF CACHE BOOL "" FORCE)
      FetchContent_Declare(magic_enum
        GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
        GIT_TAG        1384769c66bd16ec9bb1353f45fe8ec8ccc12dbd)  # v0.9.8
      FetchContent_MakeAvailable(magic_enum)
      target_link_libraries(enum_reflection_bench PRIVATE magic_enum::magic_enum)
      target_compile_definitions(enum_reflection_bench PRIVATE HAVE_MAGIC_ENUM=1)
    endif()
    # The answers of every row, without timing: identity for each member, no near miss accepted.
    add_test(NAME enum_reflection_bench_check COMMAND enum_reflection_bench --check)
    # The compile-time rows: build each of these on its own and time it.
    add_library(enum_reflection_ct OBJECT EXCLUDE_FROM_ALL bench/compile_time.cc)
    target_include_directories(enum_reflection_ct PRIVATE bench)
    target_link_libraries(enum_reflection_ct PRIVATE enum_reflection)
    add_library(enum_reflection_ct_base OBJECT EXCLUDE_FROM_ALL bench/compile_base.cc)
    target_link_libraries(enum_reflection_ct_base PRIVATE enum_reflection)
  endif()

  # A runnable tour of the library (not a test): build & run ./build/enum_reflection_demo
  add_executable(enum_reflection_demo examples/demo.cc)
  target_link_libraries(enum_reflection_demo PRIVATE enum_reflection)
endif()
