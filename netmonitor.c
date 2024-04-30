/*
 *  netmonitor : monitoring network packet and logging
 *
 *  2001/01/30 V1.00 by oga.
 *  2001/02/05 V1.01 fix sum bug
 *  2001/06/27 V1.02 support Linux 2.2.x
 *  2003/05/12 V1.03 fix ibyte, obyte overflow
 *  2003/06/17 V1.04 fix obyte always 0
 *  2005/10/02 V1.05 atoi => strtoul
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

enum pkt_id {IPKT,      /* 0: Input packets    */
             IERRS,     /* 1: Input errors     */
             IDROP,     /* 2: Input dorp       */
             IFIFO,     /* 3: Input fifo       */
             IFRAME,    /* 4: Input frame      */
             OPKT,      /* 5: Output packets   */
             OERRS,     /* 6: Output errors    */
             ODROP,     /* 7: Output drop      */
             OFIFO,     /* 8: Output fifo      */
             OCOLLS,    /* 9: Output collision */
             OCARRIER,  /*10: Output carrier   */
             NUMENT};   /*11: Num of data      */

typedef struct _if_t {
    char name[32];	          /* Interface name   */
    int active;		          /* active flag      */
    int mtu;		          /* MTU size         */
    unsigned int pktdata[NUMENT]; /* packet data      */
} if_t;

#define NDAT 20		/* max num of interfaces */
#define dprintf	if (vf) printf

/* globals */
if_t pktdat[NDAT];
if_t pktold[NDAT];

int  vf = 0;

void get_datetime(char *ymd, char *hms)
{
    time_t tt;

    tt = time(0);
    strftime(ymd, 16, "%Y/%m/%d", localtime(&tt));
    strftime(hms, 10, "%H:%M:%S", localtime(&tt));
}

/*
 *  char *get_item(buf,sep,pos,outbuf)
 *
 *     buf内のsepで区切られたpos番目の項目をoutbufにコピーして返します。
 *     項目中の前後のスペース文字は削除されます
 *     bufは1024文字までです
 *          sepに","を指定すると、",,"は一つの区切り文字として認識されます
 *     
 *     IN   buf    : 切り出し元の文字列(項目がsepで区切られている)
 *          pos    : 1以上の値 (1が最初の項目)
 *          sep    : 項目の区切り文字です " ", ","など
 *     OUT  outbuf : 指定項目の文字列(項目中の後のスペース/改行は削除)
 *          ret    : 成功時  outbuf
 *                   失敗時  (char *)0
 */
