#include "syscall.h"
#include "traps.h"
#include "errno.h"

// Call system function and save errno
#define SYSCALL(name) \
  .globl name; \
  name: \
    movl $SYS_ ## name, %eax; \
    int $T_SYSCALL; \
    cmpl $0, %eax; \
    jge ok_ ## name ; \
    cmpl $-128, %eax; \
    jl ok_ ## name ; \
    movl $0, errno; \
    subl %eax, errno; \
    movl $-1, %eax; \
  ok_ ## name : \
    ret

SYSCALL(fork)
SYSCALL(_exit)
SYSCALL(wait)
SYSCALL(pipe)
SYSCALL(read)
SYSCALL(write)
SYSCALL(close)
SYSCALL(kill)
SYSCALL(execve)
SYSCALL(open)
SYSCALL(mknod)
SYSCALL(mkfifo)
SYSCALL(unlink)
SYSCALL(fstat)
SYSCALL(link)
SYSCALL(mkdir)
SYSCALL(chdir)
SYSCALL(dup)
SYSCALL(getpid)
SYSCALL(sbrk)
SYSCALL(sleep)
SYSCALL(uptime)
SYSCALL(umask)
SYSCALL(setreuid)
SYSCALL(setregid)
SYSCALL(getuid)
SYSCALL(geteuid)
SYSCALL(getgid)
SYSCALL(getegid)
SYSCALL(chmod)
SYSCALL(chown)
SYSCALL(setgroups)
SYSCALL(getgroups)
SYSCALL(clone)
SYSCALL(exit_group)
SYSCALL(sched_yield)
SYSCALL(mount)
SYSCALL(chroot)
