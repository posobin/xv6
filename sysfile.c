//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "file.h"
#include "fcntl.h"
#include "pipe.h"
#include "errno.h"
#include "stat.h"
#include "err.h"

// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if(argint(n, &fd) < 0)
    return -1;
  if(fd < 0 || fd >= NOFILE || (f=proc->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
  int fd;

  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd] == 0){
      proc->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

int
sys_dup(void)
{
  struct file *f;
  int fd;
  
  if(argfd(0, 0, &f) < 0)
    return -EBADF;
  if((fd=fdalloc(f)) < 0)
    return -EMFILE;
  filedup(f);
  return fd;
}

int
sys_read(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -EINVAL;
  return fileread(f, p, n);
}

int
sys_write(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0)
    return -EBADF;
  if(argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -EINVAL;
  return filewrite(f, p, n);
}

int
sys_close(void)
{
  int fd;
  struct file *f;
  
  if(argfd(0, &fd, &f) < 0)
    return -EBADF;
  proc->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

int
sys_fstat(void)
{
  struct file *f;
  struct stat *st;
  
  if(argfd(0, 0, &f) < 0)
    return -EBADF;
  if(argptr(1, (void*)&st, sizeof(*st)) < 0)
    return -EINVAL;
  return filestat(f, st);
}

// Create the path new as a link to the same inode as old.
int
sys_link(void)
{
  char name[DIRSIZ], *new, *old;
  struct inode *dp, *ip;

  if(argstr(0, &old) < 0 || argstr(1, &new) < 0)
    return -EINVAL;
  if(IS_ERR(ip = namei(old)))
    return PTR_ERR(ip);

  begin_trans();

  ilock(ip);
  if(S_ISDIR(ip->mode)){
    iunlockput(ip);
    commit_trans();
    return -EPERM;
  }

  ip->nlink++;
  iupdate(ip);
  iunlock(ip);

  int status = 0;
  if(IS_ERR(dp = nameiparent(new, name))) {
    status = -PTR_ERR(dp);
    goto bad;
  }
  ilock(dp);
  if(dp->dev != ip->dev) {
    iunlockput(dp);
    status = EXDEV;
    goto bad;
  }
  if((status = dirlink(dp, name, ip->inum)) < 0) {
    status = -status;
    iunlockput(dp);
    goto bad;
  }
  iunlockput(dp);
  iput(ip);

  commit_trans();

  return 0;

bad:
  ilock(ip);
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);
  commit_trans();
  return -status;
}

// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct inode *dp)
{
  int off;
  struct dirent de;

  for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("isdirempty: readi");
    if(de.inum != 0)
      return 0;
  }
  return 1;
}

//PAGEBREAK!
int
sys_unlink(void)
{
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ], *path;
  uint off;

  if(argstr(0, &path) < 0)
    return -EINVAL;
  if(IS_ERR(dp = nameiparent(path, name)))
    return PTR_ERR(dp);

  begin_trans();

  ilock(dp);

  int status;
  // Cannot unlink "." or "..".
  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0) {
    status = ENOTDIR;
    goto bad;
  }

  if((get_current_permissions(dp) & 3) != 3) {
    status = EPERM;
    goto bad;
  }

  if(IS_ERR(ip = dirlookup(dp, name, &off))) {
    status = -PTR_ERR(ip);
    goto bad;
  }
  ilock(ip);

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");
  if(S_ISDIR(ip->mode) && !isdirempty(ip)){
    iunlockput(ip);
    status = ENOTEMPTY;
    goto bad;
  }
  if(S_ISFIFO(ip->mode) && ip->read_file != 0){
    struct pipe* p = ip->read_file->pipe;
    acquire(&p->lock);
    p->is_deleted = 1;
    wakeup(&p->nread);
    wakeup(&p->nwrite);
    release(&p->lock);
  }

  memset(&de, 0, sizeof(de));
  if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  if(S_ISDIR(ip->mode)){
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);

  commit_trans();

  return 0;

