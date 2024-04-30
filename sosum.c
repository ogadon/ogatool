/*
 *   sosu.c : 1～10000までの素数を求める(マルチプロセス版)
 *
 *     97/08/27 V1.01 time support
 *     97/09/04 V1.02 usage changed.
 *     99/01/14 V1.03 add S'Mark99
 *     -------------------------------
 *     07/09/03 V1.10 support multi process
 *     07/09/30 V1.11 Support Win32 thread and Linux pthread
 *     14/01/15 V1.12 Collectin of a program name.
 *
 */
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>    /* V1.11-A    */
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>  /* for fork() */
#include <unistd.h>     /* for fork() */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>       /* for logf() */

/* configure */
#define VER	  "1.12"
#define LIM	  10000
#define MAX_PROC    128 /* max process */
#define CLOCK2          /* if define, use gettimeofday         */
#define USE_PTHREAD     /* if define, use pthread (linux only) */

#define dprintf if (vf > 1) printf


#ifdef CLOCK2
#define clock clock2
#endif

#ifdef _WIN32
typedef unsigned long     ulong;
typedef unsigned int      uint;
typedef unsigned __int64  uint64;
typedef HANDLE        PID_T;  /* thread handle */

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
#else  /* _WIN32 */
typedef unsigned long long int uint64;
#ifdef USE_PTHREAD
typedef pthread_t     PID_T;  /* thread handle */
#else  /* USE_PTHREAD */
typedef pid_t         PID_T;  /* process ID */
#endif /* USE_PTHREAD */
#endif /* _WIN32 */

typedef struct _procarg_t {
    int    thd_no;     /* thread No.   */
    int    st_num;     /* start number */
    int    en_num;     /* edn   number */
    uint64 calc_cnt;   /* calc counter */
    uint64 sosu_cnt;   /* sosu counter */
} procarg_t;

procarg_t procarg[MAX_PROC]; 

int vf = 0;  /* -v: verbose */
int ff = 0;  /* -f: flush        V1.10-A */


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

float log10f(float x)
{
    return (log(x)/log(10));
}
#endif /* _WIN32 */

int clock2()
{
    struct timeval  tv;
    struct timezone tz;
    gettimeofday(&tv, &tz);
    return (tv.tv_sec * 1000 + tv.tv_usec/1000);
}

/*
 *   IN:  max        求める素数のmax値
 *   IN:  nproc      分割プロセス数
 *   OUT: num_tbl[]  各プロセスの計算max値 
 */
void get_count(int max, int nproc, int num_tbl[])
{
    int i;
    int x;
    int step = 100;
    float cal_num;  /* max=x 時の計算概数 */
    float cal_max;  /* max   時の計算概数 */

    cal_max = (1/log10f((float)max)/4.1 - 0.00328) * max*max;
    dprintf("cal_max = %.1f\n", cal_max); /* DEBUG */

    if (max < 1000) {
	step = 1;
    }

    i = 0;
    for (x = 2; x<max; x+=step) {
        cal_num = (1/log10f((float)x)/4.1 - 0.00328) * x*x;
	if (cal_num > cal_max*(i+1)/nproc) {
            dprintf("cal_num[%d] = %.1f / n = %.1f\n", i, cal_num,
		     cal_max*(i+1)/nproc); /* DEBUG */
	    num_tbl[i] = x;
	    dprintf("get_count: i=%d num=%d\n", i, x);
	    ++i;
	}
	if (i >= nproc-1) {
	    break;
	}
    }
    num_tbl[nproc-1] = max;
}

/*
 *  procarg
 *    IN:  st_num: start num
 *    IN:  en_num  : end num
 *    OUT: calc_cnt : calc count
 *    OUT: sosu_cnt : sosu count
 */
void sosu(procarg_t *procarg)
{
    int i, x;

    if (vf != 1) {
	printf("start sosu(%d) %d-%d\n", procarg->thd_no,
               procarg->st_num, procarg->en_num);
        fflush(stdout);
    }

    for (x = procarg->st_num; x <= procarg->en_num; x++) {
	for (i = 2; i < x; i++) {
	    (procarg->calc_cnt)++;
	    if((x % i) == 0) {
		break;
	    }
	}
	if (x == i) {
	    if (vf == 1) {
		printf("%d\n",x);      /* V1.10-C */
		if (ff) {
		    fflush(stdout);
		}
	    }
	    (procarg->sosu_cnt)++;     /* V1.10-A */
	}
    }
    if (vf != 1) {
#ifdef _WIN32
	/* VC6 does not support %llu ... */
	printf("end   sosu(%d) calc=%I64u sosu=%I64u\n", 
               procarg->thd_no, procarg->calc_cnt, procarg->sosu_cnt);
#else  /* _WIN32 */
	printf("end   sosu(%d) calc=%llu sosu=%llu\n", 
               procarg->thd_no, procarg->calc_cnt, procarg->sosu_cnt);
#endif /* _WIN32 */
        fflush(stdout);
    }
}


