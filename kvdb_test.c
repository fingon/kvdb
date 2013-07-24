/*
 * $Id: kvdb_test.c $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Wed Jul 24 13:26:37 2013 mstenber
 * Last modified: Wed Jul 24 19:07:20 2013 mstenber
 * Edit time:     7 min
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

#define FILENAME "kvdb-test.dat"

int main(int argc, char **argv)
{
  kvdb k;
  bool r;

  unlink(FILENAME);

  r = kvdb_init();
  KVASSERT(r, "kvdb_init failed");

  r = kvdb_create(FILENAME, &k);
  KVASSERT(r, "kvdb_create call failed: %s", kvdb_strerror(k));
  /* XXX */
  /* Create object */
  kvdb_o o = kvdb_create_o(k, "app", "cl");
  KVASSERT(o, "kvdb_create_o failed");

  kvdb_destroy(k);
  return 0;
}
