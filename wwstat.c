/*
 *	wwstat : httpd-logの統計情報をとり、結果をHTMLで出力する。
 *
 *			95/10/17  V0.10 by Hyper Halx.oga 
 *			96/06/17  V0.11 add href
 *			96/07/08  V0.12 add hosts href
 *			96/07/12  V0.13 expand freq area
 *			01/09/19  V0.14 support Linux
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VER		"0.14"
/* #define	HTTPD_LOG	"/usr/local/etc/httpd/httpd-log" */
#define	HTTPD_LOG	"/var/log/httpd/access_log"
#define	MAX_HOST	5000
#define	MAX_URL		10000

#define dprintf	if (vbf) printf

struct name_t {
	int	refc;
	char	*namep;
};

int	nh=0,nu=0;		/* host, urlの数 */
int	vf = 0, hlf = 0, ulf = 0, vbf = 0;
char	*vhost = (char *)0;

void usage()
{
	printf("wwstat Ver %s by oga.\n",VER);
	printf("usage : wwstat [-v <hostname>] [-hl] [-ul] [log_file]\n");
	printf("        -v  : 指定したホストを統計から除く\n");
	printf("        -hl : HOSTをリンクする\n");
	printf("        -ul : URLをリンクする\n");
	exit(1);
}

main(a,b)
int a;
char *b[];
{
	int	i, c;
	char	*filename = HTTPD_LOG;
	char	*hostp,*urlp;
	char	buf[4096];      /* V1.04-C */
	char	wk[128];
	FILE	*fp;
	struct  name_t	*host;	
	struct  name_t	*url;	

	for (i = 1; i< a ; i++) {
		if (!strcmp(b[i],"-h")) {
			usage();
		}
		if (!strcmp(b[i],"-v")) {
			if (i+1 >= a) {
			    printf("-v require hostname\n");
			    usage();
			}
			vhost = b[++i];
			vf = 1;
			continue;
		}
		if (!strcmp(b[i],"-hl")) {
			hlf = 1;
			continue;
		}
		if (!strcmp(b[i],"-ul")) {
			ulf = 1;
			continue;
		}
		if (!strcmp(b[i],"-vb")) {
			vbf = 1;		/* verbose */
			continue;
		}
		filename = b[i];		/* - : stdin */
	}

	host = (struct name_t *) malloc(sizeof(struct name_t)*MAX_HOST);
	url  = (struct name_t *) malloc(sizeof(struct name_t)*MAX_URL);
	bzero(host,sizeof(struct name_t)*MAX_HOST);
	bzero(url,sizeof(struct name_t)*MAX_URL);

        dprintf("fopen\n");
	if (!strcmp(filename, "-")) {
	    fp = stdin;
	} else {
	    if (!(fp = fopen(filename,"r"))) {
		printf("log_file=%s\n",filename);
		perror("wwstat");
		exit(1);
	    }
	}

        dprintf("readline\n");
	while((c = readline(fp,buf)) != EOF) {
		add_host_url(buf,host,url);
	}

        dprintf("fclose\n");
	if (strcmp(filename, "-")) {
	    fclose(fp);
	}

	/* xsort_byname(host,nh); */
	xsort_byref(host,nh);
	xsort_byref(url,nu);

	printf("<TITLE>Access frequency</TITLE>\n");
	printf("<H1>ホスト別 アクセス頻度</H1>\n");
	printf("<PRE>\n");
#ifdef LINEAR
	printf("---------------+-----+----+----+----+----+----+----+----+----+---\n");
	printf("Hostname/IPaddr| freq|   10   20   30   40   50   60   70   80回\n");
	printf("---------------+-----+----+----+----+----+----+----+----+----+---\n");
#else
	printf("---------------+-----+----+----+---+----+---+----+---+----+---\n");
	printf("Hostname/IPaddr| freq|        10       100     1000     10000 回\n");
	printf("---------------+-----+----+----+---+----+---+----+---+----+---\n");
#endif 
	i=0;
	while(host[i].refc != 0) {
		if (hlf) {
			printf("<a href=%chttp://%s/%c>%-15s</a>|%5d|",'"',host[i].namep,'"',host[i].namep,host[i].refc);
		} else {
			printf("%-15s|%5d|",host[i].namep,host[i].refc);
		}
#ifdef LINEAR
		bzero(wk,sizeof(wk));
		strncpy(wk,"****|****|****|****|****|****|****|****|>>",host[i].refc/2);
		wk[(host[i].refc/2)+1]='\0';
		printf("%s\n",wk);
		printf("---------------+-----+----+----+----+----+----+----+----+----+---\n");
#else
		disp_bar(host[i].refc);
		printf("---------------+-----+----+----+---+----+---+----+---+----+---\n");
#endif
		++i;
	}
	printf("<ADDRESS>Copyright (C) 1995, 2001, M.Ogasawara, All Rights Reserved.<ADDRESS>\n");
	printf("</PRE>\n");
	printf("<HR>\n");
	printf("<HR>\n");
	printf("<H1>ＵＲＬ別 アクセス頻度</H1>\n");
	printf("<PRE>\n");
	i=0;
	while(url[i].refc != 0) {
		if (ulf) {
			printf("%5d  <a href=%c%s%c>%s</a>\n",url[i].refc,
					     34,url[i].namep,34,url[i].namep);
		} else {
			printf("%5d  %s\n",url[i].refc,url[i].namep);
		}
		++i;
	}
	printf("</PRE>\n");

}
int readline(fp,buf)
FILE *fp;
char *buf;
{
	int c;       /* V1.04-C */
	int i = 0;
	int first = 1;

        dprintf("readline start\n");
	c = getc(fp);
	while (c != '\n' && c != EOF) {
		if (vf) {
		    putchar(c);
		    fflush(stdout);
		}
		if (i < 128) {
		    buf[i++] = c;
		}
		c = getc(fp);
	}
	buf[i] = '\0';
#ifdef DEBUG
	printf("LINE = %s\n",buf);
#endif
        dprintf("readline end\n");
	return c;
}

