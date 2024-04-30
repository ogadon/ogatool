/* 
 *    getip : ホスト名/IPアドレスから、IPアドレス/ホスト名を得る
 *
 *      97/08/08 V1.00 by oga
 *      06/02/11 V1.02 add return 1
 *      12/12/24 V1.03 code share Linux & Win 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
#include "winsock.h"
#define errno WSAGetLastError()
#else
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif /* _WIN32 */


usage()
{
    printf("usage: getip [-a] {hostname|ip_addr}\n");
    exit(1);
}

int main(a,b)
int a;
char *b[];
{
#ifdef _WIN32
    WSADATA  WsaData;
#endif /* _WIN32 */
    int ipf = 1;	/* IP Address 形式 */
    int af  = 0;	/* 1:全情報出力    */
    int i;
    char hname[256];
    char target[128];   /* 指定引数        */
    struct hostent *hostp;
    struct sockaddr_in sin;
    unsigned long  iaddr;

    if (a < 2) {
	usage();
    }

    for (i = 1; i<a; i++) {
	if (!strncmp(b[i],"-h",2)) {
	    usage();
	}
	if (!strncmp(b[i],"-a",2)) {
	    af = 1;
	    continue;
	}
	strcpy(target, b[i]);
    }

    for (i = 0; i<strlen(target); i++) {
	if (!isdigit(target[i]) && target[i] != '.') {
	    ipf = 0;	/* 名称形式　*/
	}
    }
#ifdef _WIN32
    if (WSAStartup(0x0101, &WsaData)) {
	sprintf(hname,"WSAStartup Error : %d",WSAGetLastError());
	printf(hname);
	return -1;
    }
#endif /* _WIN32 */

    if (ipf) {
	iaddr = inet_addr(target);
	sin.sin_addr.s_addr = iaddr;
	hostp = gethostbyaddr((char *)&sin.sin_addr,
				sizeof(sin.sin_addr),
				AF_INET);
	if (hostp) {
	    /* print hostname */
	    if (!af) printf("%s\n",hostp->h_name);
	} else {
	    printf("gethostbyaddr error. errno=%d\n",errno);
	    return 1;
	}
    } else {
	hostp = gethostbyname(target);
	if (hostp) {
	    memcpy(&sin.sin_addr, hostp->h_addr, sizeof(sin.sin_addr));

	    /* print IP Address */
	    if (!af) printf("%s\n",inet_ntoa(sin.sin_addr));
	} else {
	    printf("gethostbyname error. errno=%d\n",errno);
	    return 1;
	}
    }

    if (hostp && af) {
        /* print all IP addrs */
        i=0;
        while (hostp->h_addr_list[i]) {
            memcpy(&sin.sin_addr, hostp->h_addr_list[i], sizeof(sin.sin_addr));
	    printf("address%d : %s\n",i+1,inet_ntoa(sin.sin_addr));
	    i++;
        }

        /* print all hostname/aliases */
        /* print hostname */
	printf("hostname : %s\n",hostp->h_name);
        /* print aliases */
        i=0;
        while (hostp->h_aliases[i]) {
	    printf("alias%d   : %s\n",i+1,hostp->h_aliases[i]);
	    i++;
        }
    }
#ifdef _WIN32
    WSACleanup();
#endif /* _WIN32 */
    return 0;
}

#if 0
#include "winsock.h"

main(a,b)
int a;
char *b[];
{
	WSADATA  WsaData;
	char     msg[256];
	PSERVENT servp;
	HOSTENT  *hostp;
	char	 ghost[256];
	SOCKADDR_IN sin;

	if (a > 1) {
            strcpy(ghost,b[1]);
	} else {
	    printf("usage: gethost <hostname> [servicename]\n");
	    exit(1);
	}

	if (WSAStartup(0x0101, &WsaData)) {
	    sprintf(msg,"WSAStartup Error : %d",WSAGetLastError());
	    printf(msg);
	    return -1;
	}

	if (a > 2) {
	    servp = getservbyname(b[2], "tcp");
	    if (servp) {
	        printf("found biffd in services\n");
	    } else {
	        printf("not found biffd in services\n");
	    }
	}

	hostp = gethostbyname((const char *)ghost);
	if (hostp) {
	    memcpy(&sin.sin_addr, hostp->h_addr, sizeof(sin.sin_addr));
	    printf("gethostbyname %s ok!\n",ghost);
	    printf("IP address is %s\n",inet_ntoa(sin.sin_addr));
	} else {
	    sprintf(msg,"gethostbyname(%s) Error : %d",
						ghost,WSAGetLastError());
	    printf(msg);
	}

        WSACleanup();
	return 0;
}
#endif
