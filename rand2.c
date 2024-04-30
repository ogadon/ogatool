/*
 *   rand : ランダムテスト 
 *
 *     2006/05/14 V0.10 by oga.
 *     2014/01/01 V0.11 fix win prob.
 */

#ifdef _WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* macros */
#define RAND(x)     	((rand() & 0xffff)*(x)/(RAND_MAX & 0xffff))

/* globals */
int  vf    = 0;            /* -v     */

void Usage()
{
    printf("usage: rand [<num(100)>]\n");
}

int main(int a, char *b[])
{
    int    i;
    int    max = 100;
    time_t tt;

#ifdef _WIN32
    SYSTEMTIME syst;
#endif

    for (i=1;i<a;i++) {
	if (!strncmp(b[i],"-v",2)) {
	    ++vf;
	    continue;
	}
	if (!strncmp(b[i],"-h",2)) {
	    Usage();
	    exit(1);
	}
	max = atoi(b[i]);
    }

#ifndef _WIN32
	/* Windowsではイマイチ */
    tt = time(0);

#else  /* _WIN32 */
    GetLocalTime(&syst);
    tt = syst.wHour * 3600 * 1000 +
         syst.wMinute * 60 * 1000 +
         syst.wSecond      * 1000 +
	 syst.wMilliseconds;
#endif /* _WIN32 */

    srand(tt);

	if (sizeof(time_t) == sizeof(int)) {
    	printf("tt:%d RAND(%d):%d\n", tt, max, RAND(max));
	} else {
#ifdef _WIN32
    	printf("tt:%I64u RAND(%d):%d\n", tt, max, RAND(max));
#else
    	printf("tt:%llu RAND(%d):%d\n", tt, max, RAND(max));
#endif
	}

    return 0;
}



