/*
 *   miscclient.c
 *
 *   何でもクライアント 
 *
 *     サーバにファイルで指定された任意のデータを送信する
 *     受信結果を、ダンプ形式または、そのままの形でファイルに落す
 *
 *     usage : miscclient <host> <port_num> -s <send_dat_file> 
 *                        [-o <recv_dat_file>] [-w <wait>] [-b] [-v]
 *
 *     97/10/29 V1.00 by oga.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>


/* define */
#define	VER	"1.00"
#define	HOST	"HOST:"
#define	PORT	"PORT:"
#define	SEND	"SEND"

/* funcs */
int  get_send_data();
void get_file_opt();
void memdump();
void GET_DATA(FILE*, int);	/* no use */

/* globals */
int	vf = 0;

main(a,b)
int a;
char *b[];
{
	int	sockfd;		/* ソケットFD                     */
	int	an    = 0;	/* 引数なし引数のカウンタ         */
	int	asis  = 0;	/* recvデータそのまま書き込み(-b) */
	int     i;
	int     len, slen;
	int	waitsec = 1;	/* send後のwait時間               */
	char 	*recvfile   = NULL;
	char 	*sendf      = NULL;
	char	*pt;
	char    arg[10][256];	/* host, port格納用 0,1のみ使用 */
	char	buf[4096];
	FILE	*ofp, *rfp;
	struct  sockaddr_in serv_addr;
	struct  hostent *hep;

	strcpy(arg[0],"");	/* host */
	strcpy(arg[1],"");	/* port */

	for (i = 1; i<a ; i++) {
	    if (!strcmp(b[i],"-h")) {	/* ヘルプ        */
 	        printf("\n*** miscclient Ver%s  by oga. ***\n",VER);
 	        printf("usage : miscclient [<host>] [<port_num>] -s <send_dat_file>\n");
		printf("                   [-o <recv_dat_file>] [-w <wait_sec>] [-b] [-v]\n");
		printf("        -s <send_dat_file> : send data file\n");
		printf("        -o <recv_dat_file> : recv data output file\n");
		printf("        -w                 : wait sec after send\n");
		printf("        -b                 : binary output(no dump mode)\n");
		printf("        -v                 : verbose mode\n");
		printf("\n");
		printf("send_dat_file is following format\n");
		printf("# comment\n");
		printf("HOST:<hostname>\n");
		printf("PORT:<port_num>\n");
		printf("0xnn           1byte data.\n");
		printf("0xnnnn         2byte data.\n");
		printf("0xnnnnnnnn     4byte data.\n");
		printf("[n]:<string>   string data. [n] is string area length.\n");
		printf("[n]:<string>\\n string data with CR/LF\n");
		printf("SEND           send data. (one send block end mark)\n");
 	        exit(1);
	    }
	    if (!strcmp(b[i],"-s")) {	/* 送信データscriptファイル  */
	        sendf = b[++i];
 	        continue;
	    }
	    if (!strcmp(b[i],"-o")) {	/* 受信ファイル              */
	        recvfile = b[++i];
 	        continue;
	    }
	    if (!strcmp(b[i],"-b")) {	/* recvデータそのまま出力    */
	        asis = 1;
 	        continue;
	    }
	    if (!strcmp(b[i],"-w")) {	/* send-recv間の待時間       */
	        waitsec = atoi(b[++i]);
 	        continue;
	    }
	    if (!strcmp(b[i],"-v")) {	/* verbose mode  */
	        vf = 1;
 	        continue;
	    }
	    strcpy(arg[an++],b[i]);	/* 0:host 1:port */
	}

	if (sendf) get_file_opt(sendf,arg[0], arg[1]);

	if (!strlen(arg[0])) {
	    printf("Error: hostname not specified.\n");
	    return 1;
	}
	if (!strlen(arg[1])) {
	    printf("Error: port number not specified.\n");
	    return 1;
	}

	memset((char *)&serv_addr, 0, sizeof(serv_addr));

	hep = gethostbyname(arg[0]);
	if (!hep) {
	    printf("Error: gethostbyname(%s) error \n",arg[0]);
	    return 1;
	}

	serv_addr.sin_family	  = AF_INET;
	serv_addr.sin_addr.s_addr = *(int *)hep->h_addr;
	serv_addr.sin_port	  = htons(atoi(arg[1]));

	if (vf) {
	    pt = (char *)&serv_addr.sin_addr.s_addr;
	    printf("%u,%u,%u,%u(%08x)\n", pt[0],pt[1],pt[2],pt[3],
				serv_addr.sin_addr.s_addr);
	}

	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("socket");
		exit(-1);
	}

	if(connect(sockfd, (struct sockaddr *)&serv_addr, 
					sizeof(serv_addr)) < 0){ 
		perror("connect");
		exit(-1);
	}

	if (recvfile) {
	    if (!(ofp = fopen(recvfile,"w"))) {
    	        printf("Error: recv data file open error : \n");
		perror("fopen");
                return 1;
	    }
	} else {
	    ofp = stdout;
	}

	/* read data and send */
	if (sendf) {
    	    if (!(rfp = fopen(sendf,"r"))) {
    	        printf("Error: send data file open error : \n");
                perror(sendf);
                return 1;
    	    }
    	} else {
	    rfp = stdin;
    	}

	i = 0;
	while ((len = get_send_data(buf,rfp)) != -1) {
	    ++i;
	    /* send data */
	    if (!asis) fprintf(ofp, "**** send data (%d) ****  ---> %s\n", i,arg[0]);
	    slen = send(sockfd, buf, len, 0);
	    if (slen != len) {
	        printf("Warning: send incompleted len=%d send=%d\n",len,slen);
	    }

	    sleep(waitsec);	/* 複数回sendのデータがそろうまで待つ */
	    if (!asis) fprintf(ofp, "**** recv data (%d) ****  <--- %s\n",i,arg[0]);
	    do {
	    	/* recv data */
		len = recv(sockfd,buf,sizeof(buf),0);
		if (len) {
		    if (asis) {
		        /* 読み込んだ物をそのままファイルに書き込み */
		        fwrite(buf,1,len,ofp);
		    } else {
		        /* ダンプ形式 */
		        memdump(ofp,buf,len);
		    }
		}
	    } while (len == sizeof(buf));
	}

	if (rfp != stdin) fclose(rfp);
	if (ofp != stdout) fclose(ofp);

	close(sockfd);

	exit(0);
}

