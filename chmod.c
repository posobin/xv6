#include "user.h"
#include "errno.h"

int main(int argc, char** argv)
{
  if (argc != 3) {
    printf(2, "Usage: chmod mode file\n");
    exit();
  }
  uint mode = 0;
  char* p = argv[1];
  while (*p)
  {
    mode *= 8;
    mode += *p - '0';
    p++;
  }
  if (chmod(argv[2], mode) < 0) {
    switch (errno) {
      case EPERM:
        printf(2, "Permission denied\n");
        break;
      case ENOENT:
        printf(2, "No such file or directory\n");
        break;
      default:
        printf(2, "Unknown errno: %d\n", errno);
        break;
    }
  }
  exit();
}
