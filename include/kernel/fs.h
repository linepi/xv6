// On-disk file system format.
// Both the kernel and user programs use this header file.


#define ROOTINO  1   // root i-number
#define FSMAGIC 0x10203040
#define BLOCK_SIZE 1024  // block size

// Disk layout:
// [ boot block | super block | log | inode blocks |
//                                          free bit map | data blocks]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
struct superblock {
  uint magic;        // Must be FSMAGIC
  uint size;         // Size of file system image (blocks)
  uint logstart;     // Block number of first log block
  uint nlog;         // Number of log blocks
  uint inodestart;   // Block number of first inode block
  uint ninodes;      // Number of inodes.
  uint ninode_block; // Number of inodes blocks.
  uint bmapstart;    // Block number of first free map block
  uint nbmap;        // Number of free map block
  uint datastart;    // Block number of first data block
  uint ndata;        // Number of data blocks
};

#define NDIRECT 11
#define NINDIRECT (BLOCK_SIZE / sizeof(uint))
#define NIINDIRECT (NINDIRECT * (BLOCK_SIZE / sizeof(uint)))
#define MAXFILE_BLOCKS (NDIRECT + NINDIRECT + NIINDIRECT)

// On-disk inode structure
struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEVICE only)
  short minor;          // Minor device number (T_DEVICE only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+2];   // Data block addresses
};

// Inodes per block.
#define INODES_PER_BLOCK           (BLOCK_SIZE / sizeof(struct dinode))

// Block containing inode i
#define INODE_BLOCK(i, sb)     ((i) / INODES_PER_BLOCK + sb.inodestart)

// Bitmap bits per block
#define BIT_PER_BLOCK           (BLOCK_SIZE*8)

// Block of free map containing bit for block with blockno
#define BITMAP_BLOCK(blockno, sb) ((blockno)/BIT_PER_BLOCK + sb.bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 30

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

