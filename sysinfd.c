/*
 *   sysinfd.c
 *     System Information Service Daemon
 *
 *   2001/12/03 V1.00  by oga.  (MEM, LOAD)
 *   2002/02/03 V1.01  Keep Connection
 *   2013/02/25 V1.02  add socket close()
 *
 *   Protocol
 *      HELP
 *         Return Help String\n\n
 *
 *      GET_MEM
 *         Get Memory/Swap Info (KB)
 *         Total Free Shared Buffers Cached SwapTotal SwapFree
 *         ---------------------------------------------------------
 *         69860 1520 4 3008 57292 51400 27728\n\n
 *
 *      GET_LOAD
 *         Get Load Ave. Info
 *         1min 5min 15min
 *         ---------------------------------
 *         0.00 0.00 0.00\n\n
 *
 *   All Rights Reserved. Copyright (C) 2001,2002, Moritaka Ogasawara. 
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pwd.h>

/* Keep Connection V1.01-A */
#define KEEP_CONN

#ifndef TCP_PORT
#define TCP_PORT 9998
#endif

#define SIZE	1024

#ifndef SIGCLD
#define SIGCLD SIGCHLD
#endif

/* request keywords */
#define GET_MEM		"GET_MEM"
#define GET_LOAD	"GET_LOAD"
#define LOGIN		"LOGIN"
#define HELP		"HELP"
#define EXIT		"EXIT"

/* for help */
#define HELP_STR	"GET_MEM  GET_LOAD  HELP  EXIT\n\n"

/* 仮のdefine */
#define READ	read
#define WRITE	write

/* globals */
int     vf   = 0;	/* 1:verbose mode */
int	sockfd;


/* wait child process */
void reapchild()
{
	wait(0);
	signal(SIGCLD,reapchild);
}

/* end process */
void sigint()
{
	printf("sysinfd interrupted.\n");
	close(sockfd);
	exit(1);
}


char *get_item(char *buf, char *sep, int pos, char *outbuf)
{
    int i;
    char *pt;
    char *p;
    char wk[4096];
    char msglog[4096];

    strcpy(wk,buf);      /* strtok()はbufを破壊するためコピーして利用する */

    for (i = 0; i<pos; i++) {
	if (i == 0) {
	    pt = (char *)strtok(wk,sep);
	} else {
	    pt = (char *)strtok(NULL,sep);
	}
	if (pt == NULL) break;
    }
    if (pt == NULL) {
	sprintf(msglog, "Out of item(%s) pos(%d).",buf,pos);
	printf(msglog);
	/* 結果領域クリア */
	strcpy(outbuf,"");
	return (char *)0;
    }

    strcpy(outbuf, pt);

    /* cut tail space */
    p = &outbuf[strlen(outbuf)-1];	/* last char */
    while (*p == ' ' || *p == 0x0a) --p;
    *(p+1) = '\0';

    return outbuf;
} /* get_item */


/*
 *   fd から読み込んだ内容をnewsockfdに書き込む
 *
 *   not used
 */
void PUT_DATA(int fd, int newsockfd)
{
	char c[4096];
	int size;
        while((size = read(fd, c, SIZE)) != 0) {
            write(newsockfd, c,size);
	}
}

/*
 *   READ(fd,buf,size)
 *
 *    OUT ret  : 0  success
 *             : -1 error
 */

/*
 *   WRITE(fd,buf,size)
 *
 *    OUT ret  : 0  success
 *             : -1 error
 */

/*
 *   Help(fd)
 *      send HELP
 *
 *    IN  fd   : output fd
 *    OUT ret  : 0  success
 *             : -1 error
 */
int Help(fd,path)
int fd;
char *path;
{
    WRITE(fd, HELP_STR, strlen(HELP_STR));
    return 0;
}

/*
 *   GetMem(fd)
 *
 *    IN  fd   : output fd
 *    OUT ret  : 0  success
 *             : -1 error
 */
int GetMem(int fd)
{
    char buf[2048];
    char wk[2048];
    FILE *fp;
    char *path = "/proc/meminfo";
    ulong total, freem, shared, buffer, cache, swtotal, swfree;

    if (vf) printf("GET_MEM\n");

    total = freem = shared = buffer = cache = swtotal = swfree = 0;

    if (!(fp = fopen(path, "r"))) {
        sprintf(buf,"GetMem: fopen(%s) error errno=%d\n", path, errno);
	printf(buf);
        WRITE(fd, buf, strlen(buf));
	return -1;
    }
    while (fgets(buf, sizeof(buf), fp)) {
        if (!strncmp(buf, "MemTotal:", strlen("MemTotal:"))) {
	    total = atoi(get_item(buf, " ", 2, wk));    /* 1 */

	} else if (!strncmp(buf, "MemFree:", strlen("MemFree:"))) {
	    freem = atoi(get_item(buf, " ", 2, wk));    /* 2 */

	} else if (!strncmp(buf, "MemShared:", strlen("MemShared:"))) {
	    shared  = atoi(get_item(buf, " ", 2, wk));  /* 3 */

	} else if (!strncmp(buf, "Buffers:", strlen("Buffers:"))) {
	    buffer  = atoi(get_item(buf, " ", 2, wk));  /* 4 */

	} else if (!strncmp(buf, "Cached:", strlen("Cached:"))) {
	    cache   = atoi(get_item(buf, " ", 2, wk));  /* 5 */

	} else if (!strncmp(buf, "SwapTotal:", strlen("SwapTotal:"))) {
	    swtotal = atoi(get_item(buf, " ", 2, wk));  /* 6 */

	} else if (!strncmp(buf, "SwapFree:", strlen("SwapFree:"))) {
	    swfree  = atoi(get_item(buf, " ", 2, wk));  /* 7 */
	}
    }
    fclose(fp);
    sprintf(buf, "%u %u %u %u %u %u %u\n\n", 
                  total, freem, shared, buffer, cache, swtotal, swfree);

    return WRITE(fd, buf, strlen(buf));
}

