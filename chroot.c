#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "errno.h"

int main(int argc, char** argv)
{
  if (argc < 2) {
    printf(2, "Usage: chroot dir [prog [args]]");
    return 1;
  }
  if (chdir(argv[1]) < 0) {
    printf(2, "chdir error. Errno: %d\n", errno);
    return 1;
  }
  if (chroot(argv[1]) < 0) {
    if (errno == EPERM) {
      printf(2, "Permission denied\n");
      return 1;
    }
    printf(2, "chroot error. Errno: %d\n", errno);
    return 1;
  }
  char* cmd;
  char* args[] = { "/bin/sh", 0 };
  if (argc == 2) {
    cmd = args[0];
    execvpe("/bin/sh", args, environ);
  } else {
    cmd = argv[2];
    execvpe(argv[2], argv + 2, environ);
  }
  printf(2, "Failed to execute %s. Errno = %d\n", cmd, errno);
  return 0;
}
