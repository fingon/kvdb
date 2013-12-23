/*
 * $Id: kvdb_io.c $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Mon Dec 23 17:20:54 2013 mstenber
 * Last modified: Mon Dec 23 20:05:26 2013 mstenber
 * Edit time:     59 min
 *
 */

/* This module implements the kvdb import and export features. */

/*
 * Both work with mostly sane(?) assumptions; that is, that we can
 * store timestamps so that time never _seems_ to go backwards, and
 * that we have clocks _roughly_ in sync across the whole sync cloud
 * (but they need not be exactly in sync as such).
 *
 * Log file format is just binary encoding, with 4 fields repeated
 * until file ends:
 *
 * oid data (assumed fixed length) +
 * signed varint len + key data +
 * signed varint len + value data +
 * signed varint last modified
 *
 * Note: The varints are used as canary to check sanity of the log
 * file. If they're below zero, something bad is going on and we can
 * abort reading that log.
 */

/* XXX - gzip-encode these. */

#define DEBUG

#include "kvdb_i.h"
#include "codec.h"

#include <sys/stat.h>
#include <dirent.h>

#define APP k->apps[APP_LOCAL_KVDB]

/* Singleton object */
#define IO_CLASS kvdb_define_class(k, "_kvdb_io")
#define MONOTONOUS_TIME_KEY kvdb_define_key(k, "_kvdb_io_monotonous_key", KVDB_STRING)
#define EXPORT_TIME_KEY kvdb_define_key(k, "_kvdb_io_export_key", KVDB_STRING)

/* Single 'read file' object */
#define READ_CLASS kvdb_define_class(k, "_kvdb_read")
#define READ_FILENAME_KEY kvdb_define_key(k, "_kvdb_io_read_filename_key", KVDB_STRING)
#define READ_FILESIZE_KEY kvdb_define_key(k, "_kvdb_io_read_filesize_key", KVDB_INTEGER)

bool _kvdb_io_init(kvdb k)
{
  /* Here we fetch the monotonous time (if any) within the database. */
  kvdb_o o;
  int64_t *ip;

  kvdb_get_or_create_one(o, APP, IO_CLASS);
  ip = kvdb_o_get_int64(o, MONOTONOUS_TIME_KEY);
  if (ip)
    k->monotonous_time = *ip;
  return true;
}

void _kvdb_io_pre_commit(kvdb k)
{
  kvdb_o o;
  int64_t *ip;

  kvdb_get_or_create_one(o, APP, IO_CLASS);
  ip = kvdb_o_get_int64(o, MONOTONOUS_TIME_KEY);

  /* If nothing to report changed, just avoid this. */
  if (ip && *ip == k->monotonous_time)
    return;
  /* Something changed - store the new time */
  kvdb_o_set_int64(o, MONOTONOUS_TIME_KEY, k->monotonous_time);
}

/* Utility macros for writing to the log file 'f'. */

#define PUSH_BINARY(p, len)             \
if (fwrite(p, 1, len, f) != len) {      \
  KVDEBUG("i/o error while writing");   \
  return false;                         \
 }

#define PUSH_INT(i)                                     \
do {                                                    \
  unsigned char buf[9];                                 \
  unsigned char *c = buf;                               \
  ssize_t left = sizeof(buf);                           \
  size_t len;                                           \
  encode_varint_s64(i, &c, &left);                      \
  KVASSERT(left >= 0, "should not run out of buffer");  \
  len = (int) (c-buf);                                  \
  PUSH_BINARY(buf, len);                                \
 } while(0)

