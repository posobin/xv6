#include "types.h"

#define MAX_PASSWD_LENGTH 32
#define MAX_NAME_LENGTH 32
#define MAX_PASSWD_HASH_LENGTH 32
#define MAX_GECOS_LENGTH 1024
#define MAX_DIR_LENGTH 1024
#define MAX_SHELL_LENGTH 1024
#define MAX_PASSWD_LINE_LENGTH MAX_NAME_LENGTH + MAX_PASSWD_HASH_LENGTH +\
  20 + MAX_GECOS_LENGTH + MAX_DIR_LENGTH + MAX_SHELL_LENGTH + 5
#define NUMBER_OF_PASSWD_TOKENS 7
#define PASSWD_FILE "/etc/passwd"

struct passwd
{
  char* pw_name;    // username
  char* pw_passwd;  // user password
  uid_t pw_uid;     // user ID
  gid_t pw_gid;     // group ID
  char* pw_gecos;   // user information
  char* pw_dir;     // home directory
  char* pw_shell;   // shell program
};

struct passwd* getpwnam(const char* name);
struct passwd* getpwuid(uid_t uid);
struct passwd* getpwent(void);
int putpwent(struct passwd*, int fd);
void setpwent(void);
void endpwent(void);
