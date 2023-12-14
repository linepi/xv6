// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole pages.
#include "common/types.h"
#include "common/stdlib.h"
#include "kernel/memlayout.h"
#include "kernel/spinlock.h"
#include "kernel/riscv.h"
#include "kernel/defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel, defined by kernel.ld.


struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  int max_pagenum;
  uint64 start;
  int inited;
} kmem;

struct {
  struct spinlock lock;
  uchar *v;
} kpage_ref;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&kpage_ref.lock, "kpage_ref");
  kmem.freelist = 0;
  kmem.max_pagenum = (PHYSTOP - (uint64)end) / (PGSIZE + 1);
  kpage_ref.v = (uchar *)end;
  kmem.start = PGROUNDUP((uint64)end + kmem.max_pagenum);
  
  for (int i = 0; i < kmem.max_pagenum; i++) {
    kpage_ref.v[i] = 1;
  }

  if (PHYSTOP - kmem.start != PGSIZE * kmem.max_pagenum) 
    panic("kinit");
  freerange(end + kmem.max_pagenum, (void*)PHYSTOP);
  kmem.inited = 1;
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  struct run *r = (struct run*)pa;
  if (r == kmem.freelist) {
    printf("[warning] kfree magic error\n");
    return;
  }

  ACQUIRE(&kpage_ref.lock);
  int page_ref = get_page_ref((uint64)pa);
  if (page_ref <= 0) {
    printf("[warning] kfree free page with ref <= 0\n");
    RELEASE(&kpage_ref.lock);
    return;
  }
  inc_page_ref((uint64)pa, -1);
  RELEASE(&kpage_ref.lock);

  if (page_ref == 1) { // actually free
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);
    // if (kmem.inited)
      // printf("kfree: 0x%lx\n", pa);
    ACQUIRE(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    RELEASE(&kmem.lock);
    // printf("free: 0x%lx\n", pa);
  }
}

// Allocate one page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  ACQUIRE(&kmem.lock);
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
  }
  RELEASE(&kmem.lock);

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
    // printf("kalloc: 0x%lx\n", r);
    ACQUIRE(&kpage_ref.lock);
    chg_page_ref((uint64)r, 1);
    RELEASE(&kpage_ref.lock);
  }
  // printf("kalloc: 0x%lx\n", r);
  return (void*)r;
}

int 
kmemleft()
{
  int ret = 0;
  struct run *r = kmem.freelist;
  while (r) {
    ret += PGSIZE;
    if (r == r->next) {
      printf("error memleft\n");
      return 0;
    }
    r = r->next;
  }
  return ret;
}

void 
acquire_page_ref()
{
  ACQUIRE(&kpage_ref.lock);
}

void 
release_page_ref()
{
  RELEASE(&kpage_ref.lock);
}

void 
inc_page_ref(uint64 addr, int cnt)
{
  if (addr < kmem.start || (uint64)addr >= PHYSTOP)
    return;
  int idx = (PGROUNDDOWN(addr) - kmem.start) / PGSIZE;
  int cur = kpage_ref.v[idx];
  if (cnt > 0 && cur + cnt > 255) 
    panic("inc page_ref");
  if (cnt < 0 && cur + cnt < 0)
    panic("dec page_ref");
  kpage_ref.v[idx] += cnt;
}

void 
chg_page_ref(uint64 addr, int to)
{
  if (addr < kmem.start || (uint64)addr >= PHYSTOP)
    return;
  int idx = (PGROUNDDOWN(addr) - kmem.start) / PGSIZE;
  kpage_ref.v[idx] = to;
}

int 
get_page_ref(uint64 addr)
{
  int idx = (PGROUNDDOWN(addr) - kmem.start) / PGSIZE;
  return kpage_ref.v[idx];
}