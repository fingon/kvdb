/*
 * $Id: kvdb.h $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Wed Jul 24 11:17:32 2013 mstenber
 * Last modified: Wed Jul 24 17:06:50 2013 mstenber
 * Edit time:     44 min
 *
 */

#ifndef KVDB_H
#define KVDB_H

#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct kvdb_struct *kvdb;
typedef struct kvdb_o_struct *kvdb_o;

/** Initialize the library (call this before anything else!)
 */
bool kvdb_init();

/** Create the database.
 *
 * Create (or get) database from the file in path. The return value is
 * whether the operation succeeded or not.
 */
bool kvdb_create(const char *path, kvdb *k);


/** Destroy the database (object).
 *
 * This frees the database object and everything associated with
 * it. There is no programmatic way to destroy the file on disk (but
 * unlink is your friend).
 */
void kvdb_destroy(kvdb k);


/** Get error message (if any). */
const char *kvdb_strerror(kvdb k);

typedef enum {
  KVDB_BOOL, /** Boolean - true or false. */
  KVDB_INTEGER, /** Integer - 64bit signed integer. */
  KVDB_DOUBLE, /** Floating point - C double. */
  KVDB_STRING, /** UTF-8 encoded string. */
  KVDB_COORD, /** WGS84 co-ordinate, with x and y component. */
  KVDB_BINARY, /** Just binary data */
  KVDB_UNTYPED,
} kvdb_type;

typedef struct {
  kvdb_type t;
  union {
    bool b;
    int64_t i;
    double d;
    char *s;
    struct {
      double x, y;
    } coord;
    struct {
      void *ptr;
      size_t ptr_size;
    } binary;
  } v;
} *kvdb_typed_value;

/** XXX Define schema entry.
 *
 * Define typing information for particular application and class of
 * object. If return value is false, the schema definition failed.
 */
bool kvdb_define_schema(kvdb k,
                        const char *app, const char *cl, int since_schema,
                        const char *key, kvdb_type t
                        );

/** Create new object. (New id is allocated automatically.)
 */
kvdb_o kvdb_create_o(kvdb k, const char *app, const char *cl);

/** Retrieve single object from the kvdb.
 */
kvdb_o kvdb_get_o_by_id(kvdb k, const char *id);

/** Iterator called per object.
 *
 * If return value is false, iteration is stopped.
 */
typedef bool (*kvdb_o_iterator)(kvdb_o o, void *iterator_context);

void kvdb_iterate_o_matching(kvdb k,
                             const char *app,
                             const char *cl,
                             const char *key,
                             const kvdb_typed_value value,
                             kvdb_o_iterator iterator,
                             void *iterator_context);

/** XXX Get type of a key. */
kvdb_type kvdb_o_get_type(kvdb_o o, const char *key);

/** Get value. Getters return false if the queried key does not exist. */
bool kvdb_o_get(kvdb_o o, const char *key, kvdb_typed_value value);

/** Set value. Setters return false if the set fails for some reason.*/
bool kvdb_o_set(kvdb_o o, const char *key, const kvdb_typed_value value);

/** Commit changes to disk.
 */
bool kvdb_commit(kvdb k);

#endif /* KVDB_H */
