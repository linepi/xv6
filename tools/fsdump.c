// decode fs.img, dump to a directory

#include <stdio.h>

void
Main(char *img, char *destdir)
{
	// to be implement
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(2, "usage: fsdump <image> <dest dir>\n");
		return 0;
	}
	char *img = argv[1];
	char *destdir = argv[2];
	Main(img, destdir);
	return 0;
}