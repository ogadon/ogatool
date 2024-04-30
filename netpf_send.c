/*
 *  netpf_send.c
 *
 *     network性能測定 (send only.)
 *
 *     98/04/22 V1.00 by oga.
 *     99/08/20 V1.01 add verbose mode
 *     01/02/17 V1.011 GetSystemTime => GetLocalTime
 *     03/06/07 V1.02 diff 0 対策
 *     -------------------------------
 *     16/01/14 V1.03 netpf.c => netpf_send.c
 *     16/01/19 V1.04 add shutdown for win 10054 error
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef DOS
#include <windows.h>
#include <winsock.h>
#else /* DOS */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <signal.h>
#endif /* DOS */

#define VER         "1.04"
#define LOOP        3
#define dprintf
#define	SEND_SIZE   (1024*1024*10)   /* default: 10MB */

#ifndef SHUT_RDWR
#define SHUT_RDWR	SD_BOTH
#endif

#ifndef SD_BOTH
#define SD_BOTH     0x02
#endif

void PUT_DATA(int, char *, int);
void GET_DATA(int, char *, int);
void reapchild();
void memdump(FILE *, unsigned char *, int);

int	 send_size = SEND_SIZE;        /* default: 10MB */
int	 vf        = 0;                /* verbose      */

#ifdef DOS
/*
struct timeval {
	u_int tv_sec;
	u_int tv_usec;
};
*/

int gettimeofday(struct timeval *tv, void *tz);
#endif

