/*
 * $Id: kvdb_i.h $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Wed Jul 24 13:27:34 2013 mstenber
 * Last modified: Mon Jul 29 15:57:14 2013 mstenber
 * Edit time:     19 min
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

#define SQLITE_CALL2(c,errv)            \
do {                                    \
  int rc = c;                           \
  if (rc)                               \
    {                                   \
      _kvdb_set_err_from_sqlite(k);     \
      return errv;                      \
    }                                   \
 } while(0)

#define SQLITE_CALL(c) SQLITE_CALL2(c, false)


struct kvdb_struct {
  /* SQLite 3 database handle */
  sqlite3 *db;

  /* Utility statements. */
  sqlite3_stmt *stmt_insert_log;
  sqlite3_stmt *stmt_insert_cs;
  sqlite3_stmt *stmt_delete_cs;
  sqlite3_stmt *stmt_select_cs_by_oid;

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

struct kvdb_o_struct {
  /* Where does the object live? */
  kvdb k;

  /* ! These should be sourced from the stringset so that there's
   * exactly one instance of the strings. We don't free them, we
   * simply assume stringset owns these. */
  const char *app;
  const char *cl;

  /* This on the other hand is dynamically allocated string, owned by
   * the kvdb_o itself. Oid is hopefully unique within system. */
  const char *oid;

  /* List of attributes */
  struct kvdb_o_a_struct *al;
};

typedef struct kvdb_o_a_struct {
  /* This should be sourced from the stringset so that there's exactly
   * one instance of the string. That way, we can just compare
   * pointers and no need to mess with e.g. strcmp. */
  const char *key;

  /* 'Owned' data for the value, stored here. */
  struct kvdb_typed_value_struct value;

  /* Within linked list */
  struct kvdb_o_a_struct *prev, *next;
} *kvdb_o_a;

void _kvdb_set_err(kvdb k, char *err);
void _kvdb_set_err_from_sqlite(kvdb k);
void _kvdb_set_err_from_sqlite2(kvdb k, const char *bonus);
bool _kvdb_run_stmt(kvdb k, sqlite3_stmt *stmt);

#endif /* KVDB_I_H */
