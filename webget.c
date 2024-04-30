/*
 *   webget.c
 *
 *   Webファイル一括Getツール
 *
 *     指定したURL以下のディレクトリから、指定の拡張子のファイルを取り出す。
 *
 *     usage : webget <url> [{-s <suffix> | -all}]
 *
 *     1998/01/17 V1.00 by oga.
 *     1998/01/25 V1.01 support 'frame src='
 *     1998/02/15 V1.02 bug fix on cross reference html
 *     1998/03/07 V1.03 Ignore mailto:
 *     1998/03/17 V1.04 Ignore suffix case
 *     1999/03/07 V1.05 recv error retry
 *     1999/09/03 V1.07 support ommitting "http://" and -na option
 *     2000/07/22 V1.071 bug fix upperdir check
 *     1999/11/09 V1.08 support link trace limit (-t)
 *     2000/03/23 V1.10 support proxy server (not available)
 *     2001/06/26 V1.11 support "xx%" progress disp/ support host:port
 *     2001/07/01 V1.12 delimiter "'" support
 *     2001/07/24 V1.13 support progress bar
 *     2001/10/14 V1.14 support -bd option and delete # in "xx.html#xxx"
 *     2001/10/15 V1.15 support -i option
 *     2001/11/15 V1.16 support Windows
 *     2003/04/07 V1.17 support progress bytes (KB)
 *     2003/05/12 V1.18 support "img.*src"
 *     2003/10/29 V1.19 support signal handler for debug
 *     2003/11/01 V1.20 support analyzed list (donelist)
 *     2003/11/04 V1.21 bug fix upperdir check
 *     2010/01/03 V1.22 trace level 1up for img/default get css
 *     2010/02/14 V1.23 support link trace level 2
 *
 * Varbose Level
 *   1: xx
 *   2: link trace over file
 *   3: all GetURL path
 *
 * 取得の仕様 
 *   (1)ファイルが存在する場合はコンテンツをサーバから取ってこない
 *      但し、-f 指定の場合ファイルが存在してもサーバから取ってくる
 *   (2)既に解析したhtmlファイルは解析しない(V1.20より)
 *      (-na指定の場合、存在するファイルの内部解析をしない (古いオブション))
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <winsock.h>
#else
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#endif

#ifdef _WIN32
#define strncasecmp strnicmp
#define strcasecmp stricmp
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

int gettimeofday(struct timeval *tv, struct timezone *tz);
#endif /* _WIN32 */

/* types */
typedef struct filelist {
	struct filelist *next;
	char *fname;
} filen_t;

/* define */
#define	VER		"1.23"
#define	HOST		"HOST:"
#define	PORT		"PORT:"
#define	HTTP_PREFIX	"http://"
#define	FTP_PREFIX	"ftp://"
#define	dprintf		if (vf >= 4) printf
#define	PERSEC		1000


/* funcs */
void get_file_opt();
void GET_DATA(FILE *, int);	/* no use */
int  GetURLData(char *,int, char *); 
char *GetHREF(FILE *, char *);
char *GetTag(FILE *, char *);
int  mkdirp(char *);
void Url2HostPath(char *, char *, char *, int *);
int  suffck(char *, char *);
int  IsImgPath(char *);
void DelDot2(char *);

/* list utility */
int  AddList(filen_t **, char *);
void DelList(filen_t **, char *);
void FreeList(filen_t **);

/* globals */
int	vf = 0;			/* verboseモード  */
int     ff = 0;			/* forceフラグ    */
int	allf = 0;		/*                */
int     lv = 0;			/* 許可レベル     */
int     bdf = 0;		/* -bd 指定フラグ */
int     imf = 0;		/* -i  指定フラグ V1.15                 */
int     na = 0;			/* 1:ファイルが存在した場合、解析しない */
int	trl  = 0;		/* リンクトレース制限 0:無制限          */
int	trl_plus = 0;		/* リンクトレース制限(画像1plus)        */
int	first = 1;		/* 初取得フラグ                         */
int     trace_level = 0;        /* リンクトレース階層                   */
int     prox_port;	 	/* proxy port 番号 V1.10                */
char    suff[260];		/* suffix         */
char    startdir[1025] = "";	/* start dir      */
char    prox_host[256]=""; 	/* proxy host格納用 V1.10               */
filen_t	*openlist = NULL;	/* open file list */
filen_t	*donelist = NULL;	/* 解析済みlist   */
char    *bar = "##################################################                                                  ";

