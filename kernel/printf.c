//
// formatted console output -- printf, panic.
//

#include <stdarg.h>

#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/spinlock.h"
#include "kernel/sleeplock.h"
#include "kernel/fs.h"
#include "kernel/file.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"
#include "kernel/defs.h"
#include "kernel/proc.h"
#include "common/log.h"

volatile int panicked = 0;

// lock to avoid interleaving concurrent printf's.
static struct {
  struct spinlock lock;
  int locking;
} pr;

static char digits[] = "0123456789abcdef";

static void
printint(int xx, int base, int sign)
{
  char buf[16];
  int i;
  uint x;

  if(sign && (sign = xx < 0))
    x = -xx;
  else
    x = xx;

  i = 0;
  do {
    buf[i++] = digits[x % base];
  } while((x /= base) != 0);

  if(sign)
    buf[i++] = '-';

  while(--i >= 0)
    consputc(buf[i]);
}

int
cto16(char c)
{
  switch (c){
    case '0': return 0;
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    case 'a': return 10;
    case 'b': return 11;
    case 'c': return 12;
    case 'd': return 13;
    case 'e': return 14;
    case 'f': return 15;
    default: return -1;
  }
}

uint64 
stolu(const char *s)
{
  if (s[0] == '0' && s[1] == 'x')
    s += 2;
  uint64 ret = 0;
  uint64 i = 1;
  for (int off = strlen(s) - 1; off >= 0; off--, i <<= 4) {
    ret += cto16(*(s + off)) * i;
  }
  return ret;
}

// Function to print a long integer in any base (binary, decimal, hex, etc.)
static void 
printlong(long num, int base, int sign) {
  char buf[64];
  int i = 0;
  int is_negative = 0;

  if (sign && num < 0) {
    is_negative = 1;
    num = -num;
  }

  do {
    buf[i++] = digits[num % base];
    num /= base;
  } while (num > 0);

  if (is_negative) {
    buf[i++] = '-';
  }

  while (--i >= 0)
    consputc(buf[i]);
}


static void
printptr(uint64 x)
{
  int i;
  consputc('0');
  consputc('x');
  for (i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4)
    consputc(digits[x >> (sizeof(uint64) * 8 - 4)]);
}

static void
printx64(uint64 x)
{
  int i, notzero = 0;
  if (x == 0) {
    consputc('0');
    return;
  }
  for (i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4) {
    char c = digits[x >> (sizeof(uint64) * 8 - 4)];
    if (c != '0')
      notzero = 1;
    if (!(c == '0' && notzero == 0))
      consputc(c);
  }
}

static void
printx32(uint32 x)
{
  int i, notzero = 0;
  if (x == 0) {
    consputc('0');
    return;
  }
  for (i = 0; i < (sizeof(uint32) * 2); i++, x <<= 4) {
    char c = digits[x >> (sizeof(uint32) * 8 - 4)];
    if (c != '0')
      notzero = 1;
    if (!(c == '0' && notzero == 0))
      consputc(c);
  }
}

// Print to the console. only understands %d, %x, %p, %s.
void
vprintf(const char *fmt, va_list ap)
{
  int i, c, locking;
  char *s;

  locking = pr.locking;
  if(locking)
    acquire(&pr.lock);

  if (fmt == 0)
    panic("null fmt");

  for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
    if(c != '%'){
      consputc(c);
      continue;
    }
    c = fmt[++i] & 0xff;
    if(c == 0)
      break;
    switch(c){
    case 'd':
      printint(va_arg(ap, int), 10, 1);
      break;
    case 'x':
      printx32(va_arg(ap, uint32));
      break;
    case 'l':
      c = fmt[++i] & 0xff; // Get the next character after 'l'
      if(c == 'd'){
        printlong(va_arg(ap, long), 10, 1);
      } else if(c == 'x'){
        printx64(va_arg(ap, uint64));
      } else {
        // If it is not %ld or %lx, print the 'l' and treat the next character as a new format specifier
        consputc('l');
        i--; // The next iteration will handle the format specifier
      }
      break;
    case 'p':
      printptr(va_arg(ap, uint64));
      break;
    case 's':
      if((s = va_arg(ap, char*)) == 0)
        s = "(null)";
      for(; *s; s++)
        consputc(*s);
      break;
    case '%':
      consputc('%');
      break;
    default:
      // Print unknown % sequence to draw attention.
      consputc('%');
      consputc(c);
      break;
    }
  }

  if(locking)
    release(&pr.lock);
}

void
printf(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vprintf(fmt, ap);
}

void
panic(const char *fmt, ...)
{
  pr.locking = 0;
  printf(ANSI_FMT("panic: ", ANSI_FG_RED));
  va_list ap;
  va_start(ap, fmt);
  vprintf(fmt, ap);
  printf(ANSI_FMT("\nsome info: \n", ANSI_FG_CYAN));
  procdump();
  panicked = 1; // freeze uart output from other CPUs
  for(;;)
    ;
}

void
assert(int condition)
{
  if (!condition)
    panic("assert");
}

void
printfinit(void)
{
  initlock(&pr.lock, "pr");
  pr.locking = 1;
}

// to be implemented
int
file_readline(const char *file, char *buf)
{
  return 0;
}

/**
 * @brief backtrace 回溯函数调用的返回地址
 */
void
backtrace(int user) {
  uint64 fp, end, addr;
  uint64 line_curaddr = 0;
  struct proc *p = myproc();

  printf("backtrace:\n");
  if (user) {
    fp = p->trapframe->s0;
    end = p->addrinfo.logical_stack_top;
    printf("0x%lx\n", p->trapframe->epc);
  } else {
    fp = r_fp();
    end = p->kstackbase + PGSIZE;
  }
  while (fp != end) {
    addr = *(uint64*)(fp - 8);
    // dump file and line
    char linefile[32];
    if (user)
      snprintf(linefile, sizeof(linefile), ".%s_lineinfo.txt", myproc()->name);
    else
      strcpy(linefile, ".kernel_lineinfo.txt");
    char linebuf[256], lastline[8], lastfilename[64];
    while (file_readline(linefile, linebuf) != 0) {
      char *tokens[3]; 
      int cnt = 0;
      char *token = strtok(linebuf, " ");
      while (token != NULL) {
        tokens[cnt++] = token;
        token = strtok(NULL, "/");
      }
      strcpy(lastline, tokens[1]);
      strcpy(lastfilename, tokens[2]);

      line_curaddr = stolu(tokens[0]);
      if (addr < line_curaddr) {
        printf("%s:%s\n", lastfilename, lastline);
        break;
      }
    } 
    fp = *(uint64*)(fp - 16);
  }
}