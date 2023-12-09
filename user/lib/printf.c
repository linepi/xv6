#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#include <stdarg.h>

static char digits[] = "0123456789ABCDEF";

static void
putc(int fd, char c)
{
  write(fd, &c, 1);
}

static void
printint(int fd, int xx, int base, int sgn)
{
  char buf[16];
  int i, neg;
  uint x;

  neg = 0;
  if(sgn && xx < 0){
    neg = 1;
    x = -xx;
  } else {
    x = xx;
  }

  i = 0;
  do{
    buf[i++] = digits[x % base];
  }while((x /= base) != 0);
  if(neg)
    buf[i++] = '-';

  while(--i >= 0)
    putc(fd, buf[i]);
}

// Function to print a long integer in any base (binary, decimal, hex, etc.)
static void
printlong(int fd, long num, int base, int sign) {
  char buf[64];
  int i = 0;
  int is_negative = 0;

  // Handle negative numbers if sign is true.
  if (sign && num < 0) {
    is_negative = 1;
    num = -num;
  }

  // Process individual digits
  do {
    buf[i++] = digits[num % base];
    num /= base;
  } while (num > 0);

  // If number is negative, append '-'
  if (is_negative) {
    buf[i++] = '-';
  }

  // Print the number
  while (--i >= 0) {
    putc(fd, buf[i]);
  }
}

static void
printx64(int fd, uint64 x)
{
  int i, notzero = 0;
  for (i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4) {
    char c = digits[x >> (sizeof(uint64) * 8 - 4)];
    if (c != '0')
      notzero = 1;
    if (!(c == '0' && notzero == 0))
      putc(fd, c);
  }
}

static void
printx32(int fd, uint32 x)
{
  int i, notzero = 0;
  for (i = 0; i < (sizeof(uint32) * 2); i++, x <<= 4) {
    char c = digits[x >> (sizeof(uint32) * 8 - 4)];
    if (c != '0')
      notzero = 1;
    if (!(c == '0' && notzero == 0))
      putc(fd, c);
  }
}


static void
printptr(int fd, uint64 x) {
  int i;
  putc(fd, '0');
  putc(fd, 'x');
  for (i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4)
    putc(fd, digits[x >> (sizeof(uint64) * 8 - 4)]);
}

// Print to the given fd. Only understands %d, %x, %p, %s.
void
vprintf(int fd, const char *fmt, va_list ap)
{
  char *s;
  int c, i, state;

  state = 0;
  for(i = 0; fmt[i]; i++){
    c = fmt[i] & 0xff;
    if(state == 0){
      if(c == '%'){
        state = '%';
      } else {
        putc(fd, c);
      }
    } else if(state == '%'){
      if(c == 'd'){
        printint(fd, va_arg(ap, int), 10, 1);
      } else if(c == 'l') {
        c = fmt[++i] & 0xff; // Get the next character after 'l'
        if(c == 'd'){
          printlong(fd, va_arg(ap, long), 10, 1);
        } else if(c == 'x'){
          printx64(fd, va_arg(ap, uint64));
        } else {
          // If it is not %ld or %lx, print the 'l' and treat the next character as a new format specifier
          putc(fd, 'l');
          i--; // The next iteration will handle the format specifier
        }
      } else if(c == 'x') {
        printx32(fd, va_arg(ap, uint32));
      } else if(c == 'p') {
        printptr(fd, va_arg(ap, uint64));
      } else if(c == 's'){
        s = va_arg(ap, char*);
        if(s == 0)
          s = "(null)";
        while(*s != 0){
          putc(fd, *s);
          s++;
        }
      } else if(c == 'c'){
        putc(fd, va_arg(ap, uint));
      } else if(c == '%'){
        putc(fd, c);
      } else {
        // Unknown % sequence.  Print it to draw attention.
        putc(fd, '%');
        putc(fd, c);
      }
      state = 0;
    }
  }
}

void
fprintf(int fd, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vprintf(fd, fmt, ap);
}

void
printf(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vprintf(1, fmt, ap);
}
