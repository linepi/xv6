#include "common/types.h"
#include "common/stdlib.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/riscv.h"
#include "kernel/fs.h"
#include "kernel/memlayout.h"
#include "user/user.h"
#include "common/log.h"

char*
gets(char *buf, int max)
{
  int i, cc;
  char c;

  for(i=0; i+1 < max; ){
    cc = read(0, &c, 1);
    if(cc < 1)
      break;
    buf[i++] = c;
    if(c == '\n' || c == '\r')
      break;
  }
  buf[i] = '\0';
  return buf;
}

int
stat(const char *n, struct stat *st)
{
  int fd;
  int r;

  fd = open(n, O_RDONLY);
  if(fd < 0)
    return -1;
  r = fstat(fd, st);
  close(fd);
  return r;
}

int
ugetpid(void)
{
  struct usyscall *u = (struct usyscall *)USYSCALL;
  return u->pid;
}

char *
getcwd(char *buf, int size) 
{
  struct usyscall *u = (struct usyscall *)USYSCALL;
  return strncpy(buf, u->cwd, MAXPATH);
}

// rm everything, using unlink
static int
rmdir_(char *path)
{
  struct stat st;
  if (stat(path, &st) < 0) {
    LOG("failed to stat %s\n", path);
    return -1;
  }
  if (st.type == T_FILE) {
    if(unlink(path) < 0){
      LOG("failed to unlink %s\n", path);
      return -1;
    }
  } else if (st.type == T_DIR) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
      LOG("open %s failed\n", path);
      return -1;
    }
    struct dirent de;	
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
			if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) 
				continue;
      strcat(path, "/");
      strcat(path, de.name);
      int rc = rmdir_(path);
      strrchr(path, '/')[0] = 0;
      if (rc < 0) {
        close(fd);
        return -1;
      }
    }
    close(fd);
    if(unlink(path) < 0){
      LOG("failed to unlink %s\n", path);
      return -1;
    }
  }
  return 0;
}

int
rmdir(const char *path)
{
  char buf[MAXPATH];
  strcpy(buf, path);
  return rmdir_(buf);
}