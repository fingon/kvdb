/*
 * $Id: kvdb_i.h $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Wed Jul 24 13:27:34 2013 mstenber
 * Last modified: Mon Dec 23 17:24:35 2013 mstenber
 * Edit time:     69 min
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
  KVDEBUG("SQLITE_EXEC2 %s", q);                        \
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
#define KVDB_LOCAL_APP_STRING "_kvdb_local"

/* We employ same strategy for handling pretty much every type of
 * 'semi-static' data within kvdb object: We provide enums here for
 * indexing array that contains them, and then in kvdb.c we initialize
 * them based on following basic string struct (assuming there is no
 * other state of note for now). */

typedef struct {
  int n;
  const char *s;
} kvdb_init_s;

enum {
  /* Insert statements */
  STMT_INSERT_LOG,
  STMT_INSERT_CS,
  STMT_INSERT_APP_CLASS,

  /* Delete stmt (XXX - would it be better to do update instead? We
   * _do_ know if it should be update after all). */
  STMT_DELETE_CS,

  /* Utilities for selecting objects */
  STMT_SELECT_CS_BY_OID,
  STMT_SELECT_CS_BY_KEY,

  /* Export-specific */
  STMT_SELECT_LOG_BY_TA,
  STMT_SELECT_LOG_BY_TA_OWN,

  /* Import-specific (=duplicate check) */
  STMT_SELECT_LOG_BY_LM_OID_KEY_VALUE,

  NUM_STMTS
};

enum {
  KEY_APP,
  KEY_CLASS,
  NUM_KEYS
};

enum {
  APP_LOCAL_KVDB,
  NUM_APPS
};

struct kvdb_struct {
  /* Monotonous timestamp that is actually committed to local
   * time_added fields. It cannot go down as it's used for selecting
   * parts of logs to dump. */
  kvdb_time_t monotonous_time;

  /* SQLite 3 database handle */
  sqlite3 *db;

  /* Used widely -> better to cache these */
  kvdb_key keys[NUM_KEYS];

  /* Magic application that is used for 'local' data. */
  kvdb_app apps[NUM_APPS];

  /* Utility statements. */
  sqlite3_stmt *stmts[NUM_STMTS];

  /* Export */
  sqlite3_stmt *stmt_select_log_by_ta; /* -> lm, oid, key, value */
  sqlite3_stmt *stmt_select_log_by_ta_own; /* -> lm, oid, key, value */

  /* Import */
  sqlite3_stmt *stmt_select_log_by_lm_oid_key_value; /* -> oid */

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

/* Within kvdb_o.c */
void _kvdb_set_err(kvdb k, char *err);
void _kvdb_set_err_from_sqlite(kvdb k);
void _kvdb_set_err_from_sqlite2(kvdb k, const char *bonus);
bool _kvdb_run_stmt(kvdb k, sqlite3_stmt *stmt);
bool _kvdb_run_stmt_keep(kvdb k, sqlite3_stmt *stmt);
void _kvdb_o_free(kvdb_o o);
kvdb_o_a _kvdb_o_get_a(kvdb_o o, kvdb_key key);
void _kvdb_tv_get_raw_value(kvdb_typed_value value, void **p, size_t *len);

/* Within kvdb_index.c */
bool _kvdb_index_init(kvdb k);
bool _kvdb_handle_delete_indexes(kvdb_o o, kvdb_key k);
bool _kvdb_handle_insert_indexes(kvdb_o o, kvdb_key k);

static inline kvdb_time_t kvdb_monotonous_time(kvdb k)
{
  kvdb_time_t now = kvdb_time();

  if (now < k->monotonous_time)
    now = k->monotonous_time;
  else
    k->monotonous_time = now;
  return now;
}

#endif /* KVDB_I_H */
