/*
 * $Id: kvdb_o.c $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Wed Jul 24 16:54:25 2013 mstenber
 * Last modified: Sat Dec 21 03:04:47 2013 mstenber
 * Edit time:     157 min
 *
 */

/* These operations pertain to single kvdb objects.
 *
 * (Note that flushing to disk is handled elsewhere, in the
 * kvdb_commit / kvdb.c)..
 */

#define DEBUG

#include "kvdb_i.h"
#include "util.h"

#include <string.h>
#include <errno.h>

static void _get_raw_value(kvdb_typed_value value, void **p, size_t *len)
{
  /* Simplified approach: Instead of the good old ways of dealing with
   * various bind methods, all we need to do is find a pointer, and
   * length, and use sqlite3_bind_blob. BIYU. */
  switch (value->t)
    {
    case KVDB_INTEGER:
      *p = &value->v.i;
      if (len)
        *len = sizeof(value->v.i);
      break;
    case KVDB_DOUBLE:
      *p = &value->v.d;
      if (len)
        *len = sizeof(value->v.d);
      break;
    case KVDB_STRING:
      *p = value->v.s;
      if (len)
        *len = strlen(value->v.s) + 1; /* Include null */
      break;
    case KVDB_OBJECT:
      *p = value->v.oid;
      if (len)
        *len = KVDB_OID_SIZE;
      break;
    case KVDB_BINARY:
      *p = value->v.binary.ptr;
      if (len)
        *len = value->v.binary.ptr_size;
      break;
    case KVDB_BINARY_SMALL:
      *p = value->v.binary_small + 1;
      if (len)
        {
          *len = value->v.binary_small[0];
          KVASSERT(*len <= KVDB_BINARY_SMALL_SIZE, "too big content");
        }
      break;
    case KVDB_COORD:
      *p = &value->v.coord;
      if (len)
        *len = sizeof(value->v.coord);
      break;
    }
}

static kvdb_o _create_o(kvdb k, const void *oid)
{
  kvdb_o o = calloc(1, sizeof(*o));

  if (!o)
    {
      KVDEBUG("calloc failed");
      return NULL;
    }
  memcpy(&o->oid, oid, KVDB_OID_SIZE);
  k->oid_ih = ihash_insert(k->oid_ih, o);
  if (!k->oid_ih)
    {
      KVDEBUG("ihash_insert failed");
      free(o);
      return NULL;
    }
  o->k = k;
  INIT_LIST_HEAD(&o->al);
  return o;
}

static bool _o_set_sql(kvdb_o o, const char *key, const void *p, size_t len)
{
  kvdb k = o->k;
  KVASSERT(k, "missing o->k");
  kvdb_time_t now = kvdb_time();

  /* Now we have to reflect the state in log+cs, by doing appropriate
   * deletes (if applicable) and insert. */

  /* Insert to log always */
  SQLITE_CALL(sqlite3_reset(k->stmt_insert_log));
  SQLITE_CALL(sqlite3_clear_bindings(k->stmt_insert_log));
  SQLITE_CALL(sqlite3_bind_blob(k->stmt_insert_log, 1, o->oid, KVDB_OID_SIZE, SQLITE_STATIC));
  SQLITE_CALL(sqlite3_bind_text(k->stmt_insert_log, 2, key, -1, SQLITE_STATIC));
  SQLITE_CALL(sqlite3_bind_blob(k->stmt_insert_log, 3, p, len, SQLITE_STATIC));
  SQLITE_CALL(sqlite3_bind_int64(k->stmt_insert_log, 4, now));

  if (!_kvdb_run_stmt_keep(k, k->stmt_insert_log))
    {
      KVDEBUG("stmt_insert_log failed");
      return false;
    }

  /* Delete from cs if there was something there before. */
  SQLITE_CALL(sqlite3_reset(k->stmt_delete_cs));
  SQLITE_CALL(sqlite3_clear_bindings(k->stmt_delete_cs));
  SQLITE_CALL(sqlite3_bind_blob(k->stmt_delete_cs, 1, o->oid, KVDB_OID_SIZE, SQLITE_STATIC));
  SQLITE_CALL(sqlite3_bind_text(k->stmt_delete_cs, 2, key, -1, SQLITE_STATIC));
  if (!_kvdb_run_stmt_keep(k, k->stmt_delete_cs))
    {
      KVDEBUG("stmt_delete_cs failed");
      return false;
    }

  /* Insert to cs. */
  SQLITE_CALL(sqlite3_reset(k->stmt_insert_cs));
  SQLITE_CALL(sqlite3_clear_bindings(k->stmt_insert_cs));
  SQLITE_CALL(sqlite3_bind_blob(k->stmt_insert_cs, 1, o->oid, KVDB_OID_SIZE, SQLITE_STATIC));
  SQLITE_CALL(sqlite3_bind_text(k->stmt_insert_cs, 2, key, -1, SQLITE_STATIC));
  SQLITE_CALL(sqlite3_bind_blob(k->stmt_insert_cs, 3, p, len, SQLITE_STATIC));
  SQLITE_CALL(sqlite3_bind_int64(k->stmt_insert_cs, 4, now));

  if (!_kvdb_run_stmt_keep(k, k->stmt_insert_cs))
    {
      KVDEBUG("stmt_insert_cs failed");
      return false;
    }
  return true;
}


