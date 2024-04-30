/*
 *	malloc : 指定サイズをmallocしてアクセスする
 *
 *		1999.12.27  V1.00  by hyper halx.oga
 *		2010.03.04  V1.01  support -l (loop option), -w
 */

#ifdef _WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#ifndef _WIN32
#define Sleep(x)  usleep((x)*1000)
#endif

int  lf       = 0;    /* -l: loop */
int  wait_sec = 30;   /* -w: 30sec */

void usage()
{
    printf("usage : malloc <malloc_size[MB]> [-l [-w <wait_sec(%d)>]]\n", wait_sec);
    printf("        -l: access loop\n");
    printf("        -w: loop wait time\n");
	exit(1);
}

main(a,b)
int a;
char *b[];
{
	char *adr;
	int  i;
	int  size     = 0;
	int  cnt      = 0;
	char buf[1024];

	for (i = 1; i<a; i++) {
		if (!strcmp(b[i], "-h")) {
		    usage();
		}
		if (!strcmp(b[i], "-l")) {
		    lf = 1;
			continue;
		}
		if (!strcmp(b[i], "-w") && a>i+1) {
		    wait_sec = atoi(b[++i]);
			continue;
		}
		size = atoi(b[i]) * 1024 * 1024;   /* KB→MB V1.01-C */
	}

	if (size == 0) {
		usage();
	}

	printf("malloc %d MB\n", size/1024/1024);
	if ((adr = malloc(size)) == NULL) {
		printf("malloc error %d\n",errno);
		return 1;
	}
	if (lf) {
		printf("wait sec: %d sec\n", wait_sec);
		while (1) {
			printf("access %d ....\n", ++cnt);
			for (i = 0; i<size; i++) {
	    		adr[i] = 'a';
			}
			Sleep(wait_sec*1000);  /* 10sec */
		}
	} else {
		printf("access ....\n");
		for (i = 0; i<size; i++) {
	    	adr[i] = 'a';
		}
	}
	printf("enter 'exit' to free : ");
	fflush(stdout);
	scanf("%s",buf);
	printf("free memory ...\n");
	if (adr) free(adr);
	return 0;
}


