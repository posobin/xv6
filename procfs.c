#include "types.h"
#include "defs.h"
#include "mmu.h"
#include "fs.h"
#include "file.h"
#include "proc.h"
#include "stat.h"
#include "err.h"
#include "errno.h"

static void
itoa(char* dst, int value)
{
  if (value < 0) {
    *dst = '-';
    value = -value;
    dst++;
  }
  char* start = dst;
  *dst++ = '0' + value % 10;
  value /= 10;
  while (value > 0) {
    *dst++ = '0' + value % 10;
    value /= 10;
  }
  *dst = '\0';
  dst--;
  while (dst > start)
  {
    char temp = *dst;
    *dst = *start;
    *start = temp;
    start++;
    dst--;
  }
}

uint
atoi(char* s, int len)
{
  uint current = 0;
  for (int i = 0; s[i] && i < len; ++i)
  {
    if (!('0' <= s[i] && s[i] <= '9')) {
      return 0;
    } else {
      current *= 10;
      current += s[i] - '0';
    }
  }
  return current;
}

static void
procfs_inode_update(struct inode* ip)
{ }

static int
procfs_proc_file_name_read(struct inode* ip, char* dst, uint off, uint n);
static int
procfs_proc_file_state_read(struct inode* ip, char* dst, uint off, uint n);
static int
procfs_proc_file_memory_read(struct inode* ip, char* dst, uint off, uint n);
static int
procfs_proc_file_pid_read(struct inode* ip, char* dst, uint off, uint n);
static int
procfs_proc_file_uid_read(struct inode* ip, char* dst, uint off, uint n);

struct {
  char* name;
  int (*read)(struct inode*, char*, uint, uint);
} procfs_proc_files_table[] = {
  { ".", 0 },
  { "..", 0 },
  { "parent", 0 },
  { "name", procfs_proc_file_name_read },
  { "state", procfs_proc_file_state_read },
  { "memory", procfs_proc_file_memory_read },
  { "pid", procfs_proc_file_pid_read },
  { "uid", procfs_proc_file_uid_read },
};

#define N_PROC_ENTRIES (NELEM(procfs_proc_files_table) - 2 + 1)

static int
procfs_proc_file_write(struct inode* ip, char* dst, uint off, uint n)
{
  return -1;
}

void
init_procfs_proc_file(struct inode* ip)
{
  ip->ops.update = procfs_inode_update;
  ip->ops.write = procfs_proc_file_write;
  ip->mode = (0444 | S_IFREG);
  ip->flags = I_VALID;
  ip->size = 0;
  ip->uid = 0;
  ip->gid = 0;
}

static void
read_str(char* src, uint length, char** dst, uint* off, uint n, uint* written)
{
  if (*off >= length) return;
  for (; *written < n && *off < length; ++*off, ++*written)
  {
    *((*dst)++) = src[*off];
  }
}

static int
procfs_proc_dir_read(struct inode* ip, char* dst, uint off, uint n)
{
  uint pid = ip->inum / N_PROC_ENTRIES;
  uint count = 0;
  for (int i = 0; i < NELEM(procfs_proc_files_table); ++i)
  {
    struct dirent entry;
    strncpy(entry.name, procfs_proc_files_table[i].name, 14);
    uint inum = pid * N_PROC_ENTRIES + i - 1;
    if (namecmp(entry.name, ".") == 0) {
      inum = ip->inum;
    } else if (namecmp(entry.name, "..") == 0) {
      inum = (uint)ip->additional_info;
    } else if (namecmp(entry.name, "parent") == 0) {
      struct proc* p = get_proc_by_pid(pid);
      if (p->parent == 0) {
        continue;
      }
      inum = p->parent->pid * N_PROC_ENTRIES;
    }
    struct inode* node = iget(find_fs(PROCDEV), inum);
    if (i >= 3) {
      init_procfs_proc_file(node);
      node->ops.read = procfs_proc_files_table[i].read;
    }
    entry.inum = node->inum;
    read_str((char*)&entry, sizeof(struct dirent), &dst, &off, n, &count);
    if (off >= sizeof(struct dirent)) {
      off -= sizeof(struct dirent);
    }
  }
  return count;
}

