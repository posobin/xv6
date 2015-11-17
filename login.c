#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "md5.h"
#include "pwd.h"
#include "grp.h"

#define HASH_LENGTH 32

uint getline(char** out, uint n, int fd)
{
  if (!(*out)) out = malloc(n);
  char ch;
  uint index = 0;
  while (read(fd, &ch, 1) && index < n && ch != '\n')
  {
    (*out)[index++] = ch;
  }
  return index;
}

int
main()
{
  printf(1, "Login: ");
  char username[MAX_NAME_LENGTH + 1];
  fgets(username, MAX_NAME_LENGTH + 1, 0);
  username[strlen(username) - 1] = 0;
  struct passwd* pass = getpwnam(username);
  int ok = (pass == 0 ? 0 : 1);

  printf(1, "Password: ");
  char password[MAX_PASSWD_LENGTH + 1];
  fgets(password, MAX_PASSWD_LENGTH + 1, 0);
  password[strlen(password) - 1] = 0;

  char password_hash[33];
  getmd5(password, strlen(password), password_hash);
  password_hash[32] = 0;

  if (ok && pass->pw_passwd[0] &&
      strcmp(password_hash, pass->pw_passwd) != 0) {
    ok = 0;
  }

  if (ok)
  {
    umask(022);
    if (initgroups(pass->pw_name, pass->pw_gid) < 0) {
      printf(2, "login: could not initialize groups\n");
      exit();
    }
    if (setreuid(pass->pw_uid, pass->pw_uid) < 0 ||
        setregid(pass->pw_gid, pass->pw_gid)) {
      printf(2, "login: could not change rights\n");
      exit();
    }
    char *argv[] = { pass->pw_shell, 0 };
    chdir(pass->pw_dir);
    execvpe(pass->pw_shell, argv, environ);
    printf(1, "login: exec user shell failed\n");
    exit();
  }
  printf(2, "Login incorrect\n");
  exit();
}
