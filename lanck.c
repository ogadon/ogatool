/*
 *   lanck.c : LAN内ホストの起動状況をチェックする  
 *
 *     08/02/09 V0.10 by oga
 *
 */
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>    /* V1.11-A    */
#include <sys/types.h>  /* for fork() */
#include <unistd.h>     /* for fork() */
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>       /* for logf() */
#include <time.h>

/* configure */
#define VER	  "0.10"
#define MAX_PROC    600 /* max process */
#define CHECK_PORT  80  /* check port  */

#define USE_PTHREAD     /* if define, use pthread (linux only) */

#define dprintf  if (vf > 0) printf
#define dprintf2 if (vf > 1) printf


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


/* porting definition */
#ifdef _WIN32
#define COM_ECONNREFUSED WSAECONNREFUSED
#define COM_SOCKERRNO    WSAGetLastError()
#define sleep(x)         Sleep((x)*1000)
#else
#define COM_ECONNREFUSED ECONNREFUSED
#define COM_SOCKERRNO    errno
#endif

typedef struct _procarg_t {
    int             thd_no;     /* thread No.   */
    struct in_addr  ipaddr;     /* IP Address   */
    int             result;     /* check result 1:alive 0:down */
} procarg_t;

procarg_t procarg[MAX_PROC]; 

int vf    = 0;   /* -v:   verbose        */
int ologf = 0;   /* -log: for log output */
int nhost = 0;   /* num of check host    */


/*
 *  procarg
 *    IN:  st_num: start num
 *    IN:  en_num  : end num
 *    OUT: calc_cnt : calc count
 *    OUT: sosu_cnt : sosu count
 */
void active_check(procarg_t *procarg)
{
    int     i, x;
    int     sockfd;
    struct  sockaddr_in serv_addr;

    if (vf > 1) {
	printf("start active_check(%d) %s\n", procarg->thd_no,
               inet_ntoa(procarg->ipaddr));
        fflush(stdout);
    }

    memset((char *)&serv_addr, 0, sizeof(serv_addr));

    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_addr.s_addr = procarg->ipaddr.s_addr;
    serv_addr.sin_port        = htons(CHECK_PORT);

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
	perror("socket");
	exit(-1);
    }

    if(connect(sockfd, (struct sockaddr *)&serv_addr, 
					sizeof(serv_addr)) < 0){ 
#ifdef _WIN32
	dprintf("(%s WSAGetLastError = %d)\n",
		inet_ntoa(procarg->ipaddr),
		WSAGetLastError(), errno);
	errno = WSAGetLastError();
#else
	dprintf("(%s errno = %d)\n", inet_ntoa(procarg->ipaddr), errno);
#endif
	if (COM_SOCKERRNO == COM_ECONNREFUSED) {
	    procarg->result = 1; /* alive */
	    dprintf("%s alive!!\n", inet_ntoa(procarg->ipaddr), errno);
	} else {
	    /* 133: EHOSTUNREACH: No route to host */
	    procarg->result = 0; /* down  */
	}
    } else {
	procarg->result = 1; /* alive */
        dprintf("%s alive!!\n", inet_ntoa(procarg->ipaddr), errno);
    }
    close(sockfd);
}


void usage()
{
    printf("usage: lanck [-log [-update <interval_min(15)>]] <net_addr or ip_addrs>\n");
    exit(1);
}

int add_host(char *ipstr)
{
    int  i;
    int  dotcnt = 0;
    char wkbuf[1024];

    /* かなり簡易的 */
    for (i = 0; i < strlen(ipstr); i++) {
	if (ipstr[i] == '.') {
	    /* 最後以外の.をカウント */
	    if (i != strlen(ipstr)-1) {
	    	++dotcnt;
	    }
	}
    }

    if (dotcnt == 3) {
	/* nn.nn.nn.nn ip addr */
	if (nhost >= MAX_PROC) {
	    printf("num of address overflow. less than %d\n", MAX_PROC);
	    return -1;
	}
	procarg[nhost++].ipaddr.s_addr = inet_addr(ipstr);
    } else if (dotcnt == 2) {
	/* nn.nn.nn    net addr */
	for (i = 1; i<255; i++ ) {
	    if (nhost >= MAX_PROC) {
		printf("num of address overflow. less than %d\n", MAX_PROC);
		return -1;
	    }
	    sprintf(wkbuf, "%s.%d", ipstr, i);
	    dprintf("ip=%s\n", wkbuf);
	    procarg[nhost++].ipaddr.s_addr = inet_addr(wkbuf);
	}
    }
    return 0;
}

int main(int a, char *b[])
{
    struct hostent *hostp;
    int      i,x;
    int      interval = 15*60;      /* default 15 min    */
    time_t   tt;
    char     datestr[1024];
    PID_T    pid[MAX_PROC];         /* pid               */
#ifdef _WIN32
    DWORD    threadID = 0;          /* thread id (work) */
    WSADATA  WsaData;

    WSAStartup(0x0101, &WsaData);   /* start win socket */
#endif

    memset(pid, 0, sizeof(pid));

    /* printf("lanck V%s by oga.\n",VER); */
    for (i=1; i<a; i++) {
	if (!strncmp(b[i], "-h",2)) {
	    usage();
	}
	if (!strncmp(b[i], "-v",2)) {
	    ++vf;
	    continue;
	}
	if (!strcmp(b[i], "-log")) {
	    ++ologf;
	    continue;
	}
	if (i+1 < a && !strcmp(b[i], "-update")) {
	    interval = atoi(b[++i]) * 60;
	    continue;
	}
	add_host(b[i]);
    }

    while (1) {
	if (ologf) {
	    tt = time(0);
	    strftime(datestr, sizeof(datestr), "%y/%m/%d,%H:%M:%S,", localtime(&tt));
	} else {
	    strcpy(datestr, "");
	}
	/* multi proces */
	for (i = 0; i < nhost; i++) {
	    /* set args */
	    procarg[i].thd_no   = i;   /* thread number */
	    procarg[i].result   = 0;   /* down */

#ifdef _WIN32
	    /* Windows */
	    pid[i] = CreateThread(
			NULL,
			0,
			(LPTHREAD_START_ROUTINE)active_check,
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
	    if (pthread_create(&pid[i], NULL, (void *)active_check, &procarg[i])) {
		perror("pthread_create");
		exit(1);
	    }
#else  /* USE_PTHREAD */
	    if ((pid[i] = fork()) == 0) {
		/* child */
		active_check(&procarg[i]);
		exit(0);
	    } else if (pid[i] < 0) {
		perror("fork()");
		exit(1);
	    }
#endif /* USE_PTHREAD */
#endif /* _WIN32 */
	}

	dprintf2("wait start...\n");

	/* プロセス完了待ち */
	for (i = 0; i < nhost; i++) {
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
	    dprintf2("wait(%d) end\n", i);
	}

	/* まとめて出力 */
	printf("\n");
	for (i = 0; i < nhost; i++) {
	    if (procarg[i].result == 1) {
		/* alive IP */
		printf("%s%s", datestr, inet_ntoa(procarg[i].ipaddr));
		/* get hostname */
		hostp = gethostbyaddr((void *)&procarg[i].ipaddr, 
				       sizeof(struct in_addr),
				       AF_INET);
		if (hostp) {
		    printf(",%s\n", hostp->h_name);
		} else {
		    printf("\n");
		}
	    }
	}
        fflush(stdout);

    	if (ologf == 0) break;
	sleep(interval);
    } /* while(1) */

    /* disp result */
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}

/* vim:ts=8:sw=4:
 */