bool kvdb_export(kvdb k, const char *directory, bool export_own_only)
{
  kvdb_o o;
  int64_t *ip;
  int c = 0;
  kvdb_time_t now = kvdb_monotonous_time(k);
  kvdb_time_t now_real = kvdb_time();
  FILE *f;
  char filename_tmp[128];
  char filename_final[128];

  kvdb_get_or_create_one(o, APP, IO_CLASS);
  ip = kvdb_o_get_int64(o, EXPORT_TIME_KEY);

  /* Export is relatively straightforward; pick unique-ish name
   * (hostname + NON-monototnous time), and if it exists, increment time
   * until it does not. Store the last export timestamp as well and
   * commit.
   */

  /* Start off with a commit. */
  if (!kvdb_commit(k))
    return false;

  /* Start the query of log table. Dump every entry. */
  sqlite3_stmt *stmt = k->stmts[export_own_only ? STMT_SELECT_LOG_BY_TA_OWN
                                : STMT_SELECT_LOG_BY_TA];
  SQLITE_CALL(sqlite3_reset(stmt));
  SQLITE_CALL(sqlite3_clear_bindings(stmt));
  SQLITE_CALL(sqlite3_bind_int64(stmt, 1, ip ? *ip : 0));

  int rc = sqlite3_step(stmt);
  while (rc == SQLITE_ROW)
    {
      /* oid, key, value, last_modified */
      KVASSERT(sqlite3_column_count(stmt)==4, "weird stmt count");

      size_t oid_len = sqlite3_column_bytes(stmt, 0);
      void *oid = (void *)sqlite3_column_blob(stmt, 0);

      const char *key = (const char *)sqlite3_column_text(stmt, 1);

      size_t value_len = sqlite3_column_bytes(stmt, 2);
      void *value = (void *)sqlite3_column_blob(stmt, 2);

      int64_t last_modified = sqlite3_column_int64(stmt, 3);

      if (c == 0)
        {
          /* Open file to dump stuff in. */
          while (1)
            {
              sprintf(filename_final, "%s/%lld.log", directory, now_real);
              struct stat st;
              if (lstat(filename_final, &st))
                break;
              now_real++;
            }
          sprintf(filename_tmp, "%s.tmp", filename_final);
          f = fopen(filename_tmp, "w");
          if (!f)
            {
              KVDEBUG("unable to open logfile %s for writing", filename_tmp);
              return false;
            }
        }

      KVASSERT(oid_len == KVDB_OID_SIZE, "invalid oid size");
      PUSH_BINARY(oid, oid_len);

      PUSH_INT(strlen(key));
      PUSH_BINARY(key, strlen(key));

      PUSH_INT(value_len);
      PUSH_BINARY(value, value_len);

      PUSH_INT(last_modified);

      c++;
      rc = sqlite3_step(stmt);
    }
  if (rc != SQLITE_DONE)
    {
      _kvdb_set_err_from_sqlite2(k, "export");
      return false;
    }


  /* Do commit if we actually exported something. */
  if (c)
    {
      fclose(f);
      if (rename(filename_tmp, filename_final))
        {
          KVDEBUG("rename of .tmp -> final failed");
          return false;
        }
      kvdb_get_or_create_one(o, APP, IO_CLASS);
      kvdb_o_set_int64(o, EXPORT_TIME_KEY, now);
      return kvdb_commit(k);
    }
  return true;
}


bool kvdb_import(kvdb k, const char *directory)
{
  DIR *d = opendir(directory);
  struct dirent *de;

  if (!d)
    {
      KVDEBUG("unable to open directory %s", directory);
      return false;
    }
  while ((de = readdir(d)))
    {
      if (string_endswith(de->d_name, ".log"))
        {
          struct stat st;
          char fullpath[128];
          FILE *f;
          struct kvdb_oid_struct oid;
          size_t s;

          sprintf(fullpath, "%s/%s", directory, de->d_name);
          if (lstat(fullpath, &st))
            {
              KVDEBUG("unable to stat %s", fullpath);
              return false;
            }
          /* Looks like potential candidate. Consider every log we
           * have ingested first, though. */
          kvdb_query q;
          kvdb_o o;
          bool found = false;
          kvdb_for_each_app_class(o, APP, READ_CLASS, q)
            {
              char *name = kvdb_o_get_string(o, READ_FILENAME_KEY);
              int64_t *sp = kvdb_o_get_int64(o, READ_FILESIZE_KEY);
              if (name && sp
                  && strcmp(name, de->d_name) == 0
                  && st->st_size == *sp)
                {
                  found = true;
                  break;
                }
            }
          if (found)
            {
              KVDEBUG("ignoring %s - already imported", de->d_name);
              continue;
            }

          /* NOT imported. So let's. */
          f = fopen(fullpath, "r");
          if (!f)
            {
              KVDEBUG("unable to open %s", fullpath);
              return false;
            }
          bool err = false;
          while(1)
            {
              s = fread(oid, 1, KVDB_OID_SIZE, f);
              if (!s)
                break;
              if (s != KVDB_OID_SIZE)
                {
                  KVDEBUG("invalid oid size detected - %d", s);
                  /* Hmm. Guess we're done with this file for good? */
                err:
                  err = true;
                  break;
                }
              /* Looks good. */
              char *k;
              void *p;
              size_t len;
              kvdb_time_t last_modified;

              POP_STRING(k);
              POP_BINARY(p, len);
              POP_INT(last_modified);

              o = kvdb_get_o_by_id(k, oid);
              if (!o)
                {
                  o = _kvdb_create_o(k, oid);
                  if (!o)
                    return false;
                }
              /* We haz o. */

              /* XXX - what to do with the damned kvdb_key? */
              if (!_kvdb_o_set(o, ..
            }
          fclose(f);
          /* Bail out if there was an error. We _did_ consume this
           * file anyway, to avoid annoying repetitions. */
          if (err)
            return false;
        }
    }
  closedir(d);
  return true;
}
