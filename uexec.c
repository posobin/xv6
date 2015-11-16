#include "user.h"
#include "errno.h"

int
execvpe(const char *file, char *const argv[], char *const envp[])
{
  if (strchr(file, '/')) {
    return execve(file, argv, envp);
  }
  const char* path = getenv("PATH");
  if (path == 0) { // Path is empty, abort
    errno = ENOENT;
    return -1;
  }
  int pathlen = strlen(path);
  int len = strlen(file);
  char* name = (char*)malloc(len + pathlen + 2);
  char* path_malloc;
  path_malloc = name = memmove(name + pathlen + 1, (char*)file, len + 1);
  *--name = '/';
  const char* p = path;
  int got_eacces = 0;
  do {
    char* startp;
    path = p;
    p = strchrnul(path, ':');
    if (p == path) {
      // Two adjacent colons, or a colon at the beginning or the end
      // of `PATH' means to search the current directory.
      startp = name + 1;
    }
    else {
      startp = (char*)memmove(name - (p - path), (char*)path, p - path);
    }
    execve(startp, argv, envp);
    switch (errno) {
      case EACCES:
        // Record the we got a `Permission denied' error. If we end
        // up finding no executable we can use, we want to diagnose
        // that we did find one but were denied access.
        got_eacces = 1;
      case ENOENT:
      case ESTALE:
      case ENOTDIR:
        // Those errors indicate the file is missing or not executable
        // by us, in which case we want to just try the next path
        // directory.
      case ENODEV:
      case ETIMEDOUT:
        // Some strange filesystems like AFS return even
        // stranger error numbers. They cannot reasonably mean
        // anything else so ignore those, too.
        break;

      default:
        // Some other error means we found an executable file, but
        // something went wrong executing it; return the error to our
        // caller.
        return -1;
    }
  } while(*p++ != '\0');
  // At least one failure was due to permissions, so report that
  // error.
  if (got_eacces) {
    errno = EACCES;
  }

  free(path_malloc);
  return -1;
}
