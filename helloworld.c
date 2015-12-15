#include "types.h"
#include "user.h"
#include "fcntl.h"

void*
function(void* ch)
{
  if (((int)ch) == 20) {
    printf(1, "in thread %d\n", ch);
    char* argv[] = {"ls", 0};
    execvpe("helloworld", argv, environ);
  } else {
    sleep(20);
  }
  return ch;
}

const int NTHREADS = 300;

int
main()
{
  thread_t* threads = (thread_t*)malloc(NTHREADS * sizeof(thread_t));
  int i;
  for (i = 0; i < NTHREADS; ++i) {
    if (thread_create(&threads[i], function, (void*)i, 0) < 0) {
      printf(2, "errno: %d\n", errno);
      break;
    }
  }
  for (int j = 0; j < i; ++j) {
    int returned_value;
    thread_join(threads[j], (void**)&returned_value);
    printf(1, "Returned value by %d: %d\n", j, returned_value);
  }
  free(threads);
  exit();
}
