// File system implementation.  Five layers:
//   + Blocks: allocator for raw disk blocks.
//   + Log: crash recovery for multi-step updates.
//   + Files: inode allocator, reading, writing, metadata.
//   + Directories: inode with special contents (list of other inodes!)
//   + Names: paths like /usr/rtm/xv6/fs.c for convenient naming.
//
// This file contains the low-level file system manipulation 
// routines.  The (higher-level) system call implementations
// are in sysfile.c.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "buf.h"
#include "fs.h"
#include "file.h"
#include "errno.h"
#include "err.h"

#define min(a, b) ((a) < (b) ? (a) : (b))
static void itrunc(struct inode*);

// Read the super block.
void
readsb(int dev, struct superblock *sb)
{
  struct buf *bp;
  
  bp = bread(dev, 1);
  memmove(sb, bp->data, sizeof(*sb));
  brelse(bp);
}

// Zero a block.
static void
bzero(int dev, int bno)
{
  struct buf *bp;
  
  bp = bread(dev, bno);
  memset(bp->data, 0, BSIZE);
  log_write(bp);
  brelse(bp);
}

// Blocks. 

// Allocate a zeroed disk block.
static uint
balloc(uint dev)
{
  int b, bi, m;
  struct buf *bp;
  struct superblock sb;

  bp = 0;
  readsb(dev, &sb);
  for(b = 0; b < sb.size; b += BPB){
    bp = bread(dev, BBLOCK(b, sb.ninodes));
    for(bi = 0; bi < BPB && b + bi < sb.size; bi++){
      m = 1 << (bi % 8);
      if((bp->data[bi/8] & m) == 0){  // Is block free?
        bp->data[bi/8] |= m;  // Mark block in use.
        log_write(bp);
        brelse(bp);
        bzero(dev, b + bi);
        return b + bi;
      }
    }
    brelse(bp);
  }
  panic("balloc: out of blocks");
}

// Free a disk block.
static void
bfree(int dev, uint b)
{
  struct buf *bp;
  struct superblock sb;
  int bi, m;

  readsb(dev, &sb);
  bp = bread(dev, BBLOCK(b, sb.ninodes));
  bi = b % BPB;
  m = 1 << (bi % 8);
  if((bp->data[bi/8] & m) == 0)
    panic("freeing free block");
  bp->data[bi/8] &= ~m;
  log_write(bp);
  brelse(bp);
}

// Inodes.
//
// An inode describes a single unnamed file.
// The inode disk structure holds metadata: the file's type,
// its size, the number of links referring to it, and the
// list of blocks holding the file's content.
//
// The inodes are laid out sequentially on disk immediately after
// the superblock. Each inode has a number, indicating its
// position on the disk.
//
// The kernel keeps a cache of in-use inodes in memory
// to provide a place for synchronizing access
// to inodes used by multiple processes. The cached
// inodes include book-keeping information that is
// not stored on disk: ip->ref and ip->flags.
//
// An inode and its in-memory represtative go through a
// sequence of states before they can be used by the
// rest of the file system code.
//
// * Allocation: an inode is allocated if its type (on disk)
//   is non-zero. ialloc() allocates, iput() frees if
//   the link count has fallen to zero.
//
// * Referencing in cache: an entry in the inode cache
//   is free if ip->ref is zero. Otherwise ip->ref tracks
//   the number of in-memory pointers to the entry (open
//   files and current directories). iget() to find or
//   create a cache entry and increment its ref, iput()
//   to decrement ref.
//
// * Valid: the information (type, size, &c) in an inode
//   cache entry is only correct when the I_VALID bit
//   is set in ip->flags. ilock() reads the inode from
//   the disk and sets I_VALID, while iput() clears
//   I_VALID if ip->ref has fallen to zero.
//
// * Locked: file system code may only examine and modify
//   the information in an inode and its content if it
//   has first locked the inode. The I_BUSY flag indicates
//   that the inode is locked. ilock() sets I_BUSY,
//   while iunlock clears it.
//
// Thus a typical sequence is:
//   ip = iget(dev, inum)
//   ilock(ip)
//   ... examine and modify ip->xxx ...
//   iunlock(ip)
//   iput(ip)
//
// ilock() is separate from iget() so that system calls can
// get a long-term reference to an inode (as for an open file)
// and only lock it for short periods (e.g., in read()).
// The separation also helps avoid deadlock and races during
// pathname lookup. iget() increments ip->ref so that the inode
// stays cached and pointers to it remain valid.
//
// Many internal file system functions expect the caller to
// have locked the inodes involved; this lets callers create
// multi-step atomic operations.

