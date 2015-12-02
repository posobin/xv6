#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "errno.h"
#include "list.h"

struct proc_list {
  struct list_head list;
  struct proc proc;
};

struct {
  struct spinlock lock;
  struct list_head list;
} ptable;

static struct proc *initproc;

struct cache_info* proc_list_cache;
struct cache_info* mm_cache;
struct cache_info* files_struct_cache;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
  proc_list_cache = kmem_cache_create(sizeof(struct proc_list));
  if (proc_list_cache == 0) {
    panic("Could not allocate proc_list cache");
  }
  mm_cache = kmem_cache_create(sizeof(struct mm_struct));
  if (mm_cache == 0) {
    panic("Could not allocate mm_struct cache");
  }
  files_struct_cache = kmem_cache_create(sizeof(struct files_struct));
  if (files_struct_cache == 0) {
    panic("Could not allocate files_struct cache");
  }
  INIT_LIST_HEAD(&ptable.list);
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  struct proc_list* new_entry = kmem_cache_alloc(proc_list_cache);
  if (new_entry == 0) {
    return 0;
  }
  acquire(&ptable.lock);
  list_add(&new_entry->list, &ptable.list);
  p = &new_entry->proc;

  memset(p, 0, sizeof(*p));
  p->state = EMBRYO;
  p->pid = nextpid++;
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    list_del(&new_entry->list);
    kmem_cache_free(new_entry);
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;
  
  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;
  
  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

// Decrease number of users of the memory map,
// free memory when user count reaches 0.
void
free_mm(struct mm_struct* mm)
{
  if (mm->users == 1) {
    freevm(mm->pgdir);
    mm->pgdir = 0;
    mm->sz = 0;
    kmem_cache_free(mm);
  }
  else {
    mm->users--;
  }
}

static struct mm_struct*
setup_mm(void)
{
  struct mm_struct* mm = kmem_cache_alloc(mm_cache);
  if (!mm) return 0;
  mm->users = 1;
  mm->pgdir = setupkvm();
  mm->sz = PGSIZE;
  if (!mm->pgdir) {
    free_mm(mm);
    return 0;
  }
  return mm;
}

