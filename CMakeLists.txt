cmake_minimum_required(VERSION 3.13)

project(sqlite-starter-cpp)

set(CMAKE_CXX_STANDARD 23) # Enable the C++23 standard

file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS src/*.cpp src/*.hpp)

add_executable(sqlite ${SOURCE_FILES})

# src/ is an include root so layered directories can reach each other by role:
# src/utils/... includes "types/query.hpp", src/query_executor.cpp includes
# "utils/value_compare.hpp". Rule 3: directories are layered by role.
target_include_directories(sqlite PRIVATE src)