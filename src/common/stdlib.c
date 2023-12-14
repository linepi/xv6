#include "common/stdlib.h"
#include "common/limits.h"
#include "common/types.h"

static ulong next = 1;

int rand() {
  return ((next = next * 1103515245 + 12345) % ((ulong)(2147483647) + 1));
}

void srand(uint seed) { next = seed; }

int rand_r(unsigned int *seed) {
  unsigned int next = *seed;
  int result;
  next *= 1103515245;
  next += 12345;
  result = (unsigned int)(next / 65536) % 2048;
  next *= 1103515245;
  next += 12345;
  result <<= 10;
  result ^= (unsigned int)(next / 65536) % 1024;
  next *= 1103515245;
  next += 12345;
  result <<= 10;
  result ^= (unsigned int)(next / 65536) % 1024;
  *seed = next;
  return result;
}

int isspace(int c) {
  return (c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r' ||
                  c == ' '
              ? 1
              : 0);
}

int isdigit(int c) { return (c >= '0' && c <= '9' ? 1 : 0); }

int isupper(int c) { return c >= 'A' && c <= 'Z'; }

int isalpha(int c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }

void *memset(void *dst, int c, uint n) {
  char *cdst = (char *)dst;
  int i;
  for (i = 0; i < n; i++) {
    cdst[i] = c;
  }
  return dst;
}

int memcmp(const void *v1, const void *v2, uint n) {
  const uchar *s1, *s2;

  s1 = v1;
  s2 = v2;
  while (n-- > 0) {
    if (*s1 != *s2)
      return *s1 - *s2;
    s1++, s2++;
  }

  return 0;
}

void *memmove(void *dst, const void *src, uint n) {
  const char *s;
  char *d;

  if (n == 0)
    return dst;

  s = src;
  d = dst;
  if (s < d && s + n > d) {
    s += n;
    d += n;
    while (n-- > 0)
      *--d = *--s;
  } else
    while (n-- > 0)
      *d++ = *s++;

  return dst;
}

// memcpy exists to placate GCC.  Use memmove.
void *memcpy(void *dst, const void *src, uint n) {
  return memmove(dst, src, n);
}

int strncmp(const char *p, const char *q, uint n) {
  while (n > 0 && *p && *p == *q)
    n--, p++, q++;
  if (n == 0)
    return 0;
  return (uchar)*p - (uchar)*q;
}

int strcmp(const char *p, const char *q) {
  while (*p && *p == *q)
    p++, q++;
  return (uchar)*p - (uchar)*q;
}

char *strcat(char *s, const char *append) {
  char *save = s;
  for (; *s; ++s)
    ;
  while ((*s++ = *append++))
    ;
  return save;
}

char *strncpy(char *s, const char *t, int n) {
  char *os;

  os = s;
  while (n-- > 0 && (*s++ = *t++) != 0)
    ;
  while (n-- > 0)
    *s++ = 0;
  return os;
}

char *strcpy(char *s, const char *t) {
  char *save = s;
  while ((*s++ = *t++) != 0)
    ;
  return save;
}

// Like strncpy but guaranteed to NUL-terminate.
char *safestrcpy(char *s, const char *t, int n) {
  char *os;

  os = s;
  if (n <= 0)
    return os;
  while (--n > 0 && (*s++ = *t++) != 0)
    ;
  *s = 0;
  return os;
}

char *strchr(const char *s, char c) {
  for (; *s; s++)
    if (*s == c)
      return (char *)s;
  return 0;
}

char *strrchr(const char *s, char c) {
  char *ret = NULL;
  for (; *s; s++)
    if (*s == c)
      ret = (char *)s;
  return ret;
}

int strlen(const char *s) {
  int n;

  for (n = 0; s[n]; n++)
    ;
  return n;
}

char *strtok(char *s, const char *delim) {
  static char *last;
  return strtok_r(s, delim, &last);
}

/*
 * Convert a string to a long integer.
 *
 * Ignores `locale' stuff.  Assumes that the upper and lower case
 * alphabets and digits are each contiguous.
 */
