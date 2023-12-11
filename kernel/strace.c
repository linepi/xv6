#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"
#include "kernel/spinlock.h"
#include "kernel/proc.h"
#include "kernel/fcntl.h"
#include "kernel/stat.h"
#include "kernel/syscall.h"
#include "kernel/defs.h"

#define MAX_STR_SHOW 32
#define max(a,b) ((a) > (b) ? (a) : (b))

// int fork(void);
const char*
trace_fork()
{
	return "";
}

// int exit(int) __attribute__((noreturn));
const char*
trace_exit()
{
	static char buf[32];
	int a;
	argint(0, &a);
	snprintf(buf, sizeof(buf), "%d", a);
	return buf;
}

// int wait(int*);
const char*
trace_wait()
{
	static char buf[32];
	uint64 a;
	argaddr(0, &a);
	snprintf(buf, sizeof(buf), "0x%lx", a);
	return buf;
}

// int pipe(int*);
const char*
trace_pipe()
{
	static char buf[32];
	uint64 a;
	argaddr(0, &a);
	snprintf(buf, sizeof(buf), "0x%lx", a);
	return buf;
}

// int write(int, const void*, int);
const char*
trace_write()
{
	static char buf[64];
	int a, c;
	uint64 b;
	argint(0, &a);
	argaddr(1, &b);
	argint(2, &c);

	char tmp[MAX_STR_SHOW + 1];
	if (copyinstr(tmp, b, max(sizeof(tmp), c)) != 0) {
		printf("[warning]: trace_write copyinstr error\n");
		return "";
	}
	snprintf(buf, sizeof(buf), "%d, \"%s\", %d", a, tmp, c);
	return buf;
}

// int read(int, void*, int);
const char*
trace_read()
{
	static char buf[64];
	int a, c;
	uint64 b;
	argint(0, &a);
	argaddr(1, &b);
	argint(2, &c);

	char tmp[MAX_STR_SHOW + 1];
	if (copyinstr(tmp, b, max(sizeof(tmp), c)) != 0) {
		printf("[warning]: trace_read copyinstr error\n");
		return "";
	}
	snprintf(buf, sizeof(buf), "%d, \"%s\", %d", a, tmp, c);
	return buf;
}

// int close(int);
const char*
trace_close()
{
	static char buf[32];
	int a;
	argint(0, &a);
	snprintf(buf, sizeof(buf), "%d", a);
	return buf;
}

// int kill(int);
const char*
trace_kill()
{
	static char buf[32];
	int a;
	argint(0, &a);
	snprintf(buf, sizeof(buf), "%d", a);
	return buf;
}

// int exec(char*, char**);
const char*
trace_exec()
{
	static char buf[128];
	uint64 a, b;
	argaddr(0, &a);
	argaddr(1, &b);

	char tmp[MAX_STR_SHOW + 1];
	if (copyinstr(tmp, a, sizeof(tmp)) != 0) {
		printf("[warning]: trace_exec copyinstr error\n");
		return "";
	}
	snprintf(buf, sizeof(buf), "\"%s\", [", tmp);

	while (1) {
		uint64 param;
		if(fetchaddr(b, &param) < 0){
			printf("[warning]: trace_exec fetchaddr error\n");
			return "";
		}
		if (param == 0)
			break;
		if (copyinstr(tmp, param, sizeof(tmp)) != 0) {
			printf("[warning]: trace_exec copyinstr error\n");
			return "";
		}
		strcat(buf, "\"");
		strcat(buf, tmp);
		strcat(buf, "\", ");
		b += 8;
	}
	buf[strlen(buf) - 2] = ']';
	buf[strlen(buf) - 1] = 0;
	return buf;
}

// int open(const char*, int);
const char*
trace_open()
{
	static char buf[128];
	uint64 a;
	int b;
	argaddr(0, &a);
	argint(1, &b);

	char tmp[MAX_STR_SHOW + 1];
	if (copyinstr(tmp, a, sizeof(tmp)) != 0) {
		printf("[warning]: trace_open copyinstr error\n");
		return "";
	}
	snprintf(buf, sizeof(buf), "\"%s\", ", tmp);

	if (b & O_CREATE)
		strcat(buf, "O_CREATE|");
	if (b & O_RDWR)
		strcat(buf, "O_RDWR|");
	if (b & O_TRUNC)
		strcat(buf, "O_TRUNC|");
	if (b & O_WRONLY)
		strcat(buf, "O_WRONLY|");
	if (b == O_RDONLY)
		strcat(buf, "O_RDONLY|");
	buf[strlen(buf) - 1] = 0;
	return buf;
}

