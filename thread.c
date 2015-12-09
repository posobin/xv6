#include "types.h"
#include "user.h"
#include "syscall.h"
#include "clone_flags.h"
#include "errno.h"

struct thread
{
  void* (*user_function)(void*);
  void* arg;
  void* stack;

  int exited;
  void* result;
  int detached;
};

int
start_thread(void* arg)
{
  struct thread* thread = (struct thread*)arg;
  thread->result = thread->user_function(thread->arg);
  thread->exited = 1;
  if (thread->detached) {
    free(thread->stack);
  }
  _exit();
}

#ifndef PGSIZE
#define PGSIZE 4096
#endif

int
thread_create(thread_t* thread, void* (*fn)(void*), void* arg, int detached)
{
  void* stack = malloc(2 * PGSIZE);
  if (!stack) {
    errno = ENOMEM;
    return -1;
  }
  void* original_stack = stack;
  stack += 2 * PGSIZE;
  struct thread* result = (struct thread*)stack;
  *--result = (struct thread) {
    .user_function = fn,
    .arg = arg,
    .stack = original_stack,
    .exited = 0,
    .detached = detached,
  };
  clone_fn(start_thread, (void*)result, (void*)result);
  *thread = (thread_t)result;
  return 0;
}

int
thread_join(thread_t thread_id, void** retval)
{
  struct thread* thread = (struct thread*)thread_id;
  if (thread->detached) {
    errno = EINVAL;
    return -1;
  }
  while (!thread->exited) {
    sched_yield();
  }

  if (retval != 0) {
    *retval = thread->result;
  }
  if (thread->stack != 0) {
    free(thread->stack);
  }
  return 0;
}

int
thread_detach(thread_t thread_id)
{
  struct thread* thread = (struct thread*)thread_id;
  thread->detached = 1;
  return 0;
}
