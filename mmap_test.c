#include "user.h"
#include "fcntl.h"
#include "mmap.h"

char* ptr;
int size = 10;
int value = 0;
char str[] = "Hello world!";

void* fn(void* arg) {
  return (void*)str;
}

int main() {
  int fd = open("haha", O_RDWR | O_CREATE, 0666);
  ptr = malloc(size + 5000);
  ptr = mmap(ptr, size, PROT_READ | PROT_WRITE | PROT_EXEC, 0, fd, 0);
  for (int i = 0; i < size; ++i) {
    ptr[i] = ((char*)fn)[i];
  }
  printf(1, "Loaded up\n");
  char* result = (char*)((void* (*)(void*))ptr)(0);
  printf(1, "%s\n", result);
  return 0;
}
