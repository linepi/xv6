// Sleeping locks

#include "kernel/defs.h"
#include "kernel/spinlock.h"
#include "kernel/proc.h"
#include "kernel/sleeplock.h"

void
initsleeplock(struct sleeplock *lk, char *name)
{
  initlock(&lk->lk, "sleep lock");
  lk->name = name;
  lk->locked = 0;
  lk->pid = 0;
}

void
acquiresleep(struct sleeplock *lk)
{
  ACQUIRE(&lk->lk);
  while (lk->locked) {
    sleep(lk, &lk->lk);
  }
  lk->locked = 1;
  lk->pid = myproc()->pid;
  RELEASE(&lk->lk);
}

void
releasesleep(struct sleeplock *lk)
{
  ACQUIRE(&lk->lk);
  lk->locked = 0;
  lk->pid = 0;
  wakeup(lk);
  RELEASE(&lk->lk);
}

int
holdingsleep(struct sleeplock *lk)
{
  int r;
  
  ACQUIRE(&lk->lk);
  r = lk->locked && (lk->pid == myproc()->pid);
  RELEASE(&lk->lk);
  return r;
}



