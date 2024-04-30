/*
 *     cpkt : パケットの流量表示 (CUI版)
 *
 *             V1.00  2003/05/10  by oga.
 *             V1.01  2005/03/21  fix MTU=40
 *
 *  /proc/net/route
 *  /proc/net/dev
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>

#define VER	"Ver 1.01"
#define S_IN	"IN"
#define S_OUT	"OUT"

/* #define DEBUG 1 */

#ifdef DEBUG
#define 	dprintf	printf
#else
#define 	dprintf	if (vf) printf
#endif

/* constants */
#define NCOLOR	 8		/* num of color  */
#define RM	10		/* right margine */
#define TM	 0		/* top margine   */
#define LH	14		/* Label height  */
#define OFFS	40		/* string area   */
#define NDAT	20		/* string area   */

typedef struct _if_t {
    char name[32];	/* Interface name   */
    int mtu;		/* MTU size         */
    int active;		/* active flag V1.04*/
    int ipkt;		/* Input packet  (or bytes) */
    int opkt;		/* Output packet (or bytes) */
} if_t;

/* globals */
if_t pktdat[NDAT];
if_t pktold[NDAT];

int	vf = 0;		/* verbose flag */

int get_pktdata();
int get_bar(int);
char *get_item(char *, char *, int, char *);

/*
 *   Fill string buffer
 *
 *   OUT  : buf     string buffer
 *   IN   : chr     fill character
 *   IN   : spoint  start location
 *   IN   : length  fill length
 *       
 */
void CFill(char *buf, char chr, int spoint, int length)
{
    int i;
    /* dprintf("start:%d end:%d\n", spoint, length); */

    for (i = spoint; i<=spoint + length; i++) {
        buf[i] = chr;
    }
}

/* V1.05-A start (not available) */
/*
 *  get mtu for Linux 2.2.x
 *
 *  IN  ifname : interface name
 *  OUT omtu   : mtu
 *
 */
