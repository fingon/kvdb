#
# $Id: Makefile $
#
# Author: Markus Stenberg <fingon@iki.fi>
#
# Copyright (c) 2013 Markus Stenberg
#
# Created:       Wed Jul 24 11:49:47 2013 mstenber
# Last modified: Wed Jul 24 17:59:25 2013 mstenber
# Edit time:     19 min
#
#

EXTERNAL_SQLITE_L=-lsqlite3

CFLAGS=-Wall -g -Os
LDFLAGS=$(EXTERNAL_SQLITE_L)

TESTS=kvdb_test ihash_test stringset_test
KVDB_A_OBJS=kvdb.o kvdb_o.o ihash.o stringset.o
BINARIES=kvdb.a kvdb_admin $(TESTS)

all: $(BINARIES) test

kvdb_admin: kvdb.a kvdb_admin.o

test: $(TESTS)
	@for BINARY in $(TESTS) ; do ./$$BINARY ; done

swig:
	@swig -Wall -lua kvdb.i -o kvdbc_lua.c
	@swig -Wall -python kvdb.i -o kvdb_python.c

kvdb_test: kvdb_test.o kvdb.a

ihash_test: ihash_test.o ihash.o

stringset_test: stringset_test.o stringset.o ihash.o

kvdb.a: $(KVDB_A_OBJS) $(INTERNAL_SQLITE_O)
	@rm -rf $@
	@ar rcs $@ $^
	@ranlib $@

clean:
	@rm -f *~ *.o thirdparty/*.o $(BINARIES)
