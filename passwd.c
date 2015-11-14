#include "types.h"
#include "user.h"
#include "md5.h"
#include "fcntl.h"
#include "user.h"

#define HASH_LENGTH 32

int str_to_int(char* str, int base, int* result)
{
  int current = 0;
  while (*str) {
    int add = 0;
    if ('0' <= *str && *str <= '9') add = *str - '0';
    else if ('a' <= *str && *str <= 'z') add = *str - 'a' + 10;
    else if ('A' <= *str && *str <= 'Z') add = *str - 'A' + 10;
    else break;
    if (add >= base) return -1;
    current *= base;
    current += add;
    str++;
  }
  *result = current;
  return 0;
}

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

struct passwd
{
  int uid;
  char passwd[HASH_LENGTH];
};

struct node
{
  struct node* next;
  struct passwd* data;
  int has_password;
};

int
main(int argc, char** argv)
{
  if (argc < 2) {
    printf(2, "Usage: passwd UID\n");
    exit();
  }
  int uid;
  str_to_int(argv[1], 10, &uid);
  if (uid != getuid() && geteuid() != 0)
  {
    printf(2, "Permission denied\n");
    exit();
  }

  int fd = open("/passwd_file", O_RDONLY);
  if (fd < 0)
  {
    printf(2, "Cannot open passwd_file for reading\n");
    exit();
  }
  char ch;
  struct node* current = 0;
  int found = 0;
  while (read(fd, &ch, 1) > 0) {
    struct node* new_node = malloc(sizeof(struct node));
    new_node->next = current;
    new_node->data = malloc(sizeof(struct passwd));
    current = new_node;
    int current_uid = ch - '0';;
    while (read(fd, &ch, 1) > 0) {
      if (ch == ':') break;
      current_uid *= 10;
      current_uid += ch - '0';
    }
    current->data->uid = current_uid;
    int index = 0;
    while (read(fd, &ch, 1) > 0 && index < HASH_LENGTH) {
      if (ch == '\n') break;
      current->data->passwd[index++] = ch;
    }
    if (index < HASH_LENGTH)
      current->data->passwd[0] = 0;
    if (current->data->uid == uid)
      found = 1;
  }
  close(fd);
  if (!found)
  {
    printf(2, "Requested UID not found\n");
    exit();
  }
  fd = open("/passwd_file", O_WRONLY);
  if (fd < 0)
  {
    printf(2, "Cannot open passwd_file for writing\n");
    exit();
  }
  while (current)
  {
    if (current->data->uid == uid)
    {
      char* tmp;
      if (getuid() != 0 && current->data->passwd[0])
      {
        printf(1, "Enter old password: ");
        char old_pass[32];
        tmp = old_pass;
        int length = getline(&tmp, 32, 0);
        char old_pass_hash[32];
        getmd5(old_pass, length, old_pass_hash);
        if (strncmp(old_pass_hash, current->data->passwd, 32) != 0)
        {
          printf(2, "Wrong password\n");
          goto bad;
        }
      }
      printf(1, "Enter new password: ");
      char password[32];
      tmp = password;
      int read = getline(&tmp, 32, 0);
      if (read == 32)
      {
        printf(2, "Password is too long\n");
        goto bad;
      }
      char password_hash[32];
      getmd5(password, read, password_hash);
      printf(1, "Repeat password: ");
      tmp = password;
      read = getline(&tmp, 32, 0);
      char second_hash[32];
      getmd5(password, read, second_hash);
      if (strncmp(password_hash, second_hash, 32) != 0)
      {
        printf(2, "Passwords don't match\n");
        goto bad;
      }
      strncpy(current->data->passwd, password_hash, 32);
    }
bad:
    printf(fd, "%d:%s\n", current->data->uid, current->data->passwd);
    free(current->data);
    struct node* previous = current;
    current = current->next;
    free(previous);
  }
  close(fd);

  exit();
}
