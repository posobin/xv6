#include "user.h"
#include "fcntl.h"
#include "mmap.h"

char* ptr;

void* fn(void* arg) {
  for (int i = 0; ptr[i]; ++i) {
    write(1, &ptr[i], 1);
  }
  return 0;
}

int main() {
  int fd = open("haha", O_RDWR | O_CREATE, 0666);
  printf(1, "%p\n", ptr);
  ptr = malloc(5100);
  ptr = mmap(ptr, 100, PROT_READ | PROT_WRITE, 0, fd, 0);
  for (int i = 0; ptr[i]; ++i) {
    write(1, &ptr[i], 1);
  }
  char str[] = "Hello world!\n";
  for (int i = 0; str[i]; ++i) {
    ptr[i] = str[i];
  }
  int result = fork();
  printf(1, "%d\n", result);
  if (result == 0) {
    fn(0);
    exit();
  }
  wait();
  printf(1, "waited\n");
  for (int i = 0; ptr[i]; ++i) {
    write(1, &ptr[i], 1);
  }
  return 0;
}
