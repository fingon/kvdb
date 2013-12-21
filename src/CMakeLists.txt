cmake_minimum_required(VERSION 2.8)
project(kvdb_src C)

set(KVDB_C kvdb.c kvdb_index.c kvdb_o.c ihash.c stringset.c)

# Create the base library
add_library(kvdb STATIC ${KVDB_C})

# And binary which uses it
add_executable(kvdb_admin kvdb_admin.c)
target_link_libraries(kvdb_admin ${KVDB_L})

# XXX - add swig target
