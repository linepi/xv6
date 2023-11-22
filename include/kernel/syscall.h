// System call numbers

#ifdef __ASSEMBLER__
#define BEGIN_ENUM 
#define ENUM_VAL(name, value) .equ name, value
#define END_ENUM
#else
#define BEGIN_ENUM enum {
#define ENUM_VAL(name, value) name = value,
#define END_ENUM };
#endif

BEGIN_ENUM
#define DEF_SYSCALL(id, name) ENUM_VAL(SYS_##name, id)
#include "kernel/syscall_def.h"
#undef DEF_SYSCALL
END_ENUM