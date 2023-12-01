#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/memlayout.h"
#include "kernel/elf.h"
#include "kernel/riscv.h"
#include "kernel/defs.h"
#include "kernel/fs.h"
#include "kernel/spinlock.h"
#include "kernel/proc.h"

/*
 * the kernel's page table.
 */
pagetable_t kernel_pagetable;

extern char etext[];  // kernel.ld sets this to end of kernel code.

extern char trampoline[]; // trampoline.S

// Make a direct-map page table for the kernel.
pagetable_t
kvmmake(void)
{
  pagetable_t kpgtbl;

  kpgtbl = (pagetable_t) kalloc();
  memset(kpgtbl, 0, PGSIZE);

  // uart registers
  kvmmap(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  kvmmap(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // PLIC
  kvmmap(kpgtbl, PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  kvmmap(kpgtbl, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);
  
  return kpgtbl;
}

// Initialize the one kernel_pagetable
void
kvminit(void)
{
  kernel_pagetable = kvmmake();
}

// Switch h/w page table register to the kernel's page table,
// and enable paging.
void
kvminithart()
{
  w_satp(MAKE_SATP(kernel_pagetable));
  sfence_vma();
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}

// Look up a virtual address, return the physical page address 
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if(va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if(pte == 0)
    return 0;
  if((*pte & PTE_V) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  pa = PTE2PA(*pte);
  return pa;
}

uint64
kvmpa(uint64 va)
{
  return vmpa(myproc()->kpagetable, va);
}

uint64
vmpa(pagetable_t pagetable, uint64 va)
{
  if(va >= MAXVA)
    return 0;

  uint64 off = va % PGSIZE;
  pte_t *pte;
  uint64 pa;

  pte = walk(pagetable, va, 0);
  if(pte == 0)
    panic("vmpa");
  if((*pte & PTE_V) == 0)
    panic("vmpa");
  pa = PTE2PA(*pte);
  return pa + off;
}

// add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
void
kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(kpgtbl, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  if(size == 0)
    panic("mappages: size");
  
  a = PGROUNDDOWN(va);
  last = PGROUNDDOWN(va + size - 1);
  for(;;){
    if((pte = walk(pagetable, a, 1)) == 0)
      return -1;
    if(*pte & PTE_V)
      panic("mappages: remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// -1 for error
// -2 for remap
// 0 for success
int
map_onepage(pagetable_t pagetable, uint64 va, uint64 pa, int perm)
{
  uint64 a;
  pte_t *pte;
  a = PGROUNDDOWN(va);
  if((pte = walk(pagetable, a, 1)) == 0)
    return -1;
  if(*pte & PTE_V)
    return -2;
  *pte = PA2PTE(pa) | perm | PTE_V;
  return 0;
}

// Remove npages of mappings starting from va. va must be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
void
upageunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("upageunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0)
      panic("upageunmap: walk");
    if((*pte & PTE_V) == 0) {
      printf("[warning]upageunmap: not mapped\n");
      continue;
    }
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("upageunmap: not a leaf");
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    *pte = 0;
  }
}

// create an empty user page table.
// returns 0 if out of memory.
pagetable_t
upagecreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t) kalloc();
  if(pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// Allocate PTEs and physical memory to grow process from old_addr to new_addr
// need not be page aligned. 
// Returns allocated page number or -1 on error.
int
uvmalloc(struct proc *p, uint64 old_addr, uint64 new_addr)
{
  old_addr = PGROUNDDOWN(old_addr);
  new_addr = PGROUNDUP(new_addr);
  if(new_addr <= old_addr)
    return 0;

  int ret = 0;
  int kret = 0;
  char *mem;
  for(uint64 a = old_addr; a < new_addr; a += PGSIZE){
    mem = kalloc();
    if(mem == 0)
      goto err;
    memset(mem, 0, PGSIZE);

    int rc = map_onepage(p->pagetable, a, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U);
    if (rc == -1) {
      goto err;
    } else if (rc == -2) {
      continue;
    } else {
      ret += 1;
    }
    rc = map_onepage(p->kpagetable, a, (uint64)mem, PTE_W|PTE_X|PTE_R);
    if (rc == -1) {
      goto err;
    } else if (rc == -2) {
      continue;
    } else {
      kret += 1;
    }
  }
  if (ret != kret) 
    printf("uvmalloc: ret != kret\n");
  return ret;
err:
  if (mem)
    kfree(mem);
  upageunmap(p->pagetable, old_addr, ret, 1);
  upageunmap(p->kpagetable, old_addr, kret, 0);
  return -1;
}

// Deallocate user pages to bring the process size from old_addr to
// new_addr. oldsz and newsz need not be page-aligned.
// Returns deallocated page number or -1 on error. 
int
uvmdealloc(struct proc *p, uint64 old_addr, uint64 new_addr)
{
  if(PGROUNDUP(new_addr) >= PGROUNDUP(old_addr))
    return 0;

  int npages = (PGROUNDUP(old_addr) - PGROUNDUP(new_addr)) / PGSIZE;
  upageunmap(p->pagetable, PGROUNDUP(new_addr), npages, 1);
  upageunmap(p->kpagetable, PGROUNDUP(new_addr), npages, 0);
  return npages;
}

void
free_pagetable_impl(pagetable_t pagetable, int assert_noleaf, int freeleaf)
{
  if (!pagetable) return;
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if(pte & PTE_V){
      uint64 child = PTE2PA(pte);
      if ((pte & (PTE_R|PTE_W|PTE_X)) != 0) { // next is leaf
        if (assert_noleaf) {
          panic("free_pagetable: leaf");
        }
        if (freeleaf) {
          kfree((void *)child);
        }
      } else { // not leaf
        // this pte points to a lower-level page table.
        free_pagetable_impl((pagetable_t)child, assert_noleaf, freeleaf);
        pagetable[i] = 0;
      }
    } 
  }
  kfree((void*)pagetable);
}

// Recursively free page-table pages.
// All leaf mappings must already have been removed if assert_noleaf is set.
void
free_pagetable_pages(pagetable_t pagetable, int assert_noleaf)
{
  free_pagetable_impl(pagetable, assert_noleaf, 0);
}

// Free a page-table and its mapped memory
void
free_pagetable_all(pagetable_t pagetable)
{
  free_pagetable_impl(pagetable, 0, 1);
}


// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
int
uvmcopy(struct proc *father, struct proc *child)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;
  int idx;

  uint64 start_addrs[3] = {PROC_CODE_BASE(father), PROC_STACK_BASE(father), PROC_HEAP_BASE(father)};
  uint64 end_addrs[3] = {PROC_CODE_END(father), PROC_STACK_END(father), PROC_HEAP_END(father)};

  for (idx = 0; idx < 3; idx++) {
    for(i = start_addrs[idx]; i < end_addrs[idx]; i += PGSIZE){
      if((pte = walk(father->pagetable, i, 0)) == 0)
        panic("uvmcopy: pte should exist");
      if((*pte & PTE_V) == 0)
        panic("uvmcopy: page not present");
      pa = PTE2PA(*pte);
      flags = PTE_FLAGS(*pte);
      if((mem = kalloc()) == 0)
        goto err;
      memmove(mem, (char*)pa, PGSIZE);
      if(mappages(child->pagetable, i, PGSIZE, (uint64)mem, flags) != 0){
        kfree(mem);
        goto err;
      }

      if((pte = walk(father->kpagetable, i, 0)) == 0)
        panic("uvmcopy: pte should exist");
      if((*pte & PTE_V) == 0)
        panic("uvmcopy: page not present");
      flags = PTE_FLAGS(*pte);
      if(mappages(child->kpagetable, i, PGSIZE, (uint64)mem, flags) != 0){
        upageunmap(child->pagetable, i, 1, 0);
        kfree(mem);
        goto err;
      }
    }
  }
  return 0;

 err:
  if (idx > 0) {
    upageunmap(child->pagetable, start_addrs[0], (end_addrs[0] - start_addrs[0]) / PGSIZE, 1);
    upageunmap(child->kpagetable, start_addrs[0], (end_addrs[0] - start_addrs[0]) / PGSIZE, 0);
  }
  if (idx > 1) {
    upageunmap(child->pagetable, start_addrs[1], (end_addrs[1] - start_addrs[1]) / PGSIZE, 1);
    upageunmap(child->kpagetable, start_addrs[1], (end_addrs[1] - start_addrs[1]) / PGSIZE, 0);
  }
  upageunmap(child->pagetable, start_addrs[idx], (i - start_addrs[1]) / PGSIZE, 1);
  upageunmap(child->kpagetable, start_addrs[idx], (i - start_addrs[1]) / PGSIZE, 0);
  return -1;
}

// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
void
upageclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  
  pte = walk(pagetable, va, 0);
  if(pte == 0)
    panic("upageclear");
  *pte &= ~PTE_U;
}

int 
uaddrvalid(struct proc *p, uint64 va) 
{
  uint64 start_addrs[3] = {PROC_CODE_BASE(p), PROC_STACK_BASE(p), PROC_HEAP_BASE(p)};
  uint64 end_addrs[3] = {PROC_CODE_END(p), PROC_STACK_END(p), PROC_HEAP_END(p)};
  for (int i = 0; i < 3; i++) {
    if (va >= start_addrs[i] && va < end_addrs[i]) {
      return 1;
    }
  }
  return 0;
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  if (pagetable) {
    uint64 n, va0, pa0;

    while(len > 0){
      va0 = PGROUNDDOWN(dstva);
      pa0 = walkaddr(pagetable, va0);
      if(pa0 == 0)
        return -1;
      n = PGSIZE - (dstva - va0);
      if(n > len)
        n = len;
      memmove((void *)(pa0 + (dstva - va0)), src, n);

      len -= n;
      src += n;
      dstva = va0 + PGSIZE;
    }
    return 0;
  } else {
    struct proc *p = myproc();
    if (!uaddrvalid(p, dstva) || !uaddrvalid(p, dstva + len - 1))
      return -1;
    memmove((void *)dstva, src, len);
    return 0;
  }
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
int
copyin(char *dst, uint64 srcva, uint64 len)
{
  struct proc *p = myproc();
  if (!uaddrvalid(p, srcva) || !uaddrvalid(p, srcva + len - 1))
    return -1;
  memmove(dst, (void *)srcva, len);
  return 0;
}

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
int
copyinstr(char *dst, uint64 srcva, uint64 max)
{
  struct proc *p = myproc();
  char *src = (char *)srcva;
  int gotnull = 0;
  while(max-- > 0) {
    char t;
    if (!uaddrvalid(p, (uint64)src)) {
      return -1;
    }
    t = *dst++ = *src++;
    if (t == 0) {
      gotnull = 1;
      break;
    }
  }
  if (gotnull)
    return 0;
  else
    return -1;
}

void
xprint(pagetable_t pagetable, uint64 srcva, uint64 bytes) 
{
  char *pa = (char *)vmpa(pagetable, srcva);
  printf("xprint:\n");
  int idx = 0;
  while (bytes > 0) {
    int n = bytes >= 16 ? 16 : bytes;
    printf("-- ");
    for (int i = idx; i < idx + n; i++) {
      printf("%x ", pa[i]);
    }
    printf("\n");
    idx += n;
    bytes -= n;
  }
}

static uint64 vmprint_buf[2];
static int vmignore(int level, int i) 
{
  // 0 128 0 to
  // 2 64  0 ignore
  /*
  means...
  0 96 0 to 0 512 512
  all 1 . .
  2 0 0 to 2 64 0
  */
  if (level == 1) {
    if (i == 1) return 1;
  } else if (level == 2) {
    if (vmprint_buf[0] == 0 && i >= 96) return 1;
    if (vmprint_buf[0] == 2 && i < 64) return 1;
  }
  return 0;
}
/**
 * @param pagetable 所要打印的页表
 * @param level 页表的层级
 */
static void
_vmprint(pagetable_t pagetable, int level, int k){
  // there are 2^9 = 512 PTEs in a page table.
  for (int i = 0; i < 512; i++) {
    pte_t pte = pagetable[i];
    // PTE_V is a flag for whether the page table is valid
    if(pte & PTE_V){
      if (k && vmignore(level, i)) 
        continue;
      for (int j = 0; j < level; j++){
        if (j) printf(" ");
        printf("..");
      }
      uint64 child = PTE2PA(pte);
      printf("%d", i);
      if (level == 3)
        printf("(0x%lx)", E2VA(vmprint_buf[0], vmprint_buf[1], i));
      printf(": pte 0x%lx", pte);
      // print pte flag
      printf("[");
      if (pte & PTE_V) printf("V");
      if (pte & PTE_R) printf("R");
      if (pte & PTE_W) printf("W");
      if (pte & PTE_X) printf("X");
      if (pte & PTE_U) printf("U");
      if (pte & PTE_A) printf("A");
      printf("]");
      printf(" pa 0x%lx\n", child);
      if((pte & (PTE_R|PTE_W|PTE_X)) == 0){
        // this PTE points to a lower-level page table.
        vmprint_buf[level - 1] = i;
        _vmprint((pagetable_t)child, level + 1, k);
      }
    }
  }
}

/**
 * @brief vmprint 打印页表
 * @param pagetable 所要打印的页表
 */
void
vmprint(pagetable_t pagetable){
  if (!pagetable)
    return;
  printf("page table %p\n", pagetable);
  _vmprint(pagetable, 1, 0);
}

/**
 * @brief vmprintk 打印页表, 但不包括内核映射
 * @param kpagetable 所要打印的内核页表
 */
void
vmprintk(pagetable_t kpagetable){
  if (!kpagetable)
    return;
  printf("kpage table %p\n", kpagetable);
  _vmprint(kpagetable, 1, 1);
}

static void 
_page_info(pagetable_t pagetable, int *cnt, int *size)
{
  if (!pagetable) return;
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if(pte & PTE_V){
      uint64 child = PTE2PA(pte);
      if ((pte & (PTE_R|PTE_W|PTE_X)) != 0) { // next is leaf
        *cnt += 1;
      } else { // not leaf
        // this pte points to a lower-level page table.
        _page_info((pagetable_t)child, cnt, size);
      }
    } 
  }
  *size += PGSIZE;
}

void
print_page_info(pagetable_t pagetable) 
{
  if (!pagetable)
    return;
  int ret = 0, size = 0;
  _page_info(pagetable, &ret, &size);
  printf("mapped pages: %d, occupy space: %d kb\n", ret, size/1024);
}

void 
print_pte_info(pagetable_t pagetable, uint64 va)
{
  if (!pagetable)
    return;
  pte_t *ptep = walk(pagetable, va, 0);
  if (!ptep)
    printf("not pte available\n");
  pte_t pte = *ptep;
  printf("pte 0x%lx", pte);
  // print pte flag
  printf("[");
  if (pte & PTE_V) printf("V");
  if (pte & PTE_R) printf("R");
  if (pte & PTE_W) printf("W");
  if (pte & PTE_X) printf("X");
  if (pte & PTE_U) printf("U");
  if (pte & PTE_A) printf("A");
  printf("]");  
  printf(" pa 0x%lx\n", PTE2PA(pte));
}