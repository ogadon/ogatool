/*
 *   head : ファイルの先頭を指定した行表示する
 *
 *     2024/10/18 V0.10 by oga.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#define BUF_SIZE  (4096*10) 

int vf = 0;

/* 
 *  HeadTail : 指定ファイルの先頭1行と最後1行を読み込む
 *
 *    IN : fname   : ファイル名
 *    OUT: headbuf : はじめの1行(サイズBUF_SIZE以上)
 */
int Head(char *fname, int line)
{
    FILE   *fp;
    char   buf[BUF_SIZE];
    struct stat stbuf;
    int    filef = 1;   /* 1:file 0:stdin */
    int    cnt   = 0;
    int    line2 = 0;   /* abs(line) */

    if (!strcmp(fname, "-")) {
	filef = 0;
    } else if (stat(fname, &stbuf) < 0) {
        perror(fname);
        return -1;
    }

    if (filef) {
        if ((fp = fopen(fname, "r")) == NULL) {
            perror("fopen");
            return -2;
        }
    } else {
	fp = stdin;
    }

    if (line < 0) {
        line2 = -line;
    } else {
        line2 = line;
    }

    while (fgets(buf, BUF_SIZE, fp)) {
        if (line < 0) {
	    /* 先頭n行 */
            if (cnt < line2) {
	        printf("%s", buf);
	    } else {
	        break;
	    }
	} else {
	    /* 指定行以降を出力 */
            if (cnt >= line2) {
	        printf("%s", buf);
	    }
	}
	++cnt;
    }

    fclose(fp);

    return 0;
}

void usage()
{
    printf("usage: head [{-<n> | +<n>}] [<filename>]\n");
}

int main(int a, char *b[])
{
    char headbuf[BUF_SIZE];
    char tailbuf[BUF_SIZE];
    char *fname = NULL;
    int  i;
    int  line = -10;      /* default 先頭10行 */

    /* arg check */
    for (i = 1; i<a; i++) {
	if (!strncmp(b[i],"-v",2)) {
	    vf = 1;
	    continue;
	}
	if (b[i][0] == '-' || b[i][0] == '+') {
	    line = atoi(b[i]);
	    continue;
	}
	if (!strncmp(b[i],"-h",2)) {
            usage();
	    exit(1);
	}
	fname = b[i];
    }

    if (line == 0) {
        usage();
        exit(1);
    }

    if (fname == NULL) {
        /* usage(); */
        /* exit(1); */
	fname = "-";
    }

    Head(fname, line);

    return 0;
}
