#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>

#define stat xv6_stat  // avoid clash with host struct stat
#include "kernel/types.h"
#include "kernel/fs.h"
#include "kernel/stat.h"
#include "kernel/param.h"

#ifndef static_assert
#define static_assert(a, b) do { switch (0) case 0: case (a): ; } while (0)
#endif


// Disk layout:
// [ boot block | sb block | log | inode blocks | free bit map | data blocks ]

int nbitmap = FSSIZE/(BLOCK_SIZE*8) + 1;
int ninodeblocks = NINODES / INODES_PER_BLOCK + 1;
int nlog = LOGSIZE;
int nmeta;    // Number of meta blocks (boot, sb, nlog, inode, bitmap)
int ndata;  // Number of data blocks

int fsfd;
struct superblock sb;
char zeroes[BLOCK_SIZE];
uint freeblock;


void bitmap_alloc(int);
void write_block(uint, void*);
void winode(uint, struct dinode*);
void rinode(uint inum, struct dinode *ip);
void read_block(uint sec, void *buf);
uint inode_alloc(ushort type);
void iappend(uint inum, void *p, int n);
void die(const char *);

// convert to intel byte order
ushort
xshort(ushort x)
{
  ushort y;
  uchar *a = (uchar*)&y;
  a[0] = x;
  a[1] = x >> 8;
  return y;
}

uint
xint(uint x)
{
  uint y;
  uchar *a = (uchar*)&y;
  a[0] = x;
  a[1] = x >> 8;
  a[2] = x >> 16;
  a[3] = x >> 24;
  return y;
}

int
main(int argc, char *argv[])
{
  int i, cc, fd;
  uint root_inode_no, inum, off;
  struct dirent dir;
  char buf[BLOCK_SIZE];
  struct dinode din;


  static_assert(sizeof(int) == 4, "Integers must be 4 bytes!");

  if(argc < 2){
    fprintf(stderr, "Usage: mkfs fs.img files...\n");
    exit(1);
  }

  assert((BLOCK_SIZE % sizeof(struct dinode)) == 0);
  assert((BLOCK_SIZE % sizeof(struct dirent)) == 0);

  fsfd = open(argv[1], O_RDWR|O_CREAT|O_TRUNC, 0666);
  if(fsfd < 0)
    die(argv[1]);

  // 1 fs block = 1 disk sector
  nmeta = 2 + nlog + ninodeblocks + nbitmap;
  ndata = FSSIZE - nmeta;

  printf("nmeta %d (boot, super, log blocks %u inode blocks %u, bitmap blocks %u) blocks %d total %d\n",
         nmeta, nlog, ninodeblocks, nbitmap, ndata, FSSIZE);

  freeblock = nmeta;     // the first free block that we can allocate

  // fill all block with zero
  for(i = 0; i < FSSIZE; i++)
    write_block(i, zeroes);

  // write superblock
  sb.magic = FSMAGIC;
  sb.size = xint(FSSIZE);
  sb.logstart = xint(2);
  sb.nlog = xint(nlog);
  sb.inodestart = xint(2+nlog);
  sb.ninodes = xint(NINODES);
  sb.ninode_block = xint(ninodeblocks);
  sb.bmapstart = xint(2+nlog+ninodeblocks);
  sb.nbmap = xint(nbitmap);
  sb.datastart = xint(2+nlog+ninodeblocks+nbitmap);
  sb.ndata = xint(ndata);
  memset(buf, 0, sizeof(buf));
  memmove(buf, &sb, sizeof(sb));
  write_block(1, buf);

  // alloc root inode and append '.', '..' dir
  root_inode_no = inode_alloc(T_DIR);
  assert(root_inode_no == ROOTINO);

  bzero(&dir, sizeof(dir));
  dir.inum = xshort(root_inode_no);
  strcpy(dir.name, ".");
  iappend(root_inode_no, &dir, sizeof(dir));

  bzero(&dir, sizeof(dir));
  dir.inum = xshort(root_inode_no);
  strcpy(dir.name, "..");
  iappend(root_inode_no, &dir, sizeof(dir));

  for(i = 2; i < argc; i++){
    // get rid of "user/"
    char *shortname;
    if(strncmp(argv[i], "build/user/", 11) == 0)
      shortname = argv[i] + 11;
    else if (strncmp(argv[i], "build/kernel/", 13) == 0)
      shortname = argv[i] + 13;
    else
      shortname = argv[i];
    
    assert(index(shortname, '/') == 0);

    if((fd = open(argv[i], 0)) < 0)
      die(argv[i]);

    // Skip leading _ in name when writing to file system.
    // The binaries are named _rm, _cat, etc. to keep the
    // build operating system from trying to execute them
    // in place of system binaries like rm and cat.
    if(shortname[0] == '_')
      shortname += 1;

    inum = inode_alloc(T_FILE);

    bzero(&dir, sizeof(dir));
    dir.inum = xshort(inum);
    strncpy(dir.name, shortname, DIRSIZ);
    iappend(root_inode_no, &dir, sizeof(dir));

    while((cc = read(fd, buf, sizeof(buf))) > 0)
      iappend(inum, buf, cc);

    close(fd);
  }

  // fix size of root inode dir
  rinode(root_inode_no, &din);
  off = xint(din.size);
  off = ((off/BLOCK_SIZE) + 1) * BLOCK_SIZE;
  din.size = xint(off);
  winode(root_inode_no, &din);

  bitmap_alloc(freeblock);

  exit(0);
}

