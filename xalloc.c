/*
 *	xalloc [delta<100> KB]	: 確保出来る最大のメモリを確保し、アクセスする。
 *
 *		delta : mallocリトライ時の減少サイズ
 *
 *					1995.03.28  V1.00  by hyper halx.oga
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "cur.h"

#define DELTA	1000
#define MAX_MEM	700000*1024

void SigProc(int sig)
{
    printf("signal = %d\n", sig);
    if (sig == SIGINT) exit(1);
}

main(a,b)
int a;
char *b[];
{
	int delta, size = MAX_MEM, i;
	char *adr,buf[100];

	for (i = 0; i<=NSIG; i++) {
	    signal(i, SigProc);
	}

	if (a > 1) {
		if (strcmp(b[1],"-h") == 0) {
			printf("usage : xalloc [delta<100> KB]\n");
			return 1;
		}
		delta = atoi(b[1]) * 1024;
	} else {
		delta = DELTA * 1024;
	}
	cls();
	while ((adr = malloc(size)) == 0) {
		size -= delta;
		locate(1,1);
		printf("alloc size =%6d KB\n",size/1024);
	}
	while (1) {
		locate(1,2);
		printf("                                                       ");
		locate(1,2);
		printf("Access allocated memory!  Enter 'y' => ");
		scanf("%s",buf);
		if (strcmp(buf,"y") == 0) {
			locate(1,3);
			printf("in access... \n");
			for (i=0; i<size; i+=4)
				adr[i] = 1;
			locate(1,3);
			printf("            \n");
		} else {
			break;
		}
	}

	printf("free allocated memory (size = %d KB)\n",size/1024);
	free(adr);
	return 0;
}
