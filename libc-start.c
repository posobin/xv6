#include "user.h"

char** environ;

int __libc_start_main(int (*main) (int, char**, char**),
    int argc, char** argv, void (*init) (void),
    void (*fini) (void), void (*rtld_fini) (void),
    void (* stack_end))
{
  environ = &argv[argc + 1];
  main(argc, argv, environ);
  exit();
}

int __libc_csu_fini(void)
{
  printf(1, "fini\n");
  exit();
}

int __libc_csu_init(void)
{
  printf(1, "init\n");
  exit();
}
