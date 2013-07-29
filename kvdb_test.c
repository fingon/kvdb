/*
 * $Id: kvdb_test.c $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Wed Jul 24 13:26:37 2013 mstenber
 * Last modified: Mon Jul 29 16:37:25 2013 mstenber
 * Edit time:     14 min
 *
 */

/* Unit test for kvdb.[ch].
 *
 * We try to exercise the whole API, and still not leak memory, or do
 * bad things valgrind complains about.
 */

#define DEBUG
#include "kvdb_i.h"
#include <unistd.h>
#include <string.h>

#define FILENAME "kvdb-test.dat"

#define APP "app"
#define CL "cl"
#define KEY kvdb_intern(k, "key")
#define VALUE 42

int main(int argc, char **argv)
{
  kvdb k;
  bool r;
  char *oid;
  int64_t *v;
  
  unlink(FILENAME);


  /* First instantiation */
  r = kvdb_init();
  KVASSERT(r, "kvdb_init failed");

  r = kvdb_create(FILENAME, &k);
  KVASSERT(r, "kvdb_create call failed: %s", kvdb_strerror(k));
  /* XXX */
  /* Create object */
  kvdb_o o = kvdb_create_o(k, APP, CL);
  KVASSERT(o, "kvdb_create_o failed");

  kvdb_o o2 = kvdb_get_o_by_id(k, o->oid);
  KVASSERT(o == o2, "kvdb_get_o_by_id failed");
  oid = strdup(o->oid);

  r = kvdb_o_set_int64(o, KEY, VALUE);
  KVASSERT(r, "kvdb_o_set_int64 failed");

  r = kvdb_commit(k);
  KVASSERT(r, "kvdb_commit failed");

  kvdb_destroy(k);

  /* Second one */
  r = kvdb_init();
  KVASSERT(r, "kvdb_init failed");

  r = kvdb_create(FILENAME, &k);
  KVASSERT(r, "kvdb_create call failed: %s", kvdb_strerror(k));

  o = kvdb_get_o_by_id(k, oid);
  KVASSERT(o, "kvdb_get_o_by_id failed (no commit/no retry of data?)");

  v = kvdb_o_get_int64(o, KEY);
  KVASSERT(v, "no value from kvdb_o_get_int64");
  KVASSERT(*v == VALUE, "invalid value from kvdb_o_get_int64 - %lld<>%d",
           *v, VALUE);

  kvdb_destroy(k);

  return 0;
}
