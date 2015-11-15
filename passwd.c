#include "types.h"
#include "user.h"
#include "md5.h"
#include "fcntl.h"
#include "user.h"
#include "pwd.h"

#define HASH_LENGTH 32

struct node
{
  struct node* next;
  struct passwd* data;
};

int
main(int argc, char** argv)
{
  if (argc < 2) {
    printf(2, "Usage: passwd username\n");
    exit();
  }
  struct passwd* pass = getpwnam(argv[1]);
  if (!pass){
    printf(2, "Could not find such username or open the passwd file\n");
    exit();
  }
  if (pass->pw_uid != getuid() && getuid() != 0) {
    printf(2, "Permission denied\n");
    exit();
  }

  struct node* current = 0;
  struct node* to_change = 0;
  struct passwd* current_passwd;
  while ((current_passwd = getpwent())) {
    struct node* new_node = malloc(sizeof(struct node));
    new_node->next = current;
    new_node->data = malloc(sizeof(struct passwd));
    current = new_node;
    new_node->data->pw_name = malloc(MAX_NAME_LENGTH + 1);
    new_node->data->pw_passwd = malloc(MAX_PASSWD_HASH_LENGTH + 1);
    new_node->data->pw_gecos = malloc(MAX_GECOS_LENGTH + 1);
    new_node->data->pw_dir = malloc(MAX_DIR_LENGTH + 1);
    new_node->data->pw_shell = malloc(MAX_SHELL_LENGTH + 1);

    strcpy(new_node->data->pw_name, current_passwd->pw_name);
    strcpy(new_node->data->pw_passwd, current_passwd->pw_passwd);
    new_node->data->pw_uid = current_passwd->pw_uid;
    new_node->data->pw_gid = current_passwd->pw_gid;
    strcpy(new_node->data->pw_gecos, current_passwd->pw_gecos);
    strcpy(new_node->data->pw_dir, current_passwd->pw_dir);
    strcpy(new_node->data->pw_shell, current_passwd->pw_shell);

    if (to_change == 0 && strcmp(new_node->data->pw_name, argv[1]) == 0) {
      to_change = new_node;
    }
  }
  endpwent();

  // Don't ask root for an old password
  if (getuid() != 0 && to_change->data->pw_passwd[0])
  {
    printf(1, "Enter old password: ");
    char old_pass[33];
    fgets(old_pass, 33, 0);
    if (strlen(old_pass) > 0) old_pass[strlen(old_pass) - 1] = 0;
    char old_pass_hash[32];
    getmd5(old_pass, strlen(old_pass), old_pass_hash);
    if (strncmp(old_pass_hash, to_change->data->pw_passwd, 32) != 0)
    {
      printf(2, "Wrong password\n");
      exit();
    }
  }
  printf(1, "Enter new password: ");
  char password[34];
  fgets(password, 34, 0);
  if (strlen(password) == 33) {
    printf(2, "Password is too long\n");
    exit();
  }
  if (strlen(password) > 0) password[strlen(password) - 1] = 0;
  char password_hash[33];
  getmd5(password, strlen(password), password_hash);
  printf(1, "Repeat password: ");
  fgets(password, 34, 0);
  if (strlen(password) > 0) password[strlen(password) - 1] = 0;
  char second_hash[32];
  getmd5(password, strlen(password), second_hash);
  if (strncmp(password_hash, second_hash, 32) != 0) {
    printf(2, "Passwords don't match\n");
    exit();
  }
  password_hash[32] = 0;
  strncpy(to_change->data->pw_passwd, password_hash, 33);

  int fd = open(PASSWD_FILE, O_WRONLY);
  if (fd < 0) {
    printf(2, "Cannot open passwd file for writing\n");
    exit();
  }
  while (current) {
    putpwent(current->data, fd);
    free(current->data->pw_name);
    free(current->data->pw_passwd);
    free(current->data->pw_gecos);
    free(current->data->pw_dir);
    free(current->data->pw_shell);
    free(current->data);
    struct node* previous = current;
    current = current->next;
    free(previous);
  }
  close(fd);

  exit();
}
