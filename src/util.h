/*
 * $Id: util.h $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Wed Jul 24 14:06:57 2013 mstenber
 * Last modified: Mon Dec 23 19:50:02 2013 mstenber
 * Edit time:     43 min
 *
 */

#ifndef UTIL_H
#define UTIL_H

/* printf */
#include <stdio.h>

/* calloc, abort */
#include <stdlib.h>

/* *int64_t, stdbool, ?? */
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

/* strlen */
#include <string.h>

/* gettimeofday; more portable than clock_gettime (albeit somewhat
   inferior, but sufficient for our needs) */
#include <sys/time.h>



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
  fflush(stdout);                       \
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

static inline uint64_t hash_bytes(const void *p, size_t n)
{
  uint64_t v = 0;

  /* djb2's later iteration which uses xor instead of add */
  while (n--)
    v = v * 33 ^ *((unsigned char *)p++);
  return v;
}

static inline uint64_t hash_string(const char *s)
{
  return hash_bytes(s, strlen(s));
}

#define MS_PER_S 1000
#define US_PER_S 1000000
typedef int64_t kvdb_time_t;

static inline kvdb_time_t kvdb_time(void)
{
  struct timeval tv;

  gettimeofday(&tv, NULL);
  return (int64_t)tv.tv_sec * MS_PER_S +
    (int64_t)tv.tv_usec * MS_PER_S / US_PER_S;
}

static inline bool string_endswith(const char *s, const char *suffix)
{
  int l1 = strlen(s);
  int l2 = strlen(suffix);

  if (l2 > l1)
    return false;
  return strcmp(s + (l1 - l2), suffix) == 0;
}

#endif /* UTIL_H */