int get_mtu(char *ifname, int *omtu)
{
    struct ifreq ifr;
    static int   sfd = -1;

    strcpy(ifr.ifr_name, ifname);

    if (sfd < 0) {
	/* open one time */
        sfd = socket(PF_INET, SOCK_STREAM, 0);
	if (sfd < 0) {
	    perror("socket");
	}
    }
    if (ioctl(sfd, SIOCGIFMTU, &ifr) < 0) {
	/* failed */
	*omtu = -1;
    } else {
	/* success */
	*omtu = ifr.ifr_mtu;
	dprintf("MTU=%d\n", *omtu);
    }
    /* close(sfd);   no close */
}
/* V1.05-A end */

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

	dprintf("## get_pktdata start\n");

	for (i = 0; i<NDAT; i++) {
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
	    	/* strcat(wk,":");		/* "eth0:" V1.04-D */
	        i = 0;
	        found = 0;
	        while(pkt[i].mtu) {
	            if (!strcmp(pkt[i].name, wk)) {
	                found = 1;
	                pkt[i].active = 1;
	                break;
	            }
	            i++;
	        }
	        if (!found) {
	            /* まだ未登録なら登録 */
	            strcpy(pkt[i].name, wk);
	    	    if (get_item(buf,"\t",9,wk)) pkt[i].mtu = atoi(wk);
		    if (pkt[i].mtu == 0 || pkt[i].mtu == 40) {  /* V1.01-C */
			pkt[i].mtu = -1;
		    }
#if 0
		    if (pkt[i].mtu == 0) {
			dprintf("NAME[%s] MTU[%d]\n", pkt[i].name, pkt[i].mtu);
			get_mtu(pkt[i].name, &pkt[i].mtu);
		    }
		    dprintf("NAME[%s] MTU[%d]\n", pkt[i].name, pkt[i].mtu);
#endif
	            pkt[i].active = 1;
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
		        pt = strchr(buf, ':');
		        if (pt) *pt = ' ';
			if (pkt[i].mtu > 0) {
			    /* Linux -2.0.x (packets) */
	    		    if (get_item(buf," ",2,wk)) pkt[i].ipkt = atoi(wk);
	    		    if (get_item(buf," ",7,wk)) pkt[i].opkt = atoi(wk);
			} else {
			    /* Linux 2.2.x- (bytes)   */
			    /* V1.00-C start */
	    		    if (get_item(buf," ",2,wk)) {
			        to9digit(wk);
			        pkt[i].ipkt = atoi(wk);
			    }
	    		    if (get_item(buf," ",10,wk)) {
			        to9digit(wk);
			        pkt[i].opkt = atoi(wk);
			    }
			    /* V1.00-C end   */
			    dprintf("## ipkt=%u opkt=%u\n", pkt[i].ipkt, pkt[i].opkt);
			}
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
	        printf("IF:[%s] ACTIVE[%d] MTU[%d]  IN[%d]  OUT[%d]\n",
	    		pkt[i].name,
	    		pkt[i].active,
	    		pkt[i].mtu,
	    		pkt[i].ipkt,
	    		pkt[i].opkt);
	    }
	    i++;
	}
	dprintf("## get_pktdata end\n");
	return i;
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
			pt = strtok(wk,sep);
		} else {
			pt = strtok(NULL,sep);
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
 *  対数っぽい長さを求める 
 *
 *  val      ret
 *  1    ... 1
 *  10   ... 1
 *  100  ... 1
 *  1K   ... 1
 *  10K  ... 10
 *  100K ... 20
 *  1M   ... 30
 *  10M  ... 40
 *  100M ... 50
 *
 */
int get_bar(int val)
{
	int j=0;
	int valsv = val;

	if (valsv == 0) {
	    return 0;
	}

	dprintf("get_bar : val = %d  ",val);

	while (val > 10) {
		j += 10;
		val /= 10;
	}
	j += val;

	j -= 30;	/* 10KBを10とする */

	if (j <= 0) {
	    j = 1;
	}
	dprintf("ret = %d\n",j);

	return j;
}

void disp_bar(int len, int value)
{
    char buf[256];
    char buf2[64];
    int  i;

    memset(buf, 0, sizeof(buf));
    CFill(buf, ' ', 0, 70);
    CFill(buf, '#', 0, len);
    for (i = 0; i<8; i++) {
        buf[i*10] = '|';
    }
    sprintf(buf2, "%d", value);
    strncpy(&buf[1], buf2, strlen(buf2));
    printf("%s\n", buf);
}


int main(int a, char *b[])
{
	int i, j;
	int lf = 0;             /* line feed counter */
	int wait = 1;		/* 5sec */
	char	name[128],wk[128];
	char	value[1024];    /* V1.03        */
	char	*pt;
	int	nonumf = 0;	/* V1.03 -nonum */
	int     ifnum;		/* num of network interface */
	int  ibyte,obyte,ilen,olen;

	char	*unit[8] = {
		"10K",	/* 0 */
		"100K",	/* 1 */
		"1M",	/* 2 */
		"10M",	/* 3 */
		"100M",	/* 4 */
		"1G",	/* 5 */
		"10G",	/* 6 */
		"100G"	/* 7 */
	};

	for (i = 1; i<a ; i++) {
		if (!strncmp(b[i],"-h",2)) {
			printf("usage : cpkt [-update <time(1)>]\n");
			printf("             [-nonum]\n");
			exit(1);
		}
		if (!strncmp(b[i],"-u",2) && a > i) {
			wait = atoi(b[++i]);		/* update time */
			continue;
		}
		if (!strncmp(b[i],"-v",2)) {
			vf = 1;				/* verbose */
			continue;
		}
		if (!strcmp(b[i],"-nonum")) {
			nonumf = 1;			/* nonum V1.03 */
			continue;
		}
	}

	memset(pktdat, 0, sizeof(pktdat));
	memset(pktold, 0, sizeof(pktold));

        ifnum = get_pktdata(pktdat);

	while (1) {
	    memcpy(pktold, pktdat, sizeof(pktold));	/* pktold <= pktdat */
	    ifnum = get_pktdata(pktdat);
	    dprintf("ifnum=%d, name=[%s]\n", ifnum,  pktdat[0].name);

            /* cursor up */
	    if (!vf) {
	        for (i = 0; i<lf; i++) {
	            printf("%cM", 27);
	        }
	    }
	    lf = 0;

            /* disp scale1    */
	    printf("   (B/s)1K ");
	    for (j = 0; j<7; j++) {
		if (j == 6) {
		    /* max 79char */
		    printf("     %-4s", unit[j]);
		} else {
		    printf("      %-4s", unit[j]);
		}
	    }
	    printf("\n");
	    ++lf;

	    for (i = 0; i<ifnum; i++) {
		if (pktdat[i].mtu > 0) {
                    /* Linux -2.0.x (packets) */
	            ibyte = pktdat[i].mtu * (pktdat[i].ipkt - pktold[i].ipkt);
		    obyte = pktdat[i].mtu * (pktdat[i].opkt - pktold[i].opkt);
		} else {                /* V1.05-A */
                    /* Linux 2.2.x- (bytes)   V1.05-A */
	            ibyte = (pktdat[i].ipkt - pktold[i].ipkt);
		    obyte = (pktdat[i].opkt - pktold[i].opkt);
		}                       /* V1.05-A */
		ilen = get_bar(ibyte);
		olen = get_bar(obyte);
		dprintf("###[%s] ibyte=%d ilen=%d obyte=%d olen=%d\n", 
		        pktdat[i].name, ibyte,ilen,obyte,olen);

                printf("-------------------------------------------------------------------------------\n");
	        ++lf;

		/* interface name */
		/* receive packet bar */
		printf("%4s Rcv", pktdat[i].name);
		disp_bar(ilen, ibyte/1024);
	        ++lf;

		/* transmit packet bar */
		printf("     Snd");
		disp_bar(olen, obyte/1024);
	        ++lf;
	    }
            printf("-------------------------------------------------------------------------------\n");
	    ++lf;

	    sleep(wait);
	}
	return 0;
}
