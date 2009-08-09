/*
  copyright   : GPL
  title       : fakeroot
  description : "classical" fakeroot hashtable + text-file database
  author      : joost witteveen, joostje@debian.org
*/

struct data_node_s;
typedef struct data_node_s {
  struct data_node_s *next;
  struct fakestat     buf;
  uint32_t            remote;
} data_node_t;

#define data_node_get(n)   ((struct fakestat *) &(n)->buf)

#define data_begin()  (data_node_next(NULL))


#define HASH_TABLE_SIZE 10009
#define HASH_DEV_MULTIPLIER 8328 /* = 2^64 % HASH_TABLE_SIZE */

#define fakestat_equal(a, b)  ((a)->dev == (b)->dev && (a)->ino == (b)->ino)

static int data_hash_val(const struct fakestat *key) {
  return (key->dev * HASH_DEV_MULTIPLIER + key->ino) % HASH_TABLE_SIZE;
}

static data_node_t *data_hash_table[HASH_TABLE_SIZE];

static void init_hash_table() {
  int table_pos;

  for (table_pos = 0; table_pos < HASH_TABLE_SIZE; table_pos++)
    data_hash_table[table_pos] = NULL;
}

static data_node_t *data_find(const struct fakestat *key,
			      const uint32_t remote)
{
  data_node_t *n;

  for (n = data_hash_table[data_hash_val(key)]; n; n = n->next) {
    if (fakestat_equal(&n->buf, key) && n->remote == remote)
      break;
  }

  return n;
}

static void data_insert(const struct fakestat *buf,
			const uint32_t remote)
{
  data_node_t *n, *last = NULL;
  
  for (n = data_hash_table[data_hash_val(buf)]; n; last = n, n = n->next)
    if (fakestat_equal(&n->buf, buf) && n->remote == remote)
      break;

  if (n == NULL) {
    n = calloc(1, sizeof (data_node_t));

    if (last)
      last->next = n;
    else
      data_hash_table[data_hash_val(buf)] = n;
  }

  memcpy(&n->buf, buf, sizeof (struct fakestat));
  n->remote = (uint32_t) remote;
}

/** Erase record
 *
 * A data_put() call is implicit.
 */
static data_node_t *data_erase(data_node_t *pos)
{
  data_node_t *n, *prev = NULL, *next;

  for (n = data_hash_table[data_hash_val(&pos->buf)]; n;
       prev = n, n = n->next)
    if (n == pos)
      break;

  next = n->next;

  if (n == data_hash_table[data_hash_val(&pos->buf)])
    data_hash_table[data_hash_val(&pos->buf)] = next;
  else
    prev->next = next;

  free(n);

  return next;
}

static data_node_t *data_node_next(data_node_t *n) {
  int table_pos;

  if (n != NULL && n->next != NULL)
    return n->next;

  if (n == NULL)
    table_pos = 0;
  else
    table_pos = data_hash_val(&n->buf) + 1;
  while (table_pos < HASH_TABLE_SIZE && data_hash_table[table_pos] == NULL)
    table_pos++;
  if (table_pos < HASH_TABLE_SIZE)
    return data_hash_table[table_pos];
  else
    return NULL;
}

static unsigned int data_size(void)
{
  unsigned int size = 0;
  int table_pos;
  data_node_t *n;

  for (table_pos = 0; table_pos < HASH_TABLE_SIZE; table_pos++)
    for (n = data_hash_table[table_pos]; n; n = n->next)
      size++;

  return size;

}


void debugdata(int dummy UNUSED){
  int stored_errno = errno;
  data_node_t *i;

  fprintf(stderr," FAKED keeps data of %i inodes:\n", data_size());
  for (i = data_begin(); i; i = data_node_next(i))
    debug_stat(data_node_get(i));

  errno = stored_errno;
}



/** Called when the data on data_node_t was updated
 */
static void data_update(data_node_t *i)
{
}

/** Drop reference to struct returned by data_find()
 *
 * Passing NULL as parameter is valid.
 */
static void data_put(data_node_t *i)
{
}

static void insert_or_overwrite(struct fakestat *st,
			 const uint32_t remote){
  data_node_t *i;
  
  i = data_find(st, remote);
  if (!i) {
    if(debug){
      fprintf(stderr,"FAKEROOT: insert_or_overwrite unknown stat:\n");
      debug_stat(st);
    }
    data_insert(st, remote);
  }
  else {
    memcpy(data_node_get(i), st, sizeof (struct fakestat));
    data_update(i);
  }
  data_put(i);
}

#ifdef FAKEROOT_DB_PATH
# define DB_PATH_LEN    4095
# define DB_PATH_SCAN "%4095s"

/*
 * IN:  'path' contains the dir to scan recursively
 * OUT: 'path' contains the matching file if 1 is returned
 */
