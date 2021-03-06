#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/stat.h>

typedef struct stat __st;
#define stat xv6_stat  // avoid clash with host struct stat
#include "types.h"
#include "fs.h"
#include "stat.h"
#include "param.h"

//#define static_assert(a, b) do { switch (0) case 0: case (a): ; } while (0)
#define NELEM(x) (sizeof(x)/sizeof((x)[0]))

int nblocks = 8124;
int nlog = LOGSIZE;
int ninodes = 200;
int size = 8192;

int fsfd;
struct superblock sb;
char zeroes[512];
uint freeblock;
uint usedblocks;
uint bitblocks;
uint freeinode = 1;

void balloc(int);
void wsect(uint, void*);
void winode(uint, struct dinode*);
void rinode(uint inum, struct dinode *ip);
void rsect(uint sec, void *buf);
uint ialloc(ushort type, int mode, int uid, int gid);
void iappend(uint inum, void *p, int n);

// convert to intel byte order
ushort
xshort(ushort x)
{
  ushort y;
  uchar *a = (uchar*)&y;
  a[0] = x;
  a[1] = x >> 8;
  return y;
}

uint
xint(uint x)
{
  uint y;
  uchar *a = (uchar*)&y;
  a[0] = x;
  a[1] = x >> 8;
  a[2] = x >> 16;
  a[3] = x >> 24;
  return y;
}

struct disk_file
{
  const char* original_filename;
  const char* xv6_filename;
  int mode;
  int uid;
  int gid;
};

#define DEFAULT_DIR  S_IFDIR | 0755
#define DEFAULT_FILE S_IFREG | 0644

struct disk_file files[] =
{
  {"",            "etc",             DEFAULT_DIR, 0, 0},
  {"passwd_file", "etc/passwd",      S_IFREG | S_IRUGO | S_IWUSR, 0, 0},
  {"group_file",  "etc/group",       S_IFREG | S_IRUGO | S_IWUSR, 0, 0},
  {"",            "root",            S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP, 0, 0},
  {"",            "home",            DEFAULT_DIR, 0, 0},
  {"",            "home/user",       DEFAULT_DIR, 1, 1},
  {"README",      "home/user/README",DEFAULT_FILE, 1, 1},
  {"",            "bin",             DEFAULT_DIR, 0, 0},
  {"_cat",        "bin/cat",         S_IFREG | S_IRUGO | S_IWUSR | S_IXUGO, 0, 0},
  {"_echo",       "bin/echo",        S_IFREG | S_IRUGO | S_IWUSR | S_IXUGO, 0, 0},
  {"_grep",       "bin/grep",        S_IFREG | S_IRUGO | S_IWUSR | S_IXUGO, 0, 0},
  {"_init",       "bin/init",        S_IFREG | S_IRUGO | S_IWUSR | S_IXUGO, 0, 0},
  {"_kill",       "bin/kill",        S_IFREG | S_IRUGO | S_IWUSR | S_IXUGO, 0, 0},
  {"_ln",         "bin/ln",          S_IFREG | S_IRUGO | S_IWUSR | S_IXUGO, 0, 0},
  {"_ls",         "bin/ls",          S_IFREG | S_IRUGO | S_IWUSR | S_IXUGO, 0, 0},
  {"_mkdir",      "bin/mkdir",       S_IFREG | S_IRUGO | S_IWUSR | S_IXUGO, 0, 0},
  {"_rm",         "bin/rm",          S_IFREG | S_IRUGO | S_IWUSR | S_IXUGO, 0, 0},
  {"_sh",         "bin/sh",          S_IFREG | S_IRUGO | S_IWUSR | S_IXUGO, 0, 0},
  {"_wc",         "bin/wc",          S_IFREG | S_IRUGO | S_IWUSR | S_IXUGO, 0, 0},
  {"_zombie",     "bin/zombie",      S_IFREG | S_IRUGO | S_IWUSR | S_IXUGO, 0, 0},
  {"_helloworld", "bin/helloworld",  S_IFREG | S_IRUGO | S_IWUSR | S_IXUGO, 0, 0},
  {"_mkfifo",     "bin/mkfifo",      S_IFREG | S_IRUGO | S_IWUSR | S_IXUGO, 0, 0},
  {"_passwd",     "bin/passwd",      S_ISUID | S_IFREG | S_IRUGO | S_IWUSR | S_IXUGO, 0, 0},
  {"_login",      "bin/login",       S_IFREG | S_IRUGO | S_IWUSR | S_IXUGO, 0, 0},
  {"_id",         "bin/id",          S_IFREG | S_IRUGO | S_IWUSR | S_IXUGO, 0, 0},
  {"_chmod",      "bin/chmod",       S_IFREG | S_IRUGO | S_IWUSR | S_IXUGO, 0, 0},
  {"_chown",      "bin/chown",       S_IFREG | S_IRUGO | S_IWUSR | S_IXUGO, 0, 0},
  {"_useradd",    "bin/useradd",     S_IFREG | S_IRUGO | S_IWUSR | S_IXUGO, 0, 0},
  {"_ps",         "bin/ps",          S_IFREG | S_IRUGO | S_IWUSR | S_IXUGO, 0, 0},
  {"_chroot",     "bin/chroot",      S_IFREG | S_IRUGO | S_IWUSR | S_IXUGO, 0, 0},
  {"",            "test",            DEFAULT_DIR, 0, 0},
  {"_usertests",  "test/usertests",   S_IFREG | S_IRUGO | S_IWUSR | S_IXUGO, 0, 0},
  {"_stressfs",   "test/stressfs",    S_IFREG | S_IRUGO | S_IWUSR | S_IXUGO, 0, 0},
  {"_forktest",   "test/forktest",    S_IFREG | S_IRUGO | S_IWUSR | S_IXUGO, 0, 0},
  {"_mmap_test",  "test/mmap_test",   S_IFREG | S_IRUGO | S_IWUSR | S_IXUGO, 0, 0},
  {"_mmap_pp",    "test/mmap_pp",     S_IFREG | S_IRUGO | S_IWUSR | S_IXUGO, 0, 0},
  {"_thread_test","test/thread_test", S_IFREG | S_IRUGO | S_IWUSR | S_IXUGO, 0, 0},
  {"",            "test/chroot",     DEFAULT_DIR, 0, 0},
  {"",            "test/chroot/bin", DEFAULT_DIR, 0, 0},
  {"_sh",         "test/chroot/bin/sh", S_IFREG | S_IRUGO | S_IWUSR | S_IXUGO, 0, 0},
  {"_ls",         "test/chroot/bin/ls", S_IFREG | S_IRUGO | S_IWUSR | S_IXUGO, 0, 0},
  {"_mkdir",      "test/chroot/bin/mkdir", S_IFREG | S_IRUGO | S_IWUSR | S_IXUGO, 0, 0},
};

