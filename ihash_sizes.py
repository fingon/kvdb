#!/usr/bin/env python
# -*- coding: utf-8 -*-
# -*- Python -*-
#
# $Id: ihash_sizes.py $
#
# Author: Markus Stenberg <fingon@iki.fi>
#
# Copyright (c) 2013 Markus Stenberg
#
# Created:       Wed Jul 24 16:00:22 2013 mstenber
# Last modified: Wed Jul 24 16:02:05 2013 mstenber
# Edit time:     1 min
#
"""

Minimalist utility to dump the prime based hash sizes as used by the
ihash C code.

"""

import math

def is_prime(v):
    if v <= 3: return 1
    if (v % 2) == 0: return 0
    for x in range(3, int(math.sqrt(v))+2, 2):
        if (v % x) == 0: return 0
    return 1

if __name__ == '__main__':
    x = 13
    while x < 2**30:
        if is_prime(x):
            print x
            x = x * 3
            continue
        x = x + 1


