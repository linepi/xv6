#pragma once
#include "kernel/types.h"
// Mutual exclusion lock.
struct spinlock {
  uint locked;       // Is the lock held?

  // For debugging:
  char name[20];        // Name of lock.
  struct cpu *cpu;   // The cpu holding the lock.
};

