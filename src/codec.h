/*
 * $Id: codec.h $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Mon Dec 23 18:49:45 2013 mstenber
 * Last modified: Mon Dec 23 19:11:02 2013 mstenber
 * Edit time:     14 min
 *
 */

#ifndef CODEC_H
#define CODEC_H

#include "util.h"

/* Rather simple binary codec for en-decoding varints (both signed and
 * unsigned) and some utility functions for handling decoding of
 * binary blobs too. This is more or less copy of (documented)
 * functionality of varints of Google protocol buffers. (I rather not
 * get more external dependencies). */

#define VARINT_BITS_PER_BYTE 7
#define VARINT_HIGH_VALUE ( 1 << VARINT_BITS_PER_BYTE )

static inline void encode_varint_u64(uint64_t v,
                                     unsigned char **buf, ssize_t *left)
{
  unsigned char *c = *buf;
  
  while (v >= VARINT_HIGH_VALUE && *left > 0)
    {
      *c++ = v % VARINT_HIGH_VALUE + VARINT_HIGH_VALUE;
      (*left)--;
      v >>= VARINT_BITS_PER_BYTE;
    }
  if (*left > 0)
    {
      *c++ = v;
      (*left)--;
      *buf = c;
    }
  else
    {
      (*left) = -1;
    }
}

static inline bool decode_varint_u64(unsigned char **buf, ssize_t *left,
                                     uint64_t *v)
{
  /* Find first byte without highest bit set, and increment beyond that. */
  unsigned char *s, *e;

  *v = 0;
  e = s = *buf;
  while (*left > 0 && (*e & VARINT_HIGH_VALUE))
    {
      e++;
      (*left)--;
    }
  if (*left <= 0)
    return false;
  /* Ok, 'e' points now at highest order bits. So mark the end, and
   * then iterate from e to s, shifting bits as we go.. */
  *buf = e + 1;
  (*left)--;
  *v = *e;
  while (e > s)
    {
      e--;
      *v = (*v << VARINT_BITS_PER_BYTE) + *e % VARINT_HIGH_VALUE;
    }
  return true;
}



#endif /* CODEC_H */