kvdb_o kvdb_create_o(kvdb k, const char *app, const char *cl)
{
  kvdb_o o = NULL;

  /* First off, intern the app/cl if they're set - easy to recover
   * from failure here. */
  if (app)
    if (!(app = kvdb_intern(k, app)))
      {
        KVDEBUG("failed to intern app");
        return NULL;
      }
  if (cl)
    if (!(cl = kvdb_intern(k, cl)))
      {
        KVDEBUG("failed to intern cl");
        return NULL;
      }
  k->oidbase.seq++;
  o = _create_o(k, &k->oidbase);
  if (!o)
    return NULL;
  if (app)
    if (!kvdb_o_set_string(o, k->app_string, app))
      {
        KVDEBUG("kvdb_o_set_string failed for app");
        goto fail;
      }
  if (cl)
    if (!kvdb_o_set_string(o, k->class_string, cl))
      {
        KVDEBUG("kvdb_o_set_string failed for class");
        goto fail;
      }
  return o;

 fail:
  if (o)
    _kvdb_o_free(o);
  return NULL;
}

static bool _copy_kvdb_typed_value(const kvdb_typed_value src,
                                   kvdb_typed_value dst)
{
  switch (src->t)
    {
    case KVDB_STRING:
      dst->v.s = strdup(src->v.s);
      if (!dst->v.s)
        return false;
      break;
    case KVDB_BINARY:
      dst->v.binary.ptr = malloc(src->v.binary.ptr_size);
      if (!dst->v.binary.ptr)
        return false;
      memcpy(dst->v.binary.ptr, src->v.binary.ptr, src->v.binary.ptr_size);
      dst->v.binary.ptr_size = src->v.binary.ptr_size;
      break;
      /* Defaults are dangerous, so we don't. This is the semidefault
         copy-by-value with every supported type. */
    case KVDB_BINARY_SMALL:
    case KVDB_INTEGER:
    case KVDB_DOUBLE:
    case KVDB_COORD:
    case KVDB_OBJECT:
      dst->v = src->v;
      break;
    }
  dst->t = src->t;
  return true;
}

static void _free_kvdb_typed_value(kvdb_typed_value o)
{
  switch (o->t)
    {
    case KVDB_STRING:
      free(o->v.s);
      break;
    case KVDB_BINARY:
      if (o->v.binary.ptr)
        free(o->v.binary.ptr);
      break;
    default:
      /* Nothing to free, NOP */
      break;
    }
}

void _kvdb_o_free(kvdb_o o)
{
  kvdb_o_a a, an;

  list_for_each_entry_safe(a, an, &o->al, lh)
    {
      _free_kvdb_typed_value(&a->value);
      free(a);
    }
  free(o);
}

static kvdb_o_a _kvdb_o_get_a(kvdb_o o, const char *key)
{
  kvdb_o_a a;

  list_for_each_entry(a, &o->al, lh)
    if (a->key == key)
      return a;
  return NULL;
}

static kvdb_o_a _o_set(kvdb_o o, const char *key, const kvdb_typed_value value)
{
  kvdb_o_a a;

  if (!value)
    return NULL;

  /* Magic handling of setting app; it's stored on o instead of as
   * separate attr. */
  if (key == o->k->app_string)
    {
      void *p;
      _get_raw_value(value, &p, NULL);
      o->app = kvdb_intern(o->k, (const char *)p);
      if (!o->app)
        {
          KVDEBUG("unable to intern raw app %p", p);
          return NULL;
        }
      return (kvdb_o_a)o;
    }
  if (key == o->k->class_string)
    {
      void *p;
      _get_raw_value(value, &p, NULL);
      o->cl = kvdb_intern(o->k, (const char *)p);
      if (!o->cl)
        {
          KVDEBUG("unable to intern raw class %p", p);
          return NULL;
        }
      return (kvdb_o_a)o;
    }

  a = _kvdb_o_get_a(o, key);
  if (!a)
    {
      if (!value)
        return NULL;

      /* No object - have to create it */
      a = calloc(1, sizeof(*a));
      if (!a)
        return NULL;

      /* Fill fields */
      a->key = key;
      _copy_kvdb_typed_value(value, &a->value);

      /* Add it to attribute list */
      list_add(&a->lh, &o->al);
    }
  else
    {
      _free_kvdb_typed_value(&a->value);
      _copy_kvdb_typed_value(value, &a->value);
    }
  return a;
}

