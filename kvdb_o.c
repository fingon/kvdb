/*
 * $Id: kvdb_o.c $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Wed Jul 24 16:54:25 2013 mstenber
 * Last modified: Wed Jul 24 19:11:05 2013 mstenber
 * Edit time:     11 min
 *
 */

/* These operations pertain to single kvdb objects.
 *
 * (Note that flushing to disk is handled elsewhere, in the
 * kvdb_commit / kvdb.c)..
 */

#include "kvdb_i.h"
#include "util.h"

#include <string.h>

struct kvdb_o_struct {
  const char *oid;
  const char *app;
  const char *cl;
};

typedef struct kvdb_o_a_struct {
  const char *key;
} *kvdb_o_a;

kvdb_o kvdb_create_o(kvdb k, const char *app, const char *cl)
{
  char buf[64];
  kvdb_o o = calloc(1, sizeof(*o));

  if (!o)
    return NULL;
  sprintf(buf, "%d.%d.%s", k->seq, k->boot, k->name);
  k->seq++;
  /* This _better_ be unique. XXX check? */
  o->oid = strdup(buf);
  o->app = stringset_get_or_insert(k->ss, app);
  o->cl = stringset_get_or_insert(k->ss, cl);
  if (!o->app || !o->cl || !o->oid)
    goto fail;
  k->oid_ih = ihash_insert(k->oid_ih, o);
  if (!k->oid_ih)
    goto fail;
  return o;

 fail:
  if (o->oid)
    free((void *)o->oid);
  return NULL;
}
