#include "types.h"
#include "user.h"
#include "fcntl.h"

int
function(void* ch)
{
  printf(1, "in thread %d\n", ch);
  return 10;
}

const int NTHREADS = 50;

int
main()
{
  /*char cc[] = "ha";*/
  for (int i = 0; i < NTHREADS; ++i) {
    int pid = thread_create(function, (void*)i);
    printf(1, "created thread with pid %d\n", pid);
    printf(1, "errno: %d\n", errno);
  }
  for (int i = 0; i < NTHREADS; ++i) {
    wait();
  }
  exit();
}
