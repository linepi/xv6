#pragma once
#include "common/types.h"
#include "common/stdlib.h"
#include "kernel/stat.h"
#include "kernel/date.h"
#include "user/system.h"

// system calls
int fork(void);
int exit(int) __attribute__((noreturn));
int wait(int*);
int pipe(int*);
int write(int, const void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(char*, char**);
int open(const char*, int);
int mknod(const char*, short, short);
int unlink(const char*);
int fstat(int fd, struct stat*);
int link(const char*, const char*);
int mkdir(const char*);
int chdir(const char*);
int dup(int);
int getpid(void);
char* sbrk(int);
int sleep(int);
int uptime(void);
int trace(int);
int pgaccess(const void*, int, void*);
int system_info(struct system_info*);

// ulib.c
int stat(const char*, struct stat*);
int rmdir(const char *);
void fprintf(int, const char*, ...);
void printf(const char*, ...);
char* gets(char*, int max);
void* malloc(uint);
void free(void*);
int ugetpid(void);
char *getcwd(char *buf, int size);