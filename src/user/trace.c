#include "kernel/param.h"
#include "common/types.h"
#include "kernel/stat.h"
#include "user/user.h"

char *sys_map[] = {
#define DEF_SYSCALL(id, name) [id] #name,
#include "kernel/syscall_def.h"
};

int
main(int argc, char *argv[])
{
  int i;
  uint64 para = 0;
  char *nargv[MAXARG];

  if(argc < 3){
    fprintf(2, "Usage: %s <sysname[-sysname...]|all> command\n", argv[0]);
    exit(1);
  }

  printf("[trace]: ");
  if (strcmp(argv[1], "all") == 0) {
    para = ~para;
  } else {
    char *token = strtok(argv[1], "-");
    while (token != NULL) {
      for (int i = 1; i < sizeof(sys_map)/sizeof(char*); i++) {
        if (strcmp(sys_map[i], token) == 0) {
          printf("%s ", token);
          para |= (1 << i);
        }
      }
      token = strtok(NULL, "-");
    }
  }
  printf("\n");

  if (trace(para) < 0) {
    fprintf(2, "%s: trace failed\n", argv[0]);
    exit(1);
  }
  
  for(i = 2; i < argc && i < MAXARG; i++){
    nargv[i-2] = argv[i];
  }
  exec(nargv[0], nargv);
  exit(0);
}