void usage()
{
    printf("webget Ver %s\n", VER);
    printf("usage : webget <url> [{-s <suffix> | -all}]\n");
    printf("               [-l {<0>|1|2|3}]\n");
    printf("               [-t <link_trace_level(0)>[+]\n");
    printf("               [-p <port>] [-f] [-na] [-v ...]\n");
    printf("               [-i] [-bd <base_dir>]\n");
#if SUPPORT_PROXY
    printf("               [-proxy <host> <port>]\n");
#endif
    printf("        -s <suffix>   : get only spcified suffix file(default .jpg)\n");
    printf("        -all          : get all file\n");
    printf("        -l {<0>|1|2}  : link allow level\n");
    printf("                        0: file under spcified URL only (default)\n");
    printf("                        1: allow same host\n");
    printf("                        2: allow all hosts (suffix only)\n");
    printf("                        3: allow all hosts\n");
    printf("        -t <level>[+] : link trace level. 0:no limit\n");
    printf("                        +: if image, trace level 1 up (ex. -t 3+)\n");
    printf("        -bd           : change base dir for -l option (no http://)\n");
    printf("        -p <port>     : port number\n");
    printf("        -f            : get duplicate file\n");
    printf("        -i            : don't send http version (for i-mode site)\n");
    printf("        -na           : no analize exist file\n");
    printf("        -v            : verbose mode\n");
#if SUPPORT_PROXY
    printf("        -proxy <host> <port> : use proxy server\n");
#endif
}

#ifndef _WIN32
void SigHup(int sig)
{
    ++vf;
    printf("### verbose level = %d\n", vf);
    signal(SIGHUP, SigHup);
}
#endif

int main(a,b)
int a;
char *b[];
{
	int	an      = 0;	/* 引数なし引数のカウンタ         */
	int     i;
	int	waitsec = 1;	/* send後のwait時間               */
	int     port;		/* port 番号 */
	char 	*recvfile   = NULL;
	char 	*sendf      = NULL;
	char 	*pt         = NULL;
	char    host[256];	/* host格納用    */
	char    path[260];	/* URL パス部分  */
	char    url[260];	/* URL           */
	char    basedir[260];	/* base dir      */

#ifdef _WIN32
        WSADATA  WsaData;
#endif


	/* init */
	first = 1;                      /* first data   */
	port = 80;			/* default port */
	strcpy(host,"localhost");	/* default host */
	strcpy(path,"/");		/* suffix */
	strcpy(suff,".jpg");		/* suffix */
	strcpy(url, "");

	/* arg */
	for (i = 1; i<a ; i++) {
	    if (!strcmp(b[i],"-h")) {	/* ヘルプ        */
		usage();
 	        exit(1);
	    }
	    if (!strcmp(b[i],"-s") && i+1<a) {	/* suffix */
	        strcpy(suff,b[++i]);
 	        continue;
	    }
	    if (!strcmp(b[i],"-all")) {	/* 全ファイル取り出し */
	        allf = 1;
 	        continue;
	    }
	    if (!strcmp(b[i],"-l") && i+1<a) {	/* allow level */
	        lv = atoi(b[++i]);
 	        continue;
	    }
	    if (!strcmp(b[i],"-bd") && i+1<a) {	/* base dir V1.  */
	        strcpy(startdir, b[++i]);
 	        continue;
	    }
	    if (!strcmp(b[i],"-p") && i+1<a) {	/* port */
	        port = atoi(b[++i]);
 	        continue;
	    }
	    if (!strcmp(b[i],"-f")) {	        /* force */
	        ff = 1;
 	        continue;
	    }
	    if (!strcmp(b[i],"-i")) {	        /* -i option V1.15-A  */
	        imf = 1;
 	        continue;
	    }
	    if (!strcmp(b[i],"-na")) {	/* no analize exist file V1.07 */
		/* クロスリンク無限ループ対策 */
	        na = 1;
 	        continue;
	    }
	    if (!strcmp(b[i],"-t") && i+1<a) {	/* link trace limit level V1.08 */
		/* リンクトレースレベル制限  0:制限なし */
	        trl = atoi(b[++i]);
		/* V1.22-A start */
		if (b[i][strlen(b[i])-1] == '+') {
		    trl_plus = 1;
		}
		/* V1.22-A end   */
 	        continue;
	    }
	    if (!strcmp(b[i],"-proxy")) {	/* proxy */
		if (i+2 < a) {
		    usage();
		    exit(1);
		}
	        strcpy(prox_host,b[++i]);
	        prox_port = atoi(b[++i]);
 	        continue;
	    }
	    if (!strcmp(b[i],"-v")) {	/* verbose  */
	        ++vf;
 	        continue;
	    }
	    strcpy(url,b[i]);		/* URL */
	}

	if (strlen(url) == 0) {         /* V1.13-A */
	    usage();
	    exit(1);
	}

#ifdef _WIN32
        WSAStartup(0x0101, &WsaData);
	/* Win don't support scroll bar.        */
	/* set i-mode flag (don't use HTTP/1.0) */
	/* imf = 1; */ /* V1.21-D */
#else  /* UNIX */
	signal(SIGHUP, SigHup);
#endif

#if 1
	Url2HostPath(url, host, path, &port);
#else
	/* Url2HostPath(url,host,path)にしよう */
	if (strstr(url,HTTP_PREFIX)) {
	    /* ホスト名取り出し */
	    strcpy(host,&url[strlen(HTTP_PREFIX)]);
	    pt = (char *)strchr(host,'/');	/* pt=/aaa/bbb/test.html */
	    if (pt) {
	        strcpy(path,pt);
		*pt = '\0';
	    }
	    dprintf("# hostname is [%s]\n",host);
	    dprintf("# path     is [%s]\n",path);
	}
#endif

	mkdir(host,0775);
	chdir(host);

	GetURLData(host,port,path);

	FreeList(&donelist);

#ifdef _WIN32
	WSACleanup();
#endif

	exit(0);
}