struct {
  struct spinlock lock;
  struct list_head list;
  struct cache_info* cache;
} fs_cache;

struct {
  struct spinlock lock;
  struct list_head list;
  struct cache_info* cache;
} icache;

void
init_root_fs(struct filesystem* fs)
{
  acquire(&fs_cache.lock);
  memset(fs, 0, sizeof(struct filesystem));
  fs->index = ROOTDEV;
  fs->dev = ROOTDEV;
  list_add(&fs->list, &fs_cache.list);
  release(&fs_cache.lock);
}

static void
procfs_put(struct filesystem* fs, struct inode *ip)
{ }

struct inode*
procfs_alloc(struct filesystem* fs, short type)
{
  return 0;
}

void
init_proc_fs(struct filesystem* fs)
{
  acquire(&fs_cache.lock);
  memset(fs, 0, sizeof(struct filesystem));
  fs->index = PROCDEV;
  fs->dev = PROCDEV;
  fs->ops.put = procfs_put;
  fs->ops.alloc = procfs_alloc;
  list_add(&fs->list, &fs_cache.list);
  release(&fs_cache.lock);
}

struct filesystem*
find_fs(uint fs_index)
{
  acquire(&fs_cache.lock);
  struct filesystem* fs;
  list_for_each_entry(fs, &fs_cache.list, list) {
    if (fs->index == fs_index) {
      release(&fs_cache.lock);
      return fs;
    }
  }
  release(&fs_cache.lock);
  return 0;
}

void
iinit(void)
{
  initlock(&icache.lock, "icache");
  INIT_LIST_HEAD(&icache.list);
  icache.cache = kmem_cache_create(sizeof(struct inode));
  INIT_LIST_HEAD(&fs_cache.list);
  fs_cache.cache = kmem_cache_create(sizeof(struct filesystem));

  // Create ROOTDEV fs
  struct filesystem* root_fs = kmem_cache_alloc(fs_cache.cache);
  init_root_fs(root_fs);
  // Create procfs
  struct filesystem* proc_fs = kmem_cache_alloc(fs_cache.cache);
  init_proc_fs(proc_fs);
}

static int
_readi(struct inode *ip, char *dst, uint off, uint n);
static int
_writei(struct inode *ip, char *dst, uint off, uint n);

int
type_to_mode(short type)
{
  switch(type){
    case T_DIR:
      return S_IFDIR;
    case T_FILE:
      return S_IFREG;
    case T_DEV:
      return S_IFCHR;
    case T_PIPE:
      return S_IFIFO;
  }
  return 0;
}

struct inode* iget(struct filesystem* dev, uint inum);

//PAGEBREAK!
// Allocate a new inode with the given type on device dev.
// A free inode has a type of zero.
struct inode*
_ialloc(struct filesystem* fs, short type)
{
  int inum;
  struct buf *bp;
  struct dinode *dip;
  struct superblock sb;
  uint dev = fs->dev;

  readsb(dev, &sb);

  for(inum = 1; inum < sb.ninodes; inum++){
    bp = bread(dev, IBLOCK(inum));
    dip = (struct dinode*)bp->data + inum%IPB;
    if(dip->type == 0){  // a free inode
      memset(dip, 0, sizeof(*dip));
      dip->type = type;
      log_write(bp);   // mark it allocated on the disk
      brelse(bp);
      return iget(fs, inum);
    }
    brelse(bp);
  }
  panic("ialloc: no inodes");
}

struct inode*
ialloc(struct filesystem* fs, short type)
{
  if (fs->ops.alloc == 0) {
    return _ialloc(fs, type);
  } else {
    return fs->ops.alloc(fs, type);
  }
}

// Copy a modified in-memory inode to disk.
void
_iupdate(struct inode *ip)
{
  struct buf *bp;
  struct dinode *dip;

  bp = bread(ip->fs->dev, IBLOCK(ip->inum));
  dip = (struct dinode*)bp->data + ip->inum%IPB;
  dip->major = ip->major;
  dip->minor = ip->minor;
  dip->nlink = ip->nlink;
  dip->size = ip->size;
  memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
  dip->uid = ip->uid;
  dip->gid = ip->gid;
  dip->mode = ip->mode;
  log_write(bp);
  brelse(bp);
}

void
iupdate(struct inode* ip)
{
  if (ip->ops.update == 0) {
    _iupdate(ip);
  } else {
    ip->ops.update(ip);
  }
}

