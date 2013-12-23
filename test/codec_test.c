/*
 * $Id: codec_test.c $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Mon Dec 23 18:57:09 2013 mstenber
 * Last modified: Mon Dec 23 19:15:48 2013 mstenber
 * Edit time:     12 min
 *
 */

#define DEBUG

#include "codec.h"

static void test_endecode_u64(uint64_t v, int buf_bytes,
                              bool exp_ok, int exp_used)
{
  unsigned char buf[128];
  unsigned char *c = buf, *d;
  ssize_t left = buf_bytes;
  int i;
  uint64_t v2;
  bool r;

  KVDEBUG("test_endecode_u64 %lld", (int64_t) v);

  encode_varint_u64(v, &c, &left);
  if (!exp_ok)
    {
      KVASSERT(left == -1, "wrong result - succeeded?!?");
      return;
    }
  /* Ok path. */
  KVASSERT(left == buf_bytes - exp_used, "wrong # left: %d (expected:%d, really used %d)", (int)left, exp_used, buf_bytes - left);
  KVASSERT(c == buf + exp_used, "buffer in wrong place");

  /* Decodes it, and make sure it looks sane (and consumes only right
   * #). */

  /* Should fail if too short too. */
  for (i = -1 ; i < 2 ; i++)
    {
      d = buf;
      left = exp_used + i; /* These are supposed to be successful decode
                            * cases after all. */
      r = decode_varint_u64(&d, &left, &v2);
      if (i < 0)
        {
          KVASSERT(!r, "should not succeed with partial input");
        }
      else
        {
          KVASSERT(r, "should succeed with complete input");
          KVASSERT(v == v2, "should have same result");
          KVASSERT(d == c, "should point at end of encode result");
          KVASSERT(left == i, "should have right left (smirk)");
        }
    }
}

int main(int argc, char **argv)
{
  test_endecode_u64(1, 3, true, 1);
  test_endecode_u64(1, 0, false, 0);
  test_endecode_u64(128, 0, false, 0);
  test_endecode_u64(128, 1, false, 0);
  test_endecode_u64(128, 2, true, 2);
  test_endecode_u64(23467742135246, 10, true, 7);
  return 0;
}
