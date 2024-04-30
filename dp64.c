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
 *    2007.03.03 V1.13 bigfile support
 *    2014.02.02 V1.14 support MSVC6
 *    2014.02.10 V1.15 fix -r
 */

#define DP_LARGE_FILE

#ifndef _WIN32
/* linux */
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
//#ifndef _WIN32
#include <fcntl.h>
//#endif


#ifdef DP_LARGE_FILE
/* linux large file */
typedef int   PFILE;  /* for open64 */

#ifdef _WIN32
typedef __int64 off64_t;
typedef __int64 uint64;
#define FSEEK            _lseeki64
#define FOPEN(x,y)       open((x), O_RDONLY)

#else  /* linux */
typedef unsigned long long uint64;
#define FSEEK            lseek64
#define FOPEN(x,y)       open64((x), O_RDONLY)
#endif

#define FREAD(x,y,z,fp)  read(fp, (x), (y)*(z))
#define FCLOSE           close


#else  /* !DP_LARGE_FILE */
/* win32 or normal file */
typedef FILE* PFILE;  /* for fopen  */
typedef unsigned int uint64;
#define FOPEN  fopen
#define FREAD  fread
#define FSEEK  fseek
#define FCLOSE fclose
#endif

#define READ_SIZE (1024)
#define START_POS 12         /* data start position */

int  vf = 0;   /* -v: verbose */

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
                if (isspace(buf[START_POS+i*9+j*2])) {
                    /* data end */
                    return;
                }
                strncpy(&wk[2],&buf[START_POS+i*9+j*2],2);
		c = strtoul(wk,(char **)0,0);
		putchar(c);
            }
        }
    }
}

main(int a, char *b[])
{
    PFILE fp;
    int c, xx, i;
    int f=0, f2=0;
    int kflag = 0;
    int fflag = 0;    /* -f flush */
    int rev   = 0;    /* 1:dpの出力結果を元のファイルに戻す */
    int size  = 0;
    int work;

    uint64  addr = 0;
    /*off64_t st_pos = 0x91770000;     start pos V1.12-A */
    off64_t st_pos = 0x00000000;    /* start pos V1.12-A */
    uint64  wk64   = 0;
    unsigned int *wk64val = (unsigned int *)&wk64;

    int pos   = 0;                  /* for read */
    unsigned char buf[READ_SIZE+1]; /* for read */
    int opos  = 0;                  /* for write */
    unsigned char obuf[1024];       /* for write */

    char asc[17];                   /* for ASCII dump */
    char *filename = NULL;

    kflag = 0;		/* 漢字出力 */
#ifdef DP_LARGE_FILE
    fp = 0;		/* ファイル名がなければ標準入力から */
#else
    fp = stdin;		/* ファイル名がなければ標準入力から */
#endif

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
	    if (!strcmp(b[i],"-v")) {
		++vf;
		continue;
	    }
	    if (i+1 < a && !strcmp(b[i],"-s")) {  /* V1.12-A */
		if (!strncmp(b[++i], "0x", 2)) {
#ifdef _WIN32
                    if (strlen(b[i]) <= 10) {
                        st_pos = strtoul(b[i], (char **)NULL, 0);  /* 0xnnnnnnnn */
                    } else {
                        /* 0xmmmmnnnnnnnn */
                        char wk[256];

                        memset(wk, 0, sizeof(wk));
                        strcpy(wk, "0x");
                        strncpy(&wk[2], &b[i][2], strlen(b[i]) - 10); /* wk     <= 0xmmmm         */
                        st_pos = strtoul(wk, (char **)NULL, 0);       /* st_pos <= 0xmmmm         */
		        if (vf) printf("wk : %s st_pos=%I64u\n", wk, st_pos);
                        st_pos = st_pos << 32;                        /* st_pos :  0xmmmm00000000 */
		        if (vf) printf("wk : %s st_pos=%I64u\n", wk, st_pos);
                        strncpy(&wk[2], &b[i][strlen(b[i]) - 8], 8);  /* wk     <= 0xnnnnnnnn     */
                        st_pos += strtoul(wk, (char **)NULL, 0);      /* st_pos += 0xnnnnnnnn     */
                    }
		    if (vf) printf("st_pos = %I64u\n", st_pos);
#else
                    st_pos = strtoull(b[i], (char **)NULL, 0);
		    if (vf) printf("st_pos = %llu\n", st_pos);
#endif
		} else {
		    st_pos = (uint64) atoi(b[i]);
		}
		continue;
	    }
	    if (filename) {
	        printf("Warning : Too many files\n");
	    }
	    filename = b[i];
    }

    if (rev) {
	FILE *fp2 = stdin; /* 指定がなければ標準入力 */
        if (filename && !(fp2 = fopen(filename,"rb"))) {
	    perror("dp: fopen");
	    exit(1);
        }
        DpToFile(fp2);
	fclose(fp2);
        exit(0);
    }

    if (filename && !(fp = FOPEN(filename,"rb"))) {
	perror("dp: fopen");
	exit(1);
    }

    if (st_pos) {  /* V1.12-A */
	uint64 ret;
	ret = FSEEK(fp, st_pos, SEEK_SET);
	if (ret < 0) {
#ifdef DP_LARGE_FILE
	    wk64 = ret;
	    printf("fseek error ret=%02x%08x errno=%d\n",
			     wk64val[1], wk64val[0], errno);
	    wk64 = st_pos;
	    printf("fseek error st_pos=%02x%08x\n", wk64val[1], wk64val[0]);
#else
	    printf("fseek error ret=%d st_pos=%d errno=%d\n", ret, st_pos, errno);
#endif
	    st_pos = 0;
	}
    }
	
    printf("Location  : +0       +4       +8       +C       /0123456789ABCDEF\n");

    /* c = getc(fp); */
    /***** (original getc(first)) *****/
    size = FREAD(buf, 1, READ_SIZE, fp);
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
#ifdef DP_LARGE_FILE
	wk64 = addr + st_pos;
	printf("%02x%08x: ", wk64val[1], wk64val[0]);
#else
	printf("%08x: ",addr + st_pos);    /* V1.12-C */
#endif
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
                size = FREAD(buf, 1, READ_SIZE, fp);
                pos  = 0;
                if (size <= 0) {
                    if (size == 0) {
                        if (vf >= 2) printf("<EOF>");
                    } else {
                        perror("dp: read");
                    }
                    c = EOF;
                } else {
                    c = buf[pos++];
                }
	    } else {
                c = buf[pos++];
	    }
	    /****************************************/

	    if (f == 1 && xx >= 16) {
		/* SJISの2バイト目ASCIIエリア表示 */
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

