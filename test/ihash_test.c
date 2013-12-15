/*
 * $Id: ihash_test.c $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Wed Jul 24 15:41:53 2013 mstenber
 * Last modified: Sun Dec 15 09:27:05 2013 mstenber
 * Edit time:     20 min
 *
 */

#define DEBUG
#include "ihash.h"
#include "util.h"
#include <stdio.h>

#define MAGIC_CONTEXT 42

#define TOV(v) ((void *)((intptr_t) v))
#define TOI(v) ((intptr_t)(v))

uint64_t dummy_ihash_value_callback(void *o, void *ctx)
{
  KVASSERT(TOI(ctx) == MAGIC_CONTEXT, "wrong context");
  return (uint64_t) (TOI(o) + 1);
}

bool dummy_ihash_eq_callback(void *o1, void *o2, void *ctx)
{
  KVASSERT(TOI(ctx) == MAGIC_CONTEXT, "wrong context");
  return o1 == o2;
}

bool dummy_ihash_iterator(void *o, void *iterator_context)
{
  int *cnt = (int *)iterator_context;
  (*cnt)++;
  return true;
}

bool dummy_ihash_iterator2(void *o, void *iterator_context)
{
  int *cnt = (int *)iterator_context;
  (*cnt)++;
  return false;
}


int main(int argc, char **argv)
{
  ihash ih = ihash_create(dummy_ihash_value_callback,
                          dummy_ihash_eq_callback,
                          TOV(MAGIC_CONTEXT));
  int t1 = 13;
  int t2 = 42;
  int t3 = 1234567890;
  int cnt = 0;
  KVASSERT(ih, "unable to ihash_create");
  bool r;
  r = ihash_insert(ih, TOV(t1));
  KVASSERT(r, "insert failed");
  r = ihash_insert(ih, TOV(t2));
  KVASSERT(r, "insert failed");
  r = ihash_insert(ih, TOV(t3));
  KVASSERT(r, "insert failed");

  ihash_iterate(ih, dummy_ihash_iterator, &cnt);
  KVASSERT(cnt == 3, "invalid count: %d", cnt);
  cnt = 0;

  ihash_iterate(ih, dummy_ihash_iterator2, &cnt);
  KVASSERT(cnt == 1, "invalid count: %d", cnt);
  cnt = 0;

  KVASSERT(ihash_get(ih, TOV(t1-1)) == NULL, "got something for 12");
  KVASSERT(ihash_get(ih, TOV(t1)) != NULL, "unable to find t1");
  KVASSERT(ihash_get(ih, TOV(t2)) != NULL, "unable to find t2");
  KVASSERT(ihash_get(ih, TOV(t3)) != NULL, "unable to find t3");

  ihash_remove(ih, TOV(t2));
  KVASSERT(ihash_get(ih, TOV(t1)) != NULL, "unable to find t1");
  KVASSERT(ihash_get(ih, TOV(t2)) == NULL, "found t2 after remove");
  KVASSERT(ihash_get(ih, TOV(t3)) != NULL, "unable to find t3");

  ihash_iterate(ih, dummy_ihash_iterator, &cnt);
  KVASSERT(cnt == 2, "invalid count: %d", cnt);
  cnt = 0;

  /* Make sure we can add 'lots' of entries too. */
  int i;
  for (i = 10 ; i < 100 ; i++)
    {
      ih = ihash_insert(ih, TOV(i));
      KVASSERT(ih, "ihash_insert ran out of memory?");
    }

  ihash_iterate(ih, dummy_ihash_iterator, &cnt);
  KVASSERT(cnt == 92, "invalid count: %d", cnt);
  cnt = 0;

  ihash_destroy(ih);
  return 0;
}
