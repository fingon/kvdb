/*
 * $Id: util.h $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Wed Jul 24 14:06:57 2013 mstenber
 * Last modified: Sat Dec 14 06:20:10 2013 mstenber
 * Edit time:     19 min
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

#define KVPRINT(...)                            \
do {                                            \
  printf("[%s:%d]", __FILE__, __LINE__);        \
  printf(__VA_ARGS__);                          \
  printf("\n");                                 \
 } while(0)

#define KVVERIFY(y,...) do {            \
 if (!(y)) {                            \
  KVPRINT("Assertion %s failed: ", #y); \
  KVPRINT(__VA_ARGS__);                 \
  abort();                              \
 }                                      \
} while(0)

#if defined(DEBUG) || defined(PARANOID)

#define KVASSERT(y,...) KVVERIFY(y,__VA_ARGS__)

#else

#define KVASSERT(y,...)

#endif /* defined(DEBUG) || defined(PARANOID) */

#ifdef DEBUG

#define KVDEBUG(...) KVPRINT(__VA_ARGS__)

#else

#define KVDEBUG(x...)

#endif /* DEBUG */

static inline uint64_t hash_string(const char *s)
{
  uint64_t v = 0;

  /* djb2's later iteration which uses xor instead of add */
  while (*s)
    v = v * 33 ^ *s++;
  return v;
}

#endif /* UTIL_H */