bad:
  iunlockput(dp);
  commit_trans();
  return -status;
}

static struct inode*
create(char *path, short type, short major, short minor, uint mode)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if(!IS_ERR(ip = namei(path))){
    ilock(ip);
    if((type == T_FILE && S_ISREG(ip->mode)) ||
        (type == T_FILE && S_ISFIFO(ip->mode)))
      return ip;
    iunlockput(ip);
    return ERR_PTR(-EEXIST);
  }

  if(IS_ERR(dp = nameiparent(path, name)))
    return dp;
  ilock(dp);
  int permissions = get_current_permissions(dp);
  if(proc->euid != 0 && !(permissions & 2)) {
    iunlock(dp);
    return ERR_PTR(-EPERM);
  }
  if((ip = ialloc(dp->dev, type)) == 0)
    panic("create: ialloc");

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  ip->uid = proc->euid;
  ip->gid = proc->egid;
  ip->mode = (mode & (~(proc->umask)));
  ip->mode |= type_to_mode(type);
  iupdate(ip);

  if(type == T_DIR){  // Create . and .. entries.
    dp->nlink++;  // for ".."
    iupdate(dp);
    // Dirty hack. Otherwise, if we got some bad mode
    // (i.e. 0000), we would not have been able to
    // create dots in the new directory (if we are not root).
    int real_mode = ip->mode;
    ip->mode |= 0333;
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0) {
      panic("create dots");
    }
    ip->mode = real_mode;
    iupdate(ip);
  }

  if(dirlink(dp, name, ip->inum) < 0)
    panic("create: dirlink");

  iunlockput(dp);

  return ip;
}

int
sys_open(void)
{
  char *path;
  int fd, omode, mode;
  struct file *f;
  struct inode *ip;

  if(argstr(0, &path) < 0 || argint(1, &omode) < 0)
    return -EINVAL;
  ip = 0;
  if(omode & O_CREATE && IS_ERR(ip = namei(path))){
    if(argint(2, &mode) < 0)
      return -EINVAL;
    begin_trans();
    ip = create(path, T_FILE, 0, 0, mode);
    commit_trans();
    if(IS_ERR(ip))
      return PTR_ERR(ip);
  } else {
    if((ip && IS_ERR(ip)) ||
        (!ip && IS_ERR(ip = namei(path))))
      return PTR_ERR(ip);
    ilock(ip);
    if(!S_ISFIFO(ip->mode) && (omode & O_NONBLOCK)){
      omode -= O_NONBLOCK;
    }
    if(S_ISDIR(ip->mode) && omode != O_RDONLY){
      iunlockput(ip);
      return -EISDIR;
    }
  }
  int access_mode = get_current_permissions(ip);
  if (proc->euid != 0) {
    if ((omode & O_RDWR) || (omode & O_WRONLY)) {
      if (!(access_mode & 2)) {
        iunlockput(ip);
        return -EACCES;
      }
    }
    if ((omode & O_RDWR) || !(omode & O_WRONLY)) {
      if (!(access_mode & 4)) {
        iunlockput(ip);
        return -EACCES;
      }
    }
  }

  int status;
  if(S_ISFIFO(ip->mode)){
    if(omode & O_RDWR){ // POSIX leaves this case undefined
      iunlockput(ip);
      return -EINVAL;
    }
    if(ip->read_file == 0){ // Create pipe if it has not been already created
      if((status = pipealloc(&ip->read_file, &ip->write_file)) < 0){
        iunlockput(ip);
        return status;
      }
      ip->read_file->type = FD_FIFO;
      ip->write_file->type = FD_FIFO;
      ip->read_file->ip = ip;
      ip->write_file->ip = ip;
      ip->read_file->pipe->writeopen = 0;
      ip->read_file->pipe->readopen = 0;
    }
    if ((f = filealloc()) == 0) {
      iunlockput(ip);
      return -EMFILE;
    }
    if (!(omode & O_WRONLY)) *f = *ip->read_file;
    else *f = *ip->write_file;
    f->ref = 1;
    if((fd = fdalloc(f)) < 0) {
      iunlockput(ip);
      return -ENFILE;
    }
    struct pipe* p = ip->write_file->pipe;
    acquire(&p->lock);
    if(omode & O_WRONLY){
      p->writeopen++;
      ip->write_file->ref++;
    } else {
      p->readopen++;
      ip->read_file->ref++;
    }
    iunlock(ip);
    if(omode & O_NONBLOCK){
      if((omode & O_WRONLY) && p->readopen == 0){
        release(&p->lock);
        fileclose(f);
        proc->ofile[fd] = 0;
        return -ENXIO;
      }
      if(omode & O_WRONLY) wakeup(&p->nread);
      else wakeup(&p->nwrite);
      release(&p->lock);
      return fd;
    }
    // Wait ’till another end of pipe is open as POSIX requires to
    // when O_NONBLOCK is clear.
    int *our_end_count = &p->readopen, *other_end_count = &p->writeopen;
    uint *our_wakeup = &p->nread, *other_wakeup = &p->nwrite;
    if(omode & O_WRONLY){
      our_end_count = &p->writeopen;
      other_end_count = &p->readopen;
      our_wakeup = &p->nwrite;
      other_wakeup = &p->nread;
    }
    while (*other_end_count == 0){
      if(proc->killed || p->is_deleted){
        if(*our_end_count > 0) (*our_end_count)--;
        wakeup(other_wakeup);
        release(&p->lock);
        fileclose(f);
        proc->ofile[fd] = 0;
        return -ENOENT;
      }
      wakeup(other_wakeup);
      sleep(our_wakeup, &p->lock);
    }
    wakeup(other_wakeup);
    release(&p->lock);
    return fd;
  }

  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f) {
      status = EMFILE;
      fileclose(f);
    } else {
      status = ENFILE;
    }
    iunlockput(ip);
    return -status;
  }
  if (omode & O_APPEND) {
    f->off = ip->size;
  }
  iunlock(ip);

  f->type = FD_INODE;
  f->ip = ip;
  if (!(omode & O_APPEND)) f->off = 0;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
  return fd;
}

