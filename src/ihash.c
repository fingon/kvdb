/*
 * $Id: ihash.c $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Wed Jul 24 15:37:45 2013 mstenber
 * Last modified: Sun Dec 15 09:28:01 2013 mstenber
 * Edit time:     46 min
 *
 */

/* Internally, we keep an array of pointers for the hash use.
 *
 * There are two magic values:
 * 0 = NULL = never used (terminates looking for particular key always)
 *
 * 1 = sometime allocated (does NOT terminate looking for particular
 * key, but is not returned in e.g. iterate)
 *
 * We ensure that there's always N% chance of hitting empty slot on
 * iteration; in other words, we keep track of allocated + buckets
 * with 1, and keep it at <= (100-N%).
 *
 * The realloc we do on insert may make the structure smaller, larger,
 * or keep it same size; the main thing is that it gets rid of the
 * 'sometime allocated' data.
 */

#undef DEBUG

/* Keep this % of buckets _empty_. */
#define IHASH_N_E 30

/* On realloc, % to keep _empty_ (or more) */
#define IHASH_N_R_E 50

#include "ihash.h"
#include "util.h"

#define FAKE_EMPTY ((void *)1)

struct ihash_struct
{
  /* Callbacks and their context */
  ihash_value_callback vcb;
  ihash_eq_callback ecb;
  void *ctx;

  /* Current allocated size */
  int size;

  /* size - #ok-nonempty */
  int resize_counter;

  /* #nonempty - #fake-empty */
  int used;

  /* Rest of array */
  void *a[0];
};

static ihash _create(int size)
{
  ihash ih = calloc(1, sizeof(*ih) + sizeof(void *) * size);

  KVDEBUG("creating new with size %d", size);
  ih->size = size;
  ih->resize_counter = ih->size * (100 - IHASH_N_E) / 100;
  return ih;
}

static int sizes[] = {
  13,
  41,
  127,
  383,
  1151,
  3457,
  10391,
  31177,
  93553,
  280673,
  842021,
  2526077,
  7578239,
  22734737,
  68204249,
  204612757,
  613838293,
  0
};

int _find_new_size(int used)
{
  int i;
  int m_size = used * 100 / (100 - IHASH_N_R_E);
  for (i = 0 ; sizes[i] ; i++)
    {
      if (sizes[i] >= m_size)
        return sizes[i];
    }
  /* Should NOT be possible */
  abort();
}


ihash ihash_create(ihash_value_callback vcb, ihash_eq_callback ecb, void *ctx)
{
  ihash ih = _create(_find_new_size(0));
  if (!ih) return NULL;
  ih->vcb = vcb;
  ih->ecb = ecb;
  ih->ctx = ctx;
  return ih;
}

void ihash_destroy(ihash ih)
{
  free(ih);
}

static int _find_empty_slot(ihash ih, int idx)
{
  int i;

  for (i = idx ; i < ih->size ; i++)
    {
      void *o = ih->a[i];
      if (!(o && o != FAKE_EMPTY))
          return i;
    }
  for (i = 0 ; i < idx ; i++)
    {
      void *o = ih->a[i];
      if (!(o && o != FAKE_EMPTY))
          return i;
    }
  return -1;
}

static int _find_matching_slot(ihash ih, int idx, void *mo)
{
  int i;

  for (i = idx ; i < ih->size ; i++)
    {
      void *o = ih->a[i];
      if (!o)
        return -1;
      if (o != FAKE_EMPTY && ih->ecb(mo, o, ih->ctx))
        return i;
    }
  for (i = 0 ; i < idx ; i++)
    {
      void *o = ih->a[i];
      if (!o)
        return -1;
      if (o && o != FAKE_EMPTY && ih->ecb(mo, o, ih->ctx))
        return i;
    }
  return -1;
}

void *ihash_get(ihash ih, void *o_template)
{
  KVASSERT(ih, "no ihash to ihash_get");
  KVASSERT(o_template, "no o_template to ihash_get");
  uint64_t h = ih->vcb(o_template, ih->ctx);
  int idx = h % ih->size;
  int slot = _find_matching_slot(ih, idx, o_template);
  if (slot >= 0)
    return ih->a[slot];
  return NULL;
}

void _insert_raw(ihash ih, void *o)
{
  KVASSERT(ih, "null ih in _insert_raw");
  KVASSERT(o, "null o in _insert_raw");

  uint64_t h = ih->vcb(o, ih->ctx);
  int idx = h % ih->size;
  int slot = _find_empty_slot(ih, idx);

  KVASSERT(slot >= 0, "array somehow full?!?");
  /* If it wasn't in use.. now it is */
  if (!ih->a[slot])
    {
      ih->resize_counter--;
      KVDEBUG("resize counter decremented to %d", ih->resize_counter);
    }
  ih->used++;
  ih->a[slot] = o;
}

static bool
_insert_to(void *o, void *ih_void)
{
  ihash ih = (ihash) ih_void;
  _insert_raw(ih, o);
  return true;
}

ihash ihash_insert(ihash ih, void *o)
{
  KVASSERT(ih, "no ihash to ihash_insert");
  KVASSERT(o, "no o to ihash_insert");
  if (ih->resize_counter == 0)
    {
      ihash ih2 = _create(_find_new_size(ih->used));
      ih2->vcb = ih->vcb;
      ih2->ecb = ih->ecb;
      ih2->ctx = ih->ctx;
      ihash_iterate(ih, _insert_to, ih2);
      ihash_destroy(ih);
      ih = ih2;
    }
  _insert_raw(ih, o);
  return ih;
}

void ihash_remove(ihash ih, void *o)
{
  KVASSERT(ih, "no ihash to ihash_remove");
  KVASSERT(o, "no o to ihash_remove");
  uint64_t h = ih->vcb(o, ih->ctx);
  int idx = h % ih->size;
  int slot = _find_matching_slot(ih, idx, o);
  KVASSERT(slot >= 0, "attempt to remove non-existent object");
  ih->a[slot] = FAKE_EMPTY;
  ih->used--;
}

void ihash_iterate(ihash ih, ihash_iterator cb, void *cb_context)
{
  KVASSERT(ih, "no ihash to ihash_iterate");
  KVASSERT(cb, "no cb to ihash_iterate");
  int i;
  for (i = 0 ; i < ih->size ; i++)
    {
      void *o = ih->a[i];
      if (o && o != FAKE_EMPTY)
        if (!cb(o, cb_context))
          return;
    }
}
