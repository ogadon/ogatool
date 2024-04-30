/*
 *  cp2.c : プログレスバー付きコピー
 *    
 *    2003/01/24 V0.10 by oga.
 *    2009/10/28 V0.11 support -v and disp perf
 */
#include<stdio.h>
#include<stdlib.h>
#include<sys/stat.h>
#include<string.h>
#include<time.h>
#ifdef _WIN32
#include<windows.h>
#else  /* !_WIN32 */
#include<unistd.h>
#include<sys/time.h>
#endif  /* _WIN32 */


#ifdef _WIN32
#define PDELM	"\\"
#define PDELMC	'\\'
#define PDELM2	"/"
#define PDELM2C	'/'
#define S_ISDIR(m)  ((m) & S_IFDIR)
#else
#define PDELM	"/"
#define PDELMC	'/'
#endif

#define ISKANJI(c) (((c) >= 0x80 && (c) < 0xa0) || ((c) >= 0xe0 && (c)<0xff))

#ifdef _WIN32
/*
struct timeval {
    u_int tv_sec;
    u_int tv_usec;
};
*/

struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};

int gettimeofday(struct timeval *tv, struct timezone *tz);
#endif /* _WIN32 */

/*
 *  tv_diff(tv1, tv2, tv3)
 *
 *  IN 
 *   tv1, tv2 : time value
 *
 *  OUT 
 *   tv3      : tv1 - tv2
 *
 */
void tv_diff(tv1,tv2,tv3)
struct timeval *tv1,*tv2,*tv3;
{
    if (tv1->tv_usec < tv2->tv_usec) {
	tv3->tv_sec  = tv1->tv_sec  - tv2->tv_sec - 1;
	tv3->tv_usec = tv1->tv_usec + 1000000 - tv2->tv_usec;
    } else {
	tv3->tv_sec  = tv1->tv_sec  - tv2->tv_sec;
	tv3->tv_usec = tv1->tv_usec - tv2->tv_usec;
    }
}

#ifdef _WIN32
int gettimeofday(struct timeval *tv, struct timezone *tz)
{
    	SYSTEMTIME syst;

        //GetSystemTime(&syst);   // UTC
        GetLocalTime(&syst);
	tv->tv_sec  = syst.wHour * 3600 +
       		      syst.wMinute *60 +
        	      syst.wSecond;
	tv->tv_usec = syst.wMilliseconds * 1000;
	return 0;
}
#endif /* _WIN32 */

/*
 *  ファイル名部分を取り出す  
 * 
 *  IN  path: (フルパスの)ファイル名
 *  OUT ret : ファイル名の開始位置
 */
char *GetFileName(char *path)
{
	char *pt = path;  /* 区切り文字がなければ先頭を返す */
	int  i;

	for (i = 0; i<(int)strlen(path); i++) {
		if (ISKANJI((unsigned char)path[i])) {
			++i;         // 全角の1バイト目なら次の文字までスキップ  
			continue;
		}
		if (path[i] == PDELMC) {
		    pt = &path[i+1];   // 最後の\の次の位置を保存   
		}
	}
	return pt;
}

void usage()
{
    printf("usage : cp2 <src_file> <dest_file>\n");
    exit(1);
}