static int
procfs_proc_dir_write(struct inode* ip, char* dst, uint off, uint n)
{
  return -1;
}

static int
read_string(char* src, uint limit, char* dst, uint off, uint n)
{
  uint len = safestrlen(src, limit);
  uint last = off + n;
  int count = 0;
  for (uint i = off; i < len && i < last; ++i) {
    *dst++ = src[i];
    count++;
  }
  *dst = 0;
  if (last > len && off <= len) {
    *dst++ = '\n';
    *dst = 0;
    count++;
  }
  return count;
}

static int
procfs_proc_file_name_read(struct inode* ip, char* dst, uint off, uint n)
{
  struct proc* p = get_proc_by_pid(ip->inum / N_PROC_ENTRIES);
  if (p == 0) return 0;
  char* str = p->name;
  int len = safestrlen(str, NELEM(p->name));
  return read_string(str, len, dst, off, n);
}

static int
procfs_proc_file_state_read(struct inode* ip, char* dst, uint off, uint n)
{
  struct proc* p = get_proc_by_pid(ip->inum / N_PROC_ENTRIES);
  if (p == 0) return 0;
  char result[16];
  switch (p->state) {
    case UNUSED:
      strncpy(result, "unused", 16);
      break;
    case EMBRYO:
      strncpy(result, "embryo", 16);
      break;
    case SLEEPING:
      strncpy(result, "sleeping", 16);
      break;
    case RUNNABLE:
      strncpy(result, "runnable", 16);
      break;
    case RUNNING:
      strncpy(result, "running", 16);
      break;
    case ZOMBIE:
      strncpy(result, "zombie", 16);
      break;
  }
  uint len = safestrlen(result, 16);
  return read_string(result, len, dst, off, n);
}

static int
procfs_proc_file_memory_read(struct inode* ip, char* dst, uint off, uint n)
{
  struct proc* p = get_proc_by_pid(ip->inum / N_PROC_ENTRIES);
  if (p == 0) return 0;
  char result[16];
  itoa(result, p->mm->sz);
  int len = strlen(result);
  return read_string(result, len, dst, off, n);
}

static int
procfs_proc_file_pid_read(struct inode* ip, char* dst, uint off, uint n)
{
  struct proc* p = get_proc_by_pid(ip->inum / N_PROC_ENTRIES);
  if (p == 0) return 0;
  char result[16];
  itoa(result, p->pid);
  int len = strlen(result);
  return read_string(result, len, dst, off, n);
}

static int
procfs_proc_file_uid_read(struct inode* ip, char* dst, uint off, uint n)
{
  struct proc* p = get_proc_by_pid(ip->inum / N_PROC_ENTRIES);
  if (p == 0) return 0;
  char result[16];
  itoa(result, p->uid);
  int len = strlen(result);
  return read_string(result, len, dst, off, n);
}

static void
init_procfs_proc_dir(struct inode* ip);

struct inode*
procfs_proc_dir_lookup(struct inode* ip, char* name, uint* poff)
{
  struct inode* result = ERR_PTR(-ENOENT);
  if (namecmp(name, ".") == 0) {
    return ip;
  } else if (namecmp(name, "..") == 0) {
    return iget(ip->fs, (uint)ip->additional_info);
  } else if (namecmp(name, "parent") == 0) {
    struct proc* p = get_proc_by_pid(ip->inum / N_PROC_ENTRIES);
    if (p->parent == 0) return ERR_PTR(-ENOENT);
    struct inode* parent = iget(ip->fs, (uint)p->parent->pid * N_PROC_ENTRIES);
    init_procfs_proc_dir(parent);
    return parent;
  }
  for (int i = 2; i < NELEM(procfs_proc_files_table); ++i)
  {
    if (namecmp(name, procfs_proc_files_table[i].name) != 0) {
      continue;
    }
    result = iget(ip->fs, ip->inum + i - 1);
    init_procfs_proc_file(result);
    result->ops.read = procfs_proc_files_table[i].read;
    break;
  }
  return result;
}

