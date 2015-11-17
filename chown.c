#include "user.h"
#include "errno.h"
#include "pwd.h"

int main(int argc, char** argv)
{
  if (argc != 3) {
    printf(2, "Usage: chown username file\n");
    exit();
  }
  struct passwd* pass = getpwnam(argv[1]);
  if (pass == 0) {
    printf(2, "Invalid user: %s\n", argv[1]);
    exit();
  }
  if (chown(argv[2], pass->pw_uid, -1) < 0) {
    switch (errno) {
      case EPERM:
        printf(2, "Permission denied\n");
        break;
      case ENOENT:
        printf(2, "No such file or directory\n");
        break;
      default:
        printf(2, "Unexpected errno: %d\n", errno);
        break;
    }
  }

  exit();
}
