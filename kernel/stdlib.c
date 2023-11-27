#include "kernel/types.h"
#include <limits.h>
static ulong next = 1;

int
rand()
{
	return ((next = next * 1103515245 + 12345) % ((ulong)(2147483647) + 1));
}

void
srand(uint seed)
{
	next = seed;
}

int
rand_r (unsigned int *seed)
{
  unsigned int next = *seed;
  int result;
  next *= 1103515245;
  next += 12345;
  result = (unsigned int) (next / 65536) % 2048;
  next *= 1103515245;
  next += 12345;
  result <<= 10;
  result ^= (unsigned int) (next / 65536) % 1024;
  next *= 1103515245;
  next += 12345;
  result <<= 10;
  result ^= (unsigned int) (next / 65536) % 1024;
  *seed = next;
  return result;
}