static void
init_procfs_proc_dir(struct inode* ip)
{
  ip->ops.read = procfs_proc_dir_read;
  ip->ops.write = procfs_proc_dir_write;
  ip->ops.lookup = procfs_proc_dir_lookup;
  ip->ops.update = procfs_inode_update;
  ip->size = 0;
  ip->flags = I_VALID;
  ip->mode = (0555 | S_IFDIR);
  ip->uid = 0;
  ip->gid = 0;
  ip->additional_info = (void*)1;
}

static int
procfs_root_read(struct inode* ip, char* dst, uint off, uint n)
{
  uint written = 0;

  struct dirent entry;

  strncpy(entry.name, ".", 2);
  entry.inum = ip->inum;
  read_str((char*)&entry, sizeof(struct dirent), &dst, &off, n, &written);
  if (off >= sizeof(struct dirent)) {
    off -= sizeof(struct dirent);
  }

  strncpy(entry.name, "..", 3);
  entry.inum = (uint)ip->additional_info;
  read_str((char*)&entry, sizeof(struct dirent), &dst, &off, n, &written);
  if (off >= sizeof(struct dirent)) {
    off -= sizeof(struct dirent);
  }

  acquire(&ptable.lock);
  struct proc* p;
  list_for_each_entry(p, &ptable.list, list) {
    if (p->state == UNUSED) continue;

    itoa(entry.name, p->pid);
    struct inode* node = iget(find_fs(PROCDEV), p->pid * N_PROC_ENTRIES);
    init_procfs_proc_dir(node);
    entry.inum = ip->inum;
    read_str((char*)&entry, sizeof(struct dirent), &dst, &off, n, &written);
    if (off >= sizeof(struct dirent)) {
      off -= sizeof(struct dirent);
    }
  }
  release(&ptable.lock);

  strncpy(entry.name, "self", 5);
  entry.inum = proc->pid * N_PROC_ENTRIES;
  read_str((char*)&entry, sizeof(struct dirent), &dst, &off, n, &written);
  if (off >= sizeof(struct dirent)) {
    off -= sizeof(struct dirent);
  }

  return written;
}

static int
procfs_root_write(struct inode* ip, char* dst, uint off, uint n)
{
  return -1;
}

struct inode*
procfs_root_lookup(struct inode* ip, char* name, uint* poff)
{
  if (namecmp(name, ".") == 0) {
    return ip;
  } else if (namecmp(name, "..") == 0) {
    return _dirlookup(ip, name, poff);
  }
  uint pid = atoi(name, 10);
  if (pid == 0) {
    if (namecmp(name, "self") != 0) {
      return ERR_PTR(-ENOENT);
    }
    pid = proc->pid;
  }
  acquire(&ptable.lock);
  struct proc* p;
  list_for_each_entry(p, &ptable.list, list) {
    if (p->pid == pid) {
      struct filesystem* fs = find_fs(PROCDEV);
      struct inode* node = iget(fs, pid * N_PROC_ENTRIES);
      init_procfs_proc_dir(node);
      release(&ptable.lock);
      return node;
    }
  }
  release(&ptable.lock);
  return ERR_PTR(-ENOENT);
}

int
mount_proc_fs(struct inode* ip, struct inode* parent)
{
  ilock(ip);
  iget(ip->fs, ip->inum);
  ip->ops.read = procfs_root_read;
  ip->ops.write = procfs_root_write;
  ip->ops.lookup = procfs_root_lookup;
  ip->ops.update = procfs_inode_update;
  ip->additional_info = (void*)parent->inum;

  struct inode* temp_ip = iget(find_fs(PROCDEV), 1);
  temp_ip->mode = ip->mode;
  temp_ip->additional_info = (void*)parent->inum;
  temp_ip->ops = ip->ops;
  temp_ip->fs = find_fs(PROCDEV);
  temp_ip->inum = 1;
  temp_ip->flags = I_VALID;

  iunlock(ip);
  // We should not do iput here.
  // Otherwise, ip will be flushed out of the memory.
  return 0;
}
