/*
 * $Id: kvdb_i.h $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Wed Jul 24 13:27:34 2013 mstenber
 * Last modified: Sat Dec 21 16:33:52 2013 mstenber
 * Edit time:     51 min
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

#define SQLITE_CALL2(c,err)             \
do {                                    \
  int rc = c;                           \
  if (rc)                               \
    {                                   \
      _kvdb_set_err_from_sqlite(k);     \
      err;                              \
    }                                   \
 } while(0)

#define SQLITE_CALLR2(c,errv) SQLITE_CALL2(c, return errv)
#define SQLITE_CALL(c) SQLITE_CALLR2(c, false)

#define SQLITE_EXEC2(q,err)                             \
do {                                                    \
  char *errmsg;                                         \
  int rc = sqlite3_exec(k->db, q, NULL, NULL, &errmsg); \
  if (rc)                                               \
    {                                                   \
      _kvdb_set_err(k, errmsg);                         \
      sqlite3_free(errmsg);                             \
      err;                                              \
    }                                                   \
 } while(0)

#define SQLITE_EXECR2(q,errv) SQLITE_EXEC2(q, return errv)
#define SQLITE_EXEC(q) SQLITE_EXECR2(q, false)

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
  sqlite3_stmt *stmt_select_cs_by_key;

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
  struct kvdb_oid_struct oid;
};

typedef struct kvdb_o_a_struct {
  /* Within linked list */
  struct list_head lh;

  /* Owned by stringset in kvdb */
  kvdb_key key;

  /* 'Owned' data for the value, stored here. */
  struct kvdb_typed_value_struct value;

} *kvdb_o_a;

struct __packed kvdb_app_struct {
  /* The rest is name within stringset */
  char name[0];
};

struct __packed kvdb_class_struct {
  /* The rest is name within stringset */
  char name[0];
};

struct __packed kvdb_key_struct {
  /* List of indexes associated with this key. */
  struct list_head index_lh;

  /* The kvdb_type this key should be. */
  kvdb_type type;

  /* The rest is name within stringset */
  char name[0];
};

struct kvdb_index_struct {
  /* List header within kvdb_key */
  struct list_head lh;

  /* Associated key */
  kvdb_key key;

  /* Type of index */
  kvdb_index_type type;

  /* Prepared statement to remove by oid */
  sqlite3_stmt *stmt_delete;

  /* Prepared statement to insert oid + keyish */
  sqlite3_stmt *stmt_insert;

  char name[KVDB_INDEX_NAME_SIZE];
};

void _kvdb_set_err(kvdb k, char *err);
void _kvdb_set_err_from_sqlite(kvdb k);
void _kvdb_set_err_from_sqlite2(kvdb k, const char *bonus);
bool _kvdb_run_stmt(kvdb k, sqlite3_stmt *stmt);
bool _kvdb_run_stmt_keep(kvdb k, sqlite3_stmt *stmt);
void _kvdb_o_free(kvdb_o o);
kvdb_o_a _kvdb_o_get_a(kvdb_o o, kvdb_key key);

/* Within kvdb_index.c */
bool _kvdb_handle_delete_indexes(kvdb_o o, kvdb_key k);
bool _kvdb_handle_insert_indexes(kvdb_o o, kvdb_key k);

#endif /* KVDB_I_H */
