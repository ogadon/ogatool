/*
 *   sosu.c : Get prime number up to 10000
 *
 *     97/08/27 V1.01 time support
 *     97/09/04 V1.02 usage changed.
 *     99/01/14 V1.03 add S'Mark99
 *     99/12/10 V1.04 fflush for -v
 *     13/12/24 V1.05 delete SJIS string
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LIM	10000
#define VER	"1.05"

main(a,b)
int a;
char *b[];
{
	int	i,x;
	int	st_tm, en_tm;
	int	vf = 0;
	int	flush = 0;
	int	lim = LIM;
	unsigned long counter=0;

	printf("sosu V%s by oga.\n",VER);
	for (i=1; i<a; i++) {
	    if (!strncmp(b[i],"-h",2)) {
		printf("usage: sosu [-v] [num<%d>]\n",LIM);
		exit(1);
            }
	    if (!strncmp(b[i],"-v",2)) {
		vf = 1;
		continue;
            }
	    if (!strncmp(b[i],"-f",2)) {
		flush = 1;
		continue;
            }
	    lim = atoi(b[i]);
	}
	printf("Sosu Start(%d)...\n",lim);

	st_tm = clock();
	for (x=2; x<=lim; x++) {
	    for (i=2; i<x; i++) {
		counter++;
		if((x % i) == 0) {
		    break;
		}
	    }
	    if (x == i) {
	        if (vf) {
		    printf(" %d",x);
		    if (flush) fflush(stdout);
		}
	    }
	}
	en_tm = clock();
	printf("\nMAX %d: CALC=%lu TIME=%3.3f sec\n",
				lim,
				counter,
				(float)(en_tm-st_tm)/CLOCKS_PER_SEC);

	if (lim == 10000) printf("\nS'Mark99 => %d\n\n",(100*CLOCKS_PER_SEC)/(en_tm-st_tm));

	return 0;
}