int main(int a, char *b[])
{
    FILE   *fp1;          /* for src_file    */
    FILE   *fp2;          /* for dest_file   */
    struct stat stbuf;
    int    size;          /* read  size      */
    int    wsize;         /* write size      */
    int    pcent;         /* copy percent    */
    int    total;         /* total copy size */
    unsigned int all_size = 0;  /* file size       */
    int    i;             /*               V0.11-A */
    int    vf = 0;        /* -v: verbose   V0.11-A */
    int    msec = 0;      /* millisec      V0.11-A */
    int    blksiz = 4096; /* block size    V0.11-A */
    char   buf[4096*16];  /* buffer        V0.11-C */
    char   strbar[100];
    char   *bar = "##################################################                                                  ";
    char   *pt;
    char   src_file[1025];
    char   dest_file[1025];
    char   fname[1025];   /* file basename         */
    struct timeval tvs, tve, tvd;       /* V0.11-A */

#ifdef _WIN32
    char   *cur_up = "";
#else  /* UNIX */
    char   *cur_up = "\033M";
#endif /* _WIN32 */

    strcpy(src_file, "");
    strcpy(dest_file, "");

    for (i = 1; i < a; i++) {
    	if (!strcmp(b[i], "-h")) {
	    usage();
    	}
    	if (!strcmp(b[i], "-v")) {
	    ++vf;
	    continue;
	}
        if (src_file[0] == '\0') {
	    strcpy(src_file,  b[i]);
	    continue;
	}
        if (dest_file[0] == '\0') {
            strcpy(dest_file, b[i]);
	    continue;
	}
    }

		
    /* dest_file is directory? */
    if (stat(dest_file, &stbuf) == 0) {
        if (S_ISDIR(stbuf.st_mode)) {
	    if (strchr(src_file, PDELMC)) {
	        /* exist path delm */
	        //pt = strrchr(src_file, PDELMC);  /* ファイル名取得(仮) */
		//++pt;
		pt = GetFileName(src_file);  /* ファイル名取得 */
		strcpy(fname, pt);
#ifdef _WIN32
	    } else if (strchr(src_file, PDELM2C)) {
	        /* UNIX形式も許す */
	        pt = strrchr(src_file, PDELM2C);
		++pt;
		strcpy(fname, pt);
#endif
	    } else {
	        /* filename only */
		strcpy(fname, src_file);
	    }
	    strcat(dest_file, PDELM);
	    strcat(dest_file, fname);
	    if (vf) printf("dest_file: %s\n", dest_file);
	}
    }

    if (vf) printf("open %s ...\n", src_file);
    if ( (fp1 = fopen(src_file,"rb")) == 0) {
	perror(src_file);
	usage();
	exit(1);
    }

    if (vf) printf("open %s ...\n", dest_file);
    if ( (fp2 = fopen(dest_file,"wb")) == 0) {
	perror(dest_file);
	usage();
	exit(1);
    }

    if (vf) printf("stat %s ...\n", src_file);
    if (stat(src_file, &stbuf) == 0) {
        all_size = stbuf.st_size;    /* src_file size */

	/* V0.11-A start */
	if (all_size > sizeof(buf) * 4) {
	    blksiz = sizeof(buf);
	} else {
	    blksiz = all_size/4;
	    if (blksiz < 4096) {
	        blksiz = 4096;
	    }
	}
	/* V0.11-A end   */
    }
    if (vf) printf("blksiz = %d\n", blksiz);

    total = 0;

    gettimeofday(&tvs, NULL);                       /* V0.11-A */
    while (size = fread(buf, 1, blksiz, fp1)) {     /* V0.11-C */
        wsize = fwrite(buf, 1, size, fp2);
	if (wsize != size) {
            fprintf(stderr, "Write Incomplete %d/%d\n", wsize, size);
	}

	/* for disp progress bar */
	total += size;
	if (all_size > 1024*1024) {
            pcent = total/(all_size/100);
	} else {
            pcent = (total*100)/all_size;
	}
	strncpy(strbar, &bar[50-pcent/2], 50);
	strbar[0] = strbar[10] = strbar[20] = strbar[30] 
	          = strbar[40] = strbar[50] = '|';
	strbar[51] = '\0';
#ifdef _WIN32
        printf("  %3d%% %s %dKB/%dKB\r", pcent, strbar, total/1024, all_size/1024, cur_up);
#else
        printf("  %3d%% %s %dKB/%dKB\n%s", pcent, strbar, total/1024, all_size/1024, cur_up);
#endif
        fflush(stdout);
    }
    printf("\n");

    if (!feof(fp1)) {
        fprintf(stderr, "Copy Incompleted.\n");
    }
    if (vf) printf("close %s ...\n", src_file);
    fclose(fp1);
    if (vf) printf("close %s ...\n", dest_file);
    fclose(fp2);

    /* V0.11-A start */
    gettimeofday(&tve, NULL);

    tv_diff(&tve, &tvs, &tvd);
    msec = tvd.tv_sec*1000 + tvd.tv_usec/1000;
    printf(" (Size:%dKB  Time:%d.%03d sec  %dKB/s)\n", 
           stbuf.st_size/1024, tvd.tv_sec, tvd.tv_usec/1000,
           msec?(((stbuf.st_size/1024)*1000)/msec):0);
    /* V0.11-A end   */
}

/* vim:ts=8:sw=4:
 */