long strtol(const char *nptr, char **endptr, int base) {
  const char *s = nptr;
  unsigned long acc;
  int c;
  unsigned long cutoff;
  int neg = 0, any, cutlim;

  /*
   * Skip white space and pick up leading +/- sign if any.
   * If base is 0, allow 0x for hex and 0 for octal, else
   * assume decimal; if base is already 16, allow 0x.
   */
  do {
    c = *s++;
  } while (isspace(c));
  if (c == '-') {
    neg = 1;
    c = *s++;
  } else if (c == '+')
    c = *s++;
  if ((base == 0 || base == 16) && c == '0' && (*s == 'x' || *s == 'X')) {
    c = s[1];
    s += 2;
    base = 16;
  }
  if (base == 0)
    base = c == '0' ? 8 : 10;

  /*
   * Compute the cutoff value between legal numbers and illegal
   * numbers.  That is the largest legal value, divided by the
   * base.  An input number that is greater than this value, if
   * followed by a legal input character, is too big.  One that
   * is equal to this value may be valid or not; the limit
   * between valid and invalid numbers is then based on the last
   * digit.  For instance, if the range for longs is
   * [-2147483648..2147483647] and the input base is 10,
   * cutoff will be set to 214748364 and cutlim to either
   * 7 (neg==0) or 8 (neg==1), meaning that if we have accumulated
   * a value > 214748364, or equal but the next digit is > 7 (or 8),
   * the number is too big, and we will return a range error.
   *
   * Set any if any `digits' consumed; make it negative to indicate
   * overflow.
   */
  cutoff = neg ? -(unsigned long)LONG_MIN : LONG_MAX;
  cutlim = cutoff % (unsigned long)base;
  cutoff /= (unsigned long)base;
  for (acc = 0, any = 0;; c = *s++) {
    if (isdigit(c))
      c -= '0';
    else if (isalpha(c))
      c -= isupper(c) ? 'A' - 10 : 'a' - 10;
    else
      break;
    if (c >= base)
      break;
    if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
      any = -1;
    else {
      any = 1;
      acc *= base;
      acc += c;
    }
  }
  if (any < 0) {
    acc = neg ? LONG_MIN : LONG_MAX;
  } else if (neg)
    acc = -acc;
  if (endptr != 0)
    *endptr = (char *)(any ? s - 1 : nptr);
  return (acc);
}

unsigned long strtoul(const char *nptr, char **endptr, int base) {
  const char *s = nptr;
  unsigned long acc;
  int c;
  unsigned long cutoff;
  int neg = 0, any, cutlim;

  /*
   * See strtol for comments as to the logic used.
   */
  do {
    c = *s++;
  } while (isspace(c));
  if (c == '-') {
    neg = 1;
    c = *s++;
  } else if (c == '+')
    c = *s++;
  if ((base == 0 || base == 16) && c == '0' && (*s == 'x' || *s == 'X')) {
    c = s[1];
    s += 2;
    base = 16;
  }
  if (base == 0)
    base = c == '0' ? 8 : 10;
  cutoff = (unsigned long)ULONG_MAX / (unsigned long)base;
  cutlim = (unsigned long)ULONG_MAX % (unsigned long)base;
  for (acc = 0, any = 0;; c = *s++) {
    if (isdigit(c))
      c -= '0';
    else if (isalpha(c))
      c -= isupper(c) ? 'A' - 10 : 'a' - 10;
    else
      break;
    if (c >= base)
      break;
    if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
      any = -1;
    else {
      any = 1;
      acc *= base;
      acc += c;
    }
  }
  if (any < 0) {
    acc = ULONG_MAX;
  } else if (neg)
    acc = -acc;
  if (endptr != 0)
    *endptr = (char *)(any ? s - 1 : nptr);
  return (acc);
}

static int maxExponent = 511; /* Largest possible base 10 exponent.  Any
                               * exponent larger than this will already
                               * produce underflow or overflow, so there's
                               * no need to worry about additional digits.
                               */
static double powersOf10[] = {/* Table giving binary powers of 10.  Entry */
                              10.,  /* is 10^2^i.  Used to convert decimal */
                              100., /* exponents into floating-point numbers. */
                              1.0e4,  1.0e8,   1.0e16, 1.0e32,
                              1.0e64, 1.0e128, 1.0e256};

