/*
 * $Id: kvdb_i.h $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Wed Jul 24 13:27:34 2013 mstenber
 * Last modified: Sat Dec 21 13:58:18 2013 mstenber
 * Edit time:     36 min
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

  /* Used widely -> better to cache these */
  kvdb_key app_key, class_key;

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
    int32_t boot;
    /* Sequence number of object creation within this boot. */
    uint32_t seq;
    /* Name of the host (used when creating unique IDs). */
    char name[KVDB_HOSTNAME_SIZE];
  } oidbase;

  /* Interned strings */
  stringset ss_app;
  stringset ss_class;
  stringset ss_key;

  /* oid -> o hash */
  ihash oid_ih;
};

struct kvdb_o_struct {
  /* List of attributes */
  struct list_head al;

  /* Where does the object live? */
  kvdb k;

  /* These are from kvdb-owned stringsets -> no need to worry about
   * allocating them. */
  kvdb_app app;
  kvdb_class cl;

  /* Fixed sized buffer of binary data. Probably should not be printed
   * as is. */
  unsigned char oid[KVDB_OID_SIZE];
};

typedef struct kvdb_o_a_struct {
  /* Within linked list */
  struct list_head lh;

  /* Owned by stringset in kvdb */
  kvdb_key key;

  /* 'Owned' data for the value, stored here. */
  struct kvdb_typed_value_struct value;

} *kvdb_o_a;

struct kvdb_app_struct {
  /* The rest is name within stringset */
  char name[0];
};

struct kvdb_class_struct {
  /* The rest is name within stringset */
  char name[0];
};

struct kvdb_key_struct {
  kvdb_type type;
  /* The rest is name within stringset */
  char name[0];
};

void _kvdb_set_err(kvdb k, char *err);
void _kvdb_set_err_from_sqlite(kvdb k);
void _kvdb_set_err_from_sqlite2(kvdb k, const char *bonus);
bool _kvdb_run_stmt(kvdb k, sqlite3_stmt *stmt);
bool _kvdb_run_stmt_keep(kvdb k, sqlite3_stmt *stmt);
void _kvdb_o_free(kvdb_o o);

#endif /* KVDB_I_H */
