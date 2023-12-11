#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"
#include "kernel/spinlock.h"
#include "kernel/proc.h"
#include "kernel/syscall.h"
#include "kernel/defs.h"
#include "common/color.h"

// Fetch the uint64 at addr from the current process.
int
fetchaddr(uint64 addr, uint64 *ip)
{
  struct proc *p = myproc();
  if(!uaddrvalid(p, addr) || !uaddrvalid(p, addr + 7))
    return -1;
  if(copyin((char *)ip, addr, sizeof(*ip)) != 0)
    return -1;
  return 0;
}

// Fetch the nul-terminated string at addr from the current process.
// Returns length of string, not including nul, or -1 for error.
int
fetchstr(uint64 addr, char *buf, int max)
{
  struct proc *p = myproc();
  if(!uaddrvalid(p, addr))
    return -1;
  int err = copyinstr(buf, addr, max);
  if(err < 0)
    return err;
  return strlen(buf);
}

static uint64
argraw(int n)
{
  struct proc *p = myproc();
  switch (n) {
  case 0:
    return p->trapframe->a0;
  case 1:
    return p->trapframe->a1;
  case 2:
    return p->trapframe->a2;
  case 3:
    return p->trapframe->a3;
  case 4:
    return p->trapframe->a4;
  case 5:
    return p->trapframe->a5;
  }
  panic("argraw");
  return -1;
}

// Fetch the nth 32-bit system call argument.
int
argint(int n, int *ip)
{
  *ip = argraw(n);
  return 0;
}

// Retrieve an argument as a pointer.
// Doesn't check for legality, since
// copyin/copyout will do that.
int
argaddr(int n, uint64 *ip)
{
  *ip = argraw(n);
  return 0;
}

// Fetch the nth word-sized system call argument as a null-terminated string.
// Copies into buf, at most max.
// Returns string length if OK (including nul), -1 if error.
int
argstr(int n, char *buf, int max)
{
  uint64 addr;
  if(argaddr(n, &addr) < 0)
    return -1;
  return fetchstr(addr, buf, max);
}

#define DEF_SYSCALL(id, name) extern uint64 sys_##name(void);
#include "kernel/syscall_def.h"
#undef DEF_SYSCALL

#define DEF_SYSCALL(id, name) extern const char *trace_##name();
#include "kernel/syscall_def.h"
#undef DEF_SYSCALL

static uint64 (*syscalls[])(void) = {
#define DEF_SYSCALL(id, name) [SYS_##name] sys_##name,
#include "kernel/syscall_def.h"
#undef DEF_SYSCALL
};

static char *syscalls_name[] = {
#define DEF_SYSCALL(id, name) [SYS_##name] #name,
#include "kernel/syscall_def.h"
#undef DEF_SYSCALL
};

static const char *(*strace_param[])(void) = {
#define DEF_SYSCALL(id, name) [SYS_##name] trace_##name,
#include "kernel/syscall_def.h"
#undef DEF_SYSCALL
};

void
syscall(void)
{
  int num;
  struct proc *p = myproc();

  num = p->trapframe->a7; 
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    if ((1 << num) & p->trace_mask) {
      printf(ANSI_FMT("%s(%d): %s(%s)", ANSI_FG_CYAN), 
        p->name, p->pid, syscalls_name[num], strace_param[num]());
    }
    uint64 ret = syscalls[num](); 
    if ((1 << num) & p->trace_mask) {
      printf(ANSI_FMT(" -> %d\n", ANSI_FG_CYAN), ret);
    }
    p->trapframe->a0 = ret;
  } else {
    printf("%d %s: unknown sys call %d\n", p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}