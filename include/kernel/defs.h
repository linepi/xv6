#pragma once
#include "common/types.h"
#include "kernel/riscv.h"

struct block_buf;
struct context;
struct file;
struct inode;
struct pipe;
struct proc;
struct spinlock;
struct sleeplock;
struct stat;
struct superblock;

// bio.c
void            binit(void);
struct block_buf*     bread(uint, uint);
void            brelease(struct block_buf*);
void            bwrite(struct block_buf*);
void            bpin(struct block_buf*);
void            bunpin(struct block_buf*);

// console.c
void            consoleinit(void);
void            consoleintr(int);
void            consputc(int);

// exec.c
int             exec(char*, char**);

// file.c
struct file*    filealloc(void);
void            fileclose(struct file*);
struct file*    filedup(struct file*);
void            fileinit(void);
int             fileread(struct file*, uint64, int n);
int             filestat(struct file*, uint64 addr);
int             filewrite(struct file*, uint64, int n);

// fs.c
void            fsinit(int);
int             dirlink(struct inode*, char*, uint);
struct inode*   dirlookup(struct inode*, char*, uint*);
struct inode*   ialloc(uint, short);
struct inode*   idup(struct inode*);
void            iinit();
void            ilock(struct inode*);
void            iput(struct inode*);
void            iunlock(struct inode*);
void            iunlockput(struct inode*);
void            iupdate(struct inode*);
int             namecmp(const char*, const char*);
struct inode*   namei(const char*);
struct inode*   nameiparent(const char*, char*);
int             readi(struct inode*, int, uint64, uint, uint);
void            stati(struct inode*, struct stat*);
int             writei(struct inode*, int, uint64, uint, uint);
void            itrunc(struct inode*);
int 						diskleft();

// ramdisk.c
void            ramdiskinit(void);
void            ramdiskintr(void);
void            ramdiskrw(struct block_buf*);

// kalloc.c
void*           kalloc(void);
void            kfree(void *);
void            kinit(void);
int 						kmemleft();
void            inc_page_ref(uint64 addr, int cnt);
void            chg_page_ref(uint64 addr, int to);
int             get_page_ref(uint64 addr);
void 						acquire_page_ref();
void						release_page_ref();

// log.c
void            initlog(int, struct superblock*);
void            log_write(struct block_buf*);
void            begin_op(void);
void            end_op(void);

// pipe.c
int             pipealloc(struct file**, struct file**);
void            pipeclose(struct pipe*, int);
int             piperead(struct pipe*, uint64, int);
int             pipewrite(struct pipe*, uint64, int);

// printf.c & sprintf.c
void            printf(const char*, ...);
void            pure_printf(const char*, ...);
void            panic(const char*, ...) __attribute__((noreturn));
void            assert(int);
void            printfinit(void);
void            backtrace(int user, int lineinfo);

// proc.c
int             cpuid(void);
void            exit(int);
int             fork(void);
int             growproc(int);
void            proc_mapstacks(pagetable_t);
pagetable_t     proc_pagetable(struct proc *);
pagetable_t     proc_kpagetable(struct proc *);
pagetable_t     uclean_kpagetable();
int             kill(int);
struct cpu*     mycpu(void);
struct proc*    myproc();
void            procinit(void);
void            scheduler(void) __attribute__((noreturn));
void            sched(void);
void            sleep(void*, struct spinlock*);
void            userinit(void);
int             wait(uint64);
void            wakeup(void*);
void            yield(void);
int             either_copyout(int user_dst, uint64 dst, void *src, uint64 len);
int             either_copyin(void *dst, int user_src, uint64 src, uint64 len);
void            procdump(void);
int 						proc_rand(struct proc *p);
void            proc_free_pagetable(struct proc *p);
void            proc_free_kpagetable(struct proc *p, uint64);

// swtch.S
void            swtch(struct context*, struct context*);

// spinlock.c
void            acquire(struct spinlock*);
int             holding(struct spinlock*);
void            initlock(struct spinlock*, char*);
void            release(struct spinlock*);
void            push_off(void);
void            pop_off(void);

// sleeplock.c
void            acquiresleep(struct sleeplock*);
void            releasesleep(struct sleeplock*);
int             holdingsleep(struct sleeplock*);
void            initsleeplock(struct sleeplock*, char*);

// strtol.c
long strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);
double strtod (const char *str, char **ptr);
double atof(const char* str);

// syscall.c
#define DEF_SYSCALL(id, name) extern uint64 sys_##name(void);
#include "kernel/syscall_def.h"
#undef DEF_SYSCALL
int             argint(int, int*);
int             argstr(int, char*, int);
int             argaddr(int, uint64 *);
int             fetchstr(uint64, char*, int);
int             fetchaddr(uint64, uint64*);
void            syscall();

// trap.c
extern uint     ticks;
void            trapinit(void);
void            trapinithart(void);
extern struct spinlock tickslock;
void            usertrapret(void);

// uart.c
void            uartinit(void);
void            uartintr(void);
void            uartputc(int);
void            uartputc_sync(int);
int             uartgetc(void);

// vm.c
pagetable_t			kvmmake(void);
void            kvminit(void);
void            kvminithart(void);
void            kvmmap(pagetable_t, uint64, uint64, uint64, int);

int             mappages(pagetable_t, uint64, uint64, uint64, int);
int							map_onepage(pagetable_t, uint64, uint64, int);
pagetable_t     upagecreate(void);
void            upageunmap(pagetable_t, uint64, uint64, int);
void            upageclear(pagetable_t, uint64);

int             uvmalloc(struct proc *, uint64, uint64);
int             uvmrealloc(struct proc *, uint64);
int 						cowpage(struct proc *, uint64 addr);
int             uvmdealloc(struct proc *, uint64, uint64);
int             uvmcopy(struct proc *, struct proc *);
int             uaddrvalid(struct proc *, uint64);

uint64  				vmpa(pagetable_t pagetable, uint64 va);
uint64          kvmpa(uint64 va);
uint64          walkaddr(pagetable_t, uint64);
pte_t *         walk(pagetable_t pagetable, uint64 va, int alloc);
int             free_pagetable(pagetable_t pagetable, int);

int             copyout(pagetable_t, uint64, char *, uint64);
int             copyin(char *, uint64, uint64);
int             copyinstr(char *, uint64, uint64);

void            vmprint(pagetable_t);
void						xprint(pagetable_t pagetable, uint64 srcva, uint64 bytes);
void 						print_page_info(pagetable_t pagetable);

// plic.c
void            plicinit(void);
void            plicinithart(void);
int             plic_claim(void);
void            plic_complete(int);

// virtio_disk.c
void            virtio_disk_init(void);
void            virtio_disk_rw(struct block_buf *, int);
void            virtio_disk_intr(void);

// others
void sysinfo_dump();

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x)/sizeof((x)[0]))

#define PGTBLFREE_MUST_NOLEAF 1
#define PGTBLFREE_FREE_MEM 2
#define PGTBLFREE_JUST_NO_LEAF 4