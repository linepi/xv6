#include "kernel/types.h"
#include "user/user.h"

int main() {
	uint64 *buf = malloc(10);
	printf("%ld", *buf);
	exit(0);
}
