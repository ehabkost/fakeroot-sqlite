/* faked-sqlite.c
 *
 * sqlite support for faked
 *
 * Copyright (c) 2009, Eduardo Habkost <ehabkost@raisama.net>
 *
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <assert.h>

#include <sqlite3.h>

struct data_node_s;
typedef struct data_node_s {
  struct fakestat     buf;
} data_node_t;

#define data_node_get(i) (&(i)->buf)

#define MAX_REMOTES 1024

static sqlite3 *database;
static sqlite3_stmt *find_stmt, *update_stmt, *delete_stmt;

static void init_hash_table()
{
}

static int create_database(void)
{
  int r;
  char *error;
  r = sqlite3_exec(database,
       "CREATE TABLE IF NOT EXISTS \
        fakestats (dev INTEGER, ino INTEGER, mode INTEGER, uid INTEGER, \
                   gid INTEGER, nlink INTEGER, rdev INTEGER, \
                   PRIMARY KEY (dev, ino))",
       NULL, NULL,  &error);
  if (r) {
    fprintf(stderr, "FAKEROOT: error creating sqlite database: %s\n", error);
    return 0;
  }

  return 1;
}


static int load_database(const uint32_t remote)
{
  int r;
  assert(remote == 0);

  r = sqlite3_open(save_file, &database);
  if (r)
    goto err;

  if (!create_database())
    goto err_close;

  r = sqlite3_prepare_v2(database,
       "SELECT dev,ino,mode,uid,gid,nlink,rdev FROM fakestats WHERE dev=? AND ino=?",
       -1, &find_stmt, NULL);
  if (r)
    goto err;

  r = sqlite3_prepare_v2(database,
       "INSERT OR REPLACE INTO fakestats \
        (dev, ino, mode, uid, gid, nlink, rdev) \
        VALUES \
        (?,?,?,?,?,?,?)",
       -1, &update_stmt, NULL);
  if (r)
    goto err;

  r = sqlite3_prepare_v2(database,
       "DELETE FROM fakestats WHERE dev=? AND ino=?",
       -1, &delete_stmt, NULL);
  if (r)
    goto err;


  return 1;

err:
  fprintf(stderr, "FAKEROOT: can't open sqlite database: %s\n", sqlite3_errmsg(database));
err_close:
  sqlite3_close(database);
  return 0;
}

static void close_database(void)
{
  sqlite3_finalize(find_stmt);
  sqlite3_finalize(update_stmt);
  sqlite3_finalize(delete_stmt);
  sqlite3_close(database);
}

static int save_database(const uint32_t remote)
{
  assert(remote == 0);
}

static data_node_t *data_find(const struct fakestat *key,
			      const uint32_t remote)
{
  int r;
  data_node_t *i;

  assert(remote == 0);

  sqlite3_reset(find_stmt);
  sqlite3_clear_bindings(find_stmt);

  r = sqlite3_bind_int64(find_stmt, 1, key->dev);
  if (r)
    goto err;

  r = sqlite3_bind_int64(find_stmt, 2, key->ino);
  if (r)
    goto err;

  while (1) {
    r = sqlite3_step(find_stmt);
    switch (r) {
      case SQLITE_ROW:
        i = malloc(sizeof(*i));
        if (!i) {
          fprintf(stderr, "FAKEROOT: faked-sqlite: malloc error\n");
          abort();
        }

#       define field(n, fname) \
          i->buf.fname = sqlite3_column_int64(find_stmt, n);
        field(0, dev);
        field(1, ino);
        field(2, mode);
        field(3, uid);
        field(4, gid);
        field(5, nlink);
        field(6, rdev);
#       undef field

        return i;

        break;

      case SQLITE_DONE:
        /* No row */
        return NULL;

      default:
        goto err;
    }
  }

err:
  fprintf(stderr, "FAKEROOT: data_find() sqlite error: %d, %s\n", r, sqlite3_errmsg(database));
  abort();
}

static void data_put(data_node_t *i)
{
  free(i);
}

static data_node_t *data_erase(data_node_t *i)
{
  int r;
  sqlite3_reset(delete_stmt);
  sqlite3_clear_bindings(delete_stmt);

  r = sqlite3_bind_int64(delete_stmt, 1, i->buf.dev);
  if (r)
    goto err;

  r = sqlite3_bind_int64(delete_stmt, 2, i->buf.ino);
  if (r)
    goto err;

  r = sqlite3_step(delete_stmt);
  if (r != SQLITE_DONE)
    goto err;

  data_put(i);
  return NULL;

err:
  fprintf(stderr, "FAKEROOT: data_erase() sqlite error: %d, %s\n", r, sqlite3_errmsg(database));
  abort();
}

static void __data_update(struct fakestat *st)
{
  int r;
  sqlite3_reset(update_stmt);
  sqlite3_clear_bindings(update_stmt);

# define field(n, fname) \
    r = sqlite3_bind_int64(update_stmt, n, st->fname); \
    if (r) goto err;
  field(1, dev);
  field(2, ino);
  field(3, mode);
  field(4, uid);
  field(5, gid);
  field(6, nlink);
  field(7, rdev);
# undef field

  r = sqlite3_step(update_stmt);
  if (r != SQLITE_DONE)
    goto err;

  return;

err:
  fprintf(stderr, "FAKEROOT: __data_update() sqlite error: %d, %s\n", r, sqlite3_errmsg(database));
  abort();
}

static void data_update(data_node_t *i)
{
  __data_update(&i->buf);
}

static void insert_or_overwrite(struct fakestat *st,
			 const uint32_t remote)
{
  assert(remote == 0);
  __data_update(st);
}

void debugdata(int dummy UNUSED)
{
  fprintf(stderr, "FAKEROOT: sqlite debugdata() -- implement me!\n");
}

