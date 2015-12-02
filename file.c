//
// File descriptors
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "file.h"
#include "pipe.h"
#include "stat.h"
#include "errno.h"

struct devsw devsw[NDEV];
struct cache_info* file_cache;

void
fileinit(void)
{
  file_cache = kmem_cache_create(sizeof(struct file));
  if (file_cache == 0) {
    panic("Could not allocate file cache");
  }
}

// Allocate a file structure.
struct file*
filealloc(void)
{
  struct file *f;

  f = kmem_cache_alloc(file_cache);
  memset(f, 0, sizeof(struct file));
  f->ref = 1;
  initlock(&f->lock, "file");
  return f;
}

// Increment ref count for file f.
// Also increments readopen or writeopen, if file is an
// end of a pipe.
struct file*
filedup(struct file *f)
{
  acquire(&f->lock);
  if(f->ref < 1)
    panic("filedup");
  f->ref++;
  if (f->type == FD_FIFO) {
    acquire(&f->pipe->lock);
    f->pipe->writeopen += f->writable;
    f->pipe->readopen += f->readable;
    release(&f->pipe->lock);
  }
  release(&f->lock);
  return f;
}

// Close file f.  (Decrement ref count, close when reaches 0.)
void
fileclose(struct file *f)
{
  struct file ff;
  int readopen, writeopen;

  acquire(&f->lock);
  if(f->ref < 1)
    panic("fileclose");
  if(f->type == FD_FIFO){
    struct pipe* p = f->pipe;
    // If we are the last process using this end of the pipe,
    // wake up other processes on the other end, so that they
    // could act accordingly.
    acquire(&p->lock);
    if(f->writable && --p->writeopen <= 0){
      p->writeopen = 0;
      wakeup(&p->nread);
    }
    if(f->readable && --p->readopen <= 0){
      p->readopen = 0;
      wakeup(&p->nwrite);
    }
    readopen = p->readopen;
    writeopen = p->writeopen;
    release(&p->lock);
  }
  if(--f->ref > 0){
    release(&f->lock);
    return;
  }
  ff = *f;
  kmem_cache_free(f);
  release(&f->lock);
  
  if(ff.type == FD_FIFO && !readopen && !writeopen){
    // Close our end of the pipe,
    // and set read_file and write_file of our inode to 0,
    // so that on the next open of the FIFO we will create the pipe again.
    pipeclose(ff.pipe, ff.writable);
    begin_trans();
    ilock(ff.ip);
    ff.ip->read_file->ref = 0;
    ff.ip->write_file->ref = 0;
    ff.ip->read_file = 0;
    ff.ip->write_file = 0;
    iunlock(ff.ip);
    commit_trans();
  } else if(ff.type == FD_PIPE){
    pipeclose(ff.pipe, ff.writable);
  }
  if(ff.type == FD_INODE || ff.type == FD_FIFO){
    begin_trans();
    iput(ff.ip);
    commit_trans();
  }
}

// Get metadata about file f.
int
filestat(struct file *f, struct stat *st)
{
  ilock(f->ip);
  stati(f->ip, st);
  iunlock(f->ip);
  return 0;
}

// Read from file f.
int
fileread(struct file *f, char *addr, int n)
{
  int r;

  if(f->readable == 0)
    return -EBADF;
  if(f->type == FD_PIPE || f->type == FD_FIFO)
    return piperead(f->pipe, addr, n);
  if(f->type == FD_INODE){
    ilock(f->ip);
    if((r = readi(f->ip, addr, f->off, n)) > 0)
      f->off += r;
    iunlock(f->ip);
    return r;
  }
  panic("fileread");
}

//PAGEBREAK!
// Write to file f.
int
filewrite(struct file *f, char *addr, int n)
{
  int r;

  if(f->writable == 0)
    return -EBADF;
  if(f->type == FD_PIPE || f->type == FD_FIFO)
    return pipewrite(f->pipe, addr, n);
  if(f->type == FD_INODE){
    // write a few blocks at a time to avoid exceeding
    // the maximum log transaction size, including
    // i-node, indirect block, allocation blocks,
    // and 2 blocks of slop for non-aligned writes.
    // this really belongs lower down, since writei()
    // might be writing a device like the console.
    int max = ((LOGSIZE-1-1-2) / 2) * 512;
    int i = 0;
    while(i < n){
      int n1 = n - i;
      if(n1 > max)
        n1 = max;

      begin_trans();
      ilock(f->ip);
      if ((r = writei(f->ip, addr + i, f->off, n1)) > 0)
        f->off += r;
      iunlock(f->ip);
      commit_trans();

      if(r < 0)
        break;
      if(r != n1)
        panic("short filewrite");
      i += r;
    }
    return i == n ? n : -EIO;
  }
  panic("filewrite");
}
