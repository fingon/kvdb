cmake_minimum_required(VERSION 2.8)
project(kvdb_test C)

include_directories(../src)

add_executable(ihash_test ihash_test.c)
target_link_libraries(ihash_test ${KVDB_L})
add_test(ihash ihash_test)
add_dependencies(check ihash_test)

add_executable(stringset_test stringset_test.c)
target_link_libraries(stringset_test ${KVDB_L})
add_test(stringset stringset_test)
add_dependencies(check stringset_test)

add_executable(kvdb_test kvdb_test.c)
target_link_libraries(kvdb_test ${KVDB_L})
add_test(kvdb kvdb_test)
add_dependencies(check kvdb_test)

add_executable(kvdb_index_test kvdb_index_test.c)
target_link_libraries(kvdb_index_test ${KVDB_L})
add_test(kvdb_index kvdb_index_test)
add_dependencies(check kvdb_index_test)



