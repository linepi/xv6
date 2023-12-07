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
	for (int size = 10000; size < 100000; size += 100) {
    int array[size];
    int sum = 0;
    for (int i = 0; i < size; i++) {
      array[i] = i;
      sum += i;
    }
    if (sum != _recursion_sum(array, size, 0)) {
      exit(1);
    }
  }
	exit(0);
}