/*
 *  dgserver.c
 *
 *     データグラムサーバー
 *
 *     98/11/03 V1.00 by oga.
 *     06/01/05 V1.01 support Win32
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#ifdef _WIN32
#include <winsock.h>
#include <time.h>
#else  /* !_WIN32 */
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define  WSAGetLastError() errno
#include <signal.h>
#endif /* _WIN32 */

#include <fcntl.h>
#include <errno.h>

/* void PUT_DATA(int, int); */
/* void GET_DATA(int, int); */
/* void reapchild(); */
/* void memdump(FILE *, unsigned char *, int); */

#ifndef _WIN32
void reapchild()
{
	wait(0);
	signal(SIGCLD,reapchild);
}
#endif /* _WIN32 */

main(a,b)
int a;
char *b[];
{
	int 	sockfd;
	struct 	sockaddr_in cli_addr,serv_addr;
	int 	fd;
	int	con = 0, vf = 0;
	int	sz;
	int	port = 58000;
	unsigned char	buf[4096];
	int	alen;
	int     i;

#ifdef _WIN32
        char    wk[256];
        WSADATA WsaData;
#endif

	if (a < 2 || (a > 1 && !strcmp(b[1],"-h"))) {
		printf("usage : dgserver <port_number> [-v]\n");
		exit(1);
	}
	for (i = 1; i<a; i++) {
	    if (!strcmp(b[i],"-v")) {
	        vf = 1;
	        continue;
	    }
	    port = atoi(b[1]);
	}
	printf("wait for port %d\n",port);

#ifdef _WIN32
        if (WSAStartup(0x0101, &WsaData)) {
            sprintf(wk,"WSAStartup Error : %d",WSAGetLastError());
            printf(wk);
            return -1;
        }
#endif

	if( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ){
		perror("socket");
#ifdef _WIN32
                WSACleanup();
#endif
		exit(-1);
	} 

	memset((char *)&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons((short)port);

	if(bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
		perror("bind");
#ifdef _WIN32
                WSACleanup();
#endif
		exit(-1);
	} 

#ifndef _WIN32
	signal(SIGCLD, reapchild);
#endif

	while (1) {
	    alen = sizeof(cli_addr);
	    memset(buf,0,sizeof(buf));
	    sz = recvfrom(sockfd, buf, sizeof(buf), 0, 
	    			(struct sockaddr *)&cli_addr, &alen);
	    if (sz < 0) {
	        perror("recvfrom");
#ifdef _WIN32
                WSACleanup();
#endif
	        exit(1);
	    }

	    buf[sz] = '\0';
	    
            if (vf) {
                printf("##accept %d From %s port %d\n",con,
                       				inet_ntoa(cli_addr.sin_addr), 
                       				ntohs(cli_addr.sin_port));
	    }
	    /* サーバ処理 */
	    printf("Message : %s\n", buf);
	    /* sendto */

	    con++;
        }

#ifdef _WIN32
        WSACleanup();
#endif
}
