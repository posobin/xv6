#include "user.h"
#include "pwd.h"
#include "grp.h"

int main()
{
  uid_t uid = getuid();
  struct passwd* passwd = getpwuid(uid);
  printf(1, "uid=%d(%s) ", uid, passwd->pw_name);

  uid_t euid = geteuid();
  passwd = getpwuid(euid);
  printf(1, "euid=%d(%s) ", euid, passwd->pw_name);

  gid_t gid = getgid();
  struct group* group = getgrgid(gid);
  printf(1, "gid=%d(%s) ", gid, group->gr_name);

  gid_t egid = getegid();
  group = getgrgid(egid);
  printf(1, "egid=%d(%s) ", egid, group->gr_name);

  printf(1, "groups=");
  gid_t groups[33];
  int number_of_groups = getgroups(32, groups);
  for (int i = 0; i < number_of_groups; ++i) {
    if (i != 0) printf(1, ",");
    printf(1, "%d", groups[i]);
    struct group* group = getgrgid(groups[i]);
    if (group) printf(1, "(%s)", group->gr_name);
  }
  printf(1, "\n");
  exit();
}
