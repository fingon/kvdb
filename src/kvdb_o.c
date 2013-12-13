/*
 * $Id: kvdb_o.c $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Wed Jul 24 16:54:25 2013 mstenber
 * Last modified: Mon Jul 29 16:40:07 2013 mstenber
 * Edit time:     99 min
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
#include "sll.h"

#include <string.h>
#include <errno.h>

static kvdb_o _create_o(kvdb k,
                        const char *app,
                        const char *cl,
                        const char *oid)
{
  kvdb_o o = calloc(1, sizeof(*o));

  if (!o)
    return NULL;
  o->oid = strdup(oid);
  o->app = kvdb_intern(k, app);
  o->cl = kvdb_intern(k, cl);
  if (!o->app || !o->cl || !o->oid)
    goto fail;
  k->oid_ih = ihash_insert(k->oid_ih, o);
  if (!k->oid_ih)
    goto fail;
  o->k = k;
  return o;

 fail:
  if (o->oid)
    free((void *)o->oid);
  return NULL;
}

kvdb_o kvdb_create_o(kvdb k, const char *app, const char *cl)
{
  char buf[64];
  sprintf(buf, "%d.%d.%s", k->seq, k->boot, k->name);
  k->seq++;
  return _create_o(k, app, cl, buf);
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
    case KVDB_UNTYPED_BINARY:
      dst->v.binary.ptr = malloc(src->v.binary.ptr_size);
      if (!dst->v.binary.ptr)
        return false;
      memcpy(dst->v.binary.ptr, src->v.binary.ptr, src->v.binary.ptr_size);
      dst->v.binary.ptr_size = src->v.binary.ptr_size;
      break;
    default:
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
    case KVDB_UNTYPED_BINARY:
      free(o->v.binary.ptr);
      break;
    default:
      /* Nothing to free, NOP */
      break;
    }
}

static kvdb_o_a _kvdb_o_get_a(kvdb_o o, const char *key)
{
  kvdb_o_a a;

  SLL_FOR2(o->al, a)
    if (a->key == key)
      return a;
  return NULL;
}

static kvdb_o_a _o_set(kvdb_o o, const char *key, const kvdb_typed_value value)
{
  kvdb_o_a a = _kvdb_o_get_a(o, key);
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
      SLL_ADD2(o->al, a);
    }
  else
    {
      _free_kvdb_typed_value(&a->value);
      if (!value)
          a->value.t = KVDB_NULL;
      else
        _copy_kvdb_typed_value(value, &a->value);
    }
  return a;
}

kvdb_o _select_object_by_oid(kvdb k, const char *oid)
{
  kvdb_o r = NULL;
  sqlite3_stmt *stmt = k->stmt_select_cs_by_oid;
  SQLITE_CALL2(sqlite3_reset(stmt), NULL);
  SQLITE_CALL2(sqlite3_clear_bindings(stmt), NULL);
  SQLITE_CALL2(sqlite3_bind_text(stmt, 1, oid, -1, SQLITE_STATIC), NULL);
  int rc = sqlite3_step(stmt);
  while (rc == SQLITE_ROW)
    {
      KVASSERT(sqlite3_column_count(stmt)==4, "weird stmt count");
      /* app, cl, key, value */
      if (!r)
        {
          const unsigned char *app = sqlite3_column_text(stmt, 0);
          const unsigned char *cl = sqlite3_column_text(stmt, 1);
          r = _create_o(k, (const char *) app, (const char *) cl, oid);
          if (!r)
            return NULL;
        }

      const char *key = kvdb_intern(k,
                                    (const char *)
                                    sqlite3_column_text(stmt, 2));
      /* XXX - handle the value better than this ;) */
      struct kvdb_typed_value_struct ktv;
      ktv.t = KVDB_UNTYPED_BINARY;
      ktv.v.binary.ptr = (void *)sqlite3_column_blob(stmt, 3);
      ktv.v.binary.ptr_size = sqlite3_column_bytes(stmt, 3);
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
kvdb_o kvdb_get_o_by_id(kvdb k, const char *oid)
{
  struct kvdb_o_struct dummy;
  dummy.oid = oid;
  void *v = ihash_get(k->oid_ih, &dummy);
  if (v)
    return (kvdb_o) v;

  return _select_object_by_oid(k, oid);
}

const kvdb_typed_value kvdb_o_get(kvdb_o o, const char *key)
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
      if (ktv->t == KVDB_INTEGER)
        return &ktv->v.i;
      if (ktv->t == KVDB_UNTYPED_BINARY && ktv->v.binary.ptr)
        {
          int64_t rv;
          errno = 0;
          rv = atoll(ktv->v.binary.ptr);
          if (rv || !errno)
            {
              /* Convert it to int64 internally */
              struct kvdb_typed_value_struct ktv2;
              ktv2.t = KVDB_INTEGER;
              ktv2.v.i = rv;
              if (_o_set(o, key, &ktv2))
                return kvdb_o_get_int64(o, key);
              else
                KVDEBUG("_o_set failed for %s", key);
            }
          KVDEBUG("untyped conversion failed? %lld, %d from %s", rv, errno, ktv->v.binary.ptr);

        }
    }
  return NULL;
}


