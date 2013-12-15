/*
 * $Id: stringset_test.c $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Wed Jul 24 17:40:35 2013 mstenber
 * Last modified: Sun Dec 15 10:03:06 2013 mstenber
 * Edit time:     13 min
 *
 */

#define DEBUG
#include "stringset.h"
#include "util.h"

typedef struct {
  stringset ss;
  int latest;
} dummy_context_s, *dummy_context;

/* Funky demo thing which just stores incrementing numbers in extra space. */

void produce_extra_data_callback(const char *key, void *ctx)
{
  dummy_context dc = ctx;
  void *p = stringset_get_data_from_string(dc->ss, key);
  char *c = p;

  *c = dc->latest++;
}


int main(int argc, char **argv)
{
  stringset ss = stringset_create(0, NULL, NULL);
  char foo1[4] = "foo";
  char foo2[4] = "foo";
  char *bar = "bar";
  dummy_context_s dc;

  KVASSERT(ss, "stringset_create failed");
  KVASSERT(foo1 != foo2, "initial pointers same?!?");
  KVASSERT(stringset_get_or_insert(ss, foo1) ==
           stringset_get_or_insert(ss, foo2), "within stringset not same");
  KVASSERT(stringset_get_or_insert(ss, foo1) !=
           stringset_get_or_insert(ss, bar), "foo == bar?!?");
  stringset_destroy(ss);

  ss = stringset_create(1, produce_extra_data_callback, &dc);
  dc.latest = 42;
  dc.ss = ss;
  stringset_get_or_insert(ss, foo1);
  KVASSERT(stringset_get(ss, foo1), "find broken");
  KVASSERT(strcmp(stringset_get(ss, foo1), foo1) == 0, "find result broken");
  KVASSERT(dc.latest == 43, "wrong latest 1: %d", dc.latest);

  stringset_get_or_insert(ss, foo2);
  KVASSERT(dc.latest == 43, "wrong latest 1.1: %d", dc.latest);
  stringset_get_or_insert(ss, bar);
  KVASSERT(dc.latest == 44, "wrong latest 2: %d", dc.latest);
  KVASSERT(*((char *)stringset_get_data_from_string(ss, stringset_get(ss, bar)))
           == 42+1, "wrong magic");
  stringset_destroy(ss);
  return 0;
}
