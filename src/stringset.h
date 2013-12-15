/*
 * $Id: stringset.h $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Wed Jul 24 17:33:32 2013 mstenber
 * Last modified: Sun Dec 15 09:32:01 2013 mstenber
 * Edit time:     8 min
 *
 */

#ifndef STRINGSET_H
#define STRINGSET_H

/* This module provides for internalized set of strings. By having
   these, there is no need to handle allocation or freeing of these
   strings; they are 'always' valid for the lifetime of the string
   set. */

typedef struct stringset_struct *stringset;

typedef void *(*stringset_produce_extra_data_callback)(const char *key, void *ctx);

stringset stringset_create3(int extra_data_len,
                            stringset_produce_extra_data_callback cb,
                            void *ctx);
stringset stringset_create();

void stringset_destroy();
const char *stringset_get(stringset ss, const char *s);
const char *stringset_get_or_insert(stringset ss, const char *s);

/* Get the 'extra data' segment for particular string (that has been
 * already stringset_get'd). */
void *stringset_get_data_from_string(stringset ss, const char *s);

#endif /* STRINGSET_H */
