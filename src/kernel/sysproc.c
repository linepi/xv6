#include "common/types.h"
#include "kernel/riscv.h"
#include "kernel/defs.h"
#include "kernel/spinlock.h"
#include "kernel/proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 ret;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  ret = myproc()->addrinfo.heap_end;
  if(growproc(n) < 0)
    return -1;
  return ret;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  ACQUIRE(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      RELEASE(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  RELEASE(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  ACQUIRE(&tickslock);
  xticks = ticks;
  RELEASE(&tickslock);
  return xticks;
}

uint64
sys_trace(void)
{
  // 获取系统调用的参数
  argint(0, &(myproc()->trace_mask));
  return 0;
}

uint64
sys_pgaccess(void)
{
  uint64 addr; // 虚拟页起始地址
  int num;	   // 待检测虚拟页个数
  uint64 mask; // 待写入用户空间的buf
	
  // 得到3个参数
  if(argaddr(0, &addr) < 0)
    return -1;
  if(argint(1, &num) < 0)
    return -1;
  if(argaddr(2, &mask) < 0)
    return -1;
  // mask只有64位，所有待检测页最大个数为64
  int limit = 64;
  if(num > limit)
    return -1;
  pagetable_t pagetable = myproc()->pagetable;

  uint64 bufmask = 0;
  // 遍历所有页
  for(int i = 0; i < num; i++){
    uint64 va = addr + i*PGSIZE;
    pte_t *pte = walk(pagetable, va, 0); // 得到PTE
    if(*pte & PTE_A){
      bufmask |= (1 << i); // 设置PTE
      *pte &= ~PTE_A;	   // 清空PTE_A
    }
  }
  // 写回用户空间
  copyout(0, mask, (char *)&bufmask, num % 8 == 0 ? num / 8 : num / 8 + 1);
  return 0;
}