// Find the inode with number inum on device dev
// and return the in-memory copy. Does not lock
// the inode and does not read it from disk.
struct inode*
_iget(struct filesystem* fs, uint inum)
{
  struct inode *ip, *empty;

  acquire(&icache.lock);

  // Is the inode already cached?
  empty = 0;
  list_for_each_entry(ip, &icache.list, list) {
    if (ip->ref > 0 && ip->fs == fs && ip->inum == inum){
      ip->ref++;
      release(&icache.lock);
      return ip;
    }
    if (empty == 0 && ip->ref == 0) // Remember empty slot.
      empty = ip;
  }
  if (empty == 0) {
    ip = (struct inode*)kmem_cache_alloc(icache.cache);
    if (ip == 0) {
      panic("iget: no inodes");
    }
    list_add_tail(&ip->list, &icache.list);
  } else {
    ip = empty;
  }
  struct list_head tmp = ip->list;
  memset(ip, 0, sizeof(*ip));
  ip->list = tmp;
  ip->fs = fs;
  ip->inum = inum;
  ip->ref = 1;
  ip->flags = 0;
  ip->mode = 0;
  ip->read_file = 0;
  ip->write_file = 0;
  memset(&ip->ops, 0, sizeof(ip->ops));
  release(&icache.lock);

  return ip;
}

struct inode*
iget(struct filesystem* fs, uint inum)
{
  if (fs->ops.get == 0) {
    return _iget(fs, inum);
  } else {
    return fs->ops.get(fs, inum);
  }
}

// Increment reference count for ip.
// Returns ip to enable ip = idup(ip1) idiom.
struct inode*
idup(struct inode *ip)
{
  acquire(&icache.lock);
  ip->ref++;
  release(&icache.lock);
  return ip;
}

// Lock the given inode.
// Reads the inode from disk if necessary.
void
ilock(struct inode *ip)
{
  struct buf *bp;
  struct dinode *dip;

  if(ip == 0 || ip->ref < 1)
    panic("ilock");

  acquire(&icache.lock);
  while(ip->flags & I_BUSY)
    sleep(ip, &icache.lock);
  ip->flags |= I_BUSY;
  release(&icache.lock);

  if(!(ip->flags & I_VALID)){
    bp = bread(ip->fs->dev, IBLOCK(ip->inum));
    dip = (struct dinode*)bp->data + ip->inum%IPB;
    ip->major = dip->major;
    ip->minor = dip->minor;
    ip->nlink = dip->nlink;
    ip->size = dip->size;
    memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
    ip->uid = dip->uid;
    ip->gid = dip->gid;
    ip->mode = dip->mode;
    brelse(bp);
    ip->read_file = ip->write_file = 0;
    ip->flags |= I_VALID;
  }
}

// Unlock the given inode.
void
iunlock(struct inode *ip)
{
  if(ip == 0 || !(ip->flags & I_BUSY) || ip->ref < 1)
    panic("iunlock");

  acquire(&icache.lock);
  ip->flags &= ~I_BUSY;
  wakeup(ip);
  release(&icache.lock);
}

// Drop a reference to an in-memory inode.
// If that was the last reference, the inode cache entry can
// be recycled.
// If that was the last reference and the inode has no links
// to it, free the inode (and its content) on disk.
void
_iput(struct inode *ip)
{
  acquire(&icache.lock);
  if(ip->ref == 1 && (ip->flags & I_VALID) && ip->nlink == 0){
    // inode has no links: truncate and free inode.
    if(ip->flags & I_BUSY)
      panic("iput busy");
    ip->flags |= I_BUSY;
    release(&icache.lock);
    itrunc(ip);
    ip->mode = 0;
    iupdate(ip);
    acquire(&icache.lock);
    ip->flags = 0;
    wakeup(ip);
    list_del(&ip->list);
    kmem_cache_free(ip);
  }
  ip->ref--;
  release(&icache.lock);
}

void
iput(struct inode* ip)
{
  if (ip->fs->ops.put == 0) {
    _iput(ip);
  } else {
    ip->fs->ops.put(ip->fs, ip);
  }
}

// Common idiom: unlock, then put.
void
iunlockput(struct inode *ip)
{
  iunlock(ip);
  iput(ip);
}

//PAGEBREAK!
// Inode content
//
// The content (data) associated with each inode is stored
// in blocks on the disk. The first NDIRECT block numbers
// are listed in ip->addrs[].  The next NINDIRECT blocks are 
// listed in block ip->addrs[NDIRECT].

