#pragma once

#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/riscv.h"
#include "kernel/spinlock.h"

// Saved registers for kernel context switches.
struct context {
  uint64 ra;
  uint64 sp;

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

// Per-CPU state.
struct cpu {
  struct proc *proc;          // The process running on this cpu, or null.
  struct context context;     // swtch() here to enter scheduler().
  int noff;                   // Depth of push_off() nesting.
  int intena;                 // Were interrupts enabled before push_off()?
};

extern struct cpu cpus[NCPU];

// per-process data for the trap handling code in trampoline.S.
// sits in a page by itself just under the trampoline page in the
// user page table. not specially mapped in the kernel page table.
// the sscratch register points here.
// uservec in trampoline.S saves user registers in the trapframe,
// then initializes registers from the trapframe's
// kernel_sp, kernel_hartid, kernel_satp, and jumps to kernel_trap.
// usertrapret() and userret in trampoline.S set up
// the trapframe's kernel_*, restore user registers from the
// trapframe, switch to the user page table, and enter user space.
// the trapframe includes callee-saved user registers like s0-s11 because the
// return-to-user path via usertrapret() doesn't return through
// the entire kernel call stack.
struct trapframe {
  /*   0 */ uint64 kernel_satp;   // kernel page table
  /*   8 */ uint64 kernel_sp;     // top of process's kernel stack
  /*  16 */ uint64 kernel_trap;   // usertrap()
  /*  24 */ uint64 epc;           // saved user program counter
  /*  32 */ uint64 kernel_hartid; // saved kernel tp
  /*  40 */ uint64 ra;
  /*  48 */ uint64 sp;
  /*  56 */ uint64 gp;
  /*  64 */ uint64 tp;
  /*  72 */ uint64 t0;
  /*  80 */ uint64 t1;
  /*  88 */ uint64 t2;
  /*  96 */ uint64 s0;
  /* 104 */ uint64 s1;
  /* 112 */ uint64 a0;
  /* 120 */ uint64 a1;
  /* 128 */ uint64 a2;
  /* 136 */ uint64 a3;
  /* 144 */ uint64 a4;
  /* 152 */ uint64 a5;
  /* 160 */ uint64 a6;
  /* 168 */ uint64 a7;
  /* 176 */ uint64 s2;
  /* 184 */ uint64 s3;
  /* 192 */ uint64 s4;
  /* 200 */ uint64 s5;
  /* 208 */ uint64 s6;
  /* 216 */ uint64 s7;
  /* 224 */ uint64 s8;
  /* 232 */ uint64 s9;
  /* 240 */ uint64 s10;
  /* 248 */ uint64 s11;
  /* 256 */ uint64 t3;
  /* 264 */ uint64 t4;
  /* 272 */ uint64 t5;
  /* 280 */ uint64 t6;
};

enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

struct addrinfo {
  uint64 logical_stack_top;
  uint64 stack_top;
  uint64 stack_bottom;
  uint64 heap_end;
  uint64 heap_start;
  uint64 vm_offset;          // virtual memory offset
  uint64 program_sz;         // program code bytes
};

struct kpagetable_wrapper {
  pagetable_t page;
  int occupied;
};

#define addrinfo_clear(addrinfo) do { \
  addrinfo.logical_stack_top = 0; \
  addrinfo.stack_top = 0; \
  addrinfo.stack_bottom = 0; \
  addrinfo.heap_end = 0; \
  addrinfo.heap_start = 0; \
  addrinfo.vm_offset = 0; \
  addrinfo.program_sz = 0; \
} while(0)

// Per-process state
struct proc {
  struct spinlock lock;

  // (p)->lock must be held when using these:
  enum procstate state;        // Process state
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  int xstate;                  // Exit status to be returned to parent's wait
  int pid;                     // Process ID

  // wait_lock must be held when using this:
  struct proc *parent;         // Parent process

  // these are private to the process, so (p)->lock need not be held.
  uint64 kstackbase;           // Virtual address of kernel stack
  uint64 *kstackpage;          // kernel stack page
  struct addrinfo addrinfo; 
  struct trapframe *trapframe; // data page for trampoline.S
  struct usyscall *usyscall;   // usyscall page

  pagetable_t pagetable;       // User page table
  pagetable_t kpagetable;      // User kernel page table
  struct context context;      // swtch() here to run process
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)
  char path[MAXPATH];          // executable path (debugging)

  int trace_mask;    // trace系统调用参数
  uint random_seed;
};


#define PROC_CODE_BASE(p) (PGROUNDDOWN((p)->addrinfo.vm_offset))
#define PROC_CODE_END(p) (PGROUNDUP((p)->addrinfo.vm_offset + (p)->addrinfo.program_sz))
#define PROC_CODE_PAGES(p) \
  ((p)->addrinfo.program_sz ? (PROC_CODE_END(p) - PROC_CODE_BASE(p)) / PGSIZE : 0)

#define PROC_STACK_BASE(p) (PGROUNDDOWN((p)->addrinfo.stack_bottom))
#define PROC_STACK_END(p) (PGROUNDUP((p)->addrinfo.stack_top))
#define PROC_STACK_PAGES(p) ((PROC_STACK_END(p) - PROC_STACK_BASE(p)) / PGSIZE)

#define PROC_HEAP_BASE(p) (PGROUNDDOWN((p)->addrinfo.heap_start)) 
#define PROC_HEAP_END(p) (PGROUNDUP((p)->addrinfo.heap_end)) 
#define PROC_HEAP_PAGES(p) ((PROC_HEAP_END(p) - PROC_HEAP_BASE(p)) / PGSIZE)

#define PROC_PAGES(p) (PROC_CODE_PAGES(p) + PROC_HEAP_PAGES(p) + PROC_STACK_PAGES(p))
#define PROC_SZ(p) (PROC_PAGES(p) * PGSIZE)  // Size of process memory (bytes, multiple of page size)