int added_inode_numbers[NELEM(files)];

int
main(int argc, char *argv[])
{
  int i, cc, fd;
  uint rootino, inum, off;
  struct dirent de;
  char buf[512];
  struct dinode din;


  static_assert(sizeof(int) == 4, "Integers must be 4 bytes!");

  if(argc < 2){
    fprintf(stderr, "Usage: mkfs fs.img files...\n");
    exit(1);
  }

  assert((512 % sizeof(struct dinode)) == 0);
  assert((512 % sizeof(struct dirent)) == 0);

  fsfd = open(argv[1], O_RDWR|O_CREAT|O_TRUNC, 0666);
  if(fsfd < 0){
    perror(argv[1]);
    exit(1);
  }

  sb.size = xint(size);
  sb.nblocks = xint(nblocks); // so whole disk is size sectors
  sb.ninodes = xint(ninodes);
  sb.nlog = xint(nlog);

  bitblocks = size/(512*4) + 1;
  usedblocks = ninodes / IPB + 3 + bitblocks;
  freeblock = usedblocks;

  printf("used %d (bit %d ninode %zu) free %u log %u total %d\n", usedblocks,
         bitblocks, ninodes/IPB + 1, freeblock, nlog, nblocks+usedblocks+nlog);

  assert(nblocks + usedblocks + nlog == size);

  for(i = 0; i < nblocks + usedblocks + nlog; i++)
    wsect(i, zeroes);

  memset(buf, 0, sizeof(buf));
  memmove(buf, &sb, sizeof(sb));
  wsect(1, buf);

  rootino = ialloc(T_DIR, 0755, 0, 0);
  assert(rootino == ROOTINO);

  bzero(&de, sizeof(de));
  de.inum = xshort(rootino);
  strcpy(de.name, ".");
  iappend(rootino, &de, sizeof(de));

  bzero(&de, sizeof(de));
  de.inum = xshort(rootino);
  strcpy(de.name, "..");
  iappend(rootino, &de, sizeof(de));

  for(i = 0; i < NELEM(files); i++){
    if((fd = open(files[i].original_filename, 0)) < 0 &&
        strlen(files[i].original_filename) > 0){
      perror(files[i].original_filename);
      exit(1);
    }
    int dp = -1;
    const char* last_slash = strrchr(files[i].xv6_filename, '/');
    // Find parent inode
    if (last_slash) {
      int parent_path_length = last_slash - files[i].xv6_filename;
      for (int j = 0; j < i; ++j) {
        if (strncmp(files[j].xv6_filename, files[i].xv6_filename,
              parent_path_length) == 0 &&
            (files[j].xv6_filename[parent_path_length] == 0 ||
             (files[j].xv6_filename[parent_path_length] == '/' &&
              files[j].xv6_filename[parent_path_length + 1] == 0))) {
          dp = added_inode_numbers[j];
          break;
        }
      }
      if (last_slash == files[i].xv6_filename) {
        dp = rootino;
      }
      if (dp < 0) {
        perror("Could not find parent");
        printf("Filename: %s\n", files[i].xv6_filename);
        exit(1);
      }
    }
    else {
      dp = rootino;
      last_slash = files[i].xv6_filename - 1;
    }

    inum = ialloc(0, files[i].mode, files[i].uid, files[i].gid);

    bzero(&de, sizeof(de));
    de.inum = xshort(inum);
    strncpy(de.name, last_slash + 1, DIRSIZ);
    iappend(dp, &de, sizeof(de));

    if (S_ISDIR(files[i].mode)) {
      bzero(&de, sizeof(de));
      de.inum = xshort(inum);
      strcpy(de.name, ".");
      iappend(inum, &de, sizeof(de));

      bzero(&de, sizeof(de));
      de.inum = xshort(dp);
      strcpy(de.name, "..");
      iappend(inum, &de, sizeof(de));
    }
    added_inode_numbers[i] = inum;

    if (strlen(files[i].original_filename) != 0) {
      while((cc = read(fd, buf, sizeof(buf))) > 0)
        iappend(inum, buf, cc);
      close(fd);
    }
  }

  // fix size of root inode dir
  rinode(rootino, &din);
  off = xint(din.size);
  off = ((off/BSIZE) + 1) * BSIZE;
  din.size = xint(off);
  winode(rootino, &din);

  balloc(usedblocks);

  exit(0);
}

