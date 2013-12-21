/*
 * $Id: kvdb_index_test.c $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Sat Dec 21 16:54:26 2013 mstenber
 * Last modified: Sat Dec 21 19:37:53 2013 mstenber
 * Edit time:     34 min
 *
 */

#define DEBUG

#include "kvdb.h"
#include "kvdb_i.h"
#include "util.h"

#include <unistd.h>

#define FILENAME "kvdb-test.dat"
#define N_OBJECTS 100
#define N_FIXED_OBJECTS 3

#define APP kvdb_define_app(k, "app")
#define CL kvdb_define_class(k, "cl")
#define KEY kvdb_define_key(k, "key", KVDB_INTEGER)
#define KEYO kvdb_define_key(k, "key2", KVDB_OBJECT)
#define INDEX kvdb_define_index(k, KEY, "i64", KVDB_INTEGER_INDEX)
#define INDEXO kvdb_define_index(k, KEYO, "o", KVDB_OBJECT_INDEX)

/* There is a number of different combinations of things we should check:

   when to create index?
   - adding index first
   - adding index later (after object created)

   functioning of indexes
   - honoring of lower and upper bounds

   removal of index entries
   - when value changes

   ( XXX persistence of indexes )
   - using index at same time it was created
   - using index on next run
*/

void add_dummies(kvdb k, int start, int end)
{
  int i;
  for (i = start ; i < end ; i++)
    {
      kvdb_o o = kvdb_create_o(k, APP, CL);
      KVASSERT(o, "kvdb_create_o failed");
      kvdb_o_set_int64(o, KEY, i);
    }
}

kvdb create_test_db()
{
  kvdb k;
  bool r;

  r = kvdb_create(FILENAME, &k);
  KVASSERT(r, "kvdb_create call failed: %s", kvdb_strerror(k));
  add_dummies(k, N_OBJECTS, 3 * N_OBJECTS / 2);

  kvdb_index i = INDEX;
  KVASSERT(i, "index creation failed");

  add_dummies(k, 3 * N_OBJECTS / 2, 2 * N_OBJECTS);

  kvdb_index io = INDEXO;
  KVASSERT(io, "index creation failed");

  kvdb_o o3 = kvdb_create_o(k, APP, CL);
  kvdb_o_set_int64(o3, KEY, 44);

  kvdb_o o2 = kvdb_create_o(k, APP, CL);
  kvdb_o_set_int64(o2, KEY, 43);
  kvdb_o_set_object(o2, KEYO, o3);

  kvdb_o o = kvdb_create_o(k, APP, CL);
  kvdb_o_set_int64(o, KEY, 42);
  kvdb_o_set_object(o, KEYO, o2);

  r = kvdb_commit(k);
  KVASSERT(r, "kvdb_commit failed");
  return k;
}

void run_tests(kvdb k)
{
  int64_t bi, lv;
  int64_t *ip;
  struct kvdb_typed_value_struct v1, v2, v;
  kvdb_query q;
  int c;
  kvdb_o o;
  struct kvdb_oid_struct oid_43;
  kvdb_index i = INDEX;
  KVASSERT(i, "index definition failed");

  kvdb_index io = INDEXO;
  KVASSERT(io, "index creation failed");

  /* Minimalist query or two. */

  bi = 3 * N_OBJECTS / 2 + 2;
  kvdb_tv_set_int64(&v1, bi);
  bi += 10;
  kvdb_tv_set_int64(&v2, bi);

  /* No indexes => should return all objects */
  q = kvdb_create_q(k);
  KVASSERT(q, "kvdb query create failed");
  c = 0;
  lv = 0;
  while ((o = kvdb_q_get_next(q)))
    {
      if (*kvdb_o_get_int64(o, KEY) == 43)
        oid_43 = o->oid;
      c++;
    }
  KVASSERT(c == N_OBJECTS + N_FIXED_OBJECTS, "wrong number of results");
  kvdb_q_destroy(q);

  /* Should get 11 objects with fairly minimalist query like this */
  q = kvdb_create_q(k);
  KVASSERT(q, "kvdb query create failed");
  kvdb_q_add_index(q, i, &v1, &v2);
  kvdb_q_order_by(q, i, true);
  c = 0;
  lv = 0;
  while ((o = kvdb_q_get_next(q)))
    {
      ip = kvdb_o_get_int64(o, KEY);
      KVASSERT(ip, "kvdb_o_get_int64 failed");
      bi = *ip;
      KVDEBUG("got %lld", bi);
      KVASSERT(!lv || lv < bi, "ordering error");
      lv = bi;
      c++;
    }
  KVASSERT(c == 11, "wrong number of results");
  kvdb_q_destroy(q);

  /* Should get 11 objects with fairly minimalist query like this */
  q = kvdb_create_q(k);
  KVASSERT(q, "kvdb query create failed");
  kvdb_q_add_index(q, i, &v1, &v2);
  kvdb_q_order_by(q, i, false);
  c = 0;
  lv = 0;
  while ((o = kvdb_q_get_next(q)))
    {
      ip = kvdb_o_get_int64(o, KEY);
      KVASSERT(ip, "kvdb_o_get_int64 failed");
      bi = *ip;
      KVDEBUG("got %lld", bi);
      KVASSERT(!lv || lv > bi, "ordering error");
      lv = bi;
      c++;
    }
  KVASSERT(c == 11, "wrong number of results");
  kvdb_q_destroy(q);


  /* Test that generating stuff for multiple things works fine too. */
  q = kvdb_create_q(k);
  KVASSERT(q, "kvdb query create failed");
  kvdb_tv_set_oid(&v, &oid_43);
  kvdb_q_add_index(q, io, &v, NULL);
  kvdb_q_add_index(q, io, &v, NULL);
  c = 0;
  lv = 0;
  while ((o = kvdb_q_get_next(q)))
    {
      c++;
    }
  KVASSERT(c == 1, "wrong number of results");
  kvdb_q_destroy(q);
  
}

int main(int argc, char **argv)
{
  kvdb k;
  bool r;

  unlink(FILENAME);

  /* Call to initialize the library (should be called only once) */
  r = kvdb_init();
  KVASSERT(r, "kvdb_init failed");

  /* First run tests on 'fresh' database */
  k = create_test_db();
  run_tests(k);
  kvdb_destroy(k);

  /* Then with 'old' database */
  KVDEBUG("retrying with 'old' database");
  r = kvdb_create(FILENAME, &k);
  KVASSERT(r, "kvdb_create call failed: %s", kvdb_strerror(k));
  run_tests(k);
  kvdb_destroy(k);

  return 0;
}
