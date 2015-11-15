#include "user.h"

int main()
{
  printf(1, "uid=%d euid=%d gid=%d egid=%d\n",
      getuid(), geteuid(), getgid(), getegid());
  exit();
}
