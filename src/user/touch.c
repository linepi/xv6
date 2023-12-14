#include "user/user.h"
#include "kernel/fcntl.h"

int main(int argc, char *argv[])
{
	int i;

  if(argc < 2){
    fprintf(2, "Usage: touch files...\n");
    exit(1);
  }

  for(i = 1; i < argc; i++){
    if(open(argv[i], O_CREATE) < 0){
      fprintf(2, "failed to open %s\n", argv[i]);
      break;
    }
  }

  exit(0);
}