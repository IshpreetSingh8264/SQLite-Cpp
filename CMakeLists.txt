cmake_minimum_required(VERSION 3.13)

project(sqlite-starter-cpp)

set(CMAKE_CXX_STANDARD 23) # Enable the C++23 standard

file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS src/*.cpp src/*.hpp)

# Everything except main() goes in a library so the fuzz probe can drive the
# real utils/ code instead of a copy of it. The `sqlite` target is unchanged:
# it is still main() plus all of src.
list(REMOVE_ITEM SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)
add_library(sqlitecore STATIC ${SOURCE_FILES})
# src/ is an include root so layered directories can reach each other by role:
# src/utils/... includes "types/query.hpp", src/query_executor.cpp includes
# "utils/value_compare.hpp". Rule 3: directories are layered by role.
target_include_directories(sqlitecore PUBLIC src)

add_executable(sqlite src/main.cpp)
target_link_libraries(sqlite PRIVATE sqlitecore)

# ---- tests -----------------------------------------------------------------
# tests/run.sh is the entrypoint and does the build itself; these targets exist
# so `ctest` works for anyone who expects it to.
# See tests/README.md for what the suites do and do not cover.
enable_testing()
find_package(Python3 COMPONENTS Interpreter REQUIRED)

# Drives sqlLikeMatch / columnAffinity / valueMatches directly. Going through
# the CLI would mean one process per case, and these functions are pure for
# exactly this reason.
add_executable(sqlite_probe tests/unit/sqlite_probe.cpp)
target_link_libraries(sqlite_probe PRIVATE sqlitecore)

add_test(NAME fuzz_utils
         COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/fuzz_utils.py
                 $<TARGET_FILE:sqlite_probe>)

add_test(NAME differential
         COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/differential/compare_cli.sh
                 $<TARGET_FILE:sqlite>)

add_test(NAME suite_size
         COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_size.py
                 ${CMAKE_CURRENT_SOURCE_DIR}/tests)
