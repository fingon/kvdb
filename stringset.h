/*
 * $Id: stringset.h $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Wed Jul 24 17:33:32 2013 mstenber
 * Last modified: Wed Jul 24 17:43:16 2013 mstenber
 * Edit time:     5 min
 *
 */

#ifndef STRINGSET_H
#define STRINGSET_H

/* This module provides for internalized set of strings. By having
   these, there is no need to handle allocation or freeing of these
   strings; they are 'always' valid for the lifetime of the string
   set. */

typedef struct stringset_struct *stringset;

stringset stringset_create();
void stringset_destroy();
const char *stringset_get(stringset ss, const char *s);
const char *stringset_get_or_insert(stringset ss, const char *s);

#endif /* STRINGSET_H */
