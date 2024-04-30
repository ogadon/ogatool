/*
 *   SJIS‘Î‰žfold 
 *
 *      08/01/17 V0.10 by oga.
 *      14/12/31 V0.11 fix arg bug.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ISKANJI(c)  (((unsigned char)(c) >= 0x80 && (unsigned char)(c) < 0xa0) || \
                     ((unsigned char)(c) >= 0xe0 && (unsigned char)(c) < 0xff))

int main(int a, char* b[])
{
    int  i;
    int  cnt;
    int  width = 80;
    int  kf = 0;
    int  vf = 0;
    char *fname = NULL;
    unsigned char buf[100000];
    FILE *fp;

    for (i = 1; i < a; i++) {            /* V0.11-C */
        if (!strcmp(b[i], "-h")) {
	    printf("usage: fold [-w <width>] [<filename>]\n");
	    exit(1);
	}
        if (!strcmp(b[i], "-v")) {
	    vf = 1;
	    continue;
	}
        if (i+1 < a && !strcmp(b[i], "-w")) {
	    width = atoi(b[++i]);
	    continue;
	}
        fname = b[i];
    }

    if (fname != NULL) {
        if ((fp = fopen(fname, "r")) == NULL) {
	    perror(fname);
	    exit(1);
	}
    } else {
        fp = stdin;
    }

    while (fgets(buf, sizeof(buf), fp)) {
    	cnt = 0;
	kf  = 0;
    	for (i = 0; i < strlen(buf); i++) {
	    if (buf[i] == '\t') {
	    	cnt += (8-(i & 8));
	    }
	    if (kf == 0 && ISKANJI(buf[i])) {
	    	kf = 1;
	    } else {
	    	kf = 0;
	    }
	    if (!vf) putchar(buf[i]);
	    if (vf) printf("kf:%d cnt=%d >= %d\n", kf, cnt, width - 1 - (ISKANJI(buf[i+1])?1:0) );
	    if (kf == 0 && (cnt >= width - 1 - (ISKANJI(buf[i+1])?1:0))) {
	    	if (buf[i] != '\n') putchar('\n');
	    	cnt = 0;
		continue;
	    }
	    ++cnt;
	}
    }

    if (fp != stdin) fclose(fp);

}

/* vim:ts=8:sw=4:
 */
