/*
 * $Id: kvdb_query.c $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Sat Dec 21 18:33:32 2013 mstenber
 * Last modified: Mon Dec 23 18:16:36 2013 mstenber
 * Edit time:     63 min
 *
 */

#define DEBUG

/* This module implements the kvdb query API. */

#include "kvdb_i.h"

/* Completely arbitrary; causes some memory consumption */
#define MAX_INDEXES 4

struct kvdb_query_struct {
  kvdb k;
  int first_free_index;
  kvdb_index i[MAX_INDEXES];
  struct kvdb_typed_value_struct bound1[MAX_INDEXES];
  struct kvdb_typed_value_struct bound2[MAX_INDEXES];
  int order_by;
  bool order_by_asc;
  kvdb_app app;
  kvdb_class cl;

  sqlite3_stmt *stmt;
};

kvdb_query kvdb_create_q(kvdb k)
{
  kvdb_query q = calloc(1, sizeof(*q));

  if (!q)
    return NULL;
  /* Do whatever init is needed here. */
  q->order_by = -1;
  q->k = k;
  return q;
}

kvdb_query kvdb_create_q_o_referring_us(kvdb_o o, kvdb_index i)
{
  kvdb_query q = kvdb_create_q(o->k);
  struct kvdb_typed_value_struct tv;

  if (!q)
    return NULL;
  kvdb_tv_set_object(&tv, o);
  kvdb_q_add_index(q, i, &tv, &tv);
  return q;
}

void kvdb_q_set_match_app_class(kvdb_query q, kvdb_app app, kvdb_class cl)
{
  q->app = app;
  q->cl = cl;
}

void kvdb_q_add_index(kvdb_query q, kvdb_index idx,
                      kvdb_typed_value start, kvdb_typed_value end)
{
  int i = q->first_free_index++;

  KVASSERT(i < MAX_INDEXES, "too many indexes created, blowing up (change MAX_INDEXES)");
  q->i[i] = idx;
  q->bound1[i] = *start;
  if (!end) end = start;
  q->bound2[i] = *end;
}

void kvdb_q_order_by(kvdb_query q, kvdb_index idx, bool ascending)
{
  int i;

  for (i = 0 ; i < q->first_free_index ; i++)
    if (q->i[i] == idx)
      break;
  if (i == q->first_free_index)
    {
      struct kvdb_typed_value_struct s;
      memset(&s, 0, sizeof(s));
      kvdb_q_add_index(q, idx, &s, NULL);
    }
  q->order_by = i;
  q->order_by_asc = ascending;
}

#define APPEND2(e, ...)                         \
do {                                            \
  int r = snprintf(c, (e) - c, __VA_ARGS__);    \
  if (r <= 0)                                   \
    {                                           \
      KVDEBUG("invalid snprintf return value"); \
      goto err;                                 \
    }                                           \
  c += r;                                       \
  if (c >= (e))                                 \
    {                                           \
      KVDEBUG("output buffer full");            \
      goto err;                                 \
    }                                           \
 } while(0)

#define APPEND(...) APPEND2(buf + sizeof(buf), __VA_ARGS__)

static int _sql_bound_size(kvdb_typed_value tv)
{
  /* Random number, bit more than is needed anyway. */
  if (tv->t == KVDB_INTEGER)
    return 32;
  /* By default, binary.. */
  void *p;
  size_t len;
  _kvdb_tv_get_raw_value(tv, &p, &len);
  return 5 + 2 * len;
}

static char *_sql_dump(char *buf, kvdb_typed_value tv)
{
  void *p;
  size_t len;

  char *c;
  char *e = buf + _sql_bound_size(tv);

  switch (tv->t)
    {
    case KVDB_INTEGER:
      sprintf(buf, "%lld", tv->v.i);
      break;
    default:
      /* By default, we just dump the binary..
       * ugh, but hopefully this works :p */
      _kvdb_tv_get_raw_value(tv, &p, &len);
      c = buf;
      APPEND2(e, "X'");
      while (len--)
        {
          APPEND2(e, "%02x", *((unsigned char *)p));
          p++;
        }
      APPEND2(e, "'");
      break;
    }
  return buf;
 err:
  /* We don't have good way to deal with errors here; it should not
   * happen(tm) though. */
  abort();
  return NULL;
}


