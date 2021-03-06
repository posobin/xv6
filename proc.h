#include "spinlock.h"
#include "list.h"
#include "clone_flags.h"
#include "param.h"
#include "mmap.h"
// Segments in proc->gdt.
#define NSEGS     7

// Per-CPU state
struct cpu {
  uchar id;                    // Local APIC ID; index into cpus[] below
  struct context *scheduler;   // swtch() here to enter scheduler
  struct taskstate ts;         // Used by x86 to find stack for interrupt
  struct segdesc gdt[NSEGS];   // x86 global descriptor table
  volatile uint started;       // Has the CPU started?
  int ncli;                    // Depth of pushcli nesting.
  int intena;                  // Were interrupts enabled before pushcli?
  
  // Cpu-local storage variables; see below
  struct cpu *cpu;
  struct proc *proc;           // The currently-running process.
};

extern struct cpu cpus[NCPU];
extern int ncpu;

// Per-CPU variables, holding pointers to the
// current cpu and to the current process.
// The asm suffix tells gcc to use "%gs:0" to refer to cpu
// and "%gs:4" to refer to proc.  seginit sets up the
// %gs segment register so that %gs refers to the memory
// holding those two variables in the local cpu's struct cpu.
// This is similar to how thread-local variables are implemented
// in thread libraries such as Linux pthreads.
extern struct cpu *cpu asm("%gs:0");       // &cpus[cpunum()]
extern struct proc *proc asm("%gs:4");     // cpus[cpunum()].proc

//PAGEBREAK: 17
// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.
// Contexts are stored at the bottom of the stack they
// describe; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly,
// but it is on the stack and allocproc() manipulates it.
struct context {
  uint edi;
  uint esi;
  uint ebx;
  uint ebp;
  uint eip;
};

// Info about process’s mmaps.
struct mmap_struct {
  char* start; // Better be page-aligned.
  int length;
  int prot;
  int flags;

  struct file* file;
  int offset;

  int users;

  struct spinlock lock;
};

struct mmap_list {
  struct list_head list;
  struct mmap_struct* mmap;
};

struct mm_struct {
  pde_t* pgdir;  // Page table
  uint users;    // Number of links to the page table
  uint sz;       // Size of process memory (bytes)
  struct list_head mmap_list; // List of mmaps
  struct spinlock lock;
  struct spinlock mmap_list_lock;
};

extern struct cache_info* mm_cache;

struct files_struct {
  struct file** fd;
  struct spinlock lock;
  uint users; // Number of tasks using this files_struct
};

struct fs_info_struct {
  struct inode* root;          // Current root directory
  struct inode* cwd;           // Current directory
  uint umask;                  // File mode creation mask
  uint users;
  struct spinlock lock;
};

#define NGROUPS_MAX 16

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// Per-process state
struct proc {
  char *kstack;                // Bottom of kernel stack for this process
  struct mm_struct* mm;        // Page table
  enum procstate state;        // Process state
  volatile int pid;            // Process ID
  struct proc *parent;         // Parent process
  struct trapframe *tf;        // Trap frame for current syscall
  struct context *context;     // swtch() here to run process
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  struct files_struct* files;  // Open files
  struct fs_info_struct* fs;   // Cwd, current root, umask
  uid_t uid;                   // User ID
  uid_t euid;                  // Effective user ID
  uid_t suid;                  // Saved UID
  gid_t gid;                   // Group ID
  gid_t egid;                  // Effective group ID
  gid_t sgid;                  // Saved GID
  char name[16];               // Process name (debugging)
  uint ngroups;
  gid_t groups[NGROUPS_MAX];   // Supplementary groups that the current
                               // user (uid) belongs to
  struct list_head children;   // List of our children
  struct list_head siblings;   // List of our siblings
  struct proc* group_leader;   // Leader of our group
  struct list_head thread_group; // List of processess in the same thread group
  int tgid;                    // Thread group ID
  int detached;                // Is thread detached?

  struct list_head list;
};

struct ptable {
  struct spinlock lock;
  struct list_head list;
};

extern struct ptable ptable;

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
