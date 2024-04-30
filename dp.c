/*
 *  dp [-k] <filename> : <filename>を16進ダンプする。
 *
 *    1992.      V1.00 Initial Revision.  by halx.oga
 *    1995.01.30 V1.01 kanji file support.
 *    1995.07.20 V1.02 bug fix and add usage
 *    1997.08.13 V1.03 -r support
 *    2004.01.13 V1.10 Performance improvement (x20)
 *                     dp /bin/ls  0.61sec => 0.03sec
 *    2007.02.25 V1.11 support Large File
 *    2007.02.26 V1.12 support -s option
 */

#ifndef _WIN32
#define _LARGEFILE_SOURCE
#define _FILE_OFFSET_BITS 64
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define READ_SIZE (1024)

/*
 *  dp output file to original file (V1.03)
 *
 */
void DpToFile(FILE *fp)
{
    char buf[256];
    char wk[5];
    int  i,j,c;

    strcpy(wk,"0x");
    wk[4] = '\0';
    while (fgets(buf,sizeof(buf),fp)) {
        if (buf[0] != '0') {
            /* no data line */
            continue;
        }
        for (i=0; i<4; i++) {
            for (j=0; j<4; j++) {
                if (isspace(buf[10+i*9+j*2])) {
                    /* data end */
                    return;
                }
                strncpy(&wk[2],&buf[10+i*9+j*2],2);
		c = strtoul(wk,(char **)0,0);
		putchar(c);
            }
        }
    }
}

main(int a, char *b[])
{
    FILE *fp;
    int c, xx, addr = 0, i;
    int f=0, f2=0;
    int kflag = 0;
    int fflag = 0;    /* -f flush */
    int rev   = 0;    /* 1:dpの出力結果を元のファイルに戻す */
    int size  = 0;
    int work;
    unsigned int st_pos = 0;        /* start pos V1.12-A */

    int pos   = 0;                  /* for read */
    unsigned char buf[READ_SIZE+1]; /* for read */
    int opos  = 0;                  /* for write */
    unsigned char obuf[1024];       /* for write */

    char asc[17];                   /* for ASCII dump */
    char *filename = NULL;

    kflag = 0;		/* 漢字出力 */
    fp = stdin;		/* ファイル名がなければ標準入力から */

    for (i = 1;i < a; i++) {
	    if (!strcmp(b[i],"-h")) {
		printf("usage : dp [-k] [-r] [-s <start_pos>] [<filename>]\n");
		printf("        -k : kanji output\n");
		printf("        -f : fflush any bytes\n");
		printf("        -r : dp output to binary file\n");
		printf("        -s : dump start position\n");
		exit(1);
	    } 
	    if (!strcmp(b[i],"-k")) {
		kflag = 1;
		continue;
	    }
	    if (!strcmp(b[i],"-f")) {
		fflag = 1;
		continue;
	    }
	    if (!strcmp(b[i],"-r")) {
		rev = 1;
		continue;
	    }
	    if (i+1 < a && !strcmp(b[i],"-s")) {  /* V1.12-A */
		if (!strncmp(b[++i], "0x", 2)) {
		    st_pos = strtoul(b[i], (char **)NULL, 0);
		} else {
		    st_pos = atoi(b[i]);
		}
		continue;
	    }
	    if (filename) {
	        printf("Warning : Too many files\n");
	    }
	    filename = b[i];
    }
    if (filename && !(fp = fopen(filename,"rb"))) {
	perror("dp: fopen");
	exit(1);
    }

    if (rev) {
        DpToFile(fp);
        exit(0);
    }

    if (st_pos) {  /* V1.12-A */
	int ret;
	ret = fseek(fp, st_pos, SEEK_SET);
	if (ret < 0) {
	    printf("fseek error st_pos=%d errno=%d\n", st_pos, errno);
	    st_pos = 0;
	}
    }
	
    printf("Location: +0       +4       +8       +C       /0123456789ABCDEF\n");

    /* c = getc(fp); */
    /***** (original getc(first)) *****/
    size = fread(buf, 1, READ_SIZE, fp);
    pos  = 0;
    if (size <= 0) {
        c = EOF;
    } else {
        c = buf[pos++];
    }
    /**********************************/

    opos = 0;
    while(c != EOF) {
	xx = 0;
	strcpy(asc,"                ");
	printf("%08x: ",addr + st_pos);    /* V1.12-C */
	f = 0;
	while(c != EOF && xx < 16) {
	    if (fflag) {
	        printf("%02x",c);          /* これが実行時間の60% */
	        fflush(stdout);            /* 2.2倍くらい遅くなる */
	    } else {
	        work = c >> 4;
	        obuf[opos++] = ((work > 9)?work-10+'a':work+'0');
	        work = c & 0x0f;
	        obuf[opos++] = ((work > 9)?work-10+'a':work+'0');
	    }

	    if (c < 32) {
		if (f) {
		    asc[xx] = c;
		    if (c == 10 || c == 13) {
			asc[xx] = '.'; /* 暫定 */
		    }
		} else {
		    asc[xx] = '.';
		}
		f = 0;
	    } else if (c >= 127 ) {
		if (kflag) {
		    if (f) {
			asc[xx] = c;
			f = 0;
		    } else {
			asc[xx] = c;
			f = 1;
		    }
		} else {
		    asc[xx] = '.';
		}
	    } else {
		asc[xx] = c;
		f = 0;
	    }
	    if (xx == 0 && f2) {
		asc[xx] = '.';
		f = 0;
		f2 = 0;
	    }
	    if ((xx % 4) == 3) {
	        if (fflag) {
	            printf(" ");
		} else {
	            obuf[opos++] = ' ';
		}
	    }
	    ++xx;
	    ++addr;

	    /* c = getc(fp); */
            /***** (original getc(next))  10%up *****/
	    if (pos >= size) {
                size = fread(buf, 1, READ_SIZE, fp);
                pos  = 0;
                if (size <= 0) {
                    c = EOF;
                } else {
                    c = buf[pos++];
                }
	    } else {
                c = buf[pos++];
	    }
	    /****************************************/

	    if (f == 1 && xx >= 16) {
		asc[xx++] = c;
		f = 0;
		f2 = 1;
	    }
	}
	if (!fflag) {
	    obuf[opos] = '\0';
	    printf("%s", obuf);
	    opos = 0;
	}

	while (xx <16) {
		printf("  ");
		if ((xx % 4) == 3) printf(" ");
		++xx;
	}
	asc[xx]='\0';

	printf("/%16s \n",asc);
    }
}

/* vim:ts=8:sw=8:
 */

