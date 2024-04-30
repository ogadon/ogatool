/*
 *   headtail : 先頭1行と最終行一行を表示する
 *
 *     2003/07/18 V1.00 by oga.
 *     2007/05/14 V1.01 support stdin
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#define LAST_SIZE 10000   /* ファイルの最後を読み込むサイズ */
#define BUF_SIZE  4096    /* V1.01-A */

int vf = 0;

/* 
 *  HeadTail : 指定ファイルの先頭1行と最後1行を読み込む
 *
 *    IN : fname   : ファイル名
 *    OUT: headbuf : はじめの1行(サイズBUF_SIZE以上)
 *    OUT: tailbuf : 最後の1行  (サイズBUF_SIZE以上)
 */
int HeadTail(char *fname, char *headbuf, char *tailbuf)
{
    FILE *fp;
    char buf[BUF_SIZE];
    struct stat stbuf;
    int    filef = 1;   /* 1:file 0:stdin V1.01-A */

    headbuf[0] = '\0';
    tailbuf[0] = '\0';

    /* V1.01-A start */
    if (!strcmp(fname, "-")) {
	filef = 0;
    } else 
    /* V1.01-A end   */
    if (stat(fname, &stbuf) < 0) {
        perror(fname);
        return -1;
    }

    if (filef) {     /* V1.01-A */
        if ((fp = fopen(fname, "r")) == NULL) {
            perror("fopen");
            return -2;
        }
    } else {         /* V1.01-A */
	fp = stdin;  /* V1.01-A */
    }                /* V1.01-A */

    fgets(headbuf, BUF_SIZE, fp);          /* 1行目取得 */
    if (filef) {
        if (stbuf.st_size > LAST_SIZE) {
            /* 最後の10KBの位置にseek */
	    if (vf) printf("seeking...\n");
            if (fseek(fp, stbuf.st_size - LAST_SIZE, SEEK_SET) < 0) {
	        perror("fseek");
	    }
        }

        fgets(buf, sizeof(buf), fp);       /* ゴミの行を空読み */
    }

    while (fgets(tailbuf, BUF_SIZE, fp)) {
        ;
    }

    fclose(fp);

    return 0;
}

void usage()
{
    printf("usage: headtail [<filename>]\n");
}

int main(int a, char *b[])
{
    char headbuf[BUF_SIZE];
    char tailbuf[BUF_SIZE];
    char *fname = NULL;
    int  i;

    /* arg check */
    for (i = 1; i<a; i++) {
	if (!strncmp(b[i],"-v",2)) {
	    vf = 1;
	    continue;
	}
	if (!strncmp(b[i],"-h",2)) {
            usage();
	    exit(1);
	}
	fname = b[i];
    }

    if (fname == NULL) {
        /* usage(); */
        /* exit(1); */
	fname = "-";   /* V1.01-C */
    }

    HeadTail(fname, headbuf, tailbuf);

    printf("%s", headbuf);
    printf("%s", tailbuf);

    return 0;
}