/*
 *  urlをhost名とpath部分に分解する
 *  V1.11 support host:port
 *
 *  http://host.domain[:port]/aaa/bbb/test.html
 *
 *    => host ... host.domain
 *    => path ... aaa/bbb/test.html
 *    => port ... nnnn
 */
void Url2HostPath(char *url, char *host, char *path, int *port)
{
	char *pt;
	char urlwk[2048];  /* V1.07 */

	if (!strncmp(url, HTTP_PREFIX, strlen(HTTP_PREFIX))) {
	    strcpy(urlwk, url);
	} else {
	    sprintf(urlwk, "%s%s",HTTP_PREFIX, url);
	}

	if (strstr(urlwk,HTTP_PREFIX)) {
	    /* ホスト名取り出し */
	    strcpy(host,&urlwk[strlen(HTTP_PREFIX)]);
	    pt = (char *)strchr(host,'/');	/* pt=/aaa/bbb/test.html */
	    if (pt) {
	        strcpy(path,pt);
		*pt = '\0';
	    }

            /* V1.11 get port */
	    pt = (char *)strchr(host,':');	/* pt=:8080 */
	    if (pt) {
	        *port = atoi(pt+1);
		if (*port == 0) {
		    *port = 80;
		}
		*pt = '\0';
	    }

	    dprintf("# hostname is [%s]\n",host);
	    dprintf("# port     is [%d]\n",*port);
	    dprintf("# path     is [%s]\n",path);
	}
}

/*
 * GetURLData()
 *
 *  IN  host : web hostname
 *      port : port (80)
 *      path : path without hostname (fullpath) (eg. /xxx/yyy.html)
 *
 */
