#include "types.h"
#include "x86.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "errno.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_clone(void)
{
  char* stack;
  if (argptr(0, &stack, 0) < 0)
    return -EINVAL;
  return clone(stack);
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -EINVAL;
  return kill(pid);
}

int
sys_getpid(void)
{
  return proc->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -EINVAL;
  addr = proc->mm->sz;
  if(growproc(n) < 0)
    return -ENOMEM;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;
  
  if(argint(0, &n) < 0)
    return -EINVAL;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(proc->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;
  
  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int
sys_setreuid(void)
{
  int ruid, euid;
  if(argint(0, &ruid) < 0 || argint(1, &euid) < 0)
    return -EINVAL;
  int update_suid = 0;
  int new_uid = proc->uid, new_euid = proc->euid;
  if(ruid != -1)
  {
    update_suid = 1;
    if(ruid != proc->euid &&
        ruid != proc->uid &&
        proc->euid != 0)
      return -EPERM;
    new_uid = ruid;
  }
  if(euid != -1)
  {
    if(euid != proc->euid &&
        euid != proc->uid &&
        euid != proc->suid &&
        proc->euid != 0)
      return -EPERM;
    if(proc->euid != euid)
      update_suid = 1;
    new_euid = euid;
  }
  proc->uid = new_uid;
  proc->euid = new_euid;
  if(update_suid)
    proc->suid = proc->euid;
  return 0;
}

int
sys_setregid(void)
{
  int rgid, egid;
  if(argint(0, &rgid) < 0 || argint(1, &egid) < 0)
    return -EINVAL;
  int update_sgid = 0;
  int new_gid = proc->gid, new_egid = proc->egid;
  if(rgid != -1)
  {
    update_sgid = 1;
    if(rgid != proc->egid &&
        rgid != proc->gid &&
        proc->egid != 0)
      return -EPERM;
    new_gid = rgid;
  }
  if(egid != -1)
  {
    if(egid != proc->egid &&
        egid != proc->gid &&
        egid != proc->sgid &&
        proc->egid != 0)
      return -EPERM;
    if(proc->egid != egid)
      update_sgid = 1;
    new_egid = egid;
  }
  proc->gid = new_gid;
  proc->egid = new_egid;
  if(update_sgid)
    proc->sgid = proc->egid;
  return 0;
}

int
sys_getuid(void)
{
  return proc->uid;
}

int
sys_geteuid(void)
{
  return proc->euid;
}

int
sys_getgid(void)
{
  return proc->gid;
}

int
sys_getegid(void)
{
  return proc->egid;
}

int
sys_setgroups(void)
{
  uint count;
  gid_t* groups;
  if (argint(0, (int*)&count) < 0 ||
      argptr(1, (char**)&groups, count * 4) < 0) {
    return -EINVAL;
  }
  if (count >= NGROUPS_MAX) {
    return -EINVAL;
  }
  if (proc->euid != 0) {
    return -EPERM;
  }
  for (int i = 0; i < count; ++i) {
    proc->groups[i] = groups[i];
  }
  proc->ngroups = count;
  return 0;
}

int
sys_getgroups(void)
{
  int size;
  gid_t* list;
  if (argint(0, &size) < 0 ||
      (size > 0 && argptr(1, (char**)&list, size * 4) < 0)) {
    return -EINVAL;
  }
  if (size == 0) return proc->ngroups;
  if (size < proc->ngroups) return -EINVAL;
  for (int i = 0; i < proc->ngroups; ++i) {
    list[i] = proc->groups[i];
  }
  return proc->ngroups;
}
