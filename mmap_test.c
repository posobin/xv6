#include "user.h"
#include "fcntl.h"
#include "mmap.h"

char* ptr;
int value = 0;
char str[] = "Hello world!";
int size = 10;

void* fn(void* arg) {
  return (void*)str;
}

int main() {
  ptr = malloc(size + 5000);
  ptr = mmap(ptr, strlen(str), PROT_READ | PROT_WRITE | PROT_EXEC,
      MAP_ANONYMOUS | MAP_SHARED, 0, 0);
  ptr[0] = 'b';
  if (!fork()) {
    for (int i = 0; str[i]; ++i) {
      ptr[i] = str[i];
    }
    exit();
  }
  wait();
  printf(1, "%s\n", ptr);
  
  return 0;
}
