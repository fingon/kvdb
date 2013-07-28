/*
 * $Id: kvdb_o.c $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Wed Jul 24 16:54:25 2013 mstenber
 * Last modified: Sun Jul 28 16:28:32 2013 mstenber
 * Edit time:     24 min
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

kvdb_o kvdb_get_o_by_id(kvdb k, const char *oid)
{
  struct kvdb_o_struct dummy;
  dummy.oid = oid;
  void *v = ihash_get(k->oid_ih, &dummy);
  if (v)
    return (kvdb_o) v;
  return NULL;
}

const kvdb_typed_value kvdb_o_get(kvdb_o o, const char *key)
{
  kvdb_o_a a;

  for (a = o->al ; a ; a = a->next)
    if (a->key == key)
      return &a->value;
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
      free(o->v.binary.ptr);
      break;
    default:
      /* Nothing to free, NOP */
      break;
    }
}

bool kvdb_o_set(kvdb_o o, const char *key, const kvdb_typed_value value)
{
  kvdb_o_a a = (kvdb_o_a) kvdb_o_get(o, key);

  if (!a)
    {
      if (!value)
        return NULL;

      /* No object - have to create it */
      a = calloc(1, sizeof(*a));
      if (!a)
        return false;

      /* Fill fields */
      a->key = key;
      _copy_kvdb_typed_value(value, &a->value);

      /* Add it to attribute list */
      a->next = o->al;
      if (o->al)
        o->al->prev = a;
      o->al = a;
    }
  else
    {
      _free_kvdb_typed_value(&a->value);
      if (!value)
        {
          a->value.t = KVDB_NULL;
          return true;
        }
      _copy_kvdb_typed_value(value, &a->value);
    }
  return true;
}