// Return the disk block address of the nth block in inode ip.
// If there is no such block, bmap allocates one.
uint
bmap(struct inode *ip, uint bn)
{
  uint addr, *a;
  struct buf *bp;

  if(bn < NDIRECT){
    if((addr = ip->addrs[bn]) == 0)
      ip->addrs[bn] = addr = balloc(ip->fs->dev);
    return addr;
  }
  bn -= NDIRECT;

  if(bn < NINDIRECT){
    // Load indirect block, allocating if necessary.
    if((addr = ip->addrs[NDIRECT]) == 0)
      ip->addrs[NDIRECT] = addr = balloc(ip->fs->dev);
    bp = bread(ip->fs->dev, addr);
    a = (uint*)bp->data;
    if((addr = a[bn]) == 0){
      a[bn] = addr = balloc(ip->fs->dev);
      log_write(bp);
    }
    brelse(bp);
    return addr;
  }

  panic("bmap: out of range");
}

// Truncate inode (discard contents).
// Only called when the inode has no links
// to it (no directory entries referring to it)
// and has no in-memory reference to it (is
// not an open file or current directory).
static void
itrunc(struct inode *ip)
{
  int i, j;
  struct buf *bp;
  uint *a;

  for(i = 0; i < NDIRECT; i++){
    if(ip->addrs[i]){
      bfree(ip->fs->dev, ip->addrs[i]);
      ip->addrs[i] = 0;
    }
  }
  
  if(ip->addrs[NDIRECT]){
    bp = bread(ip->fs->dev, ip->addrs[NDIRECT]);
    a = (uint*)bp->data;
    for(j = 0; j < NINDIRECT; j++){
      if(a[j])
        bfree(ip->fs->dev, a[j]);
    }
    brelse(bp);
    bfree(ip->fs->dev, ip->addrs[NDIRECT]);
    ip->addrs[NDIRECT] = 0;
  }

  ip->size = 0;
  iupdate(ip);
}

// Copy stat information from inode.
void
stati(struct inode *ip, struct stat *st)
{
  st->dev = ip->fs->dev;
  st->ino = ip->inum;
  st->nlink = ip->nlink;
  st->size = ip->size;
  st->uid = ip->uid;
  st->gid = ip->gid;
  st->mode = ip->mode;
}

//PAGEBREAK!
// Read data from inode.
static int
_readi(struct inode *ip, char *dst, uint off, uint n)
{
  uint tot, m;
  struct buf *bp;

  if(S_ISCHR(ip->mode)){
    if(ip->major < 0 || ip->major >= NDEV || !devsw[ip->major].read)
      return -1;
    return devsw[ip->major].read(ip, dst, n);
  }

  if(off > ip->size || off + n < off)
    return -1;
  if(off + n > ip->size)
    n = ip->size - off;

  for(tot=0; tot<n; tot+=m, off+=m, dst+=m){
    bp = bread(ip->fs->dev, bmap(ip, off/BSIZE));
    m = min(n - tot, BSIZE - off%BSIZE);
    memmove(dst, bp->data + off%BSIZE, m);
    brelse(bp);
  }
  return n;
}

int
readi(struct inode *ip, char *dst, uint off, uint n)
{
  if (ip->ops.read == 0) {
    return _readi(ip, dst, off, n);
  } else {
    return ip->ops.read(ip, dst, off, n);
  }
}

// PAGEBREAK!
// Write data to inode.
static int
_writei(struct inode *ip, char *src, uint off, uint n)
{
  uint tot, m;
  struct buf *bp;

  if(S_ISCHR(ip->mode)){
    if(ip->major < 0 || ip->major >= NDEV || !devsw[ip->major].write)
      return -1;
    return devsw[ip->major].write(ip, src, n);
  }

  if(off > ip->size || off + n < off)
    return -1;
  if(off + n > MAXFILE*BSIZE)
    return -1;

  for(tot=0; tot<n; tot+=m, off+=m, src+=m){
    bp = bread(ip->fs->dev, bmap(ip, off/BSIZE));
    m = min(n - tot, BSIZE - off%BSIZE);
    memmove(bp->data + off%BSIZE, src, m);
    log_write(bp);
    brelse(bp);
  }

  if(n > 0 && off > ip->size){
    ip->size = off;
    iupdate(ip);
  }
  return n;
}

int
writei(struct inode *ip, char *dst, uint off, uint n)
{
  if (ip->ops.write == 0) {
    return _writei(ip, dst, off, n);
  } else {
    return ip->ops.write(ip, dst, off, n);
  }
}

//PAGEBREAK!
// Directories

int
namecmp(const char *s, const char *t)
{
  return strncmp(s, t, DIRSIZ);
}

