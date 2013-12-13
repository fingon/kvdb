/*
 * $Id: kvdb_admin.c $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Wed Jul 24 11:57:27 2013 mstenber
 * Last modified: Wed Jul 24 12:12:20 2013 mstenber
 * Edit time:     3 min
 *
 */

#include "kvdb.h"

#include <stdio.h>

/* XXX - add some better default path name based on the home directory
 * of the user. e.g. ~/.ms/kvdb.dat. */

int main(int argc, char **argv)
{
  kvdb k;
  bool r = kvdb_create("kvdb.dat", &k);
  if (!r)
    {
      fprintf(stderr, "unable to create kvdb: %s", kvdb_strerror(k));
      return 1;
    }
  /* XXX */
  kvdb_destroy(k);
  return 0;
}