static int
copy_mm(unsigned int clone_flags, struct proc* p)
{
  struct mm_struct *mm, *old_mm;
  old_mm = p->mm;
  if (!old_mm) return 0;
  if (clone_flags & CLONE_VM) {
    p->mm->users++;
    return 0;
  }
  mm = kmem_cache_alloc(mm_cache);
  if (!mm) return -ENOMEM;
  mm->users = 1;
  mm->pgdir = copyuvm(old_mm->pgdir, old_mm->sz);
  if (mm->pgdir == 0) {
    free_mm(mm);
    return -ENOMEM;
  }
  mm->sz = old_mm->sz;
  p->mm = mm;
  return 0;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];
  
  p = allocproc();
  initproc = p;
  if((p->mm = setup_mm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->mm->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->mm->sz = PGSIZE;
  p->files = kmem_cache_alloc(files_struct_cache);
  p->files->users = 1;
  initlock(&p->files->lock, "proc->files");
  p->files->fd = (struct file**)kalloc();
  memset(p->files->fd, 0, PGSIZE);
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");
  p->uid = 0;
  p->euid = 0;
  p->gid = 0;
  p->egid = 0;
  p->umask = 0;

  p->state = RUNNABLE;
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  
  sz = proc->mm->sz;
  if(n > 0){
    if((sz = allocuvm(proc->mm->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(proc->mm->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->mm->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;

  // Allocate process.
  if((np = allocproc()) == 0)
    return -ENOMEM;

  // Copy process state from p.
  int retval;
  np->mm = proc->mm;
  if((retval = copy_mm(0, np)) < 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return retval;
  }
  np->parent = proc;
  *np->tf = *proc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  np->files = kmem_cache_alloc(files_struct_cache);
  initlock(&np->files->lock, "proc->files");
  np->files->users = 1;
  np->files->fd = (struct file**)kalloc();
  memset(np->files->fd, 0, PGSIZE);
  for(i = 0; i < NOFILE; i++)
    if(proc->files->fd[i])
      np->files->fd[i] = filedup(proc->files->fd[i]);
  np->cwd = idup(proc->cwd);

  np->uid = proc->uid;
  np->euid = proc->euid;
  np->suid = proc->suid;
  np->gid = proc->gid;
  np->egid = proc->egid;
  np->sgid = proc->sgid;
  np->umask = proc->umask;

  np->ngroups = proc->ngroups;
  for (int i = 0; i < proc->ngroups; ++i) {
    np->groups[i] = proc->groups[i];
  }

  pid = np->pid;
  np->state = RUNNABLE;
  safestrcpy(np->name, proc->name, sizeof(proc->name));
  return pid;
}

int
clone(void* child_stack)
{
  int pid;
  struct proc *np;

  // Allocate process.
  if((np = allocproc()) == 0)
    return -ENOMEM;

  int retval;
  np->mm = proc->mm;
  if ((retval = copy_mm(CLONE_VM, np)) < 0) {
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return retval;
  }
  np->parent = proc;
  *np->tf = *proc->tf;

  // Clear %eax so that clone returns 0 in the child.
  np->tf->eax = 0;

  np->files = proc->files;
  np->files->users++;
  np->cwd = idup(proc->cwd);

  np->uid = proc->uid;
  np->euid = proc->euid;
  np->suid = proc->suid;
  np->gid = proc->gid;
  np->egid = proc->egid;
  np->sgid = proc->sgid;
  np->umask = proc->umask;
  np->tf->esp = (uint)child_stack;

  np->ngroups = proc->ngroups;
  for (int i = 0; i < proc->ngroups; ++i) {
    np->groups[i] = proc->groups[i];
  }

  pid = np->pid;
  np->state = RUNNABLE;
  safestrcpy(np->name, proc->name, sizeof(proc->name));
  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  acquire(&proc->files->lock);
  if (--proc->files->users == 0) {
    for(fd = 0; fd < NOFILE; fd++){
      if(proc->files->fd[fd]){
        fileclose(proc->files->fd[fd]);
        proc->files->fd[fd] = 0;
      }
    }
    kfree((char*)proc->files->fd);
    release(&proc->files->lock);
    kmem_cache_free(proc->files);
  } else {
    release(&proc->files->lock);
  }

  iput(proc->cwd);
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  struct list_head *pos, *next;
  list_for_each_safe(pos, next, &ptable.list) {
    p = &list_entry(pos, struct proc_list, list)->proc;
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    struct list_head *pos, *next;
    list_for_each_safe(pos, next, &ptable.list) {
      p = &list_entry(pos, struct proc_list, list)->proc;
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        free_mm(p->mm);
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;

  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    struct list_head *pos;
    // We can’t use list_for_each_safe here, I don’t quite understand
    // why, but I have spent around ten hours debugging this,
    // so you’d better trust me.
    list_for_each(pos, &ptable.list) {
      p = &list_entry(pos, struct proc_list, list)->proc;
      if(p->state == UNUSED) {
        struct list_head* prev = pos;
        pos = pos->prev;
        list_del(prev);
        kmem_cache_free(list_entry(prev, struct proc_list, list));
        continue;
      }
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      proc = p;
      switchuvm(p);
      p->state = RUNNING;
      swtch(&cpu->scheduler, proc->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
    }
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  proc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot 
    // be run from main().
    first = 0;
    initlog();
  }
  
  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  if(proc == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  proc->chan = chan;
  proc->state = SLEEPING;
  sched();

  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  /*for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)*/
  struct list_head *pos, *next;
  list_for_each_safe(pos, next, &ptable.list) {
    p = &list_entry(pos, struct proc_list, list)->proc;
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
  }
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  /*for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){*/
  struct list_head *pos, *next;
  list_for_each_safe(pos, next, &ptable.list) {
    p = &list_entry(pos, struct proc_list, list)->proc;
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];
  
  /*for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){*/
  struct list_head *pos, *next;
  list_for_each_safe(pos, next, &ptable.list) {
    p = &list_entry(pos, struct proc_list, list)->proc;
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s %d %d", p->pid, state, p->name, p->uid, p->gid);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}
