#define DEF_SYSCALL(id, name) \
	.global name; \
	name: ; \
	li a7, id; \
	ecall; \
	ret;
#include "kernel/syscall_def.h"