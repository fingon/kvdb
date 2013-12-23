/*
 * $Id: kvdb_test.c $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Wed Jul 24 13:26:37 2013 mstenber
 * Last modified: Mon Dec 23 17:31:49 2013 mstenber
 * Edit time:     54 min
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
#include <sys/stat.h>

#define FILENAME "kvdb-test.dat"
#define LOGDIR "/tmp/kvdb-logs"

#define APP kvdb_define_app(k, "app")
#define CL kvdb_define_class(k, "cl")
#define KEY kvdb_define_key(k, "key", KVDB_INTEGER)
#define VALUE 42

#define KEYS kvdb_define_key(k, "key2", KVDB_STRING)
#define VALUES "foo"
#define VALUES2 "foo bar baz too long string to be short I hope"

#define KEYO kvdb_define_key(k, "key3", KVDB_OBJECT)

void check_db(kvdb k, kvdb_oid oid, kvdb_oid oid2)
{
  kvdb_o o, o2;
  int64_t *v;
  const char *s;

  o = kvdb_get_o_by_id(k, oid);
  KVASSERT(o, "kvdb_get_o_by_id failed (no commit/no retry of data?)");

  o2 = kvdb_get_o_by_id(k, oid2);
  KVASSERT(o2, "kvdb_get_o_by_id failed (no commit/no retry of data?)");
  KVASSERT(o != o2, "o != o2");

  v = kvdb_o_get_int64(o, KEY);
  KVASSERT(v, "no value from kvdb_o_get_int64");
  KVASSERT(*v == VALUE, "invalid value from kvdb_o_get_int64 - %lld<>%d",
           *v, VALUE);

  s = kvdb_o_get_string(o, KEYS);
  KVASSERT(s, "no value from kvdb_o_get_string");
  KVASSERT(strcmp(s, VALUES) == 0, "invalid return from kvdb_o_get_string");

  s = kvdb_o_get_string(o2, KEYS);
  KVASSERT(s, "no value from kvdb_o_get_string");
  KVASSERT(strcmp(s, VALUES2) == 0, "invalid return from kvdb_o_get_string: %s", s);

  o2 = kvdb_o_get_object(o, KEYO);
  KVASSERT(o2, "no value from kvdb_o_get_object");

}

int main(int argc, char **argv)
{
  kvdb k;
  bool r;
  int rc;
  struct kvdb_oid_struct oid;
  struct kvdb_oid_struct oid2;
  kvdb_index i1, i2, i3;
  char buf[128];

  unlink(FILENAME);

  /* Clear out old logdir */
  sprintf(buf, "rm -rf '%s'", LOGDIR);
  system(buf);

  /* Create new empty one */
  rc = mkdir(LOGDIR, 0700);
  KVASSERT(!rc, "mkdir failed");

  /* Call to initialize the library (should be called only once) */
  r = kvdb_init();
  KVASSERT(r, "kvdb_init failed");

  /* First instantiation */
  r = kvdb_create(FILENAME, &k);
  KVASSERT(r, "kvdb_create call failed: %s", kvdb_strerror(k));
  /* XXX */
  /* Create object */
  KVDEBUG("creating object");
  kvdb_o o = kvdb_create_o(k, APP, CL);
  KVASSERT(o, "kvdb_create_o failed");

  kvdb_o o_2 = kvdb_get_o_by_id(k, &o->oid);
  KVASSERT(o == o_2, "kvdb_get_o_by_id failed");
  oid = o->oid;

  kvdb_o o2 = kvdb_create_o(k, APP, CL);
  KVASSERT(o2, "kvdb_create_o 2 failed");
  KVASSERT(o != o2, "must be different object");
  oid2 = o2->oid;

  r = kvdb_o_set_int64(o, KEY, VALUE);
  KVASSERT(r, "kvdb_o_set_int64 failed");
  KVASSERT(kvdb_key_get_type(KEY) == KVDB_INTEGER, "non-int?!?");


  r = kvdb_o_set_string(o, KEYS, VALUES);
  KVASSERT(r, "kvdb_o_set_string failed");
  KVASSERT(kvdb_key_get_type(KEYS) == KVDB_STRING, "non-string?!?");

  r = kvdb_o_set_string(o2, KEYS, VALUES2);
  KVASSERT(r, "kvdb_o_set_string failed");

  r = kvdb_o_set_object(o, KEYO, o2);
  KVASSERT(r, "kvdb_o_set_object failed");
  KVASSERT(kvdb_key_get_type(KEYO) == KVDB_OBJECT, "non-object?!?");

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

  /* Second one (but same file) */
  r = kvdb_create(FILENAME, &k);
  KVASSERT(r, "kvdb_create call failed: %s", kvdb_strerror(k));

  check_db(k, &oid, &oid2);

  /* Even own-only should work, even across instantiations */
  r = kvdb_export(k, LOGDIR, true);
  KVASSERT(r, "kvdb_export failed");

  kvdb_destroy(k);

  /* Third one, but fresh (same filename because I'm lazy) */
  unlink(FILENAME);

  r = kvdb_create(FILENAME, &k);
  KVASSERT(r, "kvdb_create call failed: %s", kvdb_strerror(k));

  r = kvdb_import(k, LOGDIR);
  KVASSERT(r, "kvdb_import failed");

  check_db(k, &oid, &oid2);

  kvdb_destroy(k);

  return 0;
}
