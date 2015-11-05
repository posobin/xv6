#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "defs.h"
#include "x86.h"
#include "elf.h"

static int _exec(char* path, char **argv, int current_depth);

int
exec(char *path, char **argv)
{
  return _exec(path, argv, 5);
}

static int
_exec(char *path, char **argv, int current_depth)
{
  char *s, *last;
  int i, j, off, st, linelen;
  uint argc, sz, sp, ustack[3+MAXARG+1];
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pde_t *pgdir, *oldpgdir;
  char* args[MAXARG + 3];
  char* progpath;
  char tmp[2];
  if(--current_depth < 0)
    return -1;

  if((ip = namei(path)) == 0)
    return -1;
  ilock(ip);
  pgdir = 0;

  // Check for shebang
  if(readi(ip, tmp, 0, 2) < sizeof(tmp))
    goto bad;
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
      st = -1;
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
      st = -1;
      goto exit;
    }
    progpath[linelen] = 0;
    args[++argc] = path;
    for(i = 1; argv[i] && argc < MAXARG; ++i)
      args[++argc] = argv[i];

    if(argc >= MAXARG){
      st = -1;
      goto exit;
    }
    args[++argc] = 0;
    st = _exec(args[0], args, current_depth);
exit:
    kfree(progpath);
    return st;
  }
  // Check ELF header
  if(readi(ip, (char*)&elf, 0, sizeof(elf)) < sizeof(elf))
    goto bad;
  if(elf.magic != ELF_MAGIC)
    goto bad;

  if((pgdir = setupkvm()) == 0)
    goto bad;

  // Load program into memory.
  sz = 0;
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if((sz = allocuvm(pgdir, sz, ph.vaddr + ph.memsz)) == 0)
      goto bad;
    if(loaduvm(pgdir, (char*)ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }
  iunlockput(ip);
  ip = 0;

  // Allocate two pages at the next page boundary.
  // Make the first inaccessible.  Use the second as the user stack.
  sz = PGROUNDUP(sz);
  if((sz = allocuvm(pgdir, sz, sz + 2*PGSIZE)) == 0)
    goto bad;
  clearpteu(pgdir, (char*)(sz - 2*PGSIZE));
  sp = sz;

  // Push argument strings, prepare rest of stack in ustack.
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    sp = (sp - (strlen(argv[argc]) + 1)) & ~3;
    if(copyout(pgdir, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[3+argc] = sp;
  }
  ustack[3+argc] = 0;

  ustack[0] = 0xffffffff;  // fake return PC
  ustack[1] = argc;
  ustack[2] = sp - (argc+1)*4;  // argv pointer

  sp -= (3+argc+1) * 4;
  if(copyout(pgdir, sp, ustack, (3+argc+1)*4) < 0)
    goto bad;

  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(proc->name, last, sizeof(proc->name));

  // Commit to the user image.
  oldpgdir = proc->pgdir;
  proc->pgdir = pgdir;
  proc->sz = sz;
  proc->tf->eip = elf.entry;  // main
  proc->tf->esp = sp;
  switchuvm(proc);
  freevm(oldpgdir);
  return 0;

 bad:
  if(pgdir)
    freevm(pgdir);
  if(ip)
    iunlockput(ip);
  return -1;
}
