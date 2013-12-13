/*
 * $Id: stringset.c $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Wed Jul 24 17:36:09 2013 mstenber
 * Last modified: Wed Jul 24 17:58:11 2013 mstenber
 * Edit time:     9 min
 *
 */

#include "stringset.h"
#include "ihash.h"
#include "util.h"
#include <string.h>

struct stringset_struct {
  ihash ih;
};


static uint64_t _string_hash(void *o)
{
  const char *s = (const char *) o;
  return hash_string(s);
}

static bool _string_equal(void *o1, void *o2)
{
  if (o1 == o2) return true;
  const char *s1 = (const char *) o1;
  const char *s2 = (const char *) o2;
  return strcmp(s1, s2) == 0;
}

stringset stringset_create()
{
  stringset ss = calloc(1, sizeof(*ss));
  if (!ss) return NULL;
  ss->ih = ihash_create(_string_hash, _string_equal);
  if (!ss->ih)
    {
      free(ss);
      return NULL;
    }
  return ss;
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