// write one block
void
write_block(uint sec, void *buf)
{
  if(lseek(fsfd, sec * BLOCK_SIZE, 0) != sec * BLOCK_SIZE)
    die("lseek");
  if(write(fsfd, buf, BLOCK_SIZE) != BLOCK_SIZE)
    die("write");
}

// write new inode
// @param inum: inode id, start from 1
// @param ip: the inode to write
void
winode(uint inum, struct dinode *ip)
{
  char buf[BLOCK_SIZE];
  uint bn;
  struct dinode *dip;

  bn = INODE_BLOCK(inum, sb);
  read_block(bn, buf);
  dip = ((struct dinode*)buf) + (inum % INODES_PER_BLOCK);
  *dip = *ip;
  write_block(bn, buf);
}

// read the inode
void
rinode(uint inum, struct dinode *ip)
{
  char buf[BLOCK_SIZE];
  uint bn;
  struct dinode *dip;

  bn = INODE_BLOCK(inum, sb);
  read_block(bn, buf);
  dip = ((struct dinode*)buf) + (inum % INODES_PER_BLOCK);
  *ip = *dip;
}

// read one block
void
read_block(uint sec, void *buf)
{
  if(lseek(fsfd, sec * BLOCK_SIZE, SEEK_SET) != sec * BLOCK_SIZE)
    die("lseek");
  if(read(fsfd, buf, BLOCK_SIZE) != BLOCK_SIZE)
    die("read");
}

// add a new inode with type
uint
inode_alloc(ushort type)
{
  static uint freeinode = 1;
  uint inum = freeinode++;
  struct dinode din;

  bzero(&din, sizeof(din));
  din.type = xshort(type);
  din.nlink = xshort(1);
  din.size = xint(0);
  winode(inum, &din);
  return inum;
}

void
bitmap_alloc(int used)
{

  printf("bitmap_alloc: first %d blocks have been allocated\n", used);
  for (int n = 0; n < nbitmap; n++) {
    uchar buf[BLOCK_SIZE];
    bzero(buf, BLOCK_SIZE);
    for(int i = 0; i < used; i++){
      buf[i/8] = buf[i/8] | (0x1 << (i%8));
    }
    write_block(sb.bmapstart + n, buf);
    used -= BIT_PER_BLOCK;
  }
  printf("bitmap_alloc: write %d bitmap block from block %d\n", nbitmap, sb.bmapstart);
}

#define min(a, b) ((a) < (b) ? (a) : (b))

// append buf into block inum
void
iappend(uint inum, void *p_, int n)
{
  char *p = p_;
  uint fbn, off, n1;
  struct dinode din;
  char buf[BLOCK_SIZE];
  uint indirect[NINDIRECT];
  uint x;
  int one, two;

  rinode(inum, &din);
  off = xint(din.size);
  // printf("append inum %d at off %d sz %d\n", inum, off, n);
  while(n > 0){
    fbn = off / BLOCK_SIZE;
    assert(fbn < MAXFILE_BLOCKS);

    one = two = 0;
    if (fbn < NDIRECT) {
      if(xint(din.addrs[fbn]) == 0){
        din.addrs[fbn] = xint(freeblock++);
      }
      x = xint(din.addrs[fbn]);
    } else {
      fbn -= NDIRECT;
      one = 1;
    }
    if (one && fbn < NINDIRECT) {
      if(xint(din.addrs[NDIRECT]) == 0){
        din.addrs[NDIRECT] = xint(freeblock++);
      }
      read_block(xint(din.addrs[NDIRECT]), (char*)indirect);
      if(indirect[fbn] == 0){
        indirect[fbn] = xint(freeblock++);
        write_block(xint(din.addrs[NDIRECT]), (char*)indirect);
      }
      x = xint(indirect[fbn]);
    } else if (one) {
      fbn -= NINDIRECT;
      two = 1;
    }
    if (two && fbn < NIINDIRECT) {
      if(xint(din.addrs[NDIRECT+1]) == 0){
        din.addrs[NDIRECT+1] = xint(freeblock++);
      }
      read_block(xint(din.addrs[NDIRECT+1]), (char*)indirect);
      if(indirect[fbn/NINDIRECT] == 0){
        indirect[fbn/NINDIRECT] = xint(freeblock++);
        write_block(xint(din.addrs[NDIRECT+1]), (char*)indirect);
      }
      int t = xint(indirect[fbn/NINDIRECT]);
      read_block(t, (char*)indirect);
      if(indirect[fbn%NINDIRECT] == 0){
        indirect[fbn%NINDIRECT] = xint(freeblock++);
        write_block(t, (char*)indirect);
      }
      x = xint(indirect[fbn%NINDIRECT]);
    } else if (two) {
      die("file two large");
    }
    fbn = off / BLOCK_SIZE;

    n1 = min(n, (fbn + 1) * BLOCK_SIZE - off);
    read_block(x, buf);
    bcopy(p, buf + off - (fbn * BLOCK_SIZE), n1);
    write_block(x, buf);
    n -= n1;
    off += n1;
    p += n1;
  }
  din.size = xint(off);
  winode(inum, &din);
}

void
die(const char *s)
{
  if (errno != 0)
    perror(s);
  else
    printf("error: %s\n", s);
  remove("build/fs.img");
  exit(1);
}
