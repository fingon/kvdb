/*
 * $Id: kvdb.h $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Wed Jul 24 11:17:32 2013 mstenber
 * Last modified: Mon Dec 23 18:59:27 2013 mstenber
 * Edit time:     159 min
 *
 */

#ifndef KVDB_H
#define KVDB_H

#include "util.h"

typedef struct kvdb_struct *kvdb;
typedef struct kvdb_o_struct *kvdb_o;

/* ~schema-ish */
typedef struct kvdb_app_struct *kvdb_app;
typedef struct kvdb_class_struct *kvdb_class;
typedef struct kvdb_key_struct *kvdb_key;
typedef struct kvdb_index_struct *kvdb_index;

/* query stuff */
typedef struct kvdb_query_struct *kvdb_query;

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
  /* No type; used only to indicate e.g. removal of a field, or unset value.
   * (In case of schema, used to indicate unknown type. Should not be
   * used by applications.) */
  KVDB_NULL,

  /* Variable length types */
  KVDB_BINARY, /** Just binary data (this is the default way of storing things) */
  KVDB_STRING, /** UTF-8 encoded string (null terminated). */

  /* Fixed length types */
  KVDB_INTEGER, /** Integer - 64bit signed integer. */
  KVDB_DOUBLE, /** Floating point - C double. */
  KVDB_COORD, /** WGS84 co-ordinate, with x and y component and radius (in degrees, very approximate) that can be used as bounding box check. */
  KVDB_OBJECT, /** Object reference (can form 1:N this way by using reverse index) */

  /** 'Small' binary without it's own memory allocation. Semantically
   * same as KVDB_BINARY. */
  KVDB_BINARY_SMALL
} kvdb_type;

typedef enum {
  /* Index by the int64 value */
  KVDB_INTEGER_INDEX,

  /* Index by the object reference. */
  KVDB_OBJECT_INDEX,

  /* Bounding box index. */
  KVDB_BOUNDING_BOX_INDEX, /* XXX */

  /* Raw content index. */
  KVDB_VALUE_INDEX /* XXX */

  /* XXX - TBD - substring search indexes etc. */
} kvdb_index_type;

/* How many bytes in hostname 'matter' in kvdb. */
#define KVDB_HOSTNAME_SIZE 8

/* How many bytes are required to store kvdb oid. */
#define KVDB_OID_SIZE (2 * sizeof(uint32_t) + KVDB_HOSTNAME_SIZE)

#define KVDB_INDEX_NAME_SIZE 16

typedef struct kvdb_oid_struct {
  unsigned char oid[KVDB_OID_SIZE];
} *kvdb_oid;

typedef struct kvdb_typed_value_struct {
  kvdb_type t;
  /* Anything <= 16 bytes can be stored here as-is. */
  union {
    int64_t i;
    double d;
    char *s;
    struct kvdb_oid_struct oid;
    struct __packed {
      double x, y, radius;
    } coord;
    struct {
      void *ptr;
      size_t ptr_size;
    } binary;
    unsigned char binary_small[KVDB_BINARY_SMALL_SIZE + 1];
  } v;
} *kvdb_typed_value;

/** Define application. */
kvdb_app kvdb_define_app(kvdb k, const char *name);

/** Define class. */
kvdb_class kvdb_define_class(kvdb k, const char *name);

/** Define key.
 *
 * The kvdb_type given is the only allowed type for that data from
 * this call onward. (Note: Changing types during lifetime of a
 * database will result in misbehavior of the system.)
 */
kvdb_key kvdb_define_key(kvdb k, const char *key, kvdb_type t);

/** Define (search) index.
 *
 * This allows creation of search indexes, in which objects can be
 * found much faster than just linearly going through the database.
 */
kvdb_index kvdb_define_index(kvdb k,
                             kvdb_key key,
                             const char *name,
                             kvdb_index_type index_type);

/** Create KVDB query object. */
kvdb_query kvdb_create_q(kvdb k);

/** Convenience function to iterate through objects referring to this one. */
kvdb_query kvdb_create_q_o_referring_us(kvdb_o o, kvdb_index i);