int
sys_mkdir(void)
{
  char *path;
  struct inode *ip;
  int mode;
  if (argstr(0, &path) < 0 || argint(1, &mode) < 0){
    return -EINVAL;
  }

  begin_trans();
  if(IS_ERR(ip = create(path, T_DIR, 0, 0, mode))){
    commit_trans();
    return PTR_ERR(ip);
  }
  iunlockput(ip);
  commit_trans();
  return 0;
}

int
sys_mknod(void)
{
  struct inode *ip;
  char *path;
  int len;
  int major, minor;
  int mode;
  
  begin_trans();
  if((len=argstr(0, &path)) < 0 || argint(1, &major) < 0 ||
     argint(2, &minor) < 0 || argint(3, &mode) < 0) {
    commit_trans();
    return -EINVAL;
  }
  if(IS_ERR(ip = create(path, T_DEV, major, minor, mode))){
    commit_trans();
    return PTR_ERR(ip);
  }
  iunlockput(ip);
  commit_trans();
  return 0;
}

int
sys_mkfifo(void)
{
  struct inode *ip;
  char* path;
  int mode;
  int len;
  begin_trans();
  if((len = argstr(0, &path)) < 0 || argint(1, &mode) < 0) {
    commit_trans();
    return -EINVAL;
  }
  if(IS_ERR(ip = create(path, T_PIPE, 0, 0, mode))){
    commit_trans();
    return PTR_ERR(ip);
  }
  ip->read_file = ip->write_file = 0;
  iunlockput(ip);
  commit_trans();
  return 0;
}

int
sys_chdir(void)
{
  char *path;
  struct inode *ip;

  if(argstr(0, &path) < 0) {
    return -EINVAL;
  }
  if(IS_ERR(ip = namei(path))) {
    return PTR_ERR(ip);
  }
  ilock(ip);
  if(!S_ISDIR(ip->mode)){
    iunlockput(ip);
    return -ENOTDIR;
  }
  iunlock(ip);
  iput(proc->cwd);
  proc->cwd = ip;
  return 0;
}