#define SQL_BOUND(tv) _sql_dump(alloca(_sql_bound_size(tv)), tv)

#define WHERE_OR_AND()          \
do                              \
  {                             \
    if (first)                  \
      {                         \
        APPEND("WHERE ");       \
        first = false;          \
      }                         \
    else                        \
      APPEND("AND ");           \
  } while(0)

kvdb_o kvdb_q_get_next(kvdb_query q)
{
  kvdb k;

  if (!q)
    return NULL;
  k = q->k;
  if (!q->stmt)
    {
      char buf[512];
      char *c = buf;
      int i;

      /* Start the query. */
      if (q->first_free_index == 0)
        {
          if (q->app && q->cl)
            {
              /* Just match app + class == select from app_class. */
              APPEND("SELECT oid FROM app_class WHERE ");
              APPEND("app='%s' AND class='%s'", q->app->name, q->cl->name);
            }
          else
            {
              /* Trivial case - just ALL oids in database */
              strcpy(buf, "SELECT DISTINCT oid FROM cs");
            }
        }
      else
        {
          /* Somewhat more complex cases; there may be criteria, or
           * ordering.. */
          APPEND("SELECT i0.oid FROM ");
          for (i = 0 ; i < q->first_free_index ; i++)
            {
              if (i)
                APPEND(", ");
              APPEND("s_%s i%d ", q->i[i]->name, i);
            }
          if (q->app && q->cl)
            {
              APPEND(", app_class ");
            }
          bool first = true;
          for (i = 0 ; i < q->first_free_index ; i++)
            {
              /* No need to insert criteria for first null index */
              if (i == 0 && q->bound1[i].t == KVDB_NULL)
                continue;
              if (q->bound1[i].t != KVDB_NULL)
                {
                  WHERE_OR_AND();
                  if (_kvdb_tv_cmp(&q->bound1[i], &q->bound2[i]) == 0)
                    {
                      APPEND("i%d.keyish=%s ", i, SQL_BOUND(&q->bound1[i]));
                    }
                  else
                    {
                      APPEND("i%d.keyish>=%s ", i, SQL_BOUND(&q->bound1[i]));
                      APPEND("AND ");
                      APPEND("i%d.keyish<=%s ", i, SQL_BOUND(&q->bound2[i]));
                    }
                }
              if (i)
                {
                  WHERE_OR_AND();
                  APPEND("i0.oid=i%d.oid ", i);
                }
            }
          if (q->app && q->cl)
            {
              WHERE_OR_AND();
              APPEND("app='%s' AND class='%s' ", q->app->name, q->cl->name);
              APPEND("AND ");
              APPEND("i0.oid==app_class.oid ");
            }
          if (q->order_by >= 0)
            {
              APPEND(" ORDER BY i%d.keyish %s",
                     q->order_by,
                     q->order_by_asc ? "ASC" : "DESC");
            }
        }
      KVDEBUG("produced query %s", buf);
      SQLITE_CALL2(sqlite3_prepare_v2(k->db, buf, -1, &q->stmt, NULL),
                   goto err);
    }
  int rc = sqlite3_step(q->stmt);
  if (rc == SQLITE_ROW)
    {
      KVASSERT(sqlite3_column_count(q->stmt)==1, "weird stmt count");
      int len = sqlite3_column_bytes(q->stmt, 0);
      void *p = (void *)sqlite3_column_blob(q->stmt, 0);
      if (len != KVDB_OID_SIZE)
        {
          KVDEBUG("weird sized oid");
          goto err;
        }
      KVDEBUG("fetching oid");
      return kvdb_get_o_by_id(k, p);
    }
  else if (rc == SQLITE_DONE)
    {
      /* q->done = true; (now we just free ourselves) */
    }
  else
    {
      _kvdb_set_err_from_sqlite2(k, "query");
    }
 err:
  kvdb_q_destroy(q);
  return NULL;
}

void kvdb_q_destroy(kvdb_query q)
{
  sqlite3_finalize(q->stmt);
  free(q);
}
