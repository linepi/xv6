#include <stdarg.h>
#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/system.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/syscall.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"
#include "common/log.h"

int
cmd_wrapper(char* cmd, ...)
{
  va_list ap;
  va_start(ap, cmd);

  int pid = fork();
  if (pid < 0) {
    printf("fork failed\n");
  } else if (pid == 0) {
    char *args[MAXARG] = {cmd};
    int idx = 1;
    while (1) {
      args[idx++] = va_arg(ap, char *);
      if (args[idx - 1] == NULL)  
        break;
    }
    int rc = exec(cmd, args);
    exit(rc);
  }
  int state;
  wait(&state);
  return state;
}

int main()
{
  int pid = fork();
  if (pid == 0)
    cmd_wrapper("/cp", "/1234", "/4321", NULL);
  wait(0);
	exit(0);
}