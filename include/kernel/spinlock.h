#pragma once
#include "common/types.h"
#include "common/log.h"
#include "common/stdlib.h"

#define SPINLOCK_CPU_TRACE_SIZE 15
#define SPINLOCK_CPU_TRACE_INFO_SIZE 15
struct spinlock_cpu_trace {
  char info[SPINLOCK_CPU_TRACE_INFO_SIZE];
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
  uint16 cur, len;
  struct spinlock_cpu_trace trace[SPINLOCK_CPU_TRACE_SIZE];
  #endif
};

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
#define RECORD_INFO(lk, tp) \
  (lk)->cur = ((lk)->cur + 1) % SPINLOCK_CPU_TRACE_SIZE; \
  snprintf((lk)->trace[(lk)->cur].info, SPINLOCK_CPU_TRACE_INFO_SIZE, "%s:%d", __func__, __LINE__); \
  if ((lk)->len < SPINLOCK_CPU_TRACE_SIZE) { \
    (lk)->len++; \
  } \
  (lk)->trace[(lk)->cur].type = tp; \
  (lk)->trace[(lk)->cur].cpuid = cpuid()

#define ACQUIRE(lk) do { \
  acquire(lk, __func__, __LINE__); \
  RECORD_INFO(lk, 1); \
} while(0)

#define RELEASE(lk) do { \
  release(lk, __func__, __LINE__); \
  RECORD_INFO(lk, 0); \
} while(0)
#else
#define ACQUIRE(lk) acquire(lk, __func__, __LINE__)
#define RELEASE(lk) release(lk, __func__, __LINE__)
#endif