kvdb_o _select_object_by_oid(kvdb k, const void *oid)
{
  kvdb_o r = NULL;
  sqlite3_stmt *stmt = k->stmt_select_cs_by_oid;
  SQLITE_CALL2(sqlite3_reset(stmt), NULL);
  SQLITE_CALL2(sqlite3_clear_bindings(stmt), NULL);
  SQLITE_CALL2(sqlite3_bind_blob(stmt, 1, oid, KVDB_OID_SIZE, SQLITE_STATIC), NULL);
  int rc = sqlite3_step(stmt);
  while (rc == SQLITE_ROW)
    {
      KVASSERT(sqlite3_column_count(stmt)==2, "weird stmt count");
      /* key, value */
      if (!r)
        {
          r = _create_o(k, oid);
          if (!r)
            return NULL;
        }

      const char *key = kvdb_intern(k,
                                    (const char *)
                                    sqlite3_column_text(stmt, 0));
      /* XXX - handle the value better than this ;) */
      struct kvdb_typed_value_struct ktv;
      int len = sqlite3_column_bytes(stmt, 1);
      void *p = (void *)sqlite3_column_blob(stmt, 1);
      if (len <= KVDB_BINARY_SMALL_SIZE)
        {
          ktv.t = KVDB_BINARY_SMALL;
          ktv.v.binary_small[0] = (char) len;
          memcpy(ktv.v.binary_small+1, p, len);
        }
      else
        {
          ktv.t = KVDB_BINARY;
          ktv.v.binary.ptr_size = len;
          ktv.v.binary.ptr = p;
        }
      kvdb_o_a a = _o_set(r, key, &ktv);
      if (!a)
        return NULL;
      rc = sqlite3_step(stmt);

    }
  if (rc != SQLITE_DONE)
    {
      _kvdb_set_err_from_sqlite2(k, "select from cs");
    }
  return r;
}

kvdb_o kvdb_get_o_by_id(kvdb k, const void *oid)
{
  struct kvdb_o_struct dummy;
  memcpy(&dummy.oid, oid, KVDB_OID_SIZE);
  void *v = ihash_get(k->oid_ih, &dummy);
  if (v)
    return (kvdb_o) v;

  return _select_object_by_oid(k, oid);
}

kvdb_typed_value kvdb_o_get(kvdb_o o, const char *key)
{
  kvdb_o_a a = _kvdb_o_get_a(o, key);
  if (a)
    return &a->value;
  return NULL;
}

int64_t *kvdb_o_get_int64(kvdb_o o, const char *key)
{
  const kvdb_typed_value ktv = kvdb_o_get(o, key);
  if (ktv)
    {
      if (ktv->t == KVDB_BINARY_SMALL
          && ktv->v.binary_small[0] == sizeof(ktv->v.i))
        {
          int64_t v = *((int64_t *)&ktv->v.binary_small[1]);
          ktv->t = KVDB_INTEGER;
          ktv->v.i = v;
        }
      if (ktv->t == KVDB_INTEGER)
        return &ktv->v.i;
    }
  return NULL;
}

char *kvdb_o_get_string(kvdb_o o, const char *key)
{
  const kvdb_typed_value ktv = kvdb_o_get(o, key);
  if (ktv)
    {
      char *r;
      int len;

      if (ktv->t == KVDB_BINARY_SMALL)
        {
          if ((len=ktv->v.binary_small[0]) &&
              !ktv->v.binary_small[len])
            {
              /* No need to change type, perhaps. */
              return (char *)ktv->v.binary_small + 1;
            }
        }
      if (ktv->t == KVDB_BINARY && (r = ktv->v.binary.ptr) && (len=ktv->v.binary.ptr_size) > 0)
        {
          /* Make sure it ends with '0'; if not, it's probably not a string. */
          if (r[ktv->v.binary.ptr_size-1] == 0)
            {
              ktv->t = KVDB_STRING;
              ktv->v.s = ktv->v.binary.ptr;
            }
          else
            {
              KVDEBUG("non-null terminated string?");
            }
        }
      if (ktv->t == KVDB_STRING)
        return ktv->v.s;
    }
  return NULL;
}

bool kvdb_o_set_int64(kvdb_o o, const char *key, int64_t value)
{
  struct kvdb_typed_value_struct ktv;
  ktv.t = KVDB_INTEGER;
  ktv.v.i = value;
  return kvdb_o_set(o, key, &ktv);
}

bool kvdb_o_set_string(kvdb_o o, const char *key, const char *value)
{
  struct kvdb_typed_value_struct ktv;
  ktv.t = KVDB_STRING;
  ktv.v.s = (char *)value;
  return kvdb_o_set(o, key, &ktv);
}

bool kvdb_o_set(kvdb_o o, const char *key, const kvdb_typed_value value)
{
  /* If the set fails, we don't do anything to the SQL database. */
  kvdb_o_a a = _o_set(o, key, value);
  void *p;
  size_t len;

  KVDEBUG("kvdb_o_set %p/%s", o, key);
  if (!a)
    {
      KVDEBUG("_o_set failed");
      return false;
    }
  _get_raw_value(value, &p, &len);
  KVDEBUG("playing with sql %p/%d", p, (int)len);
  return _o_set_sql(o, key, p, len);
}