// Look for a directory entry in a directory.
// If found, set *poff to byte offset of entry.
struct inode*
_dirlookup(struct inode *dp, char *name, uint *poff)
{
  uint off, inum;
  struct dirent de;

  if(!S_ISDIR(dp->mode))
    panic("dirlookup not DIR");

  if(proc->euid != 0 && !(get_current_permissions(dp) & 1)) {
    return ERR_PTR(-EPERM);
  }

  for(off = 0; off < dp->size; off += sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlink read");
    if(de.inum == 0)
      continue;
    if(namecmp(name, de.name) == 0){
      // entry matches path element
      if(poff)
        *poff = off;
      inum = de.inum;
      return iget(dp->fs, inum);
    }
  }

  return ERR_PTR(-ENOENT);
}

struct inode*
dirlookup(struct inode* dp, char* name, uint* poff)
{
  if (dp->ops.lookup == 0) {
    return _dirlookup(dp, name, poff);
  } else {
    return dp->ops.lookup(dp, name, poff);
  }
}

// Write a new directory entry (name, inum) into the directory dp.
int
dirlink(struct inode *dp, char *name, uint inum)
{
  int off;
  struct dirent de;
  struct inode *ip;

  // Check that name is not present.
  if(!IS_ERR(ip = dirlookup(dp, name, 0))){
    iput(ip);
    return -EEXIST;
  }

  // We can’t either write or execute, so we can’t create new entry in dir
  if(proc->euid != 0 && (get_current_permissions(dp) & 3) != 3) {
    iput(ip);
    return -EPERM;
  }

  // Look for an empty dirent.
  for(off = 0; off < dp->size; off += sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlink read");
    if(de.inum == 0)
      break;
  }

  strncpy(de.name, name, DIRSIZ);
  de.inum = inum;
  if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de)) {
    return -EIO;
  }
  
  return 0;
}

//PAGEBREAK!
// Paths

// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return 0.
//
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
//
static char*
skipelem(char *path, char *name)
{
  char *s;
  int len;

  while(*path == '/')
    path++;
  if(*path == 0)
    return 0;
  s = path;
  while(*path != '/' && *path != 0)
    path++;
  len = path - s;
  if(len >= DIRSIZ)
    memmove(name, s, DIRSIZ);
  else {
    memmove(name, s, len);
    name[len] = 0;
  }
  while(*path == '/')
    path++;
  return path;
}

// Look up and return the inode for a path name.
// If parent != 0, return the inode for the parent and copy the final
// path element into name, which must have room for DIRSIZ bytes.
static struct inode*
namex(char *path, int nameiparent, char *name)
{
  struct inode *ip, *next;

  if(*path == '/') {
    if (proc) {
      ip = idup(proc->fs->root);
    } else {
      // proc is zero when calling namei() in userinit.
      ip = iget(find_fs(ROOTDEV), ROOTINO);
    }
  }
  else
    ip = idup(proc->fs->cwd);

  while((path = skipelem(path, name)) != 0){
    ilock(ip);
    if(!S_ISDIR(ip->mode)){
      iunlockput(ip);
      return ERR_PTR(-ENOTDIR);
    }
    if(proc->euid != 0 && !(get_current_permissions(ip) & 1)) {
      // Exec flag is not set, we are not allowed to do anything here
      iunlockput(ip);
      return ERR_PTR(-EPERM);
    }
    if(nameiparent && *path == '\0'){
      // Stop one level early.
      iunlock(ip);
      return ip;
    }
    if(IS_ERR(next = dirlookup(ip, name, 0))){
      iunlockput(ip);
      return next;
    }
    if(ip == proc->fs->root && namecmp(name, "..") == 0){
      iunlock(ip);
    } else {
      iunlockput(ip);
      ip = next;
    }
  }
  if(nameiparent){
    iput(ip);
    return ERR_PTR(-ENOENT);
  }
  return ip;
}

struct inode*
namei(char *path)
{
  char name[DIRSIZ];
  return namex(path, 0, name);
}

struct inode*
nameiparent(char *path, char *name)
{
  return namex(path, 1, name);
}

// We suppose that ip is already locked.
int
get_current_permissions(struct inode* ip)
{
  int access_mode;
  if (proc->euid == ip->uid) {
    access_mode = ((ip->mode >> 6) & 7);
  } else if (proc->egid == ip->gid) {
    access_mode = ((ip->mode >> 3) & 7);
  } else {
    access_mode = ((ip->mode) & 7);
  }
  return access_mode;
}
