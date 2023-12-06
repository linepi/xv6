#include "kernel/types.h"
#include "user/user.h"

int main() {
	uint64 *buf = (uint64 *)0;
	printf("%ld", *buf);
	exit(0);
}