int GetURLData(char *host,int port,char *path) 
{
	FILE	*ofp, *rfp;
	int	sockfd;		/* ソケットFD                     */
	int     len, slen;
	int     total,old;
	int     start,end;
	int     content_len;    /* Content Length         */
	int     port2;
	int     lvplus = 0;     /* trace level up for img V1.22-A */
	struct  sockaddr_in serv_addr;
	struct  hostent *hep;
	char	buf[4096];
	char	wk[4096];
	char	*pt;
	char	wfile[260];
	char    host2[256];	/* リンク先 host格納用    */
	char    path2[260];	/* リンク先 URL パス部分  */
	char    cwd[260];	/* 現在のディレクトリ     */
	char    strbar[100];    /* for progress bar V1.13 */
	struct stat stbuf;

	if (vf >= 3) printf("# GetURLData[%s] trace_level=%d\n",path, trace_level+1);
        ++trace_level;

	/* V1.22-A start */
	if (IsImgPath(path) && trl_plus) {
	    lvplus = 1;
	}
	/* V1.22-A end   */

	if (trl && trace_level > trl+lvplus) {  /* V1.08 V1.22-C */
	    if (vf >= 2) printf("##   Skip load %s (tracelevel %d)\n",path,trace_level);
	    --trace_level;
	    return 1;
	}

	/* コマンドに指定されたpathのディレクトリ部分を保存 */
	if (startdir[0] == '\0') {
	    strcpy(startdir,path);
	    if (pt = (char *)strrchr(startdir,'/')) {
	        if (&startdir[0] != pt) { /* V1.071-A */
	            *pt = '\0';
	        } else {                  /* V1.071-A */
		    *(pt+1) = '\0';
		}
	    }
	}

	/* allow level 0の場合は、指定ディレクトリより上のディレクトリの
	 * ファイルをGetしない
	 */
	if (!first && lv == 0 && strncmp(path,startdir,strlen(startdir))) {
	    if (vf) printf("##   Skip load %s (upper dir)\n",path);
	    --trace_level;
	    return 1;
	}
	first = 0;

        /* wk : 格納先パス用   path : 取得URL */
	if (pt = (char *)strchr(path,'#')) {
	    /* a nameの#は取る */
	    *pt = '\0';
	}
	strcpy(wk,path);
	if (pt = (char *)strchr(wk,'~')) {
	    /* ~は@に変換 */
	    *pt = '@';
	}

	/* ファイル格納用ディレクトリ作成 */
	mkdirp(wk);

	/* 格納用のファイル(wfile)は頭の/を削除 */
	if (wk[0] == '/') {
	    strcpy(wfile,&wk[1]);
	} else {
	    strcpy(wfile,wk);
	}

	/* /で終っていたら、index.htmlを付ける */
	if (!strlen(wfile) || wfile[strlen(wfile)-1] == '/') {
	    strcat(wfile,"index.html");
	}

	/* .html/.css と 指定した拡張子以外のファイルは読み込まない V1.22-C */
	if (!allf && !suffck(wfile,".html") && !suffck(wfile,".htm") 
	          && !suffck(wfile,".css") && !suffck(wfile,suff) ) {
	    /* 読む必要のないファイル */
	    --trace_level;
	    return 1;

	}

	/* cwd 設定 */
	strcpy(cwd,path);
	pt = (char *)strrchr(cwd,'/');
	if (pt) {
	    *(pt+1) = '\0';	/* cwd = /dir1/dir2/ */
	}

	/* 取得済みでない場合読み込む (-forceなら常に読む) */
	if (ff || stat(wfile,&stbuf)) {
	  if (ff) {
	    printf("## reload %s .\n");
	  }
	  /* 
	   *  start URL Get
	   */
	  memset((char *)&serv_addr, 0, sizeof(serv_addr));

	  hep = gethostbyname(host);
	  if (!hep) {
	    printf("Error: gethostbyname(%s) error \n",host);
	    --trace_level;
	    return 1;
	  }

	  serv_addr.sin_family	  = AF_INET;
	  serv_addr.sin_addr.s_addr = *(int *)hep->h_addr;
	  serv_addr.sin_port	  = htons(port);

	  if (vf >= 2) {
	    pt = (char *)&serv_addr.sin_addr.s_addr;
	    /*printf("# IP addr : %u.%u.%u.%u (%08x)\n", pt[0],pt[1],pt[2],pt[3],
				serv_addr.sin_addr.s_addr); */
	    printf("# IP addr : %s\n", inet_ntoa(serv_addr.sin_addr));
	  }

	  if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("socket");
	        --trace_level;
	        return 1;
	  }

	  if(connect(sockfd, (struct sockaddr *)&serv_addr, 
					sizeof(serv_addr)) < 0){ 
		perror("connect");
	        --trace_level;
	        return 1;
	  }

	  /* 
	   *  write file open
	   */
	  dprintf("# fopen(%s)\n",wfile);
	  if (!(ofp = fopen(wfile,"wb"))) {             /* V1.16-C */
    	        printf("Error: recv data file open error : \n");
		perror("fopen");
	        --trace_level;
                return 1;
	  }

          if (imf) {    /* V1.15-A */
	      sprintf(buf, "GET %s\n",path);            /* V1.11-D V1.15-A */
	  } else {
	      sprintf(buf, "GET %s HTTP/1.0\n\n",path); /* V1.11-A */
	  }
	  len = strlen(buf);

	  dprintf("# send request = %s",buf);

          /* send data */
	  slen = send(sockfd, buf, len, 0);
	  if (slen != len) {
	    printf("Warning: send incompleted len=%d send=%d\n",len,slen);
	  }

          /* V1.11-A start */
	  content_len = 0;
	  if (!imf) {
	    /* for "GET xx HTTP/1.0" */
	    /* Get HTTP Header       */
	    do {
	      recv_oneline(sockfd, buf, sizeof(buf));
	      if (!strncasecmp(buf, "Content-Length: ", 16)) {
		  content_len = atoi(&buf[strlen("Content-Length: ")]);
	          dprintf("content_len = %d\n", content_len);
	      }
	    } while(strlen(buf) != 0);
	  }
          /* V1.11-A end   */

	  total = 0;
	  old   = 0;
	  printf("## LV:%d loading %s .",trace_level, wfile);
	  if (content_len) printf("\n");    /* V1.11 */
          fflush(stdout);
	  start = clock1000();
	  do {
	    /* recv data */
	    len = recv(sockfd,buf,sizeof(buf),0);
	    if (len > 0) {
	        fwrite(buf,1,len,ofp);
	    }
	    if (len < 0) {	/* V1.05 */
	        printf("# recv error (%d). retry!!\n");
	        continue;
	    }
	    /* printf("receive %d bytes\n", len); */
	    total += len;

	    if (content_len) {      /* V1.11-A */
		 /* for disp by %      V1.11-A */
		 int pcent;
		 if (content_len > 1024*1024) {
	             pcent = total/(content_len/100);
		 } else {
	             pcent = (total*100)/content_len;
		 }
		 /* for disp progress bar V1.03-A */
		 strncpy(strbar, &bar[50-pcent/2], 50);
		 strbar[0] = strbar[10] = strbar[20] = strbar[30] 
			   = strbar[40] = strbar[50] = '|';
		 strbar[51] = '\0';
	         /* printf("  %3d%% %s\n\033M", pcent, strbar); */
#ifdef _WIN32
	         printf("  %3d%% %s  %d/%d KB\r", pcent, strbar, total/1024, content_len/1024); /* V1.17-C */
#else  /* !_WIN32 */
	         printf("  %3d%% %s  %d/%d KB\n\033M", pcent, strbar, total/1024, content_len/1024); /* V1.17-C */
#endif /* !_WIN32 */
	         fflush(stdout);
            } else {
		 /* disp by dot */
	        if (total > old+4096) {
	            printf(".");
	            fflush(stdout);
	            old += 4096;
	        }
	    }
	  } while (len > 0);

	  if (ofp != stdout) {
	      dprintf("\n# fclose(%s)\n",wfile);
	      fclose(ofp);
	  }
	  close(sockfd);
	  end = clock1000();

          {
	      char wk2[256];          /* V1.17-A */
	      sprintf(wk2, " (%d bytes  %.2fKB/sec)                                                                ", /* V1.17-C */
	              total,((float)total)/(end-start)/1.024);
	      wk2[79] = '\0';         /* V1.17-A */
	      printf("%s\n", wk2);    /* V1.17-A */
	  }
	} else {
	    if (vf) printf("## %s is already loaded. skip load.\n",wfile);
	    if (na) {		/* V1.07 */
		/* 既にファイルが存在する場合は、ファイル内部を解析しない */
	        if (vf) printf("## and skip analyze.\n",wfile);
	        --trace_level;
		return 0;	/* V1.07 */
	    }			/* V1.07 */
	}


	/*
	 *  取り出したファイル(wfile) がHTMLなら 内容を解析して、
	 *  GetURLData()をリカーシブコール
	 *  
	 *  既に解析済みのものは解析しない
	 *    (トレース階層がリミットレベルの場合は解析済みリストに追加しない)
	 */
	if ((suffck(wfile,".html") || suffck(wfile,".htm"))
	    && ((trace_level >= trl+lvplus) || AddList(&donelist, wfile))) { /* V1.22-C */
	    /* html && 未解析 => 内容解析 */
	    if (AddList(&openlist, wfile) && (rfp = fopen(wfile,"r"))) {
		while (GetHREF(rfp,buf)) {
		    dprintf("# get link data %s\n",buf);
		    if (strstr(buf,HTTP_PREFIX)) {
			/* http://xxxx 形式 */
			port2 = 80; /* V1.11 */
		        Url2HostPath(buf, host2, path2, &port2);  /* V1.11-C */
		        if (!strlen(host2) || !strcmp(host2,host)) {
			    /* 自サイトならリンク先を取りに行く */
		            GetURLData(host,port,path2);
		        } else if (lv == 2 && suffck(wfile, suff)) {
			    /* 自サイトでなくてもallow level 2 で指定ファイルなら 
			     * リンク先を取りに行く 
			     */
		            GetURLData(host2,port2,path2);  /* V1.11-C */
		        } else if (lv >= 3) {
			    /* 自サイトでなくてもallow level 3 以上なら
			     * リンク先を取りに行く 
			     */
		            GetURLData(host2,port2,path2);  /* V1.11-C */
		        } else {
		            /* ホストが違うのでロードしない */
	    		    if (vf) printf("##   Skip load %s (other host)\n",buf);
		        }
		    } else if (strstr(buf,FTP_PREFIX)) {
		        continue;  /* V1.071-A */
		    } else {
		        /* http://指定なし */
			if (buf[0] == '/') {
			    /* 自ホスト絶対パス */
		            GetURLData(host,port,buf);
			} else if (!strcasecmp(buf,"mailto:")){
			    /* "mailto:<name@mail.adr>" は無視 V1.03 */
			    ;
			}else {
			    /* 自ホスト相対パス */
			    strcpy(wk,cwd);	/* Base Dir      */
			    strcat(wk,buf);	/* absolute path */
			    DelDot2(wk);
		            GetURLData(host,port,wk);
			}
		    }
		}
		fclose(rfp);
		DelList(&openlist, wfile);
	    }
	}

        --trace_level;
	return 0;

}

