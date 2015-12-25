#include "types.h"
#include "user.h"
#include "fcntl.h"

void*
function(void* ch)
{
  /*while (1) {*/
    /*sleep(1000000);*/
  /*}*/
  return ch;
}

const int NTHREADS = 30000;

int
main()
{
  thread_t* threads = (thread_t*)malloc(NTHREADS * sizeof(thread_t));
  int i;
  for (i = 0; i < NTHREADS; ++i) {
    if (i % 100 == 0) {
      printf(1, "Creating thread %d\n", i);
    }
    if (thread_create(&threads[i], function, (void*)i, 0) < 0) {
      printf(2, "errno: %d, thread number %d\n", errno, i);
      break;
    }
  }
  exit();
}
