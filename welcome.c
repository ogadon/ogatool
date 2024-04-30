/*
 *  welcome.c
 *
 *     クライアントからポートに接続があったら
 *     音を鳴らす
 *
 *     04/04/03 V1.00 by oga. (based miscserver.c)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>   /* gethostbyaddr() */

void PUT_DATA(int, int);
void GET_DATA(int, int);
void reapchild();
void memdump(FILE *, unsigned char *, int);
void print_host(struct in_addr sin_addr);

#define  DEFAULT_CHIME  "/home/oga/Media/wavs/SE/eu_info.mp3"

/* MP3ファイルの再生 */
void PlayMp3(char *snd_file)
{
    char command[2048];

    sprintf(command, "mpg123 %s", snd_file);
    system(command);
}

/* WAVファイルの再生 */
void PlayWav(char *snd_file)
{
    char command[2048];

    sprintf(command, "play %s", snd_file);
    system(command);
}

/* サウンドファイルの再生 */
void Play(char *snd_file) 
{
    if (strlen(snd_file) > 4 && !strcasecmp(&snd_file[strlen(snd_file)-4], ".mp3")) {
        PlayMp3(snd_file);
    } else {
        PlayWav(snd_file);
    }
}

#define SIZE 4096
void PUT_DATA(fd, newsockfd)
int fd;
int newsockfd;
{
	char c[SIZE];
	int size;
        while((size = read(fd, c, SIZE)) != 0)
                write(newsockfd, c,size);
}

/* 
 *   get socket data and write to file
 *
 *   IN : fd     : 出力ファイル
 *        sockfd : 入力ソケットディスクリプタ
 */
void GET_DATA(fd, sockfd)
int fd;
int sockfd;
{
	char c[SIZE];
	int size, all=0;
	struct timeval tv, tv2;
	unsigned int wk,wk2,diff;

	gettimeofday(&tv,0);
	wk = tv.tv_sec*1000000 + tv.tv_usec;
	printf("start usec : %u\n",wk);

	while((size = read(sockfd, c, SIZE)) != 0) {
		write(fd, c,size);
		/* c[size] = '\0'; */
		/* printf("%s",c); */
		/* fflush(stdout); */
		all += size;
	}
	gettimeofday(&tv,0);
	wk2 = tv.tv_sec*1000000 + tv.tv_usec;
	diff = wk2-wk;
	if (diff <0 ) diff = -diff;
	printf("  end usec : %u\n",wk2);
	printf(" diff usec : %u\n",diff);
	printf("size = %d  time = %.6fsec  perf = %dKB/sec\n",
			all,
			(float)diff/1000000,
			(all*1000)/diff);
}


/* 
 *   get socket data and store to buffer
 *
 *   IN : sockfd : 入力ソケットディスクリプタ
 *        buf    : 格納バッファ
 *        size   : 格納バッファサイズ
 */
int GET_DATA2BUF(sockfd,buf,size)
int sockfd,size;
char *buf;
{
	int sz, all=0;

	printf("start GET_DATA2BUF\n");
	while((sz = read(sockfd, &buf[all], size-all)) > 0) {
	    printf("  read %d byte\n",sz);
	    all += sz;
	}
	if (sz < 0) {
	    perror("read");
	}
	printf("  return with %d byte\n",all);
	return all;
}

void print_host(struct in_addr sin_addr)
{
    struct hostent *hostp;
    int i;

    hostp = gethostbyaddr((char *)&sin_addr.s_addr,
                          sizeof(sin_addr.s_addr),
                          AF_INET);

    /* print all hostname/aliases */
    /* print hostname */
    printf("## hostname : %s\n",hostp->h_name);

    /* print aliases */
    i = 0;
    while (hostp->h_aliases[i]) {
        printf("## alias%d   : %s\n",i+1,hostp->h_aliases[i]);
        i++;
    }
}

void reapchild()
{
	wait(0);
	signal(SIGCLD,reapchild);
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
	fflush(stdout);
}

void usage()
{
    printf("usage : welcome <port_number(59000)> [<snd_file>] \n");
    exit(1);
}

int main(int a, char *b[])
{
	int 	sockfd,newsockfd,clilen,childpid;
	struct 	sockaddr_in cli_addr,serv_addr;
	int 	fd;
	int	con  = 0;
	int	port = -1;
	unsigned char	buf[4096];
	int	sz;
	int     closef = 0;	/* close after recv  */
	int     nodump = 0;	/* no data dump flag */
	int     i;
	char    *snd_file = NULL;

	for (i = 1; i < a; i++) {
	    if (!strcmp(b[i],"-h")) {
		usage();
	    }
	    if (port < 0) {
	        port = atoi(b[i]);
		continue;
            }
	    if (!snd_file) {
	        snd_file = b[i];
		continue;
            }
	}

	if (port < 0) {
	    port = 59000;
	}
	if (snd_file == NULL) {
	    snd_file = DEFAULT_CHIME;
	}

	printf("wait for port %d (chime:%s)\n",port, snd_file);

	if( (sockfd=socket(AF_INET, SOCK_STREAM, 0)) < 0 ){
		perror("socket");
		exit(-1);
	} 

	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);

	if(bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
		perror("bind");
		exit(-1);
	} 

	signal(SIGCLD, reapchild);

	printf("listen...\n");
	if(listen(sockfd, 5) < 0){
		perror("listen");
		exit(-1);
	}
	fflush(stdout);

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
		exit(-1);
	    }
	
	    if (fork() == 0) {
	        /* child */
	        time_t tt;
		char timebuf[64];

	        close(sockfd);

		time(&tt);  /* current time */
		strftime(timebuf, sizeof(timebuf), 
		         "%y/%m/%d %H:%M:%S", localtime(&tt));

	        printf("%s accept No.%d pid=%d\n",timebuf, con, getpid());
	        printf("## client addr=%s, port=%d\n", inet_ntoa(cli_addr.sin_addr), cli_addr.sin_port);
		print_host(cli_addr.sin_addr);
		fflush(stdout);

	        /* サーバ処理 */
		Play(snd_file);

	        close(newsockfd);
	        printf("child pid=%d exit!\n\n",getpid());
	        exit(0);
	    } 
	    close(newsockfd);
	    con++;
        }
}

