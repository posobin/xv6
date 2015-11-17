#include "user.h"
#include "errno.h"
#include "pwd.h"
#include "grp.h"

int main(int argc, char** argv)
{
  if (argc != 3) {
    printf(2, "Usage: chown username file\n");
    exit();
  }
  char* owner = argv[1];
  char* group;
  if ((group = strchr(argv[1], ':'))) {
    *group = 0;
    group++;
  }
  struct passwd* pass = 0;
  uid_t uid = -1;
  if (owner[0]) {
    pass = getpwnam(owner);
    if (pass == 0) {
      printf(2, "Invalid user: %s\n", owner);
      exit();
    }
    uid = pass->pw_uid;
  }
  struct group* grp = 0;
  gid_t gid = -1;
  if (group) {
    grp = getgrnam(group);
    if (grp == 0) {
      printf(2, "Invalid group: %s\n", grp);
      exit();
    }
    gid = grp->gr_gid;
  }
  if (chown(argv[2], uid, gid) < 0) {
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
