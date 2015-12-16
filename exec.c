#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "defs.h"
#include "x86.h"
#include "elf.h"
#include "fs.h"
#include "file.h"
#include "stat.h"
#include "errno.h"
#include "err.h"

static int _exec(char* path, char **argv, char **envp, int current_depth);

int
exec(char *path, char **argv, char **envp)
{
  return _exec(path, argv, envp, 5);
}

static int
_exec(char *path, char **argv, char **envp, int current_depth)
{
  char *s, *last;
  int i, j, off, st, linelen;
  uint argc, sz, sp, ustack[4+MAXARG+1+MAXARG+1];
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pde_t *pgdir;
  char* args[MAXARG + 3];
  char* progpath;
  char tmp[2];
  if(--current_depth < 0)
    return -ELOOP;

  if(IS_ERR(ip = namei(path)))
    return PTR_ERR(ip);
  ilock(ip);
  if((!(get_current_permissions(ip) & 1)) && proc->euid != 0){
    iunlock(ip);
    return -EACCES;
  }
  pgdir = 0;

  // Check for shebang
  if(readi(ip, tmp, 0, 2) < sizeof(tmp)) {
    st = -EACCES;
    goto bad;
  }
  if(tmp[0] == '#' && tmp[1] == '!'){
    progpath = kalloc();
    i = readi(ip, progpath, 2, PGSIZE);
    iunlockput(ip);
    ip = 0;
    for(j = 0; j < i && progpath[j] == ' '; ++j)
      ;
    args[0] = progpath + j;
    for(; j < i && progpath[j] != ' ' &&
        progpath[j] != '\t' && progpath[j] != '\n'; ++j)
      ;
    if(j == PGSIZE){
      st = -E2BIG;
      goto exit;
    }
    argc = 0;
    for(linelen = j; linelen < i && progpath[linelen] != '\n' &&
        argc + 1 < sizeof(args); ++linelen){
      if(progpath[linelen] == ' ' || progpath[linelen] == '\t')
        progpath[linelen] = 0;
      else if(progpath[linelen - 1] == 0)
        args[++argc] = progpath + linelen;
    }
    if(argc >= MAXARG || linelen >= i){
      st = -E2BIG;
      goto exit;
    }
    progpath[linelen] = 0;
    args[++argc] = path;
    for(i = 1; argv[i] && argc < MAXARG; ++i)
      args[++argc] = argv[i];

    if(argc >= MAXARG){
      st = -E2BIG;
      goto exit;
    }
    args[++argc] = 0;
    st = _exec(args[0], args, envp, current_depth);
exit:
    kfree(progpath);
    return st;
  }
  // Check ELF header
  if(readi(ip, (char*)&elf, 0, sizeof(elf)) < sizeof(elf)) {
    st = -ENOEXEC;
    goto bad;
  }
  if(elf.magic != ELF_MAGIC) {
    st = -ENOEXEC;
    goto bad;
  }

  if((pgdir = setupkvm()) == 0) {
    st = -ENOMEM;
    goto bad;
  }

  // Load program into memory.
  sz = 0;
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph)) {
      st = -EIO;
      goto bad;
    }
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz) {
      st = -E2BIG;
      goto bad;
    }
    if((sz = allocuvm(pgdir, sz, ph.vaddr + ph.memsz)) == 0) {
      st = -ENOMEM;
      goto bad;
    }
    if(loaduvm(pgdir, (char*)ph.vaddr, ip, ph.off, ph.filesz) < 0) {
      st = -ENOMEM;
      goto bad;
    }
  }
  int new_euid = proc->euid;
  int new_egid = proc->egid;
  if((ip->mode & S_ISUID) == S_ISUID){
    new_euid = ip->uid;
  }
  if((ip->mode & S_ISGID) == S_ISGID){
    new_egid = ip->gid;
  }
  iunlockput(ip);
  ip = 0;

  // Allocate two pages at the next page boundary.
  // Make the first inaccessible.  Use the second as the user stack.
  sz = PGROUNDUP(sz);
  if((sz = allocuvm(pgdir, sz, sz + 2*PGSIZE)) == 0) {
    st = -ENOMEM;
    goto bad;
  }
  clearpteu(pgdir, (char*)(sz - 2*PGSIZE));
  sp = sz;

  // Push argument strings, prepare rest of stack in ustack.
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG) {
      st = -E2BIG;
      goto bad;
    }
    sp = (sp - (strlen(argv[argc]) + 1)) & ~3;
    if(copyout(pgdir, sp, argv[argc], strlen(argv[argc]) + 1) < 0) {
      st = -ENOMEM;
      goto bad;
    }
    ustack[1+argc] = sp;
  }
  ustack[1+argc] = 0;

  int envp_counter = 0;
  for(envp_counter = 0; envp[envp_counter]; envp_counter++) {
    if(envp_counter >= MAXARG) {
      st = -E2BIG;
      goto bad;
    }
    sp = (sp - (strlen(envp[envp_counter]) + 1)) & ~3;
    if(copyout(pgdir, sp, envp[envp_counter],
          strlen(envp[envp_counter]) + 1) < 0) {
      st = -ENOMEM;
      goto bad;
    }
    ustack[2+envp_counter+argc] = sp;
  }
  ustack[2+argc+envp_counter] = 0;

  ustack[0] = argc;

  sp -= (1+argc+1+envp_counter+1) * 4;
  if(copyout(pgdir, sp, ustack, (1+argc+1+envp_counter+1)*4) < 0) {
    st = -ENOMEM;
    goto bad;
  }

  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(proc->name, last, sizeof(proc->name));

  // Commit to the user image.
  proc->suid = proc->euid;
  proc->sgid = proc->egid;
  if (new_euid != proc->euid) {
    proc->euid = new_euid;
    proc->ngroups = 0;
  }
  if (new_egid != proc->egid) {
    proc->egid = new_egid;
    proc->ngroups = 0;
  }
  kill_other_threads_in_group();
  proc->group_leader = proc;
  proc->tgid = proc->pid;
  struct mm_struct* old_mm = proc->mm;
  proc->mm = kmem_cache_alloc(mm_cache);
  initlock(&proc->mm->lock, "proc->mm");
  proc->mm->users = 1;
  proc->mm->pgdir = pgdir;
  proc->mm->sz = sz;
  proc->tf->eip = elf.entry;  // main
  proc->tf->esp = sp;
  switchuvm(proc);
  free_mm(old_mm);
  return 0;

 bad:
  if(pgdir)
    freevm(pgdir);
  if(ip)
    iunlockput(ip);
  return st;
}
