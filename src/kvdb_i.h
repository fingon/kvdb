/*
 * $Id: kvdb_i.h $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Wed Jul 24 13:27:34 2013 mstenber
 * Last modified: Sat Dec 21 08:59:34 2013 mstenber
 * Edit time:     32 min
 *
 */

#ifndef KVDB_I_H
#define KVDB_I_H

#include "kvdb.h"
#include "sqlite3.h"
#include "util.h"
#include "stringset.h"
#include "ihash.h"

/* stdc99 compatibility */
#ifndef typeof
#define typeof __typeof
#endif /* !typeof */

#include <libubox/list.h>
#include <libubox/utils.h>

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

#define APP_STRING "_app"
#define CLASS_STRING "_class"

struct kvdb_struct {
  /* SQLite 3 database handle */
  sqlite3 *db;

  const char *app_string;
  const char *class_string;

  /* Utility statements. */
  sqlite3_stmt *stmt_insert_log;
  sqlite3_stmt *stmt_insert_cs;
  sqlite3_stmt *stmt_insert_app_class;
  sqlite3_stmt *stmt_delete_cs;
  sqlite3_stmt *stmt_select_cs_by_oid;

  /* Most recent error text, if any */
  char *err;

  struct __packed {
    /* Boot number. Nothing to do with time, just incremented every
     * time kvdb is opened. This makes even simultaneous access mostly
     * work. */
    uint32_t boot;
    /* Sequence number of object creation within this boot. */
    uint32_t seq;
    /* Name of the host (used when creating unique IDs). */
    char name[KVDB_HOSTNAME_SIZE];
  } oidbase;

  /* Interned strings */
  stringset ss;

  /* oid -> o hash */
  ihash oid_ih;
};

struct kvdb_o_struct {
  /* List of attributes */
  struct list_head al;

  /* Where does the object live? */
  kvdb k;

  /* ! These should be sourced from the stringset so that there's
   * exactly one instance of the strings. We don't free them, we
   * simply assume stringset owns these. */
  const char *app;
  const char *cl;

  /* Fixed sized buffer of binary data. Probably should not be printed
   * as is. */
  unsigned char oid[KVDB_OID_SIZE];
};

typedef struct kvdb_o_a_struct {
  /* Within linked list */
  struct list_head lh;

  /* This should be sourced from the stringset so that there's exactly
   * one instance of the string. That way, we can just compare
   * pointers and no need to mess with e.g. strcmp. */
  const char *key;

  /* 'Owned' data for the value, stored here. */
  struct kvdb_typed_value_struct value;

} *kvdb_o_a;

void _kvdb_set_err(kvdb k, char *err);
void _kvdb_set_err_from_sqlite(kvdb k);
void _kvdb_set_err_from_sqlite2(kvdb k, const char *bonus);
bool _kvdb_run_stmt(kvdb k, sqlite3_stmt *stmt);
bool _kvdb_run_stmt_keep(kvdb k, sqlite3_stmt *stmt);
void _kvdb_o_free(kvdb_o o);

#endif /* KVDB_I_H */
