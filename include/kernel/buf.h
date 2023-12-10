struct block_buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  struct block_buf *prev; // LRU cache list
  struct block_buf *next;
  uchar data[BLOCK_SIZE];
};