int
sys_execve(void)
{
  char *path, *argv[MAXARG], *envp[MAXARG];
  int i;
  uint uargv, uarg, uenvp;

  if(argstr(0, &path) < 0 || argint(1, (int*)&uargv) < 0 ||
      argint(2, (int*)&uenvp) < 0){
    return -EINVAL;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv))
      return -E2BIG;
    if(fetchint(uargv+4*i, (int*)&uarg) < 0)
      return -EINVAL;
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    if(fetchstr(uarg, &argv[i]) < 0)
      return -EINVAL;
  }
  uint temp;
  for(i=0;; i++){
    if(i >= NELEM(envp))
      return -E2BIG;
    if(fetchint(uenvp+4*i, (int*)&temp) < 0)
      return -EINVAL;
    if(temp == 0){
      envp[i] = 0;
      break;
    }
    if(fetchstr(temp, &envp[i]) < 0)
      return -EINVAL;
  }
  return exec(path, argv, envp);
}

int
sys_pipe(void)
{
  int *fd;
  struct file *rf, *wf;
  int fd0, fd1;

  int status;
  if(argptr(0, (void*)&fd, 2*sizeof(fd[0])) < 0)
    return -EINVAL;
  if((status = pipealloc(&rf, &wf)) < 0)
    return status;
  fd0 = -1;
  if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
    if(fd0 >= 0)
      proc->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -EMFILE;
  }
  fd[0] = fd0;
  fd[1] = fd1;
  return 0;
}

int
sys_umask()
{
  int new_value, old_value;
  if(argint(0, &new_value) < 0)
    return -EINVAL;
  old_value = ((proc->umask) & 0777);
  proc->umask = new_value;
  return old_value;
}

int
sys_chmod(void)
{
  char* path;
  mode_t mode;
  if (argstr(0, &path) < 0 || argint(1, (int*)&mode) < 0) {
    return -EINVAL;
  }
  struct inode* ip = namei(path);
  if (IS_ERR(ip)) {
    return PTR_ERR(ip);
  }

  begin_trans();
  ilock(ip);
  if (proc->euid != 0 && ip->uid != proc->euid) {
    iunlockput(ip);
    commit_trans();
    return -EPERM;
  }
  ip->mode &= ~0xFFF;
  ip->mode |= (mode & 0xFFF);
  iupdate(ip);
  iunlockput(ip);
  commit_trans();
  
  return 0;
}

static int
is_group_supplementary(gid_t group)
{
  for (int i = 0; i < proc->ngroups; ++i) {
    if (group == proc->groups[i]) {
      return 1;
    }
  }
  return 0;
}

int
sys_chown(void)
{
  char* path;
  uid_t owner;
  gid_t group;
  if (argstr(0, &path) < 0 || argint(1, (int*)&owner) < 0 ||
      argint(2, (int*)&group) < 0) {
    return -EINVAL;
  }
  struct inode* ip = namei(path);
  if (IS_ERR(ip)) {
    return PTR_ERR(ip);
  }
  begin_trans();
  ilock(ip);
  if (proc->euid != 0) {
    if ((ip->uid != proc->euid) || (owner != (uid_t)-1 && owner != ip->uid) ||
        (group != proc->egid && !is_group_supplementary(group) &&
         group != (uid_t)-1)) {
      iunlockput(ip);
      commit_trans();
      return -EPERM;
    }
  }
  if (owner != (uid_t)-1) ip->uid = owner;
  if (group != (uid_t)-1) ip->gid = group;
  if ((S_ISREG(ip->mode)) && proc->euid != 0 && (ip->mode & S_IXUGO)) {
    // Clear set-uid and set-gid bits, as POSIX requires us to.
    ip->mode -= (ip->mode & (S_ISUID | S_ISGID));
  }
  iupdate(ip);
  iunlockput(ip);
  commit_trans();
  return 0;
}
