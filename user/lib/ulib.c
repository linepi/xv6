#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/riscv.h"
#include "kernel/fs.h"
#include "kernel/memlayout.h"
#include "user/user.h"

char*
strcpy(char *s, const char *t)
{
  char *os;

  os = s;
  while((*s++ = *t++) != 0)
    ;
  return os;
}

char*
strncpy(char *s, const char *t, int n)
{
  char *os;

  os = s;
  while(n-- > 0 && (*s++ = *t++) != 0)
    ;
  while(n-- > 0)
    *s++ = 0;
  return os;
}

int
strcmp(const char *p, const char *q)
{
  while(*p && *p == *q)
    p++, q++;
  return (uchar)*p - (uchar)*q;
}

char *
strcat(char *s, const char *append)
{
	char *save = s;
	for (; *s; ++s);
	while ((*s++ = *append++));
	return save;
}

uint
strlen(const char *s)
{
  int n;

  for(n = 0; s[n]; n++)
    ;
  return n;
}

void*
memset(void *dst, int c, uint n)
{
  char *cdst = (char *) dst;
  int i;
  for(i = 0; i < n; i++){
    cdst[i] = c;
  }
  return dst;
}

char*
strchr(const char *s, char c)
{
  for(; *s; s++)
    if(*s == c)
      return (char*)s;
  return 0;
}

char*
strrchr(const char *s, char c)
{
  char *ret = NULL;
  for(; *s; s++)
    if(*s == c)
      ret = (char *)s;
  return ret;
}

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
atoi(const char *s)
{
  int n;

  n = 0;
  while('0' <= *s && *s <= '9')
    n = n*10 + *s++ - '0';
  return n;
}

void*
memmove(void *vdst, const void *vsrc, int n)
{
  char *dst;
  const char *src;

  dst = vdst;
  src = vsrc;
  if (src > dst) {
    while(n-- > 0)
      *dst++ = *src++;
  } else {
    dst += n;
    src += n;
    while(n-- > 0)
      *--dst = *--src;
  }
  return vdst;
}

int
memcmp(const void *s1, const void *s2, uint n)
{
  const char *p1 = s1, *p2 = s2;
  while (n-- > 0) {
    if (*p1 != *p2) {
      return *p1 - *p2;
    }
    p1++;
    p2++;
  }
  return 0;
}

void *
memcpy(void *dst, const void *src, uint n)
{
  return memmove(dst, src, n);
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
    fprintf(2, "rmdir: %s failed to stat\n", path);
    return -1;
  }
  if (st.type == T_FILE) {
    if(unlink(path) < 0){
      fprintf(2, "rmdir: %s failed to delete\n", path);
      return -1;
    }
  } else if (st.type == T_DIR) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
      fprintf(2, "rmdir: open %s failed\n", path);
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
      fprintf(2, "rmdir: %s failed to delete\n", path);
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

char *
strtok_r(char *s, const char *delim, char **last)
{
	char *spanp;
	int c, sc;
	char *tok;
	if (s == NULL && (s = *last) == NULL)
		return (NULL);
	/*
	 * Skip (span) leading delimiters (s += strspn(s, delim), sort of).
	 */
cont:
	c = *s++;
	for (spanp = (char *)delim; (sc = *spanp++) != 0;) {
		if (c == sc)
			goto cont;
	}
	if (c == 0) {		/* no non-delimiter characters */
		*last = NULL;
		return (NULL);
	}
	tok = s - 1;
	/*
	 * Scan token (scan for delimiters: s += strcspn(s, delim), sort of).
	 * Note that delim must have one NUL; we stop if we see that, too.
	 */
	for (;;) {
		c = *s++;
		spanp = (char *)delim;
		do {
			if ((sc = *spanp++) == c) {
				if (c == 0)
					s = NULL;
				else
					s[-1] = 0;
				*last = s;
				return (tok);
			}
		} while (sc != 0);
	}
	/* NOTREACHED */
}

char *
strtok(char *s, const char *delim)
{
	static char *last;
	return strtok_r(s, delim, &last);
}