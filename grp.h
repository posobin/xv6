#include "types.h"

#define NUMBER_OF_GROUP_TOKENS 4
#define MAX_GROUP_LINE_LENGTH 4096
#define MAX_GROUP_MEMBERS 128
#define GROUP_FILE "/etc/group"

struct group
{
  char* gr_name;    // group name
  char* gr_passwd;  // user password
  uid_t gr_gid;     // user ID
  char** gr_mem;    // NULL-terminated array of pointers to names of
                    // group members
};

struct group* getgrnam(const char* name);
struct group* getgrgid(gid_t gid);
struct group* getgrent(void);
int putgrent(struct group*, int fd);
void setgrent(void);
void endgrent(void);
int initgroups(const char* user, gid_t group);
