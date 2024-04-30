#ifdef _WIN32
#include <windows.h>
#endif /* Linux */

#include <stdio.h>
#include <fcntl.h>
#if 0
#include <sys/stat.h>
#include <sys/types.h>
#endif

#ifdef _WIN32
typedef unsigned __int64       uint64;
#else  /* Linux */
typedef unsigned long long int uint64;
#endif /* Linux */

int main(int a, char *b[])
{
	uint64  aaa = 0;
	uint64 a1;
	uint64 a2;
	int     fd;

	a1  = 1024*1024;
	a2  = 1024*8;
	aaa = a1 * a2;

#ifdef _WIN32
	printf("large value aaa = %I64u\n", aaa);
#else  /* Linux */
	printf("large value aaa = %llu\n", aaa);
	aaa = (uint64)1024 * 1024 * 1024 * 16;
	printf("large value = %llu\n", aaa);
#endif /* Linux */

	/* fd = open("c:\\a.txt", O_RDONLY); */
}


