#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#if 0
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

typedef unsigned __int64       uint64;

int main(int a, char *b[])
{
	uint64  aaa = 0;
	int     fd;

	printf("aaa=%I64u\n", aaa);

	fd = open("c:\\a.txt", O_RDONLY);
}
