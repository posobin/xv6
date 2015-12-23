#include "user.h"
#include "mmap.h"
#include "fcntl.h"

int main(int argc, char** argv)
{
  int n = atoi(argv[1]);
  char* ptr = malloc(4096);
  volatile int* p = (int*)mmap(ptr, 2,
      PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED,
      -1, 0);
  p[0] = 1;
  p[1] = 1;
  int pid = 0;
  for (int i = 1; i <= 2; ++i) {
    pid = fork();
    if (pid != 0) {
      continue;
    }
    while (p[1] <= n) {
      while (p[0] != i) { }
      if (p[1] > n) break;
      /*std::cout << i << ' ' << p[1]++ << std::endl;*/
      printf(1, "%d %d\n", i, p[1]++);
      p[0] = 3 - p[0];
      sched_yield();
    }
    exit();
  }
  if (pid != 0) {
    wait();
    wait();
  }
  exit();
}

