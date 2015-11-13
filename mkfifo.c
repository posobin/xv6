#include "types.h"
#include "user.h"
#include "stat.h"

int
main(int argc, char *argv[])
{
  if(argc < 2){
    printf(2, "Usage: mkfifo files...\n");
    exit();
  }

  for (int i = 1; i < argc; ++i){
    if(mkfifo(argv[i], 0666) < 0){
      printf(2, "mkfifo: %s failed to create\n", argv[i]);
      break;
    }
  }
  
  exit();
}
