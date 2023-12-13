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

void func(){
  int a = 3;
  printf("%d\n", a);
}

int main()
{
  func();
  func();
  func();
	exit(0);
}