char *get_item(char *buf, char *sep, int pos, char *outbuf)
{
 int i;
 char *pt = NULL;
 char *p;
 char wk[1024];

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
		printf("Out of item(%s) pos(%d).",buf,pos);
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
 *   10桁以上の場合下9桁のみに変更する
 *
 */
void to9digit(char *strnum)
{
    char buf[64];

    if (strlen(strnum) < 10) return;
    strcpy(buf, &strnum[strlen(strnum)-9]);
    strcpy(strnum, buf);
}

/* 
 *  get packet information from /proc/net/route,dev  for Linux
 * 
 *    IN  if_t *pkt : if_t *pkt[n]
 *    OUT      ret  : 見つかったインタフェーステーブル数を格納する。
 */
int get_pktdata(if_t pkt[])
{
	FILE *fp;
	char buf[1024],wk[1024];
	char *pt;
	int  i,j,found;

	dprintf("----  get_pktdata start  ----\n");

	/* memset(pkt, 0, sizeof(if_t)*NDAT); */
	for (i = 0; i<NDAT; i++) {  /* V1.01 */
	    pkt[i].active = 0;
	}

	/* get MTU,Ifname */
	if (!(fp = fopen("/proc/net/route","r"))) {
		perror("fopen /proc/net/route");
		exit(1);
	}
	fgets(buf,sizeof(buf),fp);	/* skip header   */

	/* get Interface MTU */
	while (fgets(buf,sizeof(buf),fp)) {
	    if (get_item(buf,"\t",1,wk)) {
	    	/* strcat(wk,":");		/* "eth0:" */
	        i = 0;
	        found = 0;
	        while(pkt[i].mtu) {
	            if (!strcmp(pkt[i].name, wk)) {
	                found = 1;
	                pkt[i].active = 1;      /* V1.01 */
	                break;
	            }
	            i++;
	        }
	        if (!found) {
	            /* まだ未登録なら登録 */
	            strcpy(pkt[i].name, wk);
	    	    if (get_item(buf,"\t",9,wk)) pkt[i].mtu = atoi(wk);

		    /* V1.02-A start */
		    if (pkt[i].mtu == 0 || pkt[i].mtu == 40) { /* V1.04-C */
			pkt[i].mtu = -1;
		    }
#if 0
		    if (pkt[i].mtu == 0) {
			dprintf("NAME[%s] MTU[%d]\n", pkt[i].name, pkt[i].mtu);
			get_mtu(pkt[i].name, &pkt[i].mtu);
		    }
		    dprintf("NAME[%s] MTU[%d]\n", pkt[i].name, pkt[i].mtu);
#endif
		    /* V1.02-A end   */

	    	    pkt[i].active = 1;          /* V1.01 */
		}
	    }
	}
	fclose(fp);

	/* get Packet data */
	if (!(fp = fopen("/proc/net/dev","r"))) {
		perror("fopen /proc/net/dev");
		exit(1);
	}
	fgets(buf,sizeof(buf),fp);	/* skip header1  */
	fgets(buf,sizeof(buf),fp);	/* skip header2  */

	while (fgets(buf,sizeof(buf),fp)) {
	    if (get_item(buf," ",1,wk)) {
	        i = 0;
	        while (pkt[i].mtu && i < NDAT) {
	            /* /proc/net/routeにあるifのみ設定 */
		    if (strstr(wk,pkt[i].name)) {
		        pt = (char *)strchr(buf, ':');
		        if (pt) *pt = ' ';
			if (pkt[i].mtu > 0) {      /* V1.02-A */
			  /*  Linux -2.0.x (IPKT,OPKT:packets) */
		          for (j = 0; j<NUMENT; j++) {
	    		    if (get_item(buf," ",j+2,wk)) {
	    		        pkt[i].pktdata[j] = strtoul(wk,(char **)NULL,0);
	    		    }
	    		  }
			} else {                  /* V1.02-A */
			  /*  Linux 2.2.x- (IPKT,OPKT:bytes)   V1.02-A */
	    		  if (get_item(buf," ",2,wk)) pkt[i].pktdata[IPKT] = strtoul(wk,(char **)NULL,0);
	    		  //if (get_item(buf," ",2,wk)) {
			  //    to9digit(wk);
			  //    pkt[i].pktdata[IPKT] = strtoul(wk,(char **)NULL,0);
			  //}
	    		  if (get_item(buf," ",4,wk)) pkt[i].pktdata[IERRS] = strtoul(wk,(char **)NULL,0);
	    		  if (get_item(buf," ",5,wk)) pkt[i].pktdata[IDROP] = strtoul(wk,(char **)NULL,0);
	    		  if (get_item(buf," ",6,wk)) pkt[i].pktdata[IFIFO] = strtoul(wk,(char **)NULL,0);
	    		  if (get_item(buf," ",7,wk)) pkt[i].pktdata[IFRAME] = strtoul(wk,(char **)NULL,0);

	    		  if (get_item(buf," ",10,wk)) pkt[i].pktdata[OPKT] = strtoul(wk,(char **)NULL,0);
	    		  //if (get_item(buf," ",10,wk)) {
			  //    to9digit(wk);
			  //    pkt[i].pktdata[OPKT] = strtoul(wk,(char **)NULL,0);
			  //}
	    		  if (get_item(buf," ",12,wk)) pkt[i].pktdata[OERRS] = strtoul(wk,(char **)NULL,0);
	    		  if (get_item(buf," ",13,wk)) pkt[i].pktdata[ODROP] = strtoul(wk,(char **)NULL,0);
	    		  if (get_item(buf," ",14,wk)) pkt[i].pktdata[OFIFO] = strtoul(wk,(char **)NULL,0);
	    		  if (get_item(buf," ",15,wk)) pkt[i].pktdata[OCOLLS] = strtoul(wk,(char **)NULL,0);
	    		  if (get_item(buf," ",16,wk)) pkt[i].pktdata[OCARRIER] = strtoul(wk,(char **)NULL,0);
			}                         /* V1.02-A */
	    		break;
		    }
		    i++;
	        }
	    }
	}
	fclose(fp);

        /* count num of interfaces */
        i = 0;
	while (pkt[i].mtu && i < NDAT) {
	    if (vf) {
	        printf("IF:[%4s] active[%d]  MTU[%d]  IN[%u]  OUT[%u]\n",
	    		pkt[i].name,
	    		pkt[i].active,          /* V1.01 */
	    		pkt[i].mtu,
	    		pkt[i].pktdata[IPKT],
	    		pkt[i].pktdata[OPKT]);
	    }
	    i++;
	}
	dprintf("---- get_pktdata end  ----\n");
	return i;
}

/* wait next 00 minutes */
void wait_next_00min()
{
    char ymd[32];
    char hms[32];
    int  i = 0;

    while (1) {
        get_datetime(ymd, hms);
        if (!strncmp(&hms[3], "00", 2)) {
            /* if HH:00:SS then break */
            break;
        }
        /* for temporary interface (ig. ppp0) */
        if ((++i % 18) == 0) {        /* V1.01-A */
            /* if check every 3 min */
            get_pktdata(pktdat);      /* V1.01-A */
        }                             /* V1.01-A */
        sleep(10);
    }
}

void usage()
{
    printf("usage: netmonitor [-up <interval_sec>] [-if <if_name>] [-csv] [<outfile.csv>]\n");
    exit(1);
}

int main(int a, char *b[])
{
    FILE *fp     = NULL;
    char *fname  = NULL;
    char ymd[32];
    char hms[32];

    int  i, j;
    int  ifnum;
    int  af   = 0;          /* -a   output lo: interface */
    int  csvf = 0;          /* -csv output csv format    */
    int  interval = 60;     /* -up  interval sec         */
    char *ifname = NULL;    /* -if  interface name       */
    int  existfile = 0;

    struct stat stbuf;

    /* arg check */
    for (i = 1; i<a; i++) {
	if (!strcmp(b[i],"-a")) {
	    af = 1;			/* display lo: */
	    continue;
	}
	if (!strcmp(b[i],"-up")) {
	    if (i+1 >= a) {
	        usage();
	    }
	    interval = atoi(b[++i]);
	    if (interval <= 0) {
	        printf("Warning: invalid interval. 60 sec assumed.\n");
	        interval = 60;
	    }
	    continue;
	}
	if (!strcmp(b[i],"-if")) {
	    if (i+1 >= a) {
	        usage();
	    }
	    ifname = b[++i];
	    continue;
	}
	if (!strcmp(b[i],"-csv")) {
	    csvf = 1;
	    continue;
	}
	if (!strcmp(b[i],"-v")) {
	    vf = 1;
	    continue;
	}
	if (!strncmp(b[i],"-h",2)) {
	    usage();
	}
	fname = b[i];
    }

    memset(pktdat, 0, sizeof(pktdat));
    memset(pktold, 0, sizeof(pktold));

    /* open file */
    if (fname == NULL) {
	fp = stdout;
    } else {
        if (!stat(fname, &stbuf)) {
            if (stbuf.st_size != 0) {
                existfile = 1;
            }
        }
	if ((fp = fopen(fname,"a")) == 0) {
	    perror(b[0]);
	    exit(1);
	}
    }

    get_pktdata(pktold);

    if (fp == stdout && !csvf) {
        fprintf(fp,"    Time IfName  MTU       Ipkt Ierrs Idrop       Opkt Oerrs Odrop  Ocolls\n");
    } else {
        if (!existfile) {
            fprintf(fp,"Date,Time,IfName,MTU,Ipkt,In(KB),Ierrs,Idrop,Opkt,Out(KB),Oerrs,Odrop,Ocolls\n");
            fflush(fp);
        }
    }

    while(1) {
        ifnum = get_pktdata(pktdat);
        for (i = 0; i<ifnum; i++) {
            if (pktdat[i].pktdata[IPKT] < pktold[i].pktdata[IPKT]) { /*V1.01-A*/
                /* interface restart or wraparound */
		for (j = 0; j<NUMENT; j++) {                         /*V1.01-A*/
                    pktold[i].pktdata[j] = 0;                        /*V1.01-A*/
		}                                                    /*V1.01-A*/
            }                                                        /*V1.01-A*/
            if (!af && !strcmp(pktdat[i].name, "lo")) {
                /* if not -a, skip "lo" interface */
                continue;
            }
            if (ifname) {
                /* -if option specified */
                if (!strstr(pktdat[i].name, ifname)) {
                    /* not target interface */
                    continue;
                }
            }
            get_datetime(ymd, hms);
            if (fp == stdout && !csvf) {
                fprintf(fp, "%8s %6s %4d %10u %5u %5u %10u %5u %5u %7u\n", 
                     hms,
                     pktdat[i].name,
                     pktdat[i].mtu,
                     pktdat[i].pktdata[IPKT]  -pktold[i].pktdata[IPKT],
                     pktdat[i].pktdata[IERRS] -pktold[i].pktdata[IERRS],
                     pktdat[i].pktdata[IDROP] -pktold[i].pktdata[IDROP],
                     pktdat[i].pktdata[OPKT]  -pktold[i].pktdata[OPKT],
                     pktdat[i].pktdata[OERRS] -pktold[i].pktdata[OERRS],
                     pktdat[i].pktdata[ODROP] -pktold[i].pktdata[ODROP],
                     pktdat[i].pktdata[OCOLLS]-pktold[i].pktdata[OCOLLS]);
            } else {
                /* spcify file or -csv specify */
	        if (!existfile) {
		  /* V1.02-A start */
		  int ikb, okb;
		  if (pktdat[i].mtu > 0) { /* Linux -2.0.x (packets) */
                    ikb = (pktdat[i].pktdata[IPKT]  -pktold[i].pktdata[IPKT])*pktdat[i].mtu/1024;
                    okb = (pktdat[i].pktdata[OPKT]  -pktold[i].pktdata[OPKT])*pktdat[i].mtu/1024;
		  } else { /* Linux 2.2.x- (pktdata:bytes) */
                    ikb = (pktdat[i].pktdata[IPKT] - pktold[i].pktdata[IPKT])/1024;
                    okb = (pktdat[i].pktdata[OPKT]  -pktold[i].pktdata[OPKT])/1024;
		  }
		  /* V1.02-A end */

		  /*Dt,Tm,Ifnm,MTU,Ipkt,I(KB),Ier,Idr,Opkt,O(KB),Oer,Odr,Ocol*/
                  fprintf(fp, "%s,%s,%s,%d,%u,%u,%u,%u,%u,%u,%u,%u,%u\n", 
                     ymd, hms,
                     pktdat[i].name,
                     pktdat[i].mtu,
                     pktdat[i].pktdata[IPKT]  -pktold[i].pktdata[IPKT],
		     ikb,  /* V1.02-C */
                     pktdat[i].pktdata[IERRS] -pktold[i].pktdata[IERRS],
                     pktdat[i].pktdata[IDROP] -pktold[i].pktdata[IDROP],
                     pktdat[i].pktdata[OPKT]  -pktold[i].pktdata[OPKT],
		     okb,  /* V1.02-C */
                     pktdat[i].pktdata[OERRS] -pktold[i].pktdata[OERRS],
                     pktdat[i].pktdata[ODROP] -pktold[i].pktdata[ODROP],
                     pktdat[i].pktdata[OCOLLS]-pktold[i].pktdata[OCOLLS]);
	          fflush(fp);
	        }
	        existfile = 0;
            }
        }
        memcpy(pktold, pktdat, sizeof(pktold));
        if (interval == 3600) {
            sleep(61);
            wait_next_00min();
        } else {
            sleep(interval);
        }
    }

    if (fp != stdout) fclose(fp);
}