/*
 *----------------------------------------------------------------------
 *
 * strtod --
 *
 *	This procedure converts a floating-point number from an ASCII
 *	decimal representation to internal double-precision format.
 *
 * Results:
 *	The return value is the double-precision floating-point
 *	representation of the characters in string.  If endPtr isn't
 *	NULL, then *endPtr is filled in with the address of the
 *	next character after the last one that was part of the
 *	floating-point number.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
double strtod(const char *string, char **endPtr) {
  int sign, expSign = 0;
  double fraction, dblExp, *d;
  const char *p;
  int c;
  int exp = 0;      /* Exponent read from "EX" field. */
  int fracExp = 0;  /* Exponent that derives from the fractional
                     * part.  Under normal circumstatnces, it is
                     * the negative of the number of digits in F.
                     * However, if I is very long, the last digits
                     * of I get dropped (otherwise a long I with a
                     * large negative exponent could cause an
                     * unnecessary overflow on I alone).  In this
                     * case, fracExp is incremented one for each
                     * dropped digit. */
  int mantSize;     /* Number of digits in mantissa. */
  int decPt;        /* Number of mantissa digits BEFORE decimal
                     * point. */
  const char *pExp; /* Temporarily holds location of exponent
                     * in string. */
  /*
   * Strip off leading blanks and check for a sign.
   */
  p = string;
  while (isspace(*p)) {
    p += 1;
  }
  if (*p == '-') {
    sign = 1;
    p += 1;
  } else {
    if (*p == '+') {
      p += 1;
    }
    sign = 0;
  }
  /*
   * Count the number of digits in the mantissa (including the decimal
   * point), and also locate the decimal point.
   */
  decPt = -1;
  for (mantSize = 0;; mantSize += 1) {
    c = *p;
    if (!isdigit(c)) {
      if ((c != '.') || (decPt >= 0)) {
        break;
      }
      decPt = mantSize;
    }
    p += 1;
  }
  /*
   * Now suck up the digits in the mantissa.  Use two integers to
   * collect 9 digits each (this is faster than using floating-point).
   * If the mantissa has more than 18 digits, ignore the extras, since
   * they can't affect the value anyway.
   */

  pExp = p;
  p -= mantSize;
  if (decPt < 0) {
    decPt = mantSize;
  } else {
    mantSize -= 1; /* One of the digits was the point. */
  }
  if (mantSize > 18) {
    fracExp = decPt - 18;
    mantSize = 18;
  } else {
    fracExp = decPt - mantSize;
  }
  if (mantSize == 0) {
    fraction = 0.0;
    p = string;
    goto done;
  } else {
    int frac1, frac2;
    frac1 = 0;
    for (; mantSize > 9; mantSize -= 1) {
      c = *p;
      p += 1;
      if (c == '.') {
        c = *p;
        p += 1;
      }
      frac1 = 10 * frac1 + (c - '0');
    }
    frac2 = 0;
    for (; mantSize > 0; mantSize -= 1) {
      c = *p;
      p += 1;
      if (c == '.') {
        c = *p;
        p += 1;
      }
      frac2 = 10 * frac2 + (c - '0');
    }
    fraction = (1.0e9 * frac1) + frac2;
  }
  /*
   * Skim off the exponent.
   */
  p = pExp;
  if ((*p == 'E') || (*p == 'e')) {
    p += 1;
    if (*p == '-') {
      expSign = 1;
      p += 1;
    } else {
      if (*p == '+') {
        p += 1;
      }
      expSign = 0;
    }
    while (isdigit(*p)) {
      exp = exp * 10 + (*p - '0');
      p += 1;
    }
  }
  if (expSign) {
    exp = fracExp - exp;
  } else {
    exp = fracExp + exp;
  }
  /*
   * Generate a floating-point number that represents the exponent.
   * Do this by processing the exponent one bit at a time to combine
   * many powers of 2 of 10. Then combine the exponent with the
   * fraction.
   */

  if (exp < 0) {
    expSign = 1;
    exp = -exp;
  } else {
    expSign = 0;
  }
  if (exp > maxExponent) {
    exp = maxExponent;
  }
  dblExp = 1.0;
  for (d = powersOf10; exp != 0; exp >>= 1, d += 1) {
    if (exp & 01) {
      dblExp *= *d;
    }
  }
  if (expSign) {
    fraction /= dblExp;
  } else {
    fraction *= dblExp;
  }
done:
  if (endPtr != NULL) {
    *endPtr = (char *)p;
  }
  if (sign) {
    return -fraction;
  }
  return fraction;
}

double atof(const char *str) { return strtod(str, (char **)NULL); }

char *strtok_r(char *s, const char *delim, char **last) {
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
  if (c == 0) { /* no non-delimiter characters */
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

int atoi(const char *s) {
  int n;

  n = 0;
  while ('0' <= *s && *s <= '9')
    n = n * 10 + *s++ - '0';
  return n;
}

char *basename(const char *path) {
  char *res;
  res = strrchr(path, '/');
  if (res == NULL)
    return (char *)path;
  else
    return res + 1;
}
