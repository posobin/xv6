#include "types.h"
#include "stat.h"
#include "user.h"
#include "errno.h"

int
main(int argc, char **argv)
{
  int i;

  if(argc < 1){
    printf(2, "usage: kill pid...\n");
    exit();
  }
  for(i=1; i<argc; i++) {
    if (kill(atoi(argv[i])) < 0) {
      printf(2, "Could not kill %s, ", argv[i]);
      if (errno == ESRCH) {
        printf(2, "process not found.\n");
      } else if (errno == EPERM) {
        printf(2, "permission denied.\n");
      } else {
        printf(2, "errno = %d\n", errno);
      }
    }
  }
  exit();
}
