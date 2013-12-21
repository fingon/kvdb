/*
 * $Id: kvdb_index.c $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Sat Dec 21 14:54:50 2013 mstenber
 * Last modified: Sat Dec 21 16:45:48 2013 mstenber
 * Edit time:     55 min
 *
 */

/* This module handles indexing. */

#define DEBUG

#include "kvdb_i.h"
#include <ctype.h>

/* Handle removing of single index on single object. */
static bool _kvdb_handle_delete_index(kvdb_o o, kvdb_index i)
{
  sqlite3_stmt *s = i->stmt_delete;
  kvdb k = o->k;

  SQLITE_CALL(sqlite3_reset(s));
  SQLITE_CALL(sqlite3_clear_bindings(s));
  SQLITE_CALL(sqlite3_bind_blob(s, 1, &o->oid, KVDB_OID_SIZE, SQLITE_STATIC));
  KVDEBUG("removed index %s for %p", i->name, o);
  return _kvdb_run_stmt_keep(k, s);
}

/* Handle setting of single index on single object. */
static bool _kvdb_handle_insert_index(kvdb_o o, kvdb_index i)
{
  sqlite3_stmt *s = i->stmt_insert;
  kvdb k = o->k;

  SQLITE_CALL(sqlite3_reset(s));
  SQLITE_CALL(sqlite3_clear_bindings(s));
  SQLITE_CALL(sqlite3_bind_blob(s, 1, &o->oid, KVDB_OID_SIZE, SQLITE_STATIC));
  switch (i->type)
    {
    case KVDB_INTEGER_INDEX:
      {
        int64_t *iv = kvdb_o_get_int64(o, i->key);
        if (!iv)
          return false;
        SQLITE_CALL(sqlite3_bind_int64(s, 2, *iv));
      }
      break;
    case KVDB_OBJECT_INDEX:
      {
        kvdb_o ov = kvdb_o_get_object(o, i->key);
        if (!ov)
          return false;
        SQLITE_CALL(sqlite3_bind_blob(s, 2, &ov->oid, KVDB_OID_SIZE,
                                      SQLITE_STATIC));
      }
      break;
    default:
      return false;
    }
  KVDEBUG("adding index %s for %p", i->name, o);
  return _kvdb_run_stmt_keep(k, s);
}

static bool _ih_handle_index(void *p, void *context)
{
  kvdb_o o = p;
  kvdb_index i = context;
  kvdb_o_a a = _kvdb_o_get_a(o, i->key);

  if (a)
    return _kvdb_handle_insert_index(o, i);
  return true;
}

