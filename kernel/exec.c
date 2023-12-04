#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"
#include "kernel/spinlock.h"
#include "kernel/proc.h"
#include "kernel/defs.h"
#include "kernel/elf.h"

static int loadseg(pagetable_t pagetable, uint64 va, struct inode *ip, uint offset, uint sz);

int
exec(char *path, char **argv)
{
  char *s, *last;
  int i, off;
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  uint64 argc, sp, stackbase, ustack[MAXARG];

  struct proc *p = myproc();
  struct proc p_bak = *p;
  struct proc p_new = *p;
  p_new.pagetable = 0;
  p_new.kpagetable = 0;
  addrinfo_clear(p_new.addrinfo);

  // debuging
  // printf("%s called exec(\"%s\", [", p->name, path);
  // for(char **i = &argv[1]; *i; i += sizeof(char*)){
  //   printf("\"%s\"", *i);
  //   if(*(i + sizeof(char*))){
  //     printf(", ");
  //   }
  // }
  // printf("])\n");
  // debuging

  begin_op();

  if((ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);

  // Check ELF header
  if(readi(ip, 0, (uint64)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;
  if(elf.magic != ELF_MAGIC)
    goto bad;

  if((p_new.pagetable = proc_pagetable(p)) == 0)
    goto bad;
  if((p_new.kpagetable = proc_kpagetable(p)) == 0)
    goto bad;

  // Load program into memory.
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
   if((uvmalloc(&p_new, 
          p_new.addrinfo.program_sz + ph.vaddr, 
          p_new.addrinfo.program_sz + ph.vaddr + ph.memsz
        )) == -1)
      goto bad;
    p_new.addrinfo.vm_offset = ph.vaddr;
    p_new.addrinfo.program_sz += ph.memsz;
    if(loadseg(p_new.pagetable, p_new.addrinfo.vm_offset, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }
  iunlockput(ip);
  end_op();
  ip = 0;

  // Allocate two pages at the next page boundary.
  // Use the second as the user stack.
  sp = STACK_TOP(p);
  stackbase = sp - PGSIZE;
  if((uvmalloc(&p_new, stackbase, sp)) == -1)
    goto bad;
  p_new.addrinfo.stack_top = sp;
  p_new.addrinfo.stack_bottom = stackbase;

  // Push argument strings, prepare rest of stack in ustack.
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    sp -= strlen(argv[argc]) + 1;
    sp -= sp % 16; // riscv sp must be 16-byte aligned
    if(sp < stackbase)
      goto bad;
    if(copyout(p_new.pagetable, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[argc] = sp;
  }
  ustack[argc] = 0;

  // push the array of argv[] pointers.
  sp -= (argc+1) * sizeof(uint64);
  sp -= sp % 16;
  if(sp < stackbase)
    goto bad;
  if(copyout(p_new.pagetable, sp, (char *)ustack, (argc+1)*sizeof(uint64)) < 0)
    goto bad;

  // arguments to user main(argc, argv)
  // argc is returned via the system call return
  // value, which goes in a0.
  p->trapframe->a1 = sp;

  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(p_new.name, last, sizeof(p_new.name));
    
  proc_free_pagetable(&p_bak);
  proc_free_kpagetable(&p_bak, MAKE_SATP(p_new.kpagetable));
  // Commit to the user image.
  p_new.trapframe->epc = elf.entry;  // initial program counter = main
  p_new.trapframe->sp = sp; // initial stack pointer
  p_new.addrinfo.heap_start = HEAP_START(p);
  p_new.addrinfo.heap_end = p_new.addrinfo.heap_start;
  *p = p_new;

  return argc; // this ends up in a0, the first argument to main(argc, argv)

 bad:
  proc_free_pagetable(&p_new);
  proc_free_kpagetable(&p_new, 0);
  if(ip){
    iunlockput(ip);
    end_op();
  }
  return -1;
}

// Load a program segment into pagetable at virtual address va.
// the pages from va to va+sz must already be mapped.
// Returns 0 on success, -1 on failure.
static int
loadseg(pagetable_t pagetable, uint64 va, struct inode *ip, uint offset, uint sz)
{
  uint i = 0, n;
  uint64 pa;

  while (sz > 0) {
    pa = walkaddr(pagetable, va);
    pa += va % PGSIZE;
    if(sz < PGSIZE) {
      n = sz;
    } else {
      n = PGSIZE;
    }
    if (PGROUNDUP(va) - va != 0 && PGROUNDUP(va) - va < n) 
      n = PGROUNDUP(va) - va;
    if(readi(ip, 0, (uint64)pa, offset + i, n) != n)
      return -1;
    sz -= n;
    i += n;
    va += n;
  }

  return 0;
}
