#include "user.h"
#include "pwd.h"
#include "grp.h"
#include "fcntl.h"

struct passwd_node {
  struct passwd_node* next;
  struct passwd* data;
};

int main(int argc, char** argv)
{
  int create_home = 0;
  char* username;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "-m") == 0 ||
        strcmp(argv[i], "--create-home") == 0) {
      create_home = 1;
    } else {
      username = argv[i];
    }
  }
  if (argc - create_home - 1 != 1) {
    printf(2, "Usage: useradd [-m|--create-home] username\n");
    exit();
  }
  if (getpwnam(username) != 0) {
    printf(2, "User %s already exists\n", username);
    exit();
  }
  if (strlen(username) >= 14 &&
      create_home) { // DIRSIZ == 14, but we need to create home directory
    printf(2, "Username is too long, can’t create home directory\n");
    exit();
  }
  if (strlen(username) > MAX_NAME_LENGTH) {
    printf(2, "Username is too long\n");
    exit();
  }
  int empty_uid = 1;
  for (empty_uid = 1; getpwuid(empty_uid) || getgrgid(empty_uid); ++empty_uid)
    ;
  char empty_string[] = "";
  int len = strlen(username);
  char* home_directory = malloc(7 + len);
  char shell[] = "/bin/sh";
  safestrcpy(home_directory, "/home/", 7);
  safestrcpy(home_directory + 6, username, len + 1);
  struct passwd new_entry = {
    .pw_name = username,
    .pw_passwd = empty_string,
    .pw_uid = empty_uid,
    .pw_gid = empty_uid,
    .pw_gecos = empty_string,
    .pw_dir = home_directory,
    .pw_shell = shell,
  };
  char* members[] = { username, 0 };
  struct group new_group = {
    .gr_name = username,
    .gr_passwd = empty_string,
    .gr_gid = empty_uid,
    .gr_mem = members,
  };
  int fd = open(PASSWD_FILE, O_WRONLY | O_APPEND);
  if (fd < 0) {
    free(home_directory);
    printf(2, "Could not open " PASSWD_FILE " for writing\n");
    exit();
  }
  int group_fd = open(GROUP_FILE, O_WRONLY | O_APPEND);
  if (group_fd < 0) {
    free(home_directory);
    printf(2, "Could not open " GROUP_FILE " for writing\n");
    exit();
  }
  putpwent(&new_entry, fd);
  putgrent(&new_group, group_fd);
  close(fd);
  close(group_fd);

  if (create_home) {
    if (mkdir(home_directory, 0755) < 0) {
      free(home_directory);
      printf(2, "Could not create home directory: %s\n", home_directory);
      printf(2, "errno: %d\n", errno);
      exit();
    }
    if (chown(home_directory, empty_uid, empty_uid)) {
      free(home_directory);
      printf(2, "Could not change home directory's owner\n", home_directory);
      printf(2, "errno: %d\n", errno);
      exit();
    }
  }
  free(home_directory);

  exit();
}
