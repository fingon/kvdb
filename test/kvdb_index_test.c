/*
 * $Id: kvdb_index_test.c $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Sat Dec 21 16:54:26 2013 mstenber
 * Last modified: Sat Dec 21 17:10:42 2013 mstenber
 * Edit time:     14 min
 *
 */

#define DEBUG

#include "kvdb.h"
#include "util.h"

#include <unistd.h>

#define FILENAME "kvdb-test.dat"
#define N_OBJECTS 100

#define APP kvdb_define_app(k, "app")
#define CL kvdb_define_class(k, "cl")
#define KEY kvdb_define_key(k, "key", KVDB_INTEGER)
#define INDEX kvdb_define_index(k, KEY, "i64", KVDB_INTEGER_INDEX)

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

  r = kvdb_commit(k);
  KVASSERT(r, "kvdb_commit failed");
  return k;
}

void run_tests(kvdb k)
{
  kvdb_index i = INDEX;
  bool r;

  KVASSERT(i, "index definition failed");

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