main(a,b)
int a;
char *b[];
{
	int 	sockfd, newsockfd, clilen, childpid;
	struct 	sockaddr_in cli_addr, serv_addr;
        struct  hostent *hostp;
	int 	fd;
	int	con = 0;
	int	port = 22122;
	int	sz, i;
	int     svf = 0;	/* for client process */
	char	*buf;
	char	hostn[256];
#ifdef DOS
	char	msg[256];
	WSADATA  WsaData;
#endif

	for (i = 1; i<a; i++) {
	    if (!strncmp(b[i],"-h",2)) {
		printf("netpf_send ver%s\n", VER);
		printf("usage : netpf_send { -s | <hostname> } [-p <port(%d)>] [-c <send_size(MB)>\n", port);
		printf("          -s        : start as netpf server\n");
		printf("          hostnames : netpf server name.(start as netpf client)\n");
		printf("          -c size   : test data size(MB) default:%dMB\n", SEND_SIZE/1024/1024);
		printf("          -p port   : test port\n");
		exit(1);
	    }
	    if (!strncmp(b[i],"-s",2)) {
		svf = 1;
		continue;
	    }
	    if (!strncmp(b[i],"-p",2)) {
		port = atoi(b[++i]);
		printf("port = %d\n",port);
		continue;
	    }
	    if (!strncmp(b[i],"-c",2)) {
		send_size = atoi(b[++i])*1024*1024;
		continue;
	    }
	    if (!strncmp(b[i],"-v",2)) {
	        vf = 1;
		continue;
	    }
	    strcpy(hostn,b[i]);
	}

	if (!(buf = (char *)malloc(send_size))) {
	    printf("malloc error. errno=%d\n",errno);
	    exit(1);
	}

	memset((char *)&serv_addr, 0, sizeof(serv_addr));

#ifdef DOS
	if (WSAStartup(0x0101, &WsaData)) {
	    sprintf(msg,"WSAStartup Error : %d",WSAGetLastError());
	    printf(msg);
	    return -1;
	}
#endif

	if (svf) {

            /* 
             *   netpf server 
             */
	    printf("wait for port %d\n",port);

	    if( (sockfd=socket(AF_INET, SOCK_STREAM, 0)) < 0 ){
		perror("socket");
#ifdef DOS
		WSACleanup();
#endif
		exit(-1);
	    } 

	    serv_addr.sin_family      = AF_INET;
	    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	    serv_addr.sin_port        = htons((short)port);

	    if(bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
		perror("bind");
#ifdef DOS
		WSACleanup();
#endif
		exit(-1);
	    } 

#ifndef DOS
	    signal(SIGCLD, reapchild);
#endif

	    printf("listen...\n");
	    if(listen(sockfd, 5) < 0){
		perror("listen");
#ifdef DOS
		WSACleanup();
#endif
		exit(-1);
	    }

	    while (1) {
	        clilen = sizeof(cli_addr);
	        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr,
				&clilen);
	        if(newsockfd < 0){
	            if (errno == EINTR) {
	                printf("accept interrupted\n");
	                continue;
	            }
		    perror("accept");
#ifdef DOS
		WSACleanup();
#endif
		    exit(-1);
	        }
	
#ifndef DOS
	        if (fork() == 0) {
	            /* child */
#endif
	            close(sockfd);
	            printf("accept %d pid=%d\n",con,getpid());
	            /* サーバ処理 */
	            for (i = 0; i<LOOP; i++) {
	                printf("## recv %d MB start\n",send_size/1024/1024);
	                GET_DATA(newsockfd, buf, send_size);
	                //printf("## send %d KB start\n",send_size/1024);
	                //PUT_DATA(newsockfd, buf, send_size);
	            }
		    printf("chile pid=%d exit!\n",getpid());
		    if (shutdown(newsockfd, SHUT_RDWR) < 0) {
#ifdef DOS
        		printf("send: errno=%d  LastErr=%d\n", errno, WSAGetLastError());
#else
        		printf("send: errno=%d\n", errno);
#endif
		    }
	            close(newsockfd);
#ifndef DOS
	            exit(0);
	        } 
#endif
	        close(newsockfd);
	        con++;
            }
        } else {

            /* 
             *   netpf client 
             */

	    hostp = gethostbyname(hostn);
	    if (!hostp) {
	        printf("Error: gethostbyname(%s) error \n",hostn);
	        return 1;
	    }

	    serv_addr.sin_family      = AF_INET;
#if 1
	    serv_addr.sin_addr.s_addr = *(int *)hostp->h_addr;
#else
	    memcpy((char *)&serv_addr.sin_addr.s_addr, 
	    			(char *)hostp->h_addr, 
	    			sizeof(serv_addr.sin_addr.s_addr));
#endif
	    serv_addr.sin_port	      = htons((short)port);

	    if (vf) {
	        printf("DEBUG: host(%s) IP(%s)\n", hostn, 
		                  inet_ntoa(serv_addr.sin_addr));
	    }

	    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("socket");
#ifdef DOS
		WSACleanup();
#endif
		exit(-1);
	    }

	    if (connect(sockfd, (struct sockaddr *)&serv_addr, 
					sizeof(serv_addr)) < 0){ 
		perror("connect");
#ifdef DOS
                printf("connect: winerr=%d\n", WSAGetLastError());
		WSACleanup();
#endif
		exit(-1);
	    }
            for (i = 0; i<LOOP; i++) {
                printf("## send %d MB start\n",send_size/1024/1024);
                PUT_DATA(sockfd, buf, send_size);
                //printf("## recv %d MB start\n",send_size/1024/1024);
                //GET_DATA(sockfd, buf, send_size);
            }
	    if (shutdown(sockfd, SHUT_RDWR) < 0) {
#ifdef DOS
       		printf("send: errno=%d  LastErr=%d\n", errno, WSAGetLastError());
#else
       		printf("send: errno=%d\n", errno);
#endif
	    }
            close(sockfd);

        }
#ifdef DOS
	WSACleanup();
#endif
        return 0;
}

void PUT_DATA(int sockfd, char *buf, int sz)
{
	int size, all=0;
	struct timeval tv, tv2;
	unsigned int wk,wk2,diff;
	int cnt = 0;

	gettimeofday(&tv,0);
	wk = tv.tv_sec*1000000 + tv.tv_usec;
	dprintf("Send start usec : %u\n",wk);

	do {
		++cnt;
	    size = send(sockfd, buf, sz-all, 0);
	    if (size < 0) {
#ifdef DOS
	        printf("send: errno=%d  LastErr=%d\n", errno, WSAGetLastError());
#else
	        printf("send: errno=%d\n", errno);
#endif
		exit(1);
	    }
	    all += size;
	} while (all < sz);

	if (vf) {
		printf("num of send = %d\n", cnt);
	}

	gettimeofday(&tv,0);
	wk2 = tv.tv_sec*1000000 + tv.tv_usec;
	diff = wk2-wk;
	if (diff <0 ) diff = -diff;
	dprintf("Send   end usec : %u\n",wk2);
	dprintf("      diff usec : %u\n",diff);
	printf("Send Size = %d  time = %.3fsec  perf = %.3fMB/sec\n",
			all,
			(float)diff/1000000,
			(diff/1000 == 0)?999999:(float)all/diff); /* V1.02-C */
}