static int scan_dir(const fake_dev_t dev, const fake_ino_t ino,
                    char *const path)
{
  const size_t pathlen = strlen(path) + strlen("/");
  if (pathlen >= DB_PATH_LEN)
    return 0;
  strcat(path, "/");

  DIR *const dir = opendir(path);
  if (!dir)
    return 0;

  struct dirent *ent;
  while ((ent = readdir(dir))) {
    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
      continue;

    if (ent->d_ino == ino) {
      struct stat buf;
      strncpy(path + pathlen, ent->d_name, DB_PATH_LEN - pathlen);
      if (lstat(path, &buf) == 0 && buf.st_dev == dev)
        break;
    } else if (ent->d_type == DT_DIR) {
      strncpy(path + pathlen, ent->d_name, DB_PATH_LEN - pathlen);
      if (scan_dir(dev, ino, path))
        break;
    }
  }

  closedir(dir);
  return ent != 0;
}

/*
 * Finds a path for inode/device pair--there can be several if bind mounts
 * are used.  This should not be a problem if the bind mount configuration
 * is the same when loading the database.
 *
 * IN:  'roots' contains the dirs to scan recursively (separated by colons)
 * OUT: 'path' contains the matching file if 1 is returned
 */
static int find_path(const fake_dev_t dev, const fake_ino_t ino,
                     const char *const roots, char *const path)
{
  unsigned int end = 0;

  do {
    unsigned int len, start = end;

    while (roots[end] != '\0' && roots[end] != ':')
      end++;

    len = end - start;
    if (len == 0)
      continue;

    if (roots[end - 1] == '/')
      len--;

    if (len > DB_PATH_LEN)
      len = DB_PATH_LEN;

    strncpy(path, roots + start, len);
    path[len] = '\0';

    if (scan_dir(dev, ino, path))
      return 1;
  } while (roots[end++] != '\0');

  return 0;
}

#endif



int save_database(const uint32_t remote)
{
#ifdef FAKEROOT_DB_PATH
  char path[DB_PATH_LEN + 1];
  const char *roots;
#endif
  data_node_t *i;
  FILE *f;

  if(!save_file)
    return 0;

#ifdef FAKEROOT_DB_PATH
  path[DB_PATH_LEN] = '\0';

  roots = getenv(DB_SEARCH_PATHS_ENV);
  if (!roots)
    roots = "/";
#endif

  do {
    int r,fd=0;
    struct stat s;
    r=stat(save_file,&s);
    if (r<0) {
       if (errno == ENOENT)
	  break;
       else
	  return EOF;
    }
    if (!(s.st_mode&S_IFIFO)) break;
    fd=open(save_file,O_WRONLY|O_NONBLOCK);
    if (fd<0) {
      sleep(1);
      continue;
    }
    close(fd);
    break;
  } while (1);


  f=fopen(save_file, "w");
  if(!f)
    return EOF;

  for (i = data_begin(); i; i = data_node_next(i)) {
    if (i->remote != remote)
      continue;

#ifdef FAKEROOT_DB_PATH
    if (find_path(i->buf.dev, i->buf.ino, roots, path))
      fprintf(f,"mode=%llo,uid=%llu,gid=%llu,nlink=%llu,rdev=%llu %s\n",
              (uint64_t) i->buf.mode,(uint64_t) i->buf.uid,(uint64_t) i->buf.gid,
              (uint64_t) i->buf.nlink,(uint64_t) i->buf.rdev,path);
#else
    fprintf(f,"dev=%llx,ino=%llu,mode=%llo,uid=%llu,gid=%llu,nlink=%llu,rdev=%llu\n",
            (uint64_t) i->buf.dev,(uint64_t) i->buf.ino,(uint64_t) i->buf.mode,
            (uint64_t) i->buf.uid,(uint64_t) i->buf.gid,(uint64_t) i->buf.nlink,
            (uint64_t) i->buf.rdev);
#endif
  }

  return fclose(f);
}

int load_database(const uint32_t remote)
{
  int r;

  uint64_t stdev, stino, stmode, stuid, stgid, stnlink, strdev;
  struct fakestat st;

#ifdef FAKEROOT_DB_PATH
  char path[DB_PATH_LEN + 1];
  struct stat path_st;

  path[DB_PATH_LEN] = '\0';
#endif

  while(1){
#ifdef FAKEROOT_DB_PATH
    r=scanf("mode=%llo,uid=%llu,gid=%llu,nlink=%llu,rdev=%llu "DB_PATH_SCAN"\n",
            &stmode, &stuid, &stgid, &stnlink, &strdev, &path);
    if (r != 6)
      break;

    if (stat(path, &path_st) < 0) {
      fprintf(stderr, "%s: %s\n", path, strerror(errno));
      if (errno == ENOENT || errno == EACCES)
        continue;
      else
        break;
    }
    stdev = path_st.st_dev;
    stino = path_st.st_ino;
#else
    r=scanf("dev=%llx,ino=%llu,mode=%llo,uid=%llu,gid=%llu,nlink=%llu,rdev=%llu\n",
            &stdev, &stino, &stmode, &stuid, &stgid, &stnlink, &strdev);
    if (r != 7)
      break;
#endif

    st.dev = stdev;
    st.ino = stino;
    st.mode = stmode;
    st.uid = stuid;
    st.gid = stgid;
    st.nlink = stnlink;
    st.rdev = strdev;
    data_insert(&st, remote);
  }
  if(!r||r==EOF)
    return 1;
  else
    return 0;
}

static void close_database(void)
{
}

