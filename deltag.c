/*
 *  deltag : HTMLのタグを除去するフィルタ
 *
 *     98/03/03 V1.00 by oga.
 *     10/11/21 V1.10 support -c option (convert <br><p><hr> &gt; &lt; &amp;)
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

main(a,b)
int a;
char *b[];
{
    int  i, c;
    int  tagin = 0;    /* 1:タグ中 0:タグ外 */
    int  vf = 0;
    int  cf = 0;       /* -c: convert some tag */
    int  pos;
    int  change;
    char *fname = NULL;
    FILE *fp;
    unsigned char buf[4096*4];
    unsigned char work[1024];

    /* arg check */
    for (i = 1; i<a; i++) {
	if (!strncmp(b[i],"-c",2)) {
	    cf = 1;
	    continue;
	}
	if (!strncmp(b[i],"-v",2)) {
	    vf = 1;
	    continue;
	}
	if (!strncmp(b[i],"-h",2)) {
	    printf("usage: deltag [-c] [filename]\n");
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

    while ((c = getc(fp)) != EOF) {
	if (c == '<') {
	    // start of tag
	    tagin = 1;
	    pos = 0;
	    buf[pos++] = c;
	    continue;
	}
	if (tagin) {
	    // store tag string
	    buf[pos++] = c;
	}
	if (c == '>') {
	    // end of tag
	    tagin = 0;
	    buf[pos] = '\0';
	    pos = 0;
	    if (vf) printf("buf=[%s]\n", buf);
	    if (cf) {
	        if (!strcasecmp(buf, "<br>")) {
		    printf("\n");
	        } else if (!strcasecmp(buf, "<p>")) {
		    printf("\n");
	        } else if (!strcasecmp(buf, "<hr>")) {
		    printf("\n--------------------------------------------\n");
	        }
	    }
	    continue;
	}
	if (!tagin) {
	    if (c == '&') {
		change = 0;
		work[0] = getc(fp);      /* g  l  a  */
		work[1] = getc(fp);      /* t  t  m  */
		work[2] = getc(fp);      /* ;  ;  p  */
		work[3] = '\0';
		if (!strcmp(work, "gt;")) {
		    printf(">");
		    change = 1;
		} else if (!strcmp(work, "lt;")) {
		    printf("<");
		    change = 1;
		} else if (!strcmp(work, "amp")) {
		    work[3] = getc(fp);  /*       ;  */
		    work[4] = '\0';
		    if (!strcmp(work, "amp;")) {
		        printf("&");
		        change = 1;
		    }
		}
		if (!change) {
	            putchar(c);          /* &         */
		    /* 変換対象外の場合はそのまま出力 */
		    /* EOFは\0に変更                  */
		    for (i = 0; i<strlen(work); i++) {
			if (work[i] == 0xff) {
			    work[i] = '\0';
			    break;
			}
		    }
		    printf("%s", work);
		}
	    } else {
	        /* タグ以外で、&xxでもなければならそのまま出力 */
	        putchar(c);
	    }
	}
    }
    fclose(fp);
}

