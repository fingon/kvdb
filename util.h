/*
 * $Id: util.h $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Wed Jul 24 14:06:57 2013 mstenber
 * Last modified: Wed Jul 24 16:09:22 2013 mstenber
 * Edit time:     11 min
 *
 */

#ifndef UTIL_H
#define UTIL_H

/* printf */
#include <stdio.h>

/* calloc, abort */
#include <stdlib.h>

#ifdef DEBUG

#define KVDEBUG(fmt, x...)                      \
do {                                            \
  printf("[%s:%d]", __FILE__, __LINE__);        \
  printf(fmt, ##x);                             \
  printf("\n");                                 \
 } while(0)

#define KVASSERT(y,fmt,x...) do {       \
 if (!(y)) {                            \
  KVDEBUG("Assertion %s failed: ", #y); \
  KVDEBUG(fmt,##x);                     \
  abort();                              \
 }                                      \
} while(0)

#else

#define KVDEBUG(x...)
#define KVASSERT(y,x...)

#endif /* DEBUG */


#endif /* UTIL_H */
