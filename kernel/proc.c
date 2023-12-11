#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"
#include "kernel/spinlock.h"
#include "kernel/proc.h"
#include "kernel/defs.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct kpagetable_wrapper kpagetable_pool[NPROC / 2];
struct spinlock kpool_lock;

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// initialize the proc table at boot time.
void
procinit(void)
{
  struct proc *p;
  
  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  initlock(&kpool_lock, "kpool_lock");
  for(p = proc; p < &proc[NPROC]; p++) {
    char buf[10];
    sprintf(buf, "proc %d", (int)(p - proc));
    initlock(&p->lock, buf);
    p->kstackbase = KSTACK((int) (p - proc));
    // this page is for kernel stack, which will not be freed while OS running.
    p->kstackpage = kalloc();
  }

  for(int i = 0; i < NELEM(kpagetable_pool); i++) {
    struct kpagetable_wrapper *kpw = &kpagetable_pool[i];
    kpw->occupied = 0;
    kpw->page = uclean_kpagetable();
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu*
mycpu(void) {
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc*
myproc(void) {
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int
allocpid() {
  int pid;
  
  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;
  p->random_seed = ticks;

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Allocate a usyscall page.
  if((p->usyscall = (struct usyscall *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  } else {
    p->usyscall->pid = p->pid;
    p->usyscall->cwd[0] = '/';
    p->usyscall->cwd[1] = 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  } 
  p->kpagetable = proc_kpagetable(p);
  if(p->kpagetable == 0) {
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstackbase + PGSIZE;

  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->usyscall)
    kfree((void*)p->usyscall);
  p->usyscall = 0;
  proc_free_pagetable(p);
  proc_free_kpagetable(p, 0);
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
  addrinfo_clear(p->addrinfo);
}

// Create a user page table for a given process,
// with no user memory, but with trampoline pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;
  // An empty page table.
  pagetable = upagecreate();
  if(pagetable == 0)
    return 0;
  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if(mappages(pagetable, TRAMPOLINE, PGSIZE, (uint64)trampoline, PTE_R | PTE_X) < 0){
    goto bad;
  }
  // map the trapframe just below TRAMPOLINE, for trampoline.S.
  if(mappages(pagetable, TRAPFRAME, PGSIZE, (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    goto bad;
  }
  if(mappages(pagetable, USYSCALL, PGSIZE, (uint64)(p->usyscall), PTE_R | PTE_U) < 0){
    goto bad;
  }
  return pagetable;
bad:
  // free kpgtbl
  free_pagetable(pagetable, 0);
  return 0;
}

// Create a user kpagetable for a given process,
// with no user memory, but with trampoline pages.
extern char etext[];  // kernel.ld sets this to end of kernel code.
pagetable_t
proc_kpagetable(struct proc *p)
{
  struct kpagetable_wrapper *kpw;

  acquire(&kpool_lock);
  int i;
  for (i = 0; i < NELEM(kpagetable_pool); i++) {
    if (!kpagetable_pool[i].occupied) {
      kpw = &kpagetable_pool[i];
      break;
    }
  }
  if (i == NELEM(kpagetable_pool)) 
    goto bad;
  
  // map the trapframe just below TRAMPOLINE, for trampoline.S.
  if(mappages(kpw->page, TRAPFRAME, PGSIZE, (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    goto bad;
  }
  if(mappages(kpw->page, USYSCALL, PGSIZE, (uint64)(p->usyscall), PTE_R) < 0){
    upageunmap(kpw->page, TRAPFRAME, 1, 0);
    goto bad;
  }
  if(mappages(kpw->page, p->kstackbase, PGSIZE, (uint64)p->kstackpage, PTE_R | PTE_W) < 0){
    upageunmap(kpw->page, TRAPFRAME, 1, 0);
    upageunmap(kpw->page, USYSCALL, 1, 0);
    goto bad;
  }
  kpw->occupied = 1;
  release(&kpool_lock);
  return kpw->page;
bad:
  release(&kpool_lock);
  return 0;
}

pagetable_t 
uclean_kpagetable()
{
  pagetable_t kpgtbl;

  kpgtbl = (pagetable_t) kalloc();
  if (kpgtbl == 0) 
    return 0;
  memset(kpgtbl, 0, PGSIZE);

  // uart registers
  if (mappages(kpgtbl, UART0, PGSIZE, UART0, PTE_R | PTE_W) != 0) {
    goto bad;
  }
  // virtio mmio disk interface
  if (mappages(kpgtbl, VIRTIO0, PGSIZE, VIRTIO0, PTE_R | PTE_W) != 0) {
    goto bad;
  }
  // PLIC
  if (mappages(kpgtbl, PLIC, 0x400000, PLIC, PTE_R | PTE_W) != 0) {
    goto bad;
  }
  // map kernel text executable and read-only.
  if (mappages(kpgtbl, KERNBASE, (uint64)etext-KERNBASE, KERNBASE, PTE_R | PTE_X) != 0) {
    goto bad;
  }
  // map kernel data and the physical RAM we'll make use of.
  if (mappages(kpgtbl, (uint64)etext, PHYSTOP-(uint64)etext, (uint64)etext, PTE_R | PTE_W) != 0) {
    goto bad;
  }
  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  if (mappages(kpgtbl, TRAMPOLINE, PGSIZE, (uint64)trampoline, PTE_R | PTE_X) != 0) {
    goto bad;
  }
  return kpgtbl;
bad:
  // free kpgtbl
  free_pagetable(kpgtbl, 0);
  return 0;
}

void 
proc_free_pagetable(struct proc *p)  
{
  if (!p->pagetable)
    return;
  // also from proc memory
  upageunmap(p->pagetable, PROC_CODE_BASE(p), PROC_CODE_PAGES(p), 1);
  upageunmap(p->pagetable, PROC_STACK_BASE(p), PROC_STACK_PAGES(p), 1);
  upageunmap(p->pagetable, PROC_HEAP_BASE(p), PROC_HEAP_PAGES(p), 1);
  free_pagetable(p->pagetable, 0);
  p->pagetable = 0;
}

void 
proc_free_kpagetable(struct proc *p, uint64 satp)  
{
  if (!p->kpagetable)
    return;
  if (satp != 0) {
    w_satp(satp);
    sfence_vma();
  }
  upageunmap(p->kpagetable, PROC_CODE_BASE(p), PROC_CODE_PAGES(p), 0);
  upageunmap(p->kpagetable, PROC_STACK_BASE(p), PROC_STACK_PAGES(p), 0);
  upageunmap(p->kpagetable, PROC_HEAP_BASE(p), PROC_HEAP_PAGES(p), 0);
  upageunmap(p->kpagetable, TRAPFRAME, 1, 0);
  upageunmap(p->kpagetable, USYSCALL, 1, 0);
  upageunmap(p->kpagetable, p->kstackbase, 1, 0);
  free_pagetable(p->kpagetable, PGTBLFREE_JUST_NO_LEAF);

  acquire(&kpool_lock);
  int i;
  for (i = 0; i < NELEM(kpagetable_pool); i++) {
    if (kpagetable_pool[i].page == p->kpagetable) {
      kpagetable_pool[i].occupied = 0;
      break;
    }
  }
  release(&kpool_lock);

  if (i == NELEM(kpagetable_pool))
    panic("can not find [kpagetable wrapper] while free kpagetable");
  p->kpagetable = 0;
}


// a user program that calls exec("/init")
// od -t xC initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

int proc_rand(struct proc *p) {
  return rand_r(&p->random_seed);
}

// Set up first user process.
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;
  
  // allocate one user page and copy init's instructions
  // and data into it.
  char *mem;
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(p->pagetable, 0, PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_X|PTE_U);
  mappages(p->kpagetable, 0, PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_X);
  chg_page_ref((uint64)mem, 2);
  memmove(mem, initcode, sizeof(initcode));

  p->addrinfo.vm_offset = 0;
  p->addrinfo.program_sz = sizeof(initcode);

  // prepare for the very first "return" from kernel to user.
  p->trapframe->epc = 0;      // user program counter
  p->trapframe->sp = PGSIZE;  // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&p->lock);
}

// Grow or shrink user heap memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint64 heap_end;
  struct proc *p = myproc();

  heap_end = p->addrinfo.heap_end;
  if(n > 0){
    if (heap_end + n > p->addrinfo.stack_bottom) 
      return -1;
  } else if(n < 0){
    if (heap_end + n > heap_end || heap_end + n < p->addrinfo.heap_start) 
      return -1;
    if((PGROUNDUP(heap_end) != PGROUNDUP(heap_end + n)) && 
        uvmdealloc(p, heap_end, heap_end + n) == -1) {
      return -1;
    }
  }
  p->addrinfo.heap_end += n;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child. Both table and memory.
  if(uvmcopy(p, np) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->addrinfo = p->addrinfo;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  // 将trace_mask拷贝到子进程
  np->trace_mask = p->trace_mask;

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);

  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);
  
  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(uint64 addr)
{
  struct proc *np;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(np = proc; np < &proc[NPROC]; np++){
      if(np->parent == p){
        // make sure the child isn't still in exit() or swtch().
        acquire(&np->lock);

        havekids = 1;
        if(np->state == ZOMBIE){
          // Found one.
          pid = np->pid;
          if(addr != 0 && copyout(0, addr, (char *)&np->xstate,
                                  sizeof(np->xstate)) < 0) {
            release(&np->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(np);
          release(&np->lock);
          release(&wait_lock);
          return pid;
        }
        release(&np->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || p->killed){
      release(&wait_lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &wait_lock);  //DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
extern pagetable_t kernel_pagetable;
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  
  c->proc = 0;
  for(;;){
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();

    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;

        w_satp(MAKE_SATP((uint64)p->kpagetable));
        sfence_vma();
        swtch(&c->context, &p->context);
        w_satp(MAKE_SATP((uint64)kernel_pagetable));
        sfence_vma();

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
      release(&p->lock);
    }
  }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
  release(&myproc()->lock);

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    first = 0;
    fsinit(ROOTDEV);
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock);  //DOC: sleeplock1
  release(lk);

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&p->lock);
  acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()){
      acquire(&p->lock);
      if(p->state == SLEEPING && p->chan == chan) {
        p->state = RUNNABLE;
      }
      release(&p->lock);
    }
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        // Wake process from sleep().
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  if(user_dst){
    return copyout(0, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  if(user_src){
    return copyin(dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  printf("pid  state  proc          pages\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";

    printf("%d    %s", p->pid, state);
    for (int i = 0; i < 7 - strlen(state); i++)
      consputc(' ');
    printf("%s", p->path);
    if (strlen(p->path) >= 14) consputc(' ');
    else {
      for (int i = 0; i < 14 - strlen(p->path); i++)
        consputc(' ');
    }
    printf("%d", PROC_PAGES(p));
    printf("\n");
  }
}