void
wsect(uint sec, void *buf)
{
  if(lseek(fsfd, sec * 512L, 0) != sec * 512L){
    perror("lseek");
    exit(1);
  }
  if(write(fsfd, buf, 512) != 512){
    perror("write");
    exit(1);
  }
}

uint
i2b(uint inum)
{
  return (inum / IPB) + 2;
}

void
winode(uint inum, struct dinode *ip)
{
  char buf[512];
  uint bn;
  struct dinode *dip;

  bn = i2b(inum);
  rsect(bn, buf);
  dip = ((struct dinode*)buf) + (inum % IPB);
  *dip = *ip;
  wsect(bn, buf);
}

void
rinode(uint inum, struct dinode *ip)
{
  char buf[512];
  uint bn;
  struct dinode *dip;

  bn = i2b(inum);
  rsect(bn, buf);
  dip = ((struct dinode*)buf) + (inum % IPB);
  *ip = *dip;
}

void
rsect(uint sec, void *buf)
{
  if(lseek(fsfd, sec * 512L, 0) != sec * 512L){
    perror("lseek");
    exit(1);
  }
  if(read(fsfd, buf, 512) != 512){
    perror("read");
    exit(1);
  }
}

int
type_to_mode(short type)
{
  switch(type){
    case T_DIR:
      return 0040000;
    case T_FILE:
      return 0100000;
    case T_DEV:
      return 0020000;
    case T_PIPE:
      return 0010000;
  }
  return 0;
}

uint
ialloc(ushort type, int mode, int uid, int gid)
{
  uint inum = freeinode++;
  struct dinode din;

  bzero(&din, sizeof(din));
  din.mode = xint(type_to_mode(type) | mode);
  din.nlink = xshort(1);
  din.size = xint(0);
  din.uid = uid;
  din.gid = gid;
  winode(inum, &din);
  return inum;
}

void
balloc(int used)
{
  uchar buf[512];
  int i;

  printf("balloc: first %d blocks have been allocated\n", used);
  assert(used < 512*8);
  bzero(buf, 512);
  for(i = 0; i < used; i++){
    buf[i/8] = buf[i/8] | (0x1 << (i%8));
  }
  printf("balloc: write bitmap block at sector %zu\n", ninodes/IPB + 3);
  wsect(ninodes / IPB + 3, buf);
}

#define min(a, b) ((a) < (b) ? (a) : (b))

void
iappend(uint inum, void *xp, int n)
{
  char *p = (char*)xp;
  uint fbn, off, n1;
  struct dinode din;
  char buf[512];
  uint indirect[NINDIRECT];
  uint x;

  rinode(inum, &din);

  off = xint(din.size);
  while(n > 0){
    fbn = off / 512;
    assert(fbn < MAXFILE);
    if(fbn < NDIRECT){
      if(xint(din.addrs[fbn]) == 0){
        din.addrs[fbn] = xint(freeblock++);
        usedblocks++;
      }
      x = xint(din.addrs[fbn]);
    } else {
      if(xint(din.addrs[NDIRECT]) == 0){
        // printf("allocate indirect block\n");
        din.addrs[NDIRECT] = xint(freeblock++);
        usedblocks++;
      }
      // printf("read indirect block\n");
      rsect(xint(din.addrs[NDIRECT]), (char*)indirect);
      if(indirect[fbn - NDIRECT] == 0){
        indirect[fbn - NDIRECT] = xint(freeblock++);
        usedblocks++;
        wsect(xint(din.addrs[NDIRECT]), (char*)indirect);
      }
      x = xint(indirect[fbn-NDIRECT]);
    }
    n1 = min(n, (fbn + 1) * 512 - off);
    rsect(x, buf);
    bcopy(p, buf + off - (fbn * 512), n1);
    wsect(x, buf);
    n -= n1;
    off += n1;
    p += n1;
  }
  din.size = xint(off);
  winode(inum, &din);
}
