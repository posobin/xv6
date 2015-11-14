#include "types.h"

struct stat;

// system calls
int fork(void);
int exit(void) __attribute__((noreturn));
int wait(void);
int pipe(int*);
int write(int, void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(char*, char**);
int open(char*, int, ...);
int mknod(char*, short, short, int);
int unlink(char*);
int fstat(int fd, struct stat*);
int link(char*, char*);
int mkfifo(char*, int);
int mkdir(char*, int);
int chdir(char*);
int dup(int);
int getpid(void);
char* sbrk(int);
int sleep(int);
int uptime(void);
int umask(int);
int setreuid(int, int);
int setregid(int, int);
int getuid(void);
int geteuid(void);
int getgid(void);
int getegid(void);

// ulib.c
int stat(char*, struct stat*);
char* strcpy(char*, char*);
void *memmove(void*, void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
void printf(int, char*, ...);
char* gets(char*, int max);
uint strlen(char*);
void* memset(void*, int, uint);
void* malloc(uint);
void free(void*);
int atoi(const char*);
int strncmp(const char*, const char*, uint num);
char* strncpy(char *s, const char *t, int n);