/* 
 *   get socket data and write to file
 *
 *   IN : fd     : 入力ファイル
 *        sockfd : 入力ソケットディスクリプタ
 *        sz     : 確実に読みとるサイズ
 */
void GET_DATA(int sockfd, char *buf, int sz)
{
	int size, all=0;
	struct timeval tv, tv2;
	unsigned int wk,wk2,diff;
	int cnt = 0;

	gettimeofday(&tv,0);
	wk = tv.tv_sec*1000000 + tv.tv_usec;
	dprintf("Recv start usec : %u\n",wk);

	do {
		++cnt;
	    size = recv(sockfd, buf, sz-all, 0);
	    if (size < 0) {
#ifdef DOS
	        printf("recv: errno=%d  LastErr=%d\n", errno, WSAGetLastError());
#else
	        printf("recv: errno=%d\n", errno);
#endif
		exit(1);
	    }
	    all += size;
	} while (all < sz);

	if (vf) {
		printf("num of recv = %d\n", cnt);
	}

	gettimeofday(&tv,0);
	wk2 = tv.tv_sec*1000000 + tv.tv_usec;
	diff = wk2-wk;
	if (diff <0 ) diff = -diff;
	dprintf("Recv   end usec : %u\n",wk2);
	dprintf("      diff usec : %u\n",diff);
	printf("Recv Size = %d  time = %.3fsec  perf = %.3fMB/sec\n",
			all,
			(float)diff/1000000,
			(float)all/diff);
}

void reapchild()
{
#ifndef DOS
	wait(0);
	signal(SIGCLD,reapchild);
#endif
}

/*
 *  void memdump(fp, buf, size)
 *
 *  bufから、size分を fp にヘキサ形式で出力する
 *
 *  <出力例>
 *  Location: +0       +4       +8       +C       /0123456789ABCDEF
 *  00000000: 2f2a0a20 2a205265 76697369 6f6e312e //#. # Revision1.
 *  00000010: 31203936 2e30372e 30312074 616b6173 /1 96.07.01 takas
 *  00000020: 6869206b 61696e75 6d610a20 2a2f0a2f /hi kainuma. #/./
 *
 *  IN  fp   : 出力ファイルポインタ (標準出力の場合はstdoutを指定)
 *      buf  : ダンプメモリ先頭アドレス
 *      size : ダンプサイズ
 *
 */
void memdump(fp, buf, size)
FILE *fp;
unsigned char *buf;
int  size;
{
	int c, xx, addr = 0, i;
	int f=0, f2=0;
	int kflag = 0;		/* 漢字出力 */
	int pos   = 0;
	char asc[17];
	
	/* ヘッダ出力 */
	fprintf(fp,
	  "Location: +0       +4       +8       +C       /0123456789ABCDEF\n");

	c = buf[pos++];   /* c = getc(infp); */
	while(pos <= size) {
	    xx = 0;
	    strcpy(asc,"                ");
	    fprintf(fp,"%08x: ",addr);
	    f = 0;
	    while(pos <= size && xx < 16) {
		fprintf(fp,"%02x",c);
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
		} else if (c > 127 ) {
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
		if ((xx % 4) == 3) fprintf(fp," ");
		++xx;
		++addr;
		c = buf[pos++];   /* c = getc(infp); */
		if (f == 1 && xx >= 16) {
		    asc[xx++] = c;
		    f = 0;
		    f2 = 1;
		}
	    }
	    while (xx <16) {
		fprintf(fp,"  ");
		if ((xx % 4) == 3) fprintf(fp," ");
		++xx;
	    }
	    asc[xx]='\0';
	    fprintf(fp,"/%16s \n",asc);
	}
}

#ifdef DOS
int gettimeofday(struct timeval *tv, void *tz)
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
#endif /* DOS */
