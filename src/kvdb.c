/*
 * $Id: kvdb.c $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Wed Jul 24 11:50:00 2013 mstenber
 * Last modified: Sat Dec 21 19:27:02 2013 mstenber
 * Edit time:     187 min
 *
 */

#define DEBUG

#include "kvdb.h"
#include "kvdb_i.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* Version 0 is empty database. We do the rest incrementially if and
   only if they actually exist. */

static const char *_schema_upgrades[] = {

  /* Just in case; sqlite cannot turn on auto_vacuum in a database
     later on. */
  "PRAGMA auto_vacuum = 2;"
  /* Create the db_state table which is used to keep track of database state. */
  "CREATE TABLE db_state ( key , value );"
  /* Insert _first_ version # into the db (the rest use UPDATE). */
  "INSERT INTO db_state VALUES ('version', 1);"
  "INSERT INTO db_state VALUES ('boot', 0);"

  /* Create cs (current state) and log tables */
  "CREATE TABLE cs (oid, key, value, last_modified);"
  /* xxx - some 'local' flag to indicate local modifications for
     mobile w/o log? */
  "CREATE TABLE log (oid, key, value, last_modified);"

  /* Create indexing utility tables */
  "CREATE TABLE app_class (app, class, oid);"
  "CREATE TABLE search_index(name, type, keyname, keytype);"

  /* Add actual hardcoded indexes */

  "CREATE INDEX i_cs_oid_key ON cs (oid,key);"
  /* for normal fast insert/remove/q */

  "CREATE INDEX i_cs_key ON cs (key);"
  /* for fast index creation later */

  "CREATE INDEX i_app_class ON app_class (app,class,oid);"
  ,

  /* Make sure nop entry works too */
  "",
};

#define LATEST_SCHEMA ((int) (sizeof(_schema_upgrades) / sizeof(const char *)))

void _kvdb_set_err(kvdb k, char *err)
{
  if (k->err)
    free(k->err);
  k->err = strdup(err);
  KVDEBUG("setting error to %s", err ? err : "(null)");

}

void _kvdb_set_err_from_sqlite2(kvdb k, const char *bonus)
{
  const char *msg = sqlite3_errmsg(k->db);
  char *c = malloc(strlen(msg) + 60);
  if (msg && c)
    {
      sprintf(c, "SQLite 3 error: %s %s", msg, bonus);
      _kvdb_set_err(k, c);
    }
  else
    {
      _kvdb_set_err(k, NULL);
    }
}

void _kvdb_set_err_from_sqlite(kvdb k)
{
  _kvdb_set_err_from_sqlite2(k, "");
}

static sqlite3_stmt *_prep_stmt(kvdb k, const char *q)
{
  sqlite3_stmt *stmt;

  KVDEBUG("preparing stmt %s", q);
  SQLITE_CALLR2(sqlite3_prepare_v2(k->db,
                                   q, -1,
                                   &stmt, NULL),
                NULL);
  return stmt;
}

static bool _kvdb_run_stmt_int(kvdb k, sqlite3_stmt *stmt, bool finalize)
{
  if (!stmt)
    {
      KVDEBUG("missing stmt");
      return false;
    }
  int rc = sqlite3_step(stmt);
  if (finalize)
    sqlite3_finalize(stmt);
  if (rc && rc != SQLITE_DONE)
    {
      KVDEBUG("sqlite3_step result: %d %s", rc,
              sqlite3_errmsg(k->db));
      _kvdb_set_err_from_sqlite(k);
      return false;
    }
  return true;
}

bool _kvdb_run_stmt(kvdb k, sqlite3_stmt *stmt)
{
  return _kvdb_run_stmt_int(k, stmt, true);
}

bool _kvdb_run_stmt_keep(kvdb k, sqlite3_stmt *stmt)
{
  return _kvdb_run_stmt_int(k, stmt, false);
}

