#pragma once
#include "common/types.h"
// Mutual exclusion lock.
struct spinlock {
  uint locked;       // Is the lock held?

  // For debugging:
  char name[20];        // Name of lock.
  struct cpu *cpu;   // The cpu holding the lock.
};

void acquire(struct spinlock *lk);
void release(struct spinlock *lk);
int strcmp(const char *p, const char *q);
int bscanf(const char *buffer, const char *format, ...);

static inline int lock_blacklist(struct spinlock *lk) {
  if (strcmp(lk->name, "kmem") == 0 ||
      strcmp(lk->name, "kpage_ref") == 0 ||
      strcmp(lk->name, "pr") == 0 ||
      strcmp(lk->name, "time") == 0 ||
      strcmp(lk->name, "uart") == 0)
    return 1;
  return 1;
}

#define ACQUIRE(lk) do { \
  if (!lock_blacklist(lk)) \
    printf("cpu %d acquire lock %s\n", cpuid(), (lk)->name); \
  acquire(lk); \
} while(0)

#define RELEASE(lk) do { \
  release(lk); \
  if (!lock_blacklist(lk)) \
    printf("cpu %d release lock %s\n", cpuid(), (lk)->name); \
} while(0)
