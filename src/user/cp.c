#include "user/user.h"
#include "kernel/fcntl.h"
#include "kernel/fs.h"
#include "kernel/param.h"
#include "kernel/stat.h"

// copy from srcpath to dstpath
// if src is file, dst assume to be a file
// if src is dir, dst assume to be a dir
// dstpath must not be a file or dir currently
int 
copy(char *srcpath, char *dstpath)
{
	struct stat srcst;
	int srcfd, dstfd;

	srcfd = open(srcpath, O_RDONLY);
	if (srcfd < 0) {
		fprintf(2, "open %s failed\n", srcpath);
		return -1;
	}

	if (fstat(srcfd, &srcst) < 0) {
		fprintf(2, "stat fd %d failed\n", srcfd);
		close(srcfd);
		return -1;
	}

	dstfd = open(dstpath, O_WRONLY);
	if (dstfd >= 0) {
		fprintf(2, "%s opened before\n", dstpath);
		close(srcfd);
		return -1;
	}

	if (srcst.type == T_FILE) {
		char buf[512];
		int n;
		dstfd = open(dstpath, O_CREATE | O_WRONLY);
		if (dstfd < 0) {
			fprintf(2, "open %s failed\n", dstpath);
			close(srcfd);
			return -1;
		} 
		while((n = read(srcfd, buf, sizeof(buf))) > 0) {
			if (write(dstfd, buf, n) != n) {
				fprintf(2, "write error\n");
				return -1;
			}
		}
	} else if (srcst.type == T_DIR) {
		if (mkdir(dstpath) < 0) {
			fprintf(2, "failed to create dir %s\n", dstpath);
			close(srcfd);
			return -1;
		}
		dstfd = open(dstpath, 0);
		if (dstfd < 0) {
			fprintf(2, "open dir %s failed\n", dstpath);
			close(srcfd);
			return -1;
		} 

		struct dirent de;	
    while(read(srcfd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
			if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) 
				continue;

			strcat(srcpath, "/");
			strcat(srcpath, de.name);
			strcat(dstpath, "/");
			strcat(dstpath, de.name);
			int rc = copy(srcpath, dstpath);
			strrchr(srcpath, '/')[0] = 0;
			strrchr(dstpath, '/')[0] = 0;
			
			if (rc < 0) {
				close(srcfd);
				close(dstfd);
				return -1;
			}
    }
	}
	close(srcfd);
	close(dstfd);
	return 0;
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(2, "usage: cp <src> <dst>\n");
		exit(1);
	}
	struct stat srcst, dstst;
	char srcpath[MAXPATH], dstpath[MAXPATH];
	strcpy(srcpath, argv[1]);
	strcpy(dstpath, argv[2]);

	if (stat(srcpath, &srcst) < 0) {
		fprintf(2, "stat %s failed\n", srcpath);
		exit(1);
	}

	if (stat(dstpath, &dstst) < 0) {
		// dst not exist
		if (copy(srcpath, dstpath) < -1)
			exit(1);
	} else {
		if (dstst.type == T_FILE) {
			if(unlink(dstpath) < 0){
				fprintf(2, "%s failed to delete\n", argv[2]);
				exit(1);
			}
		} else if (dstst.type == T_DIR) {
			strcat(dstpath, "/");
			char *lastname;
			if ((lastname = strrchr(argv[1], '/')) == NULL)
				strcat(dstpath, argv[1]);
			else
				strcat(dstpath, lastname);
			int exist = 0;
			int fd;
			if ((fd = open(dstpath, O_RDONLY)) >= 0) {
				close(fd);	
				exist = 1;
			}
			if(exist && unlink(dstpath) < 0){
				fprintf(2, "%s failed to delete\n", dstpath);
				exit(1);
			}
    } else {
			fprintf(2, "can not overrite device\n", argv[2]);
			exit(1);
		}

		if (copy(srcpath, dstpath) < -1)
			exit(1);
	}
	exit(0);
}