static int _kvdb_get_int(kvdb k, const char *q, int default_value)
{
  sqlite3_stmt *stmt = _prep_stmt(k, q);
  int rc;
  int rv = default_value;

  KVDEBUG("_kvdb_get_int %s", q);
  if (!stmt)
    {
      return default_value;
    }
  rc = sqlite3_step(stmt);
  if (rc == SQLITE_ROW)
    {
      KVASSERT(sqlite3_column_count(stmt)==1, "weird stmt count");
      rv = sqlite3_column_int(stmt, 0);
      KVDEBUG("got number %d", (int) rv);
    }
  else
    {
      KVDEBUG("_kvdb_get_schema: sqlite3_step returned %d", (int) rc);
    }
  sqlite3_finalize(stmt);
  return rv;
}

static int _kvdb_get_schema(kvdb k)
{
  return _kvdb_get_int(k, "SELECT value FROM db_state WHERE key='version'", 0);
}

static void _begin(kvdb k)
{
  sqlite3_stmt *stmt = _prep_stmt(k, "BEGIN TRANSACTION");
#if defined(DEBUG) || defined(PARANOID)
  bool rv =
#endif /* defined(DEBUG) || defined(PARANOID) */
    _kvdb_run_stmt(k, stmt);
  KVASSERT(rv, "failed to start transaction");
}

static void _rollback(kvdb k)
{
  sqlite3_stmt *stmt = _prep_stmt(k, "ROLLBACK TRANSACTION");
#if defined(DEBUG) || defined(PARANOID)
  bool rv =
#endif /* defined(DEBUG) || defined(PARANOID) */
    _kvdb_run_stmt(k, stmt);
  KVASSERT(rv, "failed to rollback");
}

static void _commit(kvdb k)
{
  sqlite3_stmt *stmt = _prep_stmt(k, "COMMIT");
#if defined(DEBUG) || defined(PARANOID)
  bool rv =
#endif /* defined(DEBUG) || defined(PARANOID) */
    _kvdb_run_stmt(k, stmt);
  KVASSERT(rv, "failed to commit");
}

static bool _kvdb_upgrade(kvdb k)
{
  /* Start transaction */
  _begin(k);
  int schema = _kvdb_get_schema(k);
  if (schema < 0 || schema >= LATEST_SCHEMA)
    {
      KVDEBUG("schema out of range: %d/%d", schema, (int) LATEST_SCHEMA);
      _rollback(k);
      return false;
    }
  sqlite3_stmt *stmt;
  const char *left = _schema_upgrades[schema];
  while (left && strlen(left)>0)
    {
      KVDEBUG("preparing %s", left);

      int rc =
        sqlite3_prepare_v2(k->db,
                           left, -1,
                           &stmt, &left);
      if (rc)
        {
          KVDEBUG("prepare failed");
          _rollback(k);
          return false;
        }
      /* Ok, we've got prepared stmt. Let her rip. */
      if (!_kvdb_run_stmt(k, stmt))
        {
          _rollback(k);
          return false;
        }
    }
  KVDEBUG("upgrade succeeded");
  stmt = _prep_stmt(k, "UPDATE db_state SET value=?1 WHERE key='version'");
  schema++;
  KVDEBUG("setting schema to %d", schema);
  (void)sqlite3_bind_int(stmt, 1, schema);
  bool rv = _kvdb_run_stmt(k, stmt);
  if (rv)
    _commit(k);
  else
    _rollback(k);
  return rv;
}


#ifdef DEBUG
static void _sqlite_log(void *ctx, int rc, const char *str)
{
  KVDEBUG("[sqlite] %d %s", rc, str);
}
static bool _initialized = false;
#endif /* DEBUG */

bool kvdb_init()
{
#ifdef DEBUG
  int rc;

  KVASSERT(!_initialized, "kvdb_init called more than once");
  _initialized = true;
  rc = sqlite3_config(SQLITE_CONFIG_LOG, _sqlite_log, _sqlite_log);
  if (rc)
    {
      KVDEBUG("got %d when setting up log", rc);
      return false;
    }
#endif /* DEBUG */
  return true;
}

static uint64_t
_kvdb_o_hash_value(void *v, void *ctx)
{
  kvdb_o o = (kvdb_o) v;

  return hash_bytes(&o->oid, KVDB_OID_SIZE);
}

static bool
_kvdb_o_compare(void *v1, void *v2, void *ctx)
{
  kvdb_o o1 = (kvdb_o) v1;
  kvdb_o o2 = (kvdb_o) v2;

  return memcmp(&o1->oid, &o2->oid, KVDB_OID_SIZE) == 0;
}

bool kvdb_create(const char *path, kvdb *r_k)
{
  int rc;

  KVASSERT(path, "no path to kvdb_create");
  KVASSERT(r_k, "no return value to kvdb_create");

  kvdb k = calloc(1, sizeof(*k));
  *r_k = k;
  if (!k)
    return false;
  rc = sqlite3_open(path, &k->db);
  if (rc)
    {
      _kvdb_set_err_from_sqlite(k);
fail:
      kvdb_destroy(k);
      return false;
    }
  int schema;
  while ((schema = _kvdb_get_schema(k)) < LATEST_SCHEMA)
    {
      if (!_kvdb_upgrade(k))
        {
          _kvdb_set_err(k, "_kvdb_upgrade failed");
           goto fail;
        }
      int new_schema = _kvdb_get_schema(k);
      if (new_schema <= schema)
        {
          _kvdb_set_err(k, "upgrade step did not upgrade schema#");
          goto fail;
        }
      schema = new_schema;
    }
  if (schema != LATEST_SCHEMA)
    {
      _kvdb_set_err(k, "unable to upgrade to latest");
      goto fail;
    }
  k->oidbase.boot =
    _kvdb_get_int(k, "SELECT value FROM db_state WHERE key='boot'", -1);
  KVASSERT(k->oidbase.boot >= 0, "no boot key in db_state");
  SQLITE_EXEC2("UPDATE db_state SET value=value+1 WHERE key='boot'", goto fail);
  FILE *f = popen("hostname -s", "r");
  if (!f)
    {
      _kvdb_set_err(k, "hostname callf ailed)");
      goto fail;
    }
  char *hname = k->oidbase.name;
  fgets(hname, KVDB_HOSTNAME_SIZE, f);
  hname[KVDB_HOSTNAME_SIZE-1] = 0;
  if (*hname && hname[strlen(hname)-1] == '\n')
    hname[strlen(hname)-1] = 0;
  if (*hname && hname[strlen(hname)-1] == '\r')
    hname[strlen(hname)-1] = 0;
  if (*hname && hname[strlen(hname)-1] == '\n')
    hname[strlen(hname)-1] = 0;
  KVDEBUG("got name %s", hname);
  k->ss_app = stringset_create(sizeof(struct kvdb_app_struct), NULL, NULL);
  if (!k->ss_app)
    {
      _kvdb_set_err(k, "stringset_create failed");
      goto fail;
    }
  k->ss_class = stringset_create(sizeof(struct kvdb_class_struct), NULL, NULL);
  if (!k->ss_class)
    {
      _kvdb_set_err(k, "stringset_create failed");
      goto fail;
    }
  k->ss_key = stringset_create(sizeof(struct kvdb_key_struct), NULL, NULL);
  if (!k->ss_key)
    {
      _kvdb_set_err(k, "stringset_create failed");
      goto fail;
    }
  k->app_key = kvdb_define_key(k, APP_STRING, KVDB_STRING);
  k->class_key = kvdb_define_key(k, CLASS_STRING, KVDB_STRING);

  k->oid_ih = ihash_create(_kvdb_o_hash_value, _kvdb_o_compare, NULL);
  if (!k->oid_ih)
    {
      _kvdb_set_err(k, "oid_ih create failed");
      goto fail;
    }

  if (!(k->stmt_insert_log = _prep_stmt(k, "INSERT INTO log (oid, key, value, last_modified) VALUES(?1, ?2, ?3, ?4)")))
    {
      _kvdb_set_err_from_sqlite2(k, "unable to prepare stmt_insert_log");
      goto fail;
    }

  if (!(k->stmt_insert_app_class = _prep_stmt(k, "INSERT INTO app_class (app, class, oid) VALUES(?1, ?2, ?3)")))
    {
      _kvdb_set_err_from_sqlite2(k, "unable to prepare stmt_insert_app_class");
      goto fail;
    }
  if (!(k->stmt_delete_cs = _prep_stmt(k, "DELETE FROM cs WHERE oid=?1 and key=?2")))
    {
      _kvdb_set_err_from_sqlite2(k, "unable to prepare stmt_delete_cs");
      goto fail;
    }

  if (!(k->stmt_insert_cs = _prep_stmt(k, "INSERT INTO cs (oid, key, value, last_modified) VALUES(?1, ?2, ?3, ?4)")))
    {
      _kvdb_set_err_from_sqlite2(k, "unable to prepare stmt_insert_cs");
      goto fail;
    }

  if (!(k->stmt_select_cs_by_oid = _prep_stmt(k, "SELECT key, value FROM cs WHERE oid=?1")))
    {
      _kvdb_set_err_from_sqlite2(k, "unable to prepare stmt_select_cs_by_oid");
      goto fail;
    }

  if (!(k->stmt_select_cs_by_key = _prep_stmt(k, "SELECT oid FROM cs WHERE key=?1")))
    {
      _kvdb_set_err_from_sqlite2(k, "unable to prepare stmt_select_cs_by_key");
      goto fail;
    }

  if (!_kvdb_index_init(k))
    goto fail;

  /* Start transaction - commit call commits changes. */
  _begin(k);
  return true;
}