void usage()
{
    printf("usage: sosum [-v] [-f] [-j <num_of_threads(1-%d)>] [<num(%d)>]\n", 
	    MAX_PROC, LIM);
    printf("       -v   : display prime number\n");
    printf("       -f   : flush output\n");
    printf("       -j   : use multi thread\n");
    printf("       <num>: max number\n");
    exit(1);
}

int main(int a, char *b[])
{
    int	i,x;
    int	st_tm, en_tm;
    int	ff = 0;       /* -f: flush        V1.10-A */
    int	jf = 1;       /* -j: num of jobs  V1.10-A */
    int	lim = LIM;
    int num_tbl[MAX_PROC];  /* threshold num    V1.01-A */
    PID_T   pid[MAX_PROC];  /* pid              V1.10-A */
#ifdef _WIN32
    DWORD threadID = 0;     /* thread id (work) */
#endif

    uint64  calc_cnt = 0;
    uint64  sosu_cnt = 0;   /* V1.10-A */

    memset(num_tbl, 0, sizeof(num_tbl));
    memset(pid, 0, sizeof(pid));

    printf("sosum V%s by oga.\n",VER);
    for (i=1; i<a; i++) {
	if (!strncmp(b[i],"-h",2)) {
	    usage();
	}
	if (!strncmp(b[i],"-v",2)) {
	    ++vf;
	    continue;
	}
	/* V1.10-A start */
	if (!strncmp(b[i],"-f",2)) {
	    ff = 1;
	    continue;
	}
	if (i+1 < a && !strncmp(b[i],"-j",2)) {
	    jf = atoi(b[++i]);
	    continue;
	}
	/* V1.10-A end   */
	lim = atoi(b[i]);
    }

    if (jf == 0) {
	jf = 1;
    } else if (jf > MAX_PROC) {
	usage();
    }

    /* check num of job */
    if (jf > (lim - 2)/2) {
	printf("Error: specify lower than -j %d\n", (lim - 2)/2);
	exit(1);
    }

    get_count(lim, jf, num_tbl);
    printf("start calc(%d) threads=%d ...\n", lim, jf);

    st_tm = clock2();

    /* multi proces */
    for (i = 0; i < jf; i++) {
	/* set args */
        procarg[i].thd_no   = i;          /* thread number */
        procarg[i].calc_cnt = 0;
        procarg[i].sosu_cnt = 0;

        procarg[i].st_num = (i==0)?2:num_tbl[i-1]+1;
        procarg[i].en_num = num_tbl[i];

#ifdef _WIN32
	/* Windows */
        pid[i] = CreateThread(
                    NULL,
                    0,
                    (LPTHREAD_START_ROUTINE)sosu,
                    &procarg[i],
                    0,
                    &threadID);
	if (pid[i] == NULL) {
	    printf("CreateThread Error (%d)\n", GetLastError());
	    exit(1);
	}
#else  /* _WIN32 */
	/* Linux */
#ifdef USE_PTHREAD
	if (pthread_create(&pid[i], NULL, (void *)sosu, &procarg[i])) {
	    perror("pthread_create");
	    exit(1);
	}
#else  /* USE_PTHREAD */
	if ((pid[i] = fork()) == 0) {
	    /* child */
	    sosu(&procarg[i]);
	    exit(0);
	} else if (pid[i] < 0) {
	    perror("fork()");
	    exit(1);
	}
#endif /* USE_PTHREAD */
#endif /* _WIN32 */
    }

    if (vf != 1) printf("wait start...\n");

    /* プロセス完了待ち */
    for (i = 0; i < jf; i++) {
#ifdef _WIN32
        WaitForSingleObject((HANDLE)pid[i], INFINITE);
	CloseHandle((HANDLE)pid[i]);
#else  /* _WIN32 */
#ifdef USE_PTHREAD
	pthread_join(pid[i], NULL);
#else  /* USE_PTHREAD */
	waitpid(pid[i], NULL, NULL);
#endif /* USE_PTHREAD */
#endif /* _WIN32 */
	if (vf != 1) printf("wait(%d) end\n", i);
	fflush(stdout);
	calc_cnt += procarg[i].calc_cnt;  /* effective in thread */
	sosu_cnt += procarg[i].sosu_cnt;  /* effective in thread */
    }

    en_tm = clock2();

    /* disp result */
    printf(
#ifdef _WIN32
        "\nMax:%d Calc:%I64u NumOfPrimeNumber:%I64u Threads:%d Time:%3.3f sec\n",
#else  /* _WIN32 */
        /* VC6 does not support %llu ... */
        "\nMax:%d Calc:%llu NumOfPrimeNumber:%llu Threads:%d Time:%3.3f sec\n",
#endif /* _WIN32 */
	    lim,
	    calc_cnt,
	    sosu_cnt,                              /* V1.10-A */
	    jf,                                    /* V1.11-A */
#ifdef CLOCK2
	    (float)(en_tm-st_tm)/1000              /* for gettimeofday() */
#else
	    (float)(en_tm-st_tm)/CLOCKS_PER_SEC    /* for clock()        */
#endif
	    );

    return 0;
}

/* vim:ts=8:sw=4:
 */
