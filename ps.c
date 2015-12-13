#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#include "pwd.h"

char*
append(char* dst, char* src)
{
  while ((*dst = *src))
  {
    dst++;
    src++;
  }
  return dst;
}

void
get_parent_pid(char* pid, char* dst)
{
  char path[512] = "/proc/";
  char* str = path + strlen(path);
  str = append(str, pid);
  str = append(str, "/parent/pid");
  int fd = open(path, O_RDONLY);
  dst[0] = 0;
  if (fd < 0) return;
  int r = read(fd, dst, 10);
  if (r > 0) {
    dst[r - 1] = 0;
  }
  close(fd);
}

void
get_name(char* pid, char* dst)
{
  char path[512] = "/proc/";
  char* str = path + strlen(path);
  str = append(str, pid);
  str = append(str, "/name");
  int fd = open(path, O_RDONLY);
  dst[0] = 0;
  if (fd < 0) return;
  int r = read(fd, dst, 16);
  if (r > 0) {
    dst[r - 1] = 0;
  }
  close(fd);
}

void
get_uid(char* pid, char* dst)
{
  char path[512] = "/proc/";
  char* str = path + strlen(path);
  str = append(str, pid);
  str = append(str, "/uid");
  int fd = open(path, O_RDONLY);
  dst[0] = 0;
  if (fd < 0) return;
  int r = read(fd, dst, 10);
  if (r > 0) {
    dst[r - 1] = 0;
  }
  close(fd);
}

void
get_state(char* pid, char* dst)
{
  char path[512] = "/proc/";
  char* str = path + strlen(path);
  str = append(str, pid);
  str = append(str, "/state");
  int fd = open(path, O_RDONLY);
  dst[0] = 0;
  if (fd < 0) return;
  int r = read(fd, dst, 10);
  if (r > 0) {
    dst[r - 1] = 0;
  }
  close(fd);
}

int
main()
{
  char path[] = "/proc";
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    printf(2, "ps: cannot open %s\n", path);
    printf(2, "errno: %d\n", errno);
    return 1;
  }

  struct dirent de;
  printf(1, "PID\tPPID\tUSER\tNAME\tSTATE\n");
  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (strcmp(".", de.name) == 0 ||
        strcmp("..", de.name) == 0 ||
        strcmp("self", de.name) == 0) {
      continue;
    }
    char ppid[10];
    char name[16];
    char uid_str[10];
    char username[100];
    char state[10];
    get_parent_pid(de.name, ppid);
    get_name(de.name, name);
    get_uid(de.name, uid_str);
    get_state(de.name, state);
    int uid = atoi(uid_str);
    struct passwd* passwd = getpwuid(uid);
    if (passwd) strncpy(username, passwd->pw_name, 100);
    else username[0] = 0;
    printf(1, "%s\t%s\t%s\t%s\t%s\n", de.name, ppid, username, name, state);
  }
  close(fd);
  return 0;
}
