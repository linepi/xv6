#include "user/user.h"

// cp then delete
int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(2, "usage: mv <src> <dst>\n");
		exit(1);
	}


	int pid = fork();
	if (pid < 0) {
		fprintf(2, "fork failed\n");
		exit(1);
	} else if (pid == 0) {
		char *args[] = { "/cp", argv[1], argv[2], 0 };
		int rc = exec("/cp", args);
		exit(rc);
	}

	int state;
	wait(&state);

	if (state != 0) 
		exit(state);

	if (unlink(argv[1]) < 0) {
		fprintf(2, "failed to delete %s\n", argv[1]);
		exit(1);
	}
	exit(0);
}