kvdb_index kvdb_define_index(kvdb k,
                             kvdb_key key,
                             const char *name,
                             kvdb_index_type index_type)
{
  const char *c;
  kvdb_index i;
  char buf[256];
  int rc;

  KVDEBUG("kvdb_define_index %s", name);
  for (c = name ; *c ; c++)
    if (!isalnum(*c) && *c != '_')
      {
        KVDEBUG("invalid character in index name: %c", *c);
        return NULL;
      }
  if (strlen(name) > (KVDB_INDEX_NAME_SIZE - 1))
    {
      KVDEBUG("too long index name: %s", name);
      return NULL;
    }
  /* XXX - handle persistence of indexes. */
  i = calloc(1, sizeof(*i));
  if (!i)
    return NULL;
  strcpy(i->name, name);

  /* Create fake table s_name, and index i_s_name */
  sprintf(buf,
          "CREATE TABLE s_%s (keyish, oid);"
          "CREATE INDEX i_s_%s ON s_%s(keyish);",
          name, name, name);
  SQLITE_EXEC2(buf, goto fail);

  i->key = key;
  i->type = index_type;
  switch (i->type)
    {
    case KVDB_INTEGER_INDEX:
      if (key->type != KVDB_INTEGER)
        {
          KVDEBUG("non-integer key for integer index? bad idea");
          goto fail;
        }
      break;
    case KVDB_OBJECT_INDEX:
      if (key->type != KVDB_OBJECT)
        {
          KVDEBUG("non-object key for object index? bad idea");
          goto fail;
        }
      break;
    default:
      KVDEBUG("unsupported index type:%d for %s", (int)index_type, name);
      goto fail;
    }

  /* Ok. Looks good. So all we need to do is just iterate through the
   * database, and find every object that has this key set (and load
   * them if already not loaded). Then iterate through every object in
   * memory, and call population, and bob's your uncle(?). */
  sqlite3_stmt *stmt = k->stmt_select_cs_by_key;
  SQLITE_CALL2(sqlite3_reset(stmt), goto fail);
  SQLITE_CALL2(sqlite3_clear_bindings(stmt), goto fail);
  SQLITE_CALL2(sqlite3_bind_text(stmt, 1, key->name, -1, SQLITE_STATIC), goto fail);
  rc = sqlite3_step(stmt);
  while (rc == SQLITE_ROW)
    {
      KVASSERT(sqlite3_column_count(stmt)==1, "weird stmt count");
      int len = sqlite3_column_bytes(stmt, 0);
      void *p = (void *)sqlite3_column_blob(stmt, 0);
      if (len != KVDB_OID_SIZE)
        {
          KVDEBUG("weird sized oid");
          goto fail;
        }
      KVDEBUG("fetching oid");
      kvdb_get_o_by_id(k, p);
      rc = sqlite3_step(stmt);
    }
  if (rc != SQLITE_DONE)
    {
      _kvdb_set_err_from_sqlite2(k, "select from cs (by key)");
    }

  /* Prepare insert + delete statements */
  sprintf(buf, "INSERT INTO s_%s (keyish, oid) VALUES (?1, ?2)", name);
  SQLITE_CALL2(sqlite3_prepare_v2(k->db,
                                   buf, -1,
                                   &i->stmt_insert, NULL),
               goto fail);
  sprintf(buf, "DELETE FROM s_%s WHERE oid=?1", name);
  SQLITE_CALL2(sqlite3_prepare_v2(k->db,
                                   buf, -1,
                                   &i->stmt_delete, NULL),
               goto fail);

  list_add(&i->lh, &key->index_lh);
  ihash_iterate(k->oid_ih, _ih_handle_index, i);
  return i;
 fail:
  free(i);
  return NULL;
}


bool _kvdb_handle_delete_indexes(kvdb_o o, kvdb_key k)
{
  kvdb_o_a a = _kvdb_o_get_a(o, k);
  kvdb_index i;

  /* May have to do _something_ */
  if (a)
    {
      list_for_each_entry(i, &k->index_lh, lh)
        if (!_kvdb_handle_delete_index(o, i))
          return false;
    }
  return true;
}

bool _kvdb_handle_insert_indexes(kvdb_o o, kvdb_key key)
{
  kvdb k = o->k;
  kvdb_index i;

  /* First off, the magic app+class index. */
  if (key == k->app_key
      || key == k->class_key)
    {
      /* Only insert to it when both app and cl are present in the object. */
      if (o->app && o->cl)
        {
          sqlite3_stmt *s = o->k->stmt_insert_app_class;

          SQLITE_CALL(sqlite3_reset(s));
          SQLITE_CALL(sqlite3_clear_bindings(s));
          SQLITE_CALL(sqlite3_bind_text(s, 1, o->app->name, -1, SQLITE_STATIC));
          SQLITE_CALL(sqlite3_bind_text(s, 2, o->cl->name, -1, SQLITE_STATIC));
          SQLITE_CALL(sqlite3_bind_blob(s, 3, &o->oid, KVDB_OID_SIZE, SQLITE_STATIC));
          if (!_kvdb_run_stmt_keep(k, s))
            {
              KVDEBUG("stmt_insert_app_class failed");
              return false;
            }
        }
      /* Can't be in any other index. */
      return true;
    }

  list_for_each_entry(i, &key->index_lh, lh)
    if (!_kvdb_handle_insert_index(o, i))
      return false;

  return true;
}