/* 
 *   int get_send_data(buf,fp)
 *
 *   [機能]
 *     ファイル(fp)からsend用データを1ブロック読み取り、bufに格納する。
 *     使用したbufサイズをリターン値として返す。
 *
 *   [引数]
 *   IN  fp : send用データスクリプトのファイルポインタ
 *   OUT buf: データスクリプトをsendデータにしたもの
 *       ret: sendデータのサイズ
 *            スクリプトがEOFになった場合は、-1を返す
 *
 *   [fnameファイルの記述形式]
 *   # コメント
 *   0xnn          数値(1バイト) char 
 *   0xnnnn        数値(2バイト) short
 *   0xnnnnnnnn    数値(4バイト) int
 *   n:文字列\n    改行付き文字列(\nは行末のみ認識)  n:文字列領域の大きさ
 *   n:文字列      改行なし文字列                    n:文字列領域の大きさ
 *   # n:のn省略時は文字列の長さ分だけsendする
 *   SEND	   sendデータブロックの区切り
 *
 *   [例]
 *   # SCT_SEND_ADM
 *   0x0004
 *   # SCT_OP_VERSION_REQ
 *   0x000e
 *   # SCT_OBJ_VERSION
 *   0x001e
 *   # padding
 *   0xffff
 *   # error code
 *   0x00000000
 *   # hostname
 *   50:hogehoge
 *   SEND
 *
 */