// int mknod(const char*, short, short);
const char*
trace_mknod()
{
	return "";
}

// int unlink(const char*);
const char*
trace_unlink()
{
	static char buf[64];
	uint64 a;
	argaddr(0, &a);

	char tmp[MAX_STR_SHOW + 1];
	if (copyinstr(tmp, a, sizeof(tmp)) != 0) {
		printf("[warning]: trace_unlink copyinstr error\n");
		return "";
	}
	snprintf(buf, sizeof(buf), "\"%s\"", tmp);
	return buf;
}

// int fstat(int fd, struct stat*);
const char*
trace_fstat()
{
	static char buf[128];
	int a;
	uint64 b;
	argint(0, &a);
	argaddr(1, &b);

	struct stat st;
	if (copyin((char *)&st, b, sizeof(struct stat)) != 0) {
		printf("[warning]: trace_fstat copyin error\n");
		return "";
	}

	char tmp[32] = "";
	if (st.type == T_FILE)
		strcpy(tmp, "T_FILE");
	else if (st.type == T_DIR)
		strcpy(tmp, "T_DIR");
	else if (st.type == T_DEVICE)
		strcpy(tmp, "T_DEVICE");
	
	snprintf(buf, sizeof(buf), "%d, {.dev=%d, .ino=%d, .type=%s, .nlink=%d, .size=%d}", 
		a, st.dev, st.ino, tmp, st.nlink, st.size);
	return buf;
}

// int link(const char*, const char*);
const char*
trace_link()
{
	static char buf[128];
	uint64 a, b;
	argaddr(0, &a);
	argaddr(1, &b);

	char tmp[MAX_STR_SHOW + 1], tmp2[MAX_STR_SHOW + 1];
	if (copyinstr(tmp, a, sizeof(tmp)) != 0) {
		printf("[warning]: trace_link copyinstr error\n");
		return "";
	}
	if (copyinstr(tmp2, b, sizeof(tmp2)) != 0) {
		printf("[warning]: trace_link copyinstr error\n");
		return "";
	}
	snprintf(buf, sizeof(buf), "\"%s\", \"%s\"", tmp, tmp2);
	return buf;
}

// int mkdir(const char*);
const char*
trace_mkdir()
{
	static char buf[64];
	uint64 a;
	argaddr(0, &a);

	char tmp[MAX_STR_SHOW + 1];
	if (copyinstr(tmp, a, sizeof(tmp)) != 0) {
		printf("[warning]: trace_mkdir copyinstr error\n");
		return "";
	}
	snprintf(buf, sizeof(buf), "\"%s\"", tmp);
	return buf;
}

// int chdir(const char*);
const char*
trace_chdir()
{
	static char buf[64];
	uint64 a;
	argaddr(0, &a);

	char tmp[MAX_STR_SHOW + 1];
	if (copyinstr(tmp, a, sizeof(tmp)) != 0) {
		printf("[warning]: trace_chdir copyinstr error\n");
		return "";
	}
	snprintf(buf, sizeof(buf), "\"%s\"", tmp);
	return buf;
}

// int dup(int);
const char*
trace_dup()
{
	static char buf[32];
	int a;
	argint(0, &a);
	snprintf(buf, sizeof(buf), "%d", a);
	return buf;
}

// int getpid(void);
const char*
trace_getpid()
{
	return "";
}

// char* sbrk(int);
const char*
trace_sbrk()
{
	static char buf[32];
	int a;
	argint(0, &a);
	snprintf(buf, sizeof(buf), "%d", a);
	return buf;
}

// int sleep(int);
const char*
trace_sleep()
{
	static char buf[32];
	int a;
	argint(0, &a);
	snprintf(buf, sizeof(buf), "%d", a);
	return buf;
}

// int uptime(void);
const char*
trace_uptime()
{
	return "";
}

// int trace(int);
const char*
trace_trace()
{
	static char buf[32];
	int a;
	argint(0, &a);
	snprintf(buf, sizeof(buf), "%d", a);
	return buf;
}

// int pgaccess(const void*, int, void*);
const char*
trace_pgaccess()
{
	return "";
}

// int system_info(struct system_info*);
const char*
trace_system_info()
{
	return "";
}