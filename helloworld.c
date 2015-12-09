#include "types.h"
#include "user.h"
#include "fcntl.h"

void*
function(void* ch)
{
  printf(1, "in thread %d\n", ch);
  return ch;
}

const int NTHREADS = 10;

int
main()
{
  thread_t threads[NTHREADS];
  for (int i = 0; i < NTHREADS; ++i) {
    thread_create(&threads[i], function, (void*)(i + 17), 0);
  }
  for (int i = 0; i < NTHREADS; ++i) {
    int returned_value;
    thread_join(threads[i], (void**)&returned_value);
    printf(1, "Returned value by %d: %d\n", i, returned_value);
  }
  exit();
}
