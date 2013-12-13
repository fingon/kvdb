/*
 * $Id: stringset_test.c $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Wed Jul 24 17:40:35 2013 mstenber
 * Last modified: Wed Jul 24 17:45:51 2013 mstenber
 * Edit time:     3 min
 *
 */

#define DEBUG
#include "stringset.h"
#include "util.h"

int main(int argc, char **argv)
{
  stringset ss = stringset_create();
  char foo1[4] = "foo";
  char foo2[4] = "foo";
  char *bar = "bar";

  KVASSERT(ss, "stringset_create failed");
  KVASSERT(foo1 != foo2, "initial pointers same?!?");
  KVASSERT(stringset_get_or_insert(ss, foo1) ==
           stringset_get_or_insert(ss, foo2), "within stringset not same");
  KVASSERT(stringset_get_or_insert(ss, foo1) !=
           stringset_get_or_insert(ss, bar), "foo == bar?!?");
  stringset_destroy(ss);
  return 0;
}
