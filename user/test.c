#include "user/user.h"

int 
_recursion_sum(int *array, int size, int idx) 
{
  if (idx < size)
    return array[idx] + _recursion_sum(array, size, idx + 1);
  else 
    return 0;
}

int main()
{
  int x = 0x80000000;
  printf("%x %x %x\n", x, x + 1, x - 1);
	exit(0);
}