#define kvdb_for_each_o_refer_us(o, us, i, q) \
  for (q = kvdb_create_q_o_referring_us(us, i), \
         o = kvdb_q_get_next(q) ; o ; o = kvdb_q_get_next(q))

#define kvdb_for_each_app_class(o, app, cl, q)          \
  for (q = kvdb_create_q(k),                            \
         kvdb_q_set_match_app_class(q, app, cl),        \
         o = kvdb_q_get_next(q) ; o ; o = kvdb_q_get_next(q))

#define kvdb_get_or_create_one(o, app, cl)      \
do {                                            \
  kvdb_query q;                                 \
  kvdb_for_each_app_class(o, app, cl, q)        \
    break;                                      \
  if (o)                                        \
    kvdb_q_destroy(q);                          \
  else                                          \
    o = kvdb_create_o(k, app, cl);              \
 } while(0)

/** Add index to be used with query.
 *
 * If bounds are desired, they can be supplied.
 */
void kvdb_q_add_index(kvdb_query q, kvdb_index i,
                      kvdb_typed_value start, kvdb_typed_value end);
void kvdb_q_add_index2(kvdb_query q, kvdb_index i);
void kvdb_q_set_match_app_class(kvdb_query q, kvdb_app app, kvdb_class cl);
void kvdb_q_order_by(kvdb_query q, kvdb_index i, bool ascending);

/** Iterate through objects matching the given query.
 *
 * Return value is the next object matched by the query, or NULL (and
 * invalidation of the query) at the termination of iteration.
 */
kvdb_o kvdb_q_get_next(kvdb_query q);

/** Get rid of a query before it has finished (no NULL from
 * kvdb_q_get_next yet). */
void kvdb_q_destroy(kvdb_query q);

/** Create new object. (New id is allocated automatically.)
 */
kvdb_o kvdb_create_o(kvdb k, kvdb_app app, kvdb_class cl);

/** Retrieve single object from the kvdb.
 */
kvdb_o kvdb_get_o_by_id(kvdb k, const kvdb_oid oid);

/** Get value. NULL is returned if the key does not exist in the given
 * object or it is of wrong type. */
kvdb_typed_value kvdb_o_get(kvdb_o o, kvdb_key key);
int64_t *kvdb_o_get_int64(kvdb_o o, kvdb_key key);
char *kvdb_o_get_string(kvdb_o o, kvdb_key key);
kvdb_o kvdb_o_get_object(kvdb_o o, kvdb_key key);

/** Set value. Setters return false if the set fails for some reason.*/
bool kvdb_o_set(kvdb_o o, kvdb_key key, const kvdb_typed_value value);
bool kvdb_o_set_int64(kvdb_o o, kvdb_key key, int64_t value);
bool kvdb_o_set_string(kvdb_o o, kvdb_key key, const char *value);
bool kvdb_o_set_object(kvdb_o o, kvdb_key key, kvdb_o o2);

/** Commit changes to disk.
 */
bool kvdb_commit(kvdb k);

/** Export log to a new file within the given directory. Implies one
 * or more commits as well. */
bool kvdb_export(kvdb k, const char *directory, bool export_own_only);

/** Import any files within the directory not already imported. */
bool kvdb_import(kvdb k, const char *directory);

/** Inlined utility setters for typed values */
static inline void kvdb_tv_set_int64(kvdb_typed_value ktv, int64_t value)
{
  ktv->t = KVDB_INTEGER;
  ktv->v.i = value;
}

static inline void kvdb_tv_set_string(kvdb_typed_value ktv, char *value)
{
  ktv->t = KVDB_STRING;
  ktv->v.s = value;
}

static inline void kvdb_tv_set_oid(kvdb_typed_value ktv, kvdb_oid oid)
{
  ktv->t = KVDB_OBJECT;
  ktv->v.oid = *oid;
}

/* Intentionally not showing kvdb_o structure here => oh well. */
void kvdb_tv_set_object(kvdb_typed_value ktv, kvdb_o o);

/** Get type of a key. */
kvdb_type kvdb_key_get_type(kvdb_key key);

#endif /* KVDB_H */
