/*
 *   HDDにランダムな値を書き込み廃棄用に初期化する 
 *
 *   2003/10/14 V0.10 by oga.
 *   2004/04/09 V0.11 fill completely
 *   2012/12/31 V0.12 add perf
 */

#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#endif
#include <sys/stat.h>

#define BUF_SIZE 1024*1024
#define FILESIZE 512         /* 単体ファイルサイズ(MB) */

#define VER  "0.12"

#ifdef _WIN32
struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};

int gettimeofday(struct timeval *tv, struct timezone *tz);
#endif

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
void tv_diff(struct timeval *tv1, struct timeval *tv2, struct timeval *tv3)
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
int main(int a, char *b[])
{
    char *fname = NULL;
    FILE *fp;
    int  vf = 0;
    int  rawf = 0;  /* 1:raw device */
    int  i;
    int  j;
    int  k;
    int  ret;
    int  size;
    int  total = 0;
    int  sub_total = 0;
    int  write_sz = BUF_SIZE;
    char *buf;
    char fname2[1024];
    char work[1024];
    struct stat stbuf;
    struct timeval st_time, en_time, diff_time;

#ifdef _WIN32
    SYSTEMTIME syst;
#else
    struct timeval tv;
#endif

    /* arg check */
    for (i = 1; i<a; i++) {
	if (!strncmp(b[i],"-v",2)) {
	    ++vf;
	    continue;
	}
	if (!strcmp(b[i],"-raw")) {
	    rawf = 1;
	    continue;
	}
	if (!strncmp(b[i],"-h",2)) {
	    printf("hddinit Ver %s\n", VER);
	    printf("usage: hddinit [-raw <raw_device_name>]\n");
	    exit(1);
	}
	fname = b[i];
    }

    printf("create random data file OK? (y/n) : ");
    fflush(stdout);
    scanf("%s", work);
    if (work[0] != 'y') {
        printf("Canceled.\n");
        exit(0);
    }

    buf = (char *) malloc(BUF_SIZE);
    if (!buf) {
        perror("malloc");
	exit(1);
    }

    /* init random seed */
#ifdef _WIN32
    GetLocalTime(&syst);
    srand(syst.wMilliseconds);
#else
    gettimeofday(&tv,0);
    srand(tv.tv_usec);
#endif

    /* open file */
    if (rawf) {
        printf("sorry, not support...\n");
	exit(1);
    } else {
        j = 1;
        while (1) {
	    while(1) {
	        sprintf(fname2, "random%05d", j);  /* create filename */
	        if (stat(fname2, &stbuf)) break;   /* file not found  */

		/* file already exist. challenge next name */
		j++;
	    }
            if ((fp = fopen(fname2, "wb")) == 0) {
                perror(fname);
                exit(1);
            }
	    //printf("create %s / total = %dMB\n", fname2, total);
	    printf("create %s ", fname2);
	    fflush(stdout);

	    gettimeofday(&st_time, NULL);
	    sub_total = 0;
	    /* 512MB 単位で作成 */
	    while (sub_total < FILESIZE*BUF_SIZE) {
	        /* create new random data */
                for (k = 0; k < write_sz; k++) {
                    buf[k] = (rand() & 0xff);
                }
	        size = fwrite(buf, 1, write_sz, fp);
		sub_total += size;
		ret = fflush(fp);

		if (vf > 1) {
		    printf("\nDEBUG: 0 write_sz =%d size = %d\n", 
		           write_sz, size);
                }
	        if (size < write_sz || ret != 0) {
		    /* 指定サイズ分書き込めなかったか、flushでエラー */
                    write_sz /= 2;
		    if (vf) printf("new write size = %d\n", write_sz);
		}
		if (vf > 1) {
		    printf("DEBUG: 1 write_sz = %d, size = %d flush=%d\n", 
		           write_sz, size, ret);
	        }
		if (write_sz <= 0) break;
	    }
	    fclose(fp);

	    if (vf > 1) {
	        printf("DEBUG: 2 write_sz = %d\n", write_sz);
	    }

	    j++;
	    total += sub_total/1024/1024;  /* total MB */

	    gettimeofday(&en_time, NULL);
	    tv_diff(&en_time, &st_time, &diff_time);
	    printf("/ total = %dMB  (%d sec  %d MB/sec)\n", total, diff_time.tv_sec,
		    sub_total/(diff_time.tv_sec*1000+diff_time.tv_usec/1000)/1000);


	    if (write_sz <= 0) break;
	}
    }
    printf("HDD init done. (total %d MB)\n", total);
}
/* vim:ts=8:sw=8:
 */


