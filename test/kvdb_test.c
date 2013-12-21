/*
 * $Id: kvdb_test.c $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Wed Jul 24 13:26:37 2013 mstenber
 * Last modified: Sat Dec 21 15:54:24 2013 mstenber
 * Edit time:     30 min
 *
 */

/* Unit test for kvdb.[ch].
 *
 * We try to exercise the whole API, and still not leak memory, or do
 * bad things valgrind complains about.
 */

#ifndef DEBUG
#define DEBUG
#endif /* !DEBUG */
#include "kvdb_i.h"
#include <unistd.h>
#include <string.h>

#define FILENAME "kvdb-test.dat"

#define APP kvdb_define_app(k, "app")
#define CL kvdb_define_class(k, "cl")
#define KEY kvdb_define_key(k, "key", KVDB_INTEGER)
#define VALUE 42

#define KEYS kvdb_define_key(k, "key2", KVDB_STRING)
#define VALUES "foo"

#define KEYO kvdb_define_key(k, "key3", KVDB_OBJECT)

int main(int argc, char **argv)
{
  kvdb k;
  bool r;
  struct kvdb_oid_struct oid;
  int64_t *v;
  const char *s;
  kvdb_index i1, i2, i3;

  unlink(FILENAME);


  /* First instantiation */
  r = kvdb_init();
  KVASSERT(r, "kvdb_init failed");

  r = kvdb_create(FILENAME, &k);
  KVASSERT(r, "kvdb_create call failed: %s", kvdb_strerror(k));
  /* XXX */
  /* Create object */
  KVDEBUG("creating object");
  kvdb_o o = kvdb_create_o(k, APP, CL);
  KVASSERT(o, "kvdb_create_o failed");

  kvdb_o o2 = kvdb_get_o_by_id(k, &o->oid);
  KVASSERT(o == o2, "kvdb_get_o_by_id failed");
  oid = o->oid;

  o2 = kvdb_create_o(k, APP, CL);
  KVASSERT(o2, "kvdb_create_o 2 failed");
  KVASSERT(o != o2, "must be different object");

  r = kvdb_o_set_int64(o, KEY, VALUE);
  KVASSERT(r, "kvdb_o_set_int64 failed");

  r = kvdb_o_set_string(o, KEYS, VALUES);
  KVASSERT(r, "kvdb_o_set_string failed");

  r = kvdb_o_set_object(o, KEYO, o2);
  KVASSERT(r, "kvdb_o_set_object failed");

  /* Intentionally define search indexes only after setting stuff up
   * within objects (this should still work). */
  i1 = kvdb_define_index(k, KEY, "i64", KVDB_INTEGER_INDEX);
  KVASSERT(i1, "kvdb_define_index failed");

  i2 = kvdb_define_index(k, KEYO, "o", KVDB_OBJECT_INDEX);
  KVASSERT(i2, "kvdb_define_index 2 failed");

  i3 = kvdb_define_index(k, KEYS, "v", KVDB_VALUE_INDEX);
  KVASSERT(!i3, "kvdb_define_index 3 should have failed");

  r = kvdb_commit(k);
  KVASSERT(r, "kvdb_commit failed");

  kvdb_destroy(k);

  /* Second one */
  r = kvdb_init();
  KVASSERT(r, "kvdb_init failed");

  r = kvdb_create(FILENAME, &k);
  KVASSERT(r, "kvdb_create call failed: %s", kvdb_strerror(k));

  o = kvdb_get_o_by_id(k, &oid);
  KVASSERT(o, "kvdb_get_o_by_id failed (no commit/no retry of data?)");

  v = kvdb_o_get_int64(o, KEY);
  KVASSERT(v, "no value from kvdb_o_get_int64");
  KVASSERT(*v == VALUE, "invalid value from kvdb_o_get_int64 - %lld<>%d",
           *v, VALUE);

  s = kvdb_o_get_string(o, KEYS);
  KVASSERT(s, "no value from kvdb_o_get_string");
  KVASSERT(strcmp(s, VALUES) == 0, "invalid return from kvdb_o_get_string");

  o2 = kvdb_o_get_object(o, KEYO);
  KVASSERT(o2, "no value from kvdb_o_get_object");

  kvdb_destroy(k);

  return 0;
}
