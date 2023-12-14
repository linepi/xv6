#include <stdarg.h>
#include "kernel/param.h"
#include "common/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/system.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/syscall.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"
#include "common/log.h"

int main()
{
  double a = 3.3;
  double b = -1.00111;
  int c = a * b - a + b;
  printf("%d\n", c);
	exit(0);
}