/*
 *   GetLoad(fd)
 *
 *    IN  fd   : output fd
 *    OUT ret  : 0  success
 *             : -1 error
 */
int GetLoad(int fd)
{
    char buf[2048];
    FILE *fp;
    char *path = "/proc/loadavg";
    char *pt;

    if (vf) printf("GET_LOAD\n");

    if (!(fp = fopen(path, "r"))) {
        sprintf(buf,"GetLoad: fopen(%s) error errno=%d\n", path, errno);
	printf(buf);
        WRITE(fd, buf, strlen(buf));
	return -1;
    }
    if (fgets(buf,sizeof(buf),fp) == NULL) {
        sprintf(buf,"GetLoad: fgets(%s) error errno=%d\n", path, errno);
	printf(buf);
        WRITE(fd, buf, strlen(buf));
        fclose(fp);
	return -1;
    }
    fclose(fp);

    /* "0.00 0.00 0.00 2/42 494"  => "0.00 0.00 0.00" */
    pt = strchr(buf, ' ');
    ++pt;
    pt = strchr(pt, ' ');
    ++pt;
    pt = strchr(pt, ' ');
    *pt = '\n';
    ++pt;
    *pt = '\n';
    ++pt;
    *pt = '\0';

    return WRITE(fd, buf, strlen(buf));
}

/*
 *   ErrorMsg(fd,str)
 *      send error message
 *
 *    IN  fd   : output fd
 *        str  : request string
 *    OUT ret  : 0  success
 *             : -1 error
 */
int ErrorMsg(fd,str)
int fd;
char *str;
{
    char buf[2048];

    sprintf(buf,"%s request could not understand.\n",str); 
    return WRITE(fd,buf,strlen(buf)) ;
}

/* 
 *  メッセージ受信と処理
 *
 *    IN  sfd : accept fd
 *        con : connection number
 *    OUT ret : 0  success
 *            : -1 error
 */
int GetAndProc(int sfd, int con)
{
	char buf[SIZE];
	char sndbuf[SIZE];
	int  size;
	int  st = 0;

	/* get request */
	size = READ(sfd, buf, SIZE);
	if (size < 0) {
	    perror("read");
	    return -1;
	}

	/* delete CR/LF */
	if (buf[size-2] == 0x0d && buf[size-1] == 0x0a) {
	    buf[size-2] = '\0';
	} else if (buf[size-1] == 0x0a) {
	    buf[size-1] = '\0';
	} else {
	    buf[size] = '\0';
	}

	/* dispatch request process */
	if (!strncasecmp(buf,HELP,strlen(HELP))) {
	    /* HELP */
	    st = Help(sfd);
	} else if (!strncasecmp(buf, EXIT, strlen(EXIT))) {
	    /* EXIT */
	    if (vf) printf("exit.\n");
	    st = -1;
	} else if (!strncasecmp(buf, GET_MEM, strlen(GET_MEM))) {
	    /* GET_MEM */
	    st = GetMem(sfd);
	} else if (!strncasecmp(buf, GET_LOAD, strlen(GET_LOAD))) {
	    /* GET_LOAD */
	    st = GetLoad(sfd);
	} else {
	    st = ErrorMsg(sfd,buf);
	}
	return st;
}


int main(int a, char *b[])
{
	int 	newsockfd,clilen,childpid;
	struct 	sockaddr_in cli_addr,serv_addr;
	int 	fd;
	char 	*sendfile;
	int	con = 0;
	int     i;
	int     port = 0;	/* port number    */

	/* pars args */
        for (i = 1; i<a; i++) {
            if (!strncmp(b[i],"-h",2)) {
		printf("usage : sysinfd [-v] [<portnum>]\n");
		exit(1);
            }
            if (!strncmp(b[i],"-v",2)) {
		vf = 1;
		continue;
            }
            port = atoi(b[i]);
        }

        if (!port) {
            port = TCP_PORT;
        }

	if( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ){
		perror("socket");
		exit(-1);
	} 

	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);

	if (vf) printf("wait on port (%d)\n", port);

	if(bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
		perror("bind");
		exit(-1);
	} 

	signal(SIGCLD, reapchild);
	signal(SIGINT, sigint);

	if (vf) printf("listen...\n");
	if(listen(sockfd, 5) < 0){
		perror("listen");
		exit(-1);
	}

	while (1) {
	    clilen = sizeof(cli_addr);
	    newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr,
				&clilen);
	    if(newsockfd < 0){
	        if (errno == EINTR) {
	            if (vf) printf("accept interrupted\n");
	            continue;
	        }
		perror("accept");
		exit(-1);
	    }
	
	    if (fork() == 0) {
	        /* child */
	        close(sockfd);
	        if (vf) printf("accept %d pid=%d\n",con,getpid());

	        /* サーバ処理 */	
#ifdef KEEP_CONN
                while (1) {
		    /* もらったデータに対する処理 */
	            if (GetAndProc(newsockfd,con) < 0) {
		        if (vf) printf("Connection closed.\n");
	    		close(newsockfd);   /* V1.02-A */
	                exit(0);
		    }
		}
#else
	        GetAndProc(newsockfd,con);  /* もらったデータに対する処理 */
    		close(newsockfd);           /* V1.02-A */
	        exit(0);
#endif
	    } 
	    close(newsockfd);
	    con++;
        }
}

