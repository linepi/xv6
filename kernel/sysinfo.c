#include "kernel/riscv.h"
#include "kernel/defs.h"
#include "user/system.h"

void 
sysinfo_dump()
{
	printf("memleft:\n %d kb | %d page\n", kmemleft() / 1024, kmemleft() / PGSIZE);
}

uint64 
sys_system_info()
{
	uint64 p;
	if (argaddr(0, &p) < 0)
		return -1;
	struct system_info si;
	si.memleft = kmemleft();
	si.n_cpu = 0;	
	if (copyout(0, p, (char *)&si, sizeof(struct system_info)) == -1) 
		return -1;
	return 0;
}