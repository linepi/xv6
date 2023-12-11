#include "user/user.h"
#include "kernel/fcntl.h"
#include "kernel/fs.h"
#include "kernel/param.h"
#include "kernel/stat.h"

// to be implemented
int
du(char *path, int depth)
{
	struct stat st;
  if (stat(path, &st) < 0) {
    fprintf(2, "du: %s failed to stat\n", path);
    return -1;
  }
  if (st.type == T_FILE) {
  } else if (st.type == T_DIR) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
      fprintf(2, "du: open %s failed\n", path);
      return -1;
    }
    struct dirent de;	
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
			if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) 
				continue;
      strcat(path, "/");
      strcat(path, de.name);
      int rc = du(path, depth);
      strrchr(path, '/')[0] = 0;
      if (rc < 0) {
        close(fd);
        return -1;
      }
    }
    close(fd);
  }
  return 0;
}

int main(int argc, char **argv)
{
	int i;

  if (argc < 2) {
    fprintf(2, "Usage: du files...\n");
    exit(1);
  }

  for(i = 1; i < argc; i++){
		char buf[MAXPATH];
		strcpy(buf, argv[i]);
    if (du(buf, 0) < 0) 
      exit(1);
  }
  exit(0);
}