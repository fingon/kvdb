cmake_minimum_required(VERSION 2.8)
project(kvdb C)

add_definitions(-g -DDEBUG -DPARANOID -Wall -Wextra -Wno-unused-parameter -std=c99)

# These are needed for coverage to work; making them an option might be
# possible.
add_definitions(-fprofile-arcs -ftest-coverage)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")

# Reused definition for library in few places
set(KVDB_L kvdb sqlite3 ubox)

add_subdirectory(src)

# Enable unit clean build + testing with 'check' target
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})
enable_testing()
add_subdirectory(test)

# Coverage stuff (requires lcov)
add_custom_target(coverage_clean lcov -z -d .)
add_custom_target(coverage_build DEPENDS coverage_clean check)
add_custom_target(coverage_info lcov -c -d . -o coverage.info DEPENDS
coverage_build)
add_custom_target(coverage_html genhtml coverage.info --output-directory
coverage DEPENDS coverage_info)
add_custom_target(coverage open coverage/index.html DEPENDS coverage_html)

