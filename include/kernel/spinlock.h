#pragma once
#include "common/types.h"
#include "common/log.h"
#include "common/stdlib.h"

#define SPINLOCK_CPU_TRACE_SIZE 16
struct spinlock_cpu_trace {
  char info[100];
  uint8 cpuid;
  uint8 type; // 1 for acquire, 0 for release
};

// Mutual exclusion lock.
struct spinlock {
  uint locked;       // Is the lock held?
  /* For debugging: */
  char name[20];        // Name of lock.
  struct cpu *cpu;   // The cpu holding the lock.
  #ifdef DEBUG
  uint16 l, r, len;
  struct spinlock_cpu_trace trace[SPINLOCK_CPU_TRACE_SIZE];
  #endif
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
  // if (!lock_blacklist(lk)) 
  //   LOG("cpu %d acquire lock %s\n", cpuid(), (lk)->name); 
  return 0;
}

#ifdef DEBUG
#define ACQUIRE(lk) do { \
  (lk)->r = ((lk)->r + 1) % SPINLOCK_CPU_TRACE_SIZE; \
  sprintf((lk)->trace[(lk)->r].info, "[%s:%d %s]", __FILE__, __LINE__, __func__); \
  if ((lk)->len < SPINLOCK_CPU_TRACE_SIZE) { \
    (lk)->len++; \
  } else { \
    (lk)->l = ((lk)->l + 1) % SPINLOCK_CPU_TRACE_SIZE; \
  } \
  (lk)->trace[(lk)->r].type = 1; \
  (lk)->trace[(lk)->r].cpuid = cpuid(); \
  acquire(lk); \
} while(0)

#define RELEASE(lk) do { \
  (lk)->r = ((lk)->r + 1) % SPINLOCK_CPU_TRACE_SIZE; \
  sprintf((lk)->trace[(lk)->r].info, "[%s:%d %s]", __FILE__, __LINE__, __func__); \
  if ((lk)->len < SPINLOCK_CPU_TRACE_SIZE) { \
    (lk)->len++; \
  } else { \
    (lk)->l = ((lk)->l + 1) % SPINLOCK_CPU_TRACE_SIZE; \
  } \
  (lk)->trace[(lk)->r].type = 0; \
  (lk)->trace[(lk)->r].cpuid = cpuid(); \
  release(lk); \
} while(0)
#else
#define ACQUIRE(lk) acquire(lk, NULL)
#define RELEASE(lk) release(lk, NULL)
#endif