#include "user.h"
#include "fcntl.h"
#include "mmap.h"

int main() {
  int fd = open("haha", O_RDWR | O_CREATE, 0666);
  char* ptr = malloc(5100);
  printf(1, "%p\n", ptr);
  char* ptr2 = mmap(ptr, 100, PROT_READ | PROT_WRITE, 0, fd, 0);
  for (int i = 10; i < 20; ++i) {
    /*printf(1, "%d: %d", i, (int)ptr2[i]);*/
    write(1, &ptr2[i], 1);
    printf(1, "\n");
  }
  char str[] = "Hello world!\n";
  for (int i = 0; str[i]; ++i) {
    ptr2[i] = str[i];
  }
  return 0;
}
