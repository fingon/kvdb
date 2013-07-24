/*
 * $Id: kvdb_i.h $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Wed Jul 24 13:27:34 2013 mstenber
 * Last modified: Wed Jul 24 17:56:59 2013 mstenber
 * Edit time:     5 min
 *
 */

#ifndef KVDB_I_H
#define KVDB_I_H

#include "kvdb.h"
#include "sqlite3.h"
#include "util.h"
#include "stringset.h"
#include "ihash.h"

#define KVDB_HOSTNAME_SIZE 8

struct kvdb_struct {
  /* SQLite 3 database handle */
  sqlite3 *db;

  /* Most recent error text, if any */
  char *err;

  /* Name of the host (used when creating unique IDs). */
  char name[KVDB_HOSTNAME_SIZE];
  int boot;
  int seq;

  /* Interned strings */
  stringset ss;

  /* oid -> o hash */
  ihash oid_ih;
};

void _kvdb_set_err(kvdb k, char *err);
void _kvdb_set_err_from_sqlite(kvdb k);

#endif /* KVDB_I_H */
