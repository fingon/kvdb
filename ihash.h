/*
 * $Id: ihash.h $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Wed Jul 24 15:27:58 2013 mstenber
 * Last modified: Wed Jul 24 16:15:10 2013 mstenber
 * Edit time:     11 min
 *
 */

#ifndef IHASH_H
#define IHASH_H

#include <stdint.h>
#include <stdbool.h>

typedef struct ihash_struct *ihash;

/*
  This is array-based hash implementation. We use just one array for
  storing the hash, and therefore it is relatively efficient (there is
  a multiplier on memory cost based on how full we allow array to
  become).

  No assumption about ownership of the items is made.
*/

typedef uint64_t (*ihash_value_callback)(void *o);
typedef bool (*ihash_eq_callback)(void *o1, void *o2);
typedef bool (*ihash_iterator)(void *o, void *iterator_context);

ihash ihash_create(ihash_value_callback cb1, ihash_eq_callback cb2);
void ihash_destroy(ihash ih);

void *ihash_get(ihash ih, void *o_template);
ihash ihash_insert(ihash ih, void *o);
void ihash_remove(ihash ih, void *o);
void ihash_iterate(ihash ih, ihash_iterator cb, void *cb_context);

#endif /* IHASH_H */