/*
 *   suffix check
 *
 *      bufの終りがsufixであるかチェックする(大文字小文字無視)
 *
 *   IN  buf   : check path
 *       sufix : suffix
 *   OUT ret   : 1 ... match suffix
 *               0 ... no match suffix
 */
int suffck(char *buf, char *sufix)
{
    int len = strlen(buf);
    int len2 = strlen(sufix);

    if (len < len2) {
        return 0;
    }

    return (!strcasecmp(&buf[len-len2],sufix));
}

/*
 *  <...>のタグを取り出す
 *  
 *  IN   fp  : fopen("r")済みHTMLファイルポインタ 
 *  OUT  buf : タグ"<....>"を返す
 *       ret : buf タグ格納
 *             0   EOF
 */
char *GetTag(FILE *fp, char *buf)
{
    int i=0;
    int c=0;

    i = 0;
    while (c != EOF && c != '<') c = getc(fp);
    if (c == EOF) return 0;
    while (c != EOF && c != '>') {
	buf[i++] = c;
        c = getc(fp);
    }
    buf[i] = '\0';
    return buf;
}

/*
 *  bufから<link.*href="url"> を見つけたら"src"の先頭ポインタを返す V1.22
 *
 *
 */
char *GetCssSrc(char *buf)
{
    char *pt;

    if (!strstr(buf, "link ")) {
        if (!strstr(buf, "LINK ")) {
            return NULL;
        }
    }
    if (pt = strstr(buf, "href")) {
        return pt;
    }
    if (pt = strstr(buf, "HREF")) {
        return pt;
    }
    return NULL;
}

