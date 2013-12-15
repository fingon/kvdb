/*
 * $Id: stringset.c $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Wed Jul 24 17:36:09 2013 mstenber
 * Last modified: Sun Dec 15 09:32:30 2013 mstenber
 * Edit time:     14 min
 *
 */

#include "stringset.h"
#include "ihash.h"
#include "util.h"
#include <string.h>

struct stringset_struct {
  ihash ih;
  int extra_data_len;
  stringset_produce_extra_data_callback cb;
  void *ctx;
};


static uint64_t _string_hash(void *o, void *ctx)
{
  const char *s = (const char *) o;
  return hash_string(s);
}

static bool _string_equal(void *o1, void *o2, void *ctx)
{
  if (o1 == o2) return true;
  const char *s1 = (const char *) o1;
  const char *s2 = (const char *) o2;
  return strcmp(s1, s2) == 0;
}

stringset stringset_create3(int extra_data_len,
                            stringset_produce_extra_data_callback cb,
                            void *ctx)
{
  stringset ss = calloc(1, sizeof(*ss));
  if (!ss) return NULL;
  ss->extra_data_len = extra_data_len;
  ss->cb = cb;
  ss->ctx = ctx;
  ss->ih = ihash_create(_string_hash, _string_equal, ss);
  if (!ss->ih)
    {
      free(ss);
      return NULL;
    }
  return ss;
}

stringset stringset_create()
{
  return stringset_create3(0, NULL, NULL);
}

static bool _free_string(void *o, void *context)
{
  free(o);
  return true;
}

void stringset_destroy(stringset ss)
{
  ihash_iterate(ss->ih, _free_string, NULL);
  ihash_destroy(ss->ih);
  free(ss);
}

const char *stringset_get(stringset ss, const char *s)
{
  void *r = ihash_get(ss->ih, (void *)s);
  return (const char *)r;
}

const char *stringset_get_or_insert(stringset ss, const char *s)
{
  const char *r = stringset_get(ss, s);
  if (r) return r;
  char *ns = strdup(s);
  if (!ns)
    return NULL;
  ss->ih = ihash_insert(ss->ih, ns);
  return ns;
}
