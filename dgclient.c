/*
 *   dgclient.c
 *
 *   データグラムクライアント 
 *
 *     usage : dgclient <host> <port_num> -b[roadcast] [-m <msg>]
 *
 *     1998/11/03 V1.00 by oga.
 *     2000/01/04 V1.01 -m support
 *     2006/01/05 V1.02 support Win32
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>

#ifdef _WIN32
#include <winsock.h>
#include <time.h>
#else  /* !_WIN32 */
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#define  WSAGetLastError() errno
#endif /* _WIN32 */

#include <fcntl.h>


/* define */
#define	VER	"1.02"

/* funcs */

/* globals */
int	vf = 0;

void usage()
{
	printf("\n*** dgclient Ver%s  by oga. ***\n",VER);
	printf("usage : dgclient <host> <port_num> [-b] [-m <msg>]\n");
	printf("        -b       : broadcast\n");
	printf("        -m <msg> : send message\n");
	printf("        -v       : verbose mode\n");
	exit(1);
}

main(a,b)
int a;
char *b[];
{
	int	sockfd;		/* ソケットFD                     */
	int	an    = 0;	/* 引数なし引数のカウンタ         */
	int	bcast = 0;	/* broadcast                      */
	int	msgf  = 0;	/* -m flag                        */
	int     i;
	int	waitsec = 1;	/* send後のwait時間               */
	char	*pt;
	char	*msg = "dummy"; /* -m <msg>                       */
	char    arg[10][256];	/* host, port格納用 0,1のみ使用 */
	char	buf[4096];
	struct  sockaddr_in serv_addr;
	struct  hostent *hep;

#ifdef _WIN32
        char    wk[256];
        WSADATA WsaData;
#endif

	strcpy(arg[0],"");	/* host */
	strcpy(arg[1],"");	/* port */

	for (i = 1; i<a ; i++) {
	    if (!strcmp(b[i],"-h")) {	/* ヘルプ        */
	        usage();
	    }
	    if (!strcmp(b[i],"-b")) {	/* recvデータそのまま出力    */
	        bcast = 1;
 	        continue;
	    }
	    if (!strcmp(b[i],"-m")) {	/* verbose mode  */
	        msgf = 1;
		msg = b[++i];
 	        continue;
	    }
	    if (!strcmp(b[i],"-v")) {	/* verbose mode  */
	        vf = 1;
 	        continue;
	    }
	    strcpy(arg[an++],b[i]);	/* 0:host 1:port */
	}

	if (!strlen(arg[0])) {
	    printf("Error: hostname not specified.\n");
	    usage();
	}
	if (!strlen(arg[1])) {
	    printf("Error: port number not specified.\n");
	    usage();
	}

	memset((char *)&serv_addr, 0, sizeof(serv_addr));

#ifdef _WIN32
        if (WSAStartup(0x0101, &WsaData)) {
            sprintf(wk,"WSAStartup Error : %d",WSAGetLastError());
            printf(wk);
            return -1;
        }
#endif

	hep = gethostbyname(arg[0]);
	if (!hep) {
	    printf("Error: gethostbyname(%s) error (%d)\n",arg[0], WSAGetLastError());
#ifdef _WIN32
            WSACleanup();
#endif
	    return 1;
	}

	serv_addr.sin_family	  = AF_INET;
	serv_addr.sin_port	  = htons((short)atoi(arg[1]));
	if (bcast) {
	    serv_addr.sin_addr.s_addr = INADDR_BROADCAST;
	} else {
	    serv_addr.sin_addr.s_addr = *(int *)hep->h_addr;
	}

	if (vf) {
	    pt = (char *)&serv_addr.sin_addr.s_addr;
	    printf("%d,%d,%d,%d(%08x)\n", 
	    	(u_char)pt[0],(u_char)pt[1],(u_char)pt[2],(u_char)pt[3],
		serv_addr.sin_addr.s_addr);
	}

	if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		perror("socket");
#ifdef _WIN32
                WSACleanup();
#endif
		exit(-1);
	}

	if (bcast) {
	    int On = 1;
	    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST,
	    			(char *)&On, sizeof(On))) {
	        perror("setsockopt");
#ifdef _WIN32
                WSACleanup();
#endif
	        exit(1);
	    }
	    printf("Broadcast Enable!!\n");
	}

	/* read data and send */
	while(1) {
	    if (msgf) {
		strcpy(buf, msg);
	    } else {
	        printf("Send Message : ");
	        fflush(stdout);
                buf[sizeof(buf)-1] = '\0';
	        fgets(buf, sizeof(buf), stdin);
	        if (!strncmp(buf, "quit", 4) || !strncmp(buf, "exit", 4)) {
	            break;
	        }
	    }
	    sendto(sockfd, buf, strlen(buf)+1, 0,
	    		(struct sockaddr *)&serv_addr, sizeof(serv_addr));
	    if (msgf) break;
	}

	close(sockfd);

#ifdef _WIN32
        WSACleanup();
#endif
	exit(0);
}
