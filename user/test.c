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
  char buf[300];
  sprintf(buf, "%x %d %s", -1L, 34, "fdsag");
  printf("%s\n", buf);
	exit(0);
}