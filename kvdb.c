/*
 * $Id: kvdb.c $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Wed Jul 24 11:50:00 2013 mstenber
 * Last modified: Wed Jul 24 16:42:09 2013 mstenber
 * Edit time:     73 min
 *
 */

#undef DEBUG

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
  /* Create cs (current state) and log tables */
  "CREATE TABLE cs (app, class, oid, key, value, last_modified, local);"
  "CREATE TABLE log (oid, key, value, last_modified)"
  ,

  /* Make sure nop entry works too */
  "",
};

#define LATEST_SCHEMA (sizeof(_schema_upgrades) / sizeof(const char *))

static void _kvdb_set_err(kvdb k, char *err)
{
  if (k->err)
    free(k->err);
  k->err = err;
}

static void _kvdb_set_err_from_sqlite(kvdb k)
{
  const char *msg = sqlite3_errmsg(k->db);
  char *c = malloc(strlen(msg) + 60);
  if (msg && c)
    {
      sprintf(c, "SQLite 3 error: %s", msg);
      _kvdb_set_err(k, c);
    }
  else
    _kvdb_set_err(k, NULL);
}

static sqlite3_stmt *_prep_stmt(kvdb k, const char *q)
{
  sqlite3_stmt *stmt;
  int rc =
    sqlite3_prepare_v2(k->db,
                       q, -1,
                       &stmt, NULL);
  if (rc)
    return NULL;

  return stmt;
}

static bool _run_stmt(kvdb k, sqlite3_stmt *stmt)
{
  if (!stmt)
    return false;
  int rc = sqlite3_step(stmt);
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

static int _kvdb_get_schema(kvdb k)
{
  /* Get schema # from the db_state table. */
  sqlite3_stmt *stmt =
    _prep_stmt(k, "SELECT value FROM db_state WHERE key='version'");
  int rc;
  int rv = -1;
  if (!stmt)
    {
      KVDEBUG("_prep_stmt failed: %s", sqlite3_errmsg(k->db));
      return 0;
    }
  rc = sqlite3_step(stmt);
  if (rc == SQLITE_ROW)
    {
      KVASSERT(sqlite3_column_count(stmt)==1, "weird stmt count");
      rv = sqlite3_column_int(stmt, 0);
      KVDEBUG("got version number %d", (int) rv);
    }
  else
    {
      KVDEBUG("_kvdb_get_schema: sqlite3_step returned %d", (int) rc);
    }
  sqlite3_finalize(stmt);
  return rv;
}

static bool _kvdb_upgrade(kvdb k)
{
  int schema = _kvdb_get_schema(k);
  if (schema < 0 | schema >= LATEST_SCHEMA)
    {
      KVDEBUG("schema out of range: %d/%d", schema, (int) LATEST_SCHEMA);
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
          return false;
        }
      /* Ok, we've got prepared stmt. Let her rip. */
      if (!_run_stmt(k, stmt))
        return false;
    }
  KVDEBUG("upgrade succeeded");
  stmt = _prep_stmt(k, "UPDATE db_state SET value=?1 WHERE key='version'");
  schema++;
  KVDEBUG("setting schema to", schema);
  (void)sqlite3_bind_int(stmt, 1, schema);
  return _run_stmt(k, stmt);
}


#ifdef DEBUG
static void _sqlite_log(void *ctx, int rc, const char *str)
{
  KVDEBUG("[sqlite] %d %s", rc, str);
}
#endif /* DEBUG */

bool kvdb_init()
{
#ifdef DEBUG
  int rc;
  rc = sqlite3_config(SQLITE_CONFIG_LOG, _sqlite_log, _sqlite_log);
  if (rc)
    {
      KVDEBUG("got %d", rc);
      return false;
    }
#endif /* DEBUG */
  return true;
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
      return false;
    }
  int schema;
  while ((schema = _kvdb_get_schema(k)) < LATEST_SCHEMA)
    {
      if (!_kvdb_upgrade(k))
        {
          _kvdb_set_err(k, "_kvdb_upgrade failed");
          return false;
        }
      int new_schema = _kvdb_get_schema(k);
      if (new_schema <= schema)
        {
          _kvdb_set_err(k, "upgrade step did not upgrade schema#");
          return false;
        }
      schema = new_schema;
    }
  if (schema == LATEST_SCHEMA)
    return true;
  _kvdb_set_err(k, "unable to upgrade to latest");
  return false;
}

void kvdb_destroy(kvdb k)
{
  KVASSERT(k, "no object to kvdb_destroy");
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
