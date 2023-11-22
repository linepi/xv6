#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int i;
  uint64 para = 0;
  char *nargv[MAXARG];

  if(argc < 3 || ((argv[1][0] < '0' || argv[1][0] > '9') && strcmp(argv[1], "all") != 0)){
    fprintf(2, "Usage: %s mask(all) command\n", argv[0]);
    exit(1);
  }

  if(strcmp(argv[1], "all") == 0){
    para = ~para;
  }else{
    para = atoi(argv[1]);
  }

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