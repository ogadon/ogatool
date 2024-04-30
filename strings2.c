/*
 *  strings2 : Š¿Žš‘Î‰žstrings
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ISKANJI(c)  ((c >= 0x80 && c < 0xa0) || (c >= 0xe0 && c<0xff))
#define ISKANJI2(c) ((c >= 0x40 && c <= 0xfc))
#define ISHANKANA(c) ((c >= 0xa0 && c <= 0xdf))

main(a,b)
int a;
char *b[];
{
    char *fname = NULL;
    FILE *fp;
    int  i,c;
    unsigned char buf[4096];
    int  kf  = 0;
    int  num = 4;
    int  hk  = 0;
    int  vf  = 0;            /* -v: verbose */

    /* arg check */
    for (i = 1; i<a; i++) {
	if (!strncmp(b[i],"-n",2)) {
	    num = atoi(b[++i]);
	    continue;
	}
	if (!strncmp(b[i],"-hk",3)) {
	    hk = 1;
	    continue;
	}
	if (!strncmp(b[i],"-v",2)) {
	    ++vf;
	    continue;
	}
	if (!strncmp(b[i],"-h",2)) {
	    printf("usage: strings2 [filename] [-n <num(4)>] [-hk]\n");
	    printf("       -n : •¶Žš—ñ‚Æ‚µ‚Ä”»’f‚·‚é‚½‚ß‚Ì˜A‘±•¶Žš”\n");
	    printf("       -hk: ”¼ŠpƒJƒi‚à‘ÎÛ‚Æ‚·‚é\n");
	    exit(1);
	}
	fname = b[i];
    }

    /* open file */
    if (fname == NULL) {
	fp = stdin;
    } else {
	if ((fp = fopen(fname,"r")) == 0) {
	    perror(b[0]);
	    exit(1);
	}
    }
    i = 0;
    while((c = getc(fp)) != EOF) {
	if (isprint(c)) {
	    kf = 0;
	    buf[i++] = c;
	} else if (!kf && ISKANJI(c)) {
	    kf = 1;
	    buf[i++] = c;
	} else if (kf && ISKANJI2(c)) {
	    kf = 0;
	    buf[i++] = c;
	} else if (hk && ISHANKANA(c)) {
	    kf = 0;
	    buf[i++] = c;
	} else {
	    buf[i] = '\0';
	    if (i >= num) {
		if (vf) {
		    printf("[%s]\n",buf);
		} else {
		    printf("%s\n",buf);
		}
	    }
	    i = 0;
	}
    }
}

/* vim:ts=8:sw=8:
 */


