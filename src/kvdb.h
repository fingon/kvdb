/*
 * $Id: kvdb.h $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Wed Jul 24 11:17:32 2013 mstenber
 * Last modified: Fri Dec 20 16:21:05 2013 mstenber
 * Edit time:     75 min
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

#define KVDB_BINARY_SMALL_SIZE 16

typedef enum {
  /* Variable length types */
  KVDB_BINARY, /** Just binary data (this is the default way of storing things) */
  KVDB_STRING, /** UTF-8 encoded string (null terminated). */

  /* Fixed length types */
  KVDB_INTEGER, /** Integer - 64bit signed integer. */
  KVDB_DOUBLE, /** Floating point - C double. */
  KVDB_COORD, /** WGS84 co-ordinate, with x and y component. */
  KVDB_OBJECT, /** Object reference (can form 1:N this way by using reverse index) */

  /** 'Small' binary without it's own memory allocation. Semantically
   * same as KVDB_BINARY. */
  KVDB_BINARY_SMALL
} kvdb_type;

/* How many bytes in hostname 'matter' in kvdb. */
#define KVDB_HOSTNAME_SIZE 8

/* How many bytes are required to store kvdb oid. */
#define KVDB_OID_SIZE (2 * sizeof(uint32_t) + KVDB_HOSTNAME_SIZE)


/** Intern a string s.t. it is in kvdb, and owned by kvdb.
 *
 * All provided keys should be such (and app, cl ones are
 * automatically converted).
 * 
 * NOTE: Under no conditions whatsoever
 * should _values_ be interned. That would lead to huge memory leak!
 * (Interned strings are effectively never freed until kvdb instance
 * itself is destroyed.)
 */
const char *kvdb_intern(kvdb k, const char *s);

typedef struct kvdb_typed_value_struct {
  kvdb_type t;
  /* Anything <= 16 bytes can be stored here as-is. */
  union {
    int64_t i;
    double d;
    char *s;
    unsigned char oid[KVDB_OID_SIZE];
    struct __packed {
      double x, y;
    } coord;
    struct {
      void *ptr;
      size_t ptr_size;
    } binary;
    unsigned char binary_small[KVDB_BINARY_SMALL_SIZE + 1];
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
kvdb_o kvdb_get_o_by_id(kvdb k, const void *oid);

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

/** Get value. NULL is returned if the key does not exist in the given
 * object or it is of wrong type. */
kvdb_typed_value kvdb_o_get(kvdb_o o, const char *key);
int64_t *kvdb_o_get_int64(kvdb_o o, const char *key);
char *kvdb_o_get_string(kvdb_o o, const char *key);

/** Set value. Setters return false if the set fails for some reason.*/
bool kvdb_o_set(kvdb_o o, const char *key, const kvdb_typed_value value);
bool kvdb_o_set_int64(kvdb_o o, const char *key, int64_t value);
bool kvdb_o_set_string(kvdb_o o, const char *key, const char *value);

/** Commit changes to disk.
 */
bool kvdb_commit(kvdb k);

#endif /* KVDB_H */
