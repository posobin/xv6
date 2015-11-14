#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "md5.h"

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
  char uid_str[8];
  char* tmp = uid_str;
  uint length = getline(&tmp, 8, 0);
  int uid = 0;
  for (int i = 0; i < length; ++i)
  {
    uid *= 10;
    uid += uid_str[i] - '0';
  }
  int fd = open("/passwd_file", O_RDONLY);
  if (fd < 0)
  {
    printf(2, "Cannot open passwd_file for reading\n");
    exit();
  }
  char ch;

  printf(1, "Password: ");
  char password[32];
  tmp = password;
  length = getline(&tmp, 32, 0);
  char password_hash[32];
  getmd5(password, length, password_hash);

  while (read(fd, &ch, 1) > 0) {
    int current_uid = ch - '0';;
    while (read(fd, &ch, 1) > 0) {
      if (ch == ':') break;
      current_uid *= 10;
      current_uid += ch - '0';
    }
    int index = 0;
    char real_pass_hash[32];
    while (read(fd, &ch, 1) > 0 && index < HASH_LENGTH) {
      if (ch == '\n') break;
      real_pass_hash[index++] = ch;
    }
    if (index < HASH_LENGTH)
      real_pass_hash[0] = 0;
    if (current_uid == uid)
    {
      if (!real_pass_hash[0]) // User has no password set
        goto ok;
      if (strncmp(password_hash, real_pass_hash, 32) != 0)
        goto bad;
ok:
      umask(022);
      char *argv[] = { "sh", 0 };
      close(fd);
      exec("sh", argv);
      printf(1, "login: exec sh failed\n");
      exit();
    }
  }

bad:
  printf(2, "Login incorrect\n");
  close(fd);
  exit();
}