static bool _set_bind_value(kvdb k, sqlite3_stmt *stmt, int n, kvdb_o_a a)
{
  switch (a->value.t)
    {
    case KVDB_INTEGER:
      SQLITE_CALL(sqlite3_bind_int64(stmt, n, a->value.v.i));
      break;
    case KVDB_DOUBLE:
      SQLITE_CALL(sqlite3_bind_double(stmt, n, a->value.v.d));
      break;
    case KVDB_STRING:
      SQLITE_CALL(sqlite3_bind_text(stmt, n, a->value.v.s, -1, SQLITE_STATIC ));
      break;
    case KVDB_BINARY:
    case KVDB_UNTYPED_BINARY:
      SQLITE_CALL(sqlite3_bind_blob(stmt, n, a->value.v.binary.ptr, a->value.v.binary.ptr_size, SQLITE_STATIC));
      break;
    case KVDB_NULL:
      SQLITE_CALL(sqlite3_bind_null(stmt, n));
      break;
    case KVDB_COORD:
      /* XXX */
      break;
    case KVDB_BOOL:
      /* XXX */
      break;
    }
  return true;
}

bool kvdb_o_set_int64(kvdb_o o, const char *key, int64_t value)
{
  struct kvdb_typed_value_struct ktv;
  ktv.t = KVDB_INTEGER;
  ktv.v.i = value;
  return kvdb_o_set(o, key, &ktv);
}


bool kvdb_o_set(kvdb_o o, const char *key, const kvdb_typed_value value)
{
  /* If the set fails, we don't do anything to the SQL database. */
  kvdb_o_a a = _o_set(o, key, value);
  if (!a)
    return false;

  kvdb k = o->k;
  KVASSERT(k, "missing o->k");

  /* Now we have to reflect the state in log+cs, by doing appropriate
   * deletes (if applicable) and insert. */

  /* Insert to log always */
  SQLITE_CALL(sqlite3_reset(k->stmt_insert_log));
  SQLITE_CALL(sqlite3_clear_bindings(k->stmt_insert_log));
  SQLITE_CALL(sqlite3_bind_text(k->stmt_insert_log, 1, o->oid, -1, SQLITE_STATIC));
  SQLITE_CALL(sqlite3_bind_text(k->stmt_insert_log, 2, key, -1, SQLITE_STATIC));
  if (!_set_bind_value(k, k->stmt_insert_log, 3, a))
    return false;

  if (!_kvdb_run_stmt(k, k->stmt_insert_log))
    return false;

  /* Delete from cs if there was something there before. */
  SQLITE_CALL(sqlite3_reset(k->stmt_delete_cs));
  SQLITE_CALL(sqlite3_clear_bindings(k->stmt_delete_cs));
  SQLITE_CALL(sqlite3_bind_text(k->stmt_delete_cs, 1, o->oid, -1, SQLITE_STATIC));
  SQLITE_CALL(sqlite3_bind_text(k->stmt_delete_cs, 2, key, -1, SQLITE_STATIC));
  if (!_kvdb_run_stmt(k, k->stmt_delete_cs))
    return false;

  /* Insert to cs if it wasn't NULL. */
  if (a->value.t != KVDB_NULL)
    {
      SQLITE_CALL(sqlite3_reset(k->stmt_insert_cs));
      SQLITE_CALL(sqlite3_clear_bindings(k->stmt_insert_cs));
      SQLITE_CALL(sqlite3_bind_text(k->stmt_insert_cs, 1, o->app, -1, SQLITE_STATIC));
      SQLITE_CALL(sqlite3_bind_text(k->stmt_insert_cs, 2, o->cl, -1, SQLITE_STATIC));
      SQLITE_CALL(sqlite3_bind_text(k->stmt_insert_cs, 3, o->oid, -1, SQLITE_STATIC));
      SQLITE_CALL(sqlite3_bind_text(k->stmt_insert_cs, 4, key, -1, SQLITE_STATIC));
      if (!_set_bind_value(k, k->stmt_insert_cs, 5, a))
        return false;

      if (!_kvdb_run_stmt(k, k->stmt_insert_cs))
        return false;
    }
  return true;
}
