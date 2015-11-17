#include "grp.h"
#include "fcntl.h"
#include "user.h"
#include "errno.h"

static struct group current_group;
static char current_line[MAX_GROUP_LINE_LENGTH];
static char* group_members_names[MAX_GROUP_MEMBERS + 1];
static int fd;

// This function assumes that fd is already open
// Returns -1 if could not read next entry
static int
get_and_parse_grent(void)
{
  char* tokens[NUMBER_OF_GROUP_TOKENS];
  int ok = 0;
  fgets(current_line, MAX_GROUP_LINE_LENGTH, fd);
  int length = strlen(current_line);
  if (current_line[length - 1] == '\n' ||
      current_line[length - 1] == '\r') {
    current_line[length - 1] = 0;
  }
  int next_token = 0;
  int i;
  int next_group_name = 0;
  int comma = 0;
  for (i = 0; current_line[i]; ++i) {
    if (current_line[i] == ':') {
      if (ok == 0) {
        tokens[next_token++] = current_line + i;
      }
      current_line[i] = 0;
      ok = 0;
    } else if (ok == 0 && next_token < NUMBER_OF_GROUP_TOKENS) {
      ok = 1;
      tokens[next_token++] = current_line + i;
      if (next_token == NUMBER_OF_GROUP_TOKENS) {
        group_members_names[next_group_name++] = current_line + i;
      }
    } else if (current_line[i] == ',' && next_token == NUMBER_OF_GROUP_TOKENS) {
      current_line[i] = 0;
      comma = 1;
    } else if (comma) {
      group_members_names[next_group_name++] = current_line + i;
      comma = 0;
    }
  }
  if (i == 0) return -1;
  current_group.gr_name = tokens[0];
  current_group.gr_passwd = tokens[1];
  current_group.gr_gid = atoi(tokens[2]);
  current_group.gr_mem = group_members_names;
  group_members_names[next_group_name] = 0;
  return 0;
}


struct group*
getgrent(void)
{
  if (fd == 0) {
    fd = open(GROUP_FILE, O_RDONLY);
    if (fd < 0) {
      fd = 0;
      return 0;
    }
  }
  if (get_and_parse_grent() == -1) {
    return 0;
  }
  return &current_group;
}

void
setgrent(void)
{
  if (fd != 0) {
    close(fd);
    fd = 0;
  }
  fd = open(GROUP_FILE, O_RDONLY);
  if (fd < 0) fd = 0;
}

void
endgrent(void)
{
  if (fd != 0) {
    close(fd);
    fd = 0;
  }
}

struct group*
getgrnam(const char* name)
{
  setgrent();
  while (getgrent()) {
    if (strcmp(name, current_group.gr_name) == 0) {
      endgrent();
      return &current_group;
    }
  }
  endgrent();
  return 0;
}

struct group*
getgrgid(gid_t gid)
{
  setgrent();
  while (getgrent()) {
    if (current_group.gr_gid == gid) {
      endgrent();
      return &current_group;
    }
  }
  endgrent();
  return 0;
}

int
putgrent(struct group* grp, int fd)
{
  if (grp == 0) return -1;
  printf(fd, "%s:%s:%d:",
      grp->gr_name,
      grp->gr_passwd,
      grp->gr_gid);
  for (int i = 0; grp->gr_mem[i]; ++i)
  {
    if (i != 0) printf(fd, ",");
    printf(fd, "%s", grp->gr_mem[i]);
  }
  printf(fd, "\n");
  return 0;
}

int
initgroups(const char* user, gid_t additional_group)
{
  if (geteuid() != 0) {
    errno = EPERM;
    return -1;
  }
  const int max_number_of_groups = 32;
  gid_t groups[max_number_of_groups + 1];
  setgrent();
  struct group* group;
  int next_to_add = 0;
  int add_additional = 1;
  while ((group = getgrent())) {
    for (int i = 0; group->gr_mem[i]; ++i) {
      if (strcmp(group->gr_mem[i], user) == 0) {
        groups[next_to_add++] = group->gr_gid;
        if (group->gr_gid == additional_group) {
          add_additional = 0;
        }
      }
    }
  }
  if (add_additional) {
    groups[next_to_add++] = additional_group;
  }
  setgroups(next_to_add, groups);
  return 0;
}
