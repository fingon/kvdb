/*
 * $Id: kvdb_io.c $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Mon Dec 23 17:20:54 2013 mstenber
 * Last modified: Mon Dec 23 17:32:14 2013 mstenber
 * Edit time:     1 min
 *
 */

/* This module implements the kvdb import and export features. */

/* Both work with mostly sane(?) assumptions; that is, that we can
 * storally store timestamps so that time never _seems_ to go
 * backwards, and that we have clocks _roughly_ in sync across the
 * whole sync cloud (but they need not be exactly in sync as such). */

#define DEBUG

#include "kvdb_i.h"

bool kvdb_export(kvdb k, const char *directory, bool export_own_only)
{
  return false;
}


/** Import any files within the directory not already imported. */
bool kvdb_import(kvdb k, const char *directory)
{
  return false;
}