static bool _ih_free_iterator(void *o, void *context)
{
  _kvdb_o_free(o);
  return true;
}

void kvdb_destroy(kvdb k)
{
  _rollback(k);
  KVASSERT(k, "no object to kvdb_destroy");
  if (k->oid_ih)
    {
      ihash_iterate(k->oid_ih, _ih_free_iterator, NULL);
      ihash_destroy(k->oid_ih);
    }
  if (k->ss_app)
    stringset_destroy(k->ss_app);
  if (k->ss_class)
    stringset_destroy(k->ss_class);
  if (k->ss_key)
    stringset_destroy(k->ss_key);
  if (k->db)
    sqlite3_close(k->db);
  if (k->err)
    free(k->err);
  free(k);
}

const char *kvdb_strerror(kvdb k)
{
  static const char *no_kvdb = "kvdb allocation failed";
  static const char *no_error = "no error?!?";
  if (!k) return no_kvdb;
  if (!k->err) return no_error;
  return k->err;
}

bool kvdb_commit(kvdb k)
{
  /* Push current ops to disk. */
  _commit(k);

  /* Start transaction - commit call commits changes. */
  _begin(k);

  return true;
}

kvdb_app kvdb_define_app(kvdb k, const char *name)
{
  if (!name)
    return NULL;
  const char *s = stringset_get_or_insert(k->ss_app, name);
  if (s)
    return stringset_get_data_from_string(k->ss_app, s);
  return NULL;
}

kvdb_class kvdb_define_class(kvdb k, const char *name)
{
  if (!name)
    return NULL;
  const char *s = stringset_get_or_insert(k->ss_class, name);
  if (s)
    return stringset_get_data_from_string(k->ss_class, s);
  return NULL;
}

kvdb_key kvdb_define_key(kvdb k, const char *name, kvdb_type t)
{
  kvdb_key key;

  if (!name)
    return NULL;
  const char *s = stringset_get_or_insert(k->ss_key, name);
  if (!s)
    return NULL;
  key = stringset_get_data_from_string(k->ss_key, s);
  if (!key->index_lh.next)
    INIT_LIST_HEAD(&key->index_lh);
  if (t != KVDB_NULL)
    {
      if (key->type != KVDB_NULL
          && key->type != t)
        {
          KVDEBUG("attempted to redefine key type - punting");
          return NULL;
        }
      key->type = t;
    }
  return key;
}
