cmake_minimum_required(VERSION 2.8)
project(kvdb C)

add_definitions(-g -DPARANOID -Wall -Wextra -Wno-unused-parameter -std=c99)

# Reused definition for library in few places
set(KVDB_L kvdb sqlite3)

add_subdirectory(src)

# Enable unit clean build + testing with 'check' target
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})
enable_testing()
add_subdirectory(test)


