/*
 *  tee for Windows
 *
 *    07/01/07 V1.00 by oga.
 *
 */

#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VER "V1.00"

void usage()
{
    printf("tee %s by oga.\n", VER);
    printf("usage: tee [-a] <filename>\n");
    exit(1);
}

int main(int a, char *b[])
{
    int  i;
    int  cnt;                   /* line count    */
    int  vf = 0;		/* -v: verbose   */
    char *filename = NULL;
    char *openopt = "w";
    FILE *fpin, *fpout;
    char buf[1024];

    for (i = 1; i<a; i++) {
        if (!strcmp(b[i],"-a")) {
            openopt = "a";
            continue;
        }
        if (!strcmp(b[i],"-h")) {
            usage();
        }
        if (!strcmp(b[i],"-v")) {
            vf = 1;
            continue;
        }
        filename = b[i];
    }

    if (filename == NULL) {
	usage();
    }

    fpin = stdin;

    if (!(fpout = fopen(filename, openopt))) {
        perror("fopen");
        exit(1);
    }

    cnt = 0;
    while (fgets(buf, sizeof(buf), fpin)) {
	if (vf) {
	    printf("##%5d:[%s]\n", cnt, buf);
	} else {
	    printf("%s", buf);
	}
	fflush(stdout);
        fprintf(fpout, "%s", buf);
	++cnt;
    }

    fclose(fpout);
}

/* vim:ts=8:sw=4:
 */