/*
 *  bufから<img.*src="url"> を見つけたら"src"の先頭ポインタを返す V1.18
 *
 *
 */
char *GetImgSrc(char *buf)
{
    char *pt;

    if (!strstr(buf, "img ")) {
        if (!strstr(buf, "IMG ")) {
            return NULL;
        }
    }
    if (pt = strstr(buf, "src")) {
        return pt;
    }
    if (pt = strstr(buf, "SRC")) {
        return pt;
    }
    return NULL;
}

/*
 *  a href= または、img src=のリンク先をbufに返す
 *  
 *  img src=があったら、GetURLDataを呼ぶ 
 *  
 *  IN   fp  : fopen("r")済みHTMLファイルポインタ 
 *  
 *  OUT  ret : buf リンクあり
 *             0   EOF
 */
char *GetHREF(FILE *fp, char *buf)
{
	int  i = 0;
	char *pt;
	char *status;
	char wk[4096];

	while (status = GetTag(fp,wk)) {
	    if ((pt = GetImgSrc(wk)) 
	     || (pt = GetCssSrc(wk))                    /* V1.22 */
	     || (pt = (char *)strstr(wk,"frame src"))   /* V1.01 */
	     || (pt = (char *)strstr(wk,"FRAME SRC"))   /* V1.01 */
	     || (pt = (char *)strstr(wk,"a href"))
	     || (pt = (char *)strstr(wk,"A href"))
	     || (pt = (char *)strstr(wk,"A HREF"))) {
	         while (*pt != '"' && *pt != '\'' && *pt != '\0') pt++;/*V1.12*/
	         pt++;
	         while (*pt != '"' && *pt != '\'' && *pt != '\0') {    /*V1.12*/
	             buf[i++] = *pt;
	             ++pt;
	         }
	         buf[i] = '\0';
	         break;
	    }
	}
	if (status == NULL) {
	    return (char *)0;
	}
	dprintf("# find Link [%s]\n",buf);
	return buf;
}

