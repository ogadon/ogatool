/*
 *  slow.c
 *    Ç‰Ç¡Ç≠ÇËï\é¶Ç∑ÇÈ
 *
 *    2003/12/05 V0.10 by oga.
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
int main(int a, char *b[])
{
    char *fname = NULL;
    FILE *fp;
    int  vf = 0;
    int  i;
    int  wtime = 100;   /* msec */
    char buf[8192];

    /* arg check */
    for (i = 1; i<a; i++) {
	if (!strncmp(b[i],"-v",2)) {
	    vf = 1;
	    continue;
	}
	if (i+1 < a && !strncmp(b[i],"-t",2)) {
	    wtime = atoi(b[++i]);
	    continue;
	}
	if (!strncmp(b[i],"-h",2)) {
	    printf("usage: slow [filename] [-t <wait_time(100ms)>\n");
	    exit(1);
	}
	fname = b[i];
    }

    /* open file */
    if (fname == NULL) {
	fp = stdin;
    } else {
	if ((fp = fopen(fname,"r")) == 0) {
	    perror(fname);
	    exit(1);
	}
    }

    /* èàóù */
    while (fgets(buf, sizeof(buf), fp)) {
        printf("%s", buf);
	fflush(stdout);
	usleep(wtime*1000);
    }

    if (fp != stdin) fclose(fp);

}

/* vim:ts=8:sw=8:
 */

