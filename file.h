#include "list.h"

struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE, FD_FIFO } type;
  int ref; // reference count
  char readable;
  char writable;
  struct pipe *pipe;
  struct inode *ip;
  uint off;
  struct spinlock lock;
};

struct filesystem;

struct fs_operations
{
  struct inode* (*alloc)(struct filesystem*, short);
  struct inode* (*get)(struct filesystem*, uint);
  void (*put)(struct filesystem*, struct inode*);
};

struct filesystem
{
  uint index;
  uint dev;
  struct fs_operations ops;
  struct list_head list;
};

struct inode_operations
{
  int (*read)(struct inode*, char*, uint, uint);
  int (*write)(struct inode*, char*, uint, uint);
  int (*permissions)(struct inode*);
  int (*lookup)(struct inode*, struct inode*, char*);
  int (*link)(struct inode*, struct inode*, char*);
  int (*unlink)(struct inode*, char*);
  void (*update)(struct inode*);
};

// in-memory copy of an inode
struct inode {
  struct filesystem* fs; // File system containing this inode
  uint inum;             // Inode number
  int ref;               // Reference count
  int flags;             // I_BUSY, I_VALID

  short major;
  short minor;
  short nlink;
  uint size;
  uint addrs[NDIRECT+1];
  uint uid;
  uint gid;
  uint mode;

  struct list_head list;
  void* additional_info; // Additional info for the inode

  struct inode_operations ops;

  // Two files for pipe, used only when type == T_PIPE
  struct file *read_file, *write_file;
};
#define I_BUSY 0x1
#define I_VALID 0x2

// table mapping major device number to
// device functions
struct devsw {
  int (*read)(struct inode*, char*, int);
  int (*write)(struct inode*, char*, int);
};

extern struct devsw devsw[];

#define CONSOLE 1