/*
 *  指定されたパスのディレクトリ部分のディレクトリを作成する
 *
 *  path = /aaa/bbb/test.html
 *
 *  mkdir -p ./aaa/bbb
 */
int mkdirp(char *path)
{
    char *pt;
    char cwd[260];
    char wk[260];

    dprintf("# mkdirp=[%s]\n",path);

    getcwd(cwd,sizeof(cwd));

    dprintf("#   pwd=[%s]\n",cwd);

    /* path = /aaa/bbb/test.html */
    if (path[0] == '/') {
        strcpy(wk,&path[1]);
    } else {
        strcpy(wk,path);
    }
    pt = (char *)strrchr(wk,'/');
    if (!pt) {
	/* ディレクトリ部分なし */
	return 0;
    }
    *pt = '\0';
    /* wk = aaa/bbb */

    pt = (char *)strtok(wk,"/");
    while (pt) {
	dprintf("#   mkdir %s\n",pt);
	mkdir(pt, 0775);
	chdir(pt);
	pt = (char *)strtok(NULL,"/");
    }

    chdir(cwd);
    return 0;

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

/*
 *   clock1000()
 *
 *       1/PERSEC sec 単位の値をリターンする
 */
int clock1000()
{
    int code = 0;
#ifdef AIX
    struct tms buf;

    code = times(&buf)*PERSEC/HZ;
#else
    struct timeval tv;
    struct timezone tz;
    gettimeofday(&tv,&tz);

    code = tv.tv_sec*PERSEC+(tv.tv_usec)/(1000000/PERSEC);
#endif /* AIX */
    return code;
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

	if (vf >= 3) {
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

	if (vf >= 3) {
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
 *  listにpathがあるかチェックする
 *
 *  OUT ret  0:すでにpathはlistにある
 *           1:listにpathを追加した
 *
 */
int AddList(filen_t **listpp, char *path)
{
    filen_t *wk, *wkold;
    wk    = *listpp;
    wkold = (filen_t *)listpp;
    while (wk) {
        if (!strcmp(wk->fname,path)) {
            /* already open */
            dprintf("# already opend [%s]\n",path);
            return 0;
        }
        wkold = wk;
        wk = wk->next;
    }
    wk = (filen_t *)malloc(sizeof(filen_t));
    memset(wk,0,sizeof(filen_t));
    wkold->next = wk;
    wk->fname   = (char *)strdup(path);
    return 1;
}

/*
 *  listからpathのあるエントリを削除する
 *
 */
void DelList(filen_t **listpp, char *path)
{
    filen_t *wk, *wkold;
    wk    = *listpp;
    wkold = (filen_t *)listpp;
    while (wk) {
        if (!strcmp(wk->fname,path)) {
            wkold->next = wk->next;
            free(wk->fname);
            free(wk);
            return;
        }
        wkold = wk;
        wk = wk->next;
    }
    printf("# path(%s) not found in openlist\n",path);
}

/*
 *  listをすべて開放する
 *
 */
void FreeList(filen_t **listpp)
{
    filen_t *wk, *wknext;
    int     cnt = 1;

    wk = *listpp;
    while (wk) {
        if (vf >= 3) printf("# FreeList(%d:%s)\n", cnt, wk->fname);
	wknext = wk->next;
        free(wk->fname);
        free(wk);
        wk = wknext;
	++cnt;
    }
    *listpp = NULL;
}

/*
 *  path から、/../ /./を除いてきれいなパスにする
 *
 */
void DelDot2(char *path)
{
    char *pt,*pt2;
    if (pt = (char *)strstr(path,"/../")) {
        pt2 = pt + strlen("/../");    /* pt2 : aaa/bbb/../Ibbb */
        *pt = '\0';                   /* path: aaa/bbb ../bbb  */
        pt = (char *)strrchr(path,'/');       /* pt  : aaaI/bbb ../bbb */
        pt++;			      /* pt  : aaa/Ibbb ../bbb */
        strcpy(pt,pt2);
        DelDot2(path);
    }
    if (pt = (char *)strstr(path,"/./")) {
        pt2 = pt + strlen("/./");     /* pt2 : aaa/bbb/./Ibbb */
        pt++;			      /* pt  : aaa/bbb/I./bbb */
        strcpy(pt,pt2);
        DelDot2(path);
    }
}

/* 
 *   1行recv  V1.11
 *   IN   sockfd  socket fd
 *        sz      bufのサイズ
 *   OUT  buf     読み込んだ1行(改行除く)
 *        ret     読み込んだ文字数(改行除く)
 *
 */
int recv_oneline(int sockfd, char *buf, int sz)
{
    int i = 0;
    int ret = 0;

    while (i < (sz-1) && (ret = recv(sockfd,&buf[i],1,0))) {
	if (buf[i++] == 0x0a) {
	    break;
	}
    }
    buf[i] = '\0';

    /* del CRLF */
    if (buf[i-1] == 0x0a) {
        buf[i-1] = '\0';
	i--;
    }
    if (buf[i-1] == 0x0d) {
        buf[i-1] = '\0';
	i--;
    }

    dprintf("buf=[%s]\n",buf);
    return i;
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

/*
 *   イメージ等のファイルかどうかのチェック  
 *
 *   IN  path  path(URL)
 *   OUT ret   0: no image file
 *             1: image file path
 */
int IsImgPath(char *path)
{
	int pos;

	pos = strlen(path) - 4;
	/* とりあえずこれだけが対象 */
	if (!strcasecmp(&path[pos],   ".jpg")  ||
	    !strcasecmp(&path[pos-1], ".jpeg") ||
	    !strcasecmp(&path[pos],   ".gif")  ||
	    !strcasecmp(&path[pos],   ".bmp")  ||
	    !strcasecmp(&path[pos],   ".pdf")  ||
	    !strcasecmp(&path[pos],   ".png")) {
	    return 1;
	}
	return 0;
}

/* vim:ts=8:sw=8
 */