/* 
 *	n個の配列fileにポイントされている文字列を並べ変える。
 *
 *						V1.00 by oga.
 */
xsort_byname(file,n)
struct name_t *file;	
int n;
{
	char *wk;
	int wk2;
	int i,j;

	for (i=1; i<n; i++) {
		for (j=0;j<n-i;j++) {
			if (strcmp(file[j].namep,file[j+1].namep) > 0 ) {
				wk 	        = file[j].namep;
				file[j].namep   = file[j+1].namep;
				file[j+1].namep = wk;
				wk2	        = file[j].refc;
				file[j].refc    = file[j+1].refc;
				file[j+1].refc  = wk2;
			}
		}
	}
}
xsort_byref(file,n)
struct name_t *file;	
int n;
{
	char *wk;
	int wk2;
	int i,j;

	for (i=1; i<n; i++) {
		for (j=0;j<n-i;j++) {
			if (file[j].refc < file[j+1].refc ) {
				wk 	        = file[j].namep;
				file[j].namep   = file[j+1].namep;
				file[j+1].namep = wk;
				wk2	        = file[j].refc;
				file[j].refc    = file[j+1].refc;
				file[j+1].refc  = wk2;
			}
		}
	}
}

int add_host_url(buf,host,url)
char *buf;
struct name_t *host,*url;
{
	char	buf2[4096];

        dprintf("add_host_url start\n");
	if (getword(buf,1,buf2)) {
		printf("host entry not found!\n");
	} else {
		nh += add_ent(host,buf2);
	}

	if (getword(buf,7,buf2)) {
		printf("url entry not found!\n");
	} else {
		nu += add_ent(url,buf2);
	}
        dprintf("add_host_url end\n");
}

/*
 *     
 *	out : 0:新エントリ追加無し  1:新エントリ追加
 */
int add_ent(host,name)
struct name_t *host;
char	*name;
{
	int i=0;
	int hit=0;

        dprintf("add_ent start\n");
	while (host[i].refc != 0) {
		if (!strcmp(host[i].namep,name)) { 
			host[i].refc++;
			return 0;
		}
		i++;
	}

        dprintf("add_ent 2 %s/%s\n", name, vhost);
	/* -v host? */
	if (vf && !strcmp(name,vhost)) return 0;

        dprintf("add_ent 3\n");
	/* new entry */
	host[i].refc++ ;
	host[i].namep=(char *)malloc(strlen(name)+1);
	strcpy(host[i].namep,name);

        dprintf("add_ent end\n");
	return 1;
}

int getword(inbuf,n,outbuf)
char *inbuf,*outbuf;
int  n;
{
	int i = 0, ii = 0;
	int ret = 1;
	int j;

        dprintf("getword start\n");
	for (j=1 ; j<=n; j++) {
	    while (inbuf[i] == ' ' || inbuf[i] == '\t') ++i;
	    if (j != n) {
	        while (inbuf[i] != 0 && inbuf[i] != ' ' && inbuf[i] != '\t') ++i;
	    } else {
	        while (inbuf[i] != 0 && inbuf[i] != ' ' && inbuf[i] != '\t') {
		    outbuf[ii++]=inbuf[i++];
		    outbuf[ii]='\0';
		    ret = 0;
		}
	    }
	    if (!inbuf[i]) break;
	}

        dprintf("getword end\n");
	return ret;
}

disp_bar(val)
int val;
{
	int i, j=0;
	int ff=0;

	while (val > 10) {
		j += 9;
		val /= 10;
	}
	j += val;
	for (i = 1; i <= j; i++)
		if (ff && (i % 9) == 1) {
			printf("|");
		} else {
			ff = 1;
			printf("*");
		}
	printf("\n");
}
