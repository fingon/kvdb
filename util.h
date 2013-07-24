/*
 * $Id: util.h $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Wed Jul 24 14:06:57 2013 mstenber
 * Last modified: Wed Jul 24 19:09:37 2013 mstenber
 * Edit time:     14 min
 *
 */

#ifndef UTIL_H
#define UTIL_H

/* printf */
#include <stdio.h>

/* calloc, abort */
#include <stdlib.h>

/* *int64_t */
#include <stdint.h>

#define KVPRINT(fmt, x...)                      \
do {                                            \
  printf("[%s:%d]", __FILE__, __LINE__);        \
  printf(fmt, ##x);                             \
  printf("\n");                                 \
 } while(0)

#if defined(DEBUG) || defined(PARANOID)

#define KVASSERT(y,fmt,x...) do {       \
 if (!(y)) {                            \
  KVPRINT("Assertion %s failed: ", #y); \
  KVPRINT(fmt,##x);                     \
  abort();                              \
 }                                      \
} while(0)

#else

#define KVASSERT(y,x...)

#endif /* defined(DEBUG) || defined(PARANOID) */

#ifdef DEBUG

#define KVDEBUG(fmt,x...) KVPRINT(fmt,##x)

#else

#define KVDEBUG(x...)

#endif /* DEBUG */

static inline uint64_t hash_string(const char *s)
{
  uint64_t v = 0;

  /* Ok, could be better.. smirk. */
  while (*s)
    v = v * 383 + *s++;
  return v;
}

#endif /* UTIL_H */
