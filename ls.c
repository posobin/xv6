#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#include "pwd.h"
#include "grp.h"

char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;
  
  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;
  
  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
  return buf;
}

char permissions_string[11];

char*
get_permissions_string(uint mode) {
  if (S_ISLNK(mode)) permissions_string[0] = 'l';
  else if (S_ISREG(mode)) permissions_string[0] = '-';
  else if (S_ISBLK(mode)) permissions_string[0] = 'b';
  else if (S_ISDIR(mode)) permissions_string[0] = 'd';
  else if (S_ISCHR(mode)) permissions_string[0] = 'c';
  else if (S_ISFIFO(mode)) permissions_string[0] = 'p';
  else if (S_ISSOCK(mode)) permissions_string[0] = 's';

  permissions_string[1] = (S_IRUSR & mode) ? 'r' : '-';
  permissions_string[2] = (S_IWUSR & mode) ? 'w' : '-';
  permissions_string[3] = (S_IXUSR & mode) ? 'x' : '-';

  if (S_ISUID & mode) {
    if (S_IXUSR & mode) {
      permissions_string[3] = 's';
    } else {
      permissions_string[3] = 'S';
    }
  }

  permissions_string[4] = (S_IRGRP & mode) ? 'r' : '-';
  permissions_string[5] = (S_IWGRP & mode) ? 'w' : '-';
  permissions_string[6] = (S_IXGRP & mode) ? 'x' : '-';

  if (S_ISGID & mode) {
    if (S_IXGRP & mode) {
      permissions_string[6] = 's';
    } else {
      permissions_string[6] = 'S';
    }
  }

  permissions_string[7] = (S_IROTH & mode) ? 'r' : '-';
  permissions_string[8] = (S_IWOTH & mode) ? 'w' : '-';
  permissions_string[9] = (S_IXOTH & mode) ? 'x' : '-';

  if (S_ISVTX & mode) {
    if (S_IXOTH & mode) {
      permissions_string[9] = 't';
    } else {
      permissions_string[9] = 'T';
    }
  }

  permissions_string[10] = 0;
  return permissions_string;
}

void
print_file_info(char* path, struct stat* stat)
{
  printf(1, "%s %2d ", get_permissions_string(stat->mode), stat->nlink);

  struct passwd* passwd = getpwuid(stat->uid);
  if (passwd) printf(1, "%s ", passwd->pw_name);
  else printf(1, "%d ", stat->uid);

  struct group* group = getgrgid(stat->gid);
  if (group) printf(1, "%s ", group->gr_name);
  else printf(1, "%d ", stat->gid);

  printf(1, "%5d %3d %s\n", stat->size, stat->ino, fmtname(path));
}

void
ls(char *path)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;
  
  if((fd = open(path, O_NONBLOCK)) < 0){
    printf(2, "ls: cannot open %s\n", path);
    return;
  }
  
  if(fstat(fd, &st) < 0){
    printf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }
  
  if(S_ISREG(st.mode)){
    print_file_info(path, &st);
  }

  if(S_ISDIR(st.mode)){
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf(1, "ls: path too long\n");
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        printf(1, "ls: cannot stat %s\n", buf);
        continue;
      }
      print_file_info(buf, &st);
    }
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  int i;

  if(argc < 2){
    ls(".");
    exit();
  }
  for(i=1; i<argc; i++)
    ls(argv[i]);
  exit();
}