int get_send_data(buf,fp)
unsigned char *buf;
FILE *fp;
{
    int  pt = 0;
    int  len;
    int  slen;
    unsigned short *shortp;
    unsigned int   *intp;
    unsigned short xshort;
    unsigned int   xint;
    char wk[4096];
    char *strp;

    memset(wk,0,sizeof(wk));
    while (fgets(wk,sizeof(wk),fp)) {
        if (wk[0] == '#') {
            /* comment */
            memset(wk,0,sizeof(wk));
            continue;
        }
        if (!strncmp(wk,HOST,strlen(HOST))) {
	    /* HOST 埋め込み引数 */
            memset(wk,0,sizeof(wk));
	    continue;
	}
        if (!strncmp(wk,PORT,strlen(PORT))) {
	    /* PORT 埋め込み引数 */
            memset(wk,0,sizeof(wk));
	    continue;
	}
        if (!strncmp(wk,SEND,strlen(SEND))) {
	    /* sendブロック終了 */
	    break;
	}

	/* 改行削除 */
        len = strlen(wk);
        if (wk[len-1] == 0x0a) {
            wk[len-1] = 0;
            --len;
        }
	if (len == 0) {
	    /* 空行なら無視 */
	    continue;
	}

        if (!strncmp(wk,"0x",2)) {
            /* 数値 */
            switch (len) {
              case 4:	/* 1byte */
                buf[pt++] = strtol(wk, (char **)NULL, 0);
                break;
              case 6:	/* 2byte */
                xshort = htons((unsigned short) strtol(wk, (char **)NULL, 0));
		memcpy(&buf[pt],&xshort,sizeof(unsigned short));
                pt += 2;
                break;
              case 10:	/* 4byte */
                xint = htonl((unsigned int) strtoul(wk, (char **)NULL, 0));
		memcpy(&buf[pt],&xint,sizeof(unsigned int));
                pt += 4;
                break;
              default:
                printf("unknown num value (%s)\n",wk);
                break;
            }
        } else {
            /* 文字列 */
            slen = atoi(wk);	   /* 文字列領域長          */
            strp = (char *)strchr(wk,':');
            if (strp) {
                ++strp; 		   /* strp:文字列の開始位置 */
                len = strlen(strp);    /* 文字列長              */
                if (len > 1 && !strncmp(&strp[len-2],"\\n",2)) {
                    strp[len-2] = '\n';
                    strp[len-1] = '\0';
                    --len;
                }
	        if (wk[0] == ':') {
		    /* :<文字列>の場合、文字列の長さ分だけ送る */
		    slen = strlen(strp);
	        }
                memcpy(&buf[pt],strp,slen);
                pt += slen;
	    } else {
	        printf("Error: send file syntax error [%s].\n",wk);
	        printf("String entry is following format.\n",wk);
	        printf("[n]:<string>\n",wk);
	    }
        }
        memset(wk,0,sizeof(wk));
    }
    if (pt == 0) {
        return -1;	/* EOF */
    }
    return pt;
}

/*
 *  send data ファイル内の埋め込みオプションを取り込む。
 *  コマンドの引数に設定されている場合は取り込まない。
 *
 *  IN  sendf : send data file
 *  OUT host  : HOST:で指定されたホスト名
 *      port  : PORT:で指定されたポート番号(文字列)
 *      (注) host,portとも呼出時に長さが0で渡された場合のみ設定される
 *
 */
void get_file_opt(sendf,host,port)
char *sendf, *host, *port;
{
    FILE *fp;
    char buf[2048];
    int  len;

    if (!(fp = fopen(sendf,"r"))) {
        return;
    }
    while (fgets(buf,sizeof(buf),fp)) {
	/* 改行削除 */
        len = strlen(buf);
        if (buf[len-1] == 0x0a) {
            buf[len-1] = '\0';
        }
        if (!strncmp(buf,HOST,strlen(HOST)) && !strlen(host)) {
            strcpy(host,&buf[strlen(HOST)]);
        }
        if (!strncmp(buf,PORT,strlen(PORT)) && !strlen(port)) {
            strcpy(port,&buf[strlen(PORT)]);
        }
        if (strlen(host) && strlen(port)) {
            /* 両方設定された時点で終了 */
            break;
        }
    }
    fclose(fp);
}

#define SIZE 4096
/* 
 *   sockfd : input fd (socket)
 *   fd     : output fp
 */
void GET_DATA(fp, sockfd)
FILE *fp;
int sockfd;
{
	char c[SIZE];
	int size, all=0;
	struct timeval tv, tv2;
	unsigned int wk,wk2,diff;

	if (vf) {
	    gettimeofday(&tv,0);
	    wk = tv.tv_sec*1000000 + tv.tv_usec;
	    printf("start usec : %u\n",wk);
        }

	while((size = read(sockfd, c, SIZE)) != 0) {
		fwrite(c,1,size,fp);
		/* c[size] = '\0'; */
		/* printf("%s",c); */
		/* fflush(stdout); */
		all += size;
	}

	if (vf) {
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
	int kflag = 1;		/* 漢字出力 */
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
