#include <stdio.h>
#include <stdlib.h>

int main(int a, char *b[])
{
	int i;
	int ms = 0;

	for (i = 1; i < a; i++) {
		if (!strcmp(b[i], "-h")) {
			printf("usage: usleep <ms>\n");
			return 1;
		}
		ms = atoi(b[i]);
	}

	usleep(ms * 1000);

	return 0;
}
