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
  perfect_hash
  GIT_REPOSITORY https://github.com/Kronuz/perfect-hash.git
  GIT_TAG        main
)
FetchContent_Declare(
  hashes
  GIT_REPOSITORY https://github.com/Kronuz/hashes.git
  GIT_TAG        main
)
FetchContent_MakeAvailable(perfect_hash hashes)

# --- Library target ---
# enum-reflection is header-only: one header (enum.h) of preprocessor machinery
# and constexpr accessors. The target is a pure INTERFACE library that puts the
# source dir on the consumer's include path, requests cxx_std_20, and links the
# two deps so phf.hh / hashes.hh resolve and ride onto the consumer's include
# path too.
add_library(enum_reflection INTERFACE)
target_include_directories(enum_reflection INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(enum_reflection INTERFACE cxx_std_20)
target_link_libraries(enum_reflection INTERFACE perfect_hash hashes::hashes)
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

  # A runnable tour of the library (not a test): build & run ./build/enum_reflection_demo
  add_executable(enum_reflection_demo examples/demo.cc)
  target_link_libraries(enum_reflection_demo PRIVATE enum_reflection)
endif()
