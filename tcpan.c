/*
 *  tcpan
 *    analyze wireshark packet log      
 *       
 *  Input File:   
 *    [File]=>[Export]=>[File]
 *       Export Option
 *       [v]Packet summary line
 *       [v]Packet details: [As displayed]
 *       [v]Packet Bytes
 *       [ ]Each packet on a new page
 *
 *  10/02/06 V0.10 by oga.
 *  14/10/18 V0.12 support -n, -raw option
 *  14/10/18 V0.13 support time-only format
 *  14/10/23 V0.14 fix last data print, unprintable char proc
 *  22/04/19 V0.15 support latest export format
 *  22/04/20 V0.16 support -ip -csv
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>   /* V0.16-A */
#ifdef _WIN32
#include <windows.h>
#include <sys/utime.h>
#endif  /* _WIN32 */

#define VER "0.16"

#define	dprintf   if (vf) printf
#define	dprintf2  if (vf >= 2) printf
#define	dprintf3  if (vf >= 3) printf

#define MAX_NTCPDATA 200000
#define MAX_NPORTID  50000
#define MAX_IPLIST   2     /* V0.16-A */

#define KEY_TCP     "Transmission"
#define KEY_PKTDATA "0030"

typedef struct _tcpdata {
    int   frmno;
    char  date[11];        /* yyyy-mm-dd      */
    char  time[16];        /* hh:mm:ss.nnnnnn */
    char  srcip[16];       /* xxx.xxx.xxx.xxx */
    char  dstip[16];       /* xxx.xxx.xxx.xxx */
    char  proto[16];       /* protocol        */
    char  len[16];         /* length          V0.15-A */
    char  info[100];
    char  portid[12];      /* <port1>-<port2> , port1 < port2 */
    int   srcport;
    int   dstport;
    int   seq;
    int   ack;
    char  *pktdata;        /* packet bytes    */
} tcpdata_t;

int vf      = 0;           /* -v verbose mode */
int nnf     = 0;           /* -n count line   */
int rawf    = 0;           /* -raw               V0.11-A */
int portf   = 0;           /* -port              V0.11-A */
int cnt     = 0;           /* frm data count  */
int nportid = 0;           /* num of portid   */
int contf   = 0;           /* . continue flag    V0.12-A V0.14-M */
int oldf    = 0;           /* -oldf old format(no Length) V0.15-A */
int ipcnt   = 0;           /* -ip   ip addr count         V0.16-A */
int csvf    = 0;           /* -csv  output csv format     V0.16-A */
int max_ntcpdata = MAX_NTCPDATA; /* default num of data V0.11-A */
tcpdata_t *tcpdatas = NULL;
char      **portids = NULL;
char      *start_time = "00:00:00.000000";
char      *end_time   = "99:99:99.999999";
char iplist[MAX_IPLIST][32]; /* -ip target IP list      V0.16-A */

/*
 *   不定長のデータをコピーし、次のデータ項目に位置づける  
 *
 */
void CopyWord(char *obuf, int obufsiz, char **ibufp, char delm)
{
	int len = 0;
    while(**ibufp != delm) {
		*obuf = **ibufp;
		++obuf;
		++len;
		++(*ibufp);
		if (len >= obufsiz - 1) {
			*obuf = '\0';
			break;
		}
	}
	while(**ibufp == delm) ++(*ibufp);    /* skip space     */
	*obuf = '\0';
}

/* format1(Date,Time) */
/*      10 2014-10-18 06:05:07.804772 192.168.10.17         65.55.223.17          TCP      57694 > 40008 [PSH, ACK] Seq=1 Ack=1 Win=16343 Len=40 */
/* format2(Time only) */
/*      10 5.567240    192.168.10.17         65.55.223.17          TCP      57694 > 40008 [PSH, ACK] Seq=1 Ack=1 Win=16343 Len=40 */

/* get TCP packet summary data */
void GetSummary(char *buf, tcpdata_t *tcpdatp)
{
	char *pt = buf;
	int  pos;

	while(*pt == ' ') ++pt;                /* skip space     */
	tcpdatp->frmno = atoi(pt);             /* pkt frame num  */
	while(*pt != ' ') ++pt;                /* skip not space */
	while(*pt == ' ') ++pt;                /* skip space     */
	if (pt[4] == '-') {
		/* [YYYY-MM-DD hh:mm:ss.mmmmmm] format */
		strncpy(tcpdatp->date, pt, 10);    /* copy date      */
		while(*pt != ' ') ++pt;            /* skip not space */
		while(*pt == ' ') ++pt;            /* skip space     */
		strncpy(tcpdatp->time, pt, 15);    /* copy time      */
	} else {
		/* [s.mmmmmm] format */
		strcpy(tcpdatp->date, "");         /* no date        */
		pos = 0;
		while(pt[pos] != ' ') {
			tcpdatp->time[pos] = pt[pos];  /* copy time      */
			++pos;
		}
		tcpdatp->time[pos] = '\0';
	}
	while(*pt != ' ') ++pt;                /* skip not space */
	while(*pt == ' ') ++pt;                /* skip space     */
	CopyWord(tcpdatp->srcip, sizeof(tcpdatp->srcip), &pt, ' '); /* copy source ip */
	CopyWord(tcpdatp->dstip, sizeof(tcpdatp->dstip), &pt, ' '); /* copy dest   ip */
	CopyWord(tcpdatp->proto, sizeof(tcpdatp->proto), &pt, ' '); /* copy protocol  */
	/* V0.15-A start */
	if (!oldf) {
		CopyWord(tcpdatp->len, sizeof(tcpdatp->len), &pt, ' '); /* copy length    */
	}
	/* V0.15-A end   */
	CopyWord(tcpdatp->info,  sizeof(tcpdatp->info), &pt, '\n'); /* copy info      */
	/* V0.11-A start */
	if (tcpdatp->info[strlen(tcpdatp->info)-1] == 0x0d) {
		tcpdatp->info[strlen(tcpdatp->info)-1] = '\0';
	}
	/* V0.11-A end   */
}

/*
 *  Get Port Number and Seq/Ack Number
 *
 */
void GetPortNo(char *buf, tcpdata_t *tcpdatp)
{
	char *pt = buf;

	dprintf3("### Start GetPortNo\n");

	if (oldf) {    /* V0.15-A */
		/* search source port */
		pt = strchr(pt, '(');
		if (pt) {
			tcpdatp->srcport = atoi(++pt);
		}
		/* search dest port */
		pt = strchr(pt, '(');
		if (pt) {
			tcpdatp->dstport = atoi(++pt);
		}
	} else {       /* V0.15-A start */
		/* search source port */
		pt = strstr(pt, "Src Port:");
		if (pt) {
			pt += 10;
			tcpdatp->srcport = atoi(pt);
		}
		/* search dest port */
		pt = strstr(pt, "Dst Port:");
		if (pt) {
			pt += 10;
			tcpdatp->dstport = atoi(pt);
		}
	}              /* V0.15-A end   */

	/* search Seq number */
	pt = strstr(pt, "Seq:");
	if (pt) {
		pt += 5;
		tcpdatp->seq = atoi(pt);
	}
	/* search Ack number */
	pt = strstr(pt, "Ack:");
	if (pt) {
		pt += 5;
		tcpdatp->ack = atoi(pt);
	}

	dprintf3("### end   GetPortNo\n");
}

/*
 *   hex文字列をascii文字列に変換
 *
 *   "48656c6c6f" => "Hello"
 *   "48 65 6c 6c 6f" => "Hello"  間のスペースも許す
 *
 *   ret : length
 */
int hex2str(char *in, char *out)
{
    int  i = 0;
    int  j = 0;
    char wk[16];
    char cc;
	unsigned char *out2 = (unsigned char *)out;

    while (isspace(in[i])) i++;

    while (isdigit(in[i]) || (in[i] >= 'a' && in[i] <= 'f')) {
        strcpy(wk, "0x");
        memcpy(&wk[2], &in[i], 2);
        wk[4] = '\0';
        out[j] = (char)strtoul(wk, (char **)NULL, 0);
        i += 2;
        while (isspace(in[i])) i++;

		/* rawf:0 ascii以外を変換 rawf:1 無変換  V0.11-A */
		/* V0.12-C start */
		if (rawf) {
			/* 0x20未満の場合は無視する(但し改行は有効) */
			//if (out2[j] >= 0x20 || out2[j] == 0x0a || out2[j] == 0x0d) 
			/* 0x20未満の場合は無視する(改行も非表示) */
			if (out2[j] >= 0x20)
			{
        		++j;
			}
		} else {
			if (out2[j] < 0x20 || out2[j] > 0x7e) {
				if (contf) {
					/* ignore this char */
				} else {
					out2[j] = '.';
        			++j; /* avail this char */
					contf = 1;
				}
			} else {
				contf = 0;
        		++j;
			}
		}
		/* V0.12-C end   */

    }
    out[j] = '\0';
    return j;       // 変換結果がバイナリの場合のために長さを返す
}

/* V0.11-A start */
/* 
 *   countn num of packet data
 */
int CountData(char *fname)
{
	FILE *fp = NULL;
	int  cnt = 0;
	int  len = strlen(KEY_TCP);
	char buf[4096];

	if ((fp = fopen(fname, "r")) == NULL) {
	    perror(fname);
		exit(1);
	}

    while(fgets(buf, sizeof(buf), fp)) {
		if (!strncmp(buf, "No.", 3))
		{
			++cnt;
		}
	}

	if (fp) fclose(fp);

	printf("num of data: %d\n", cnt);

	return cnt;
			
}
/* V0.11-A end   */

void GetPktData(char *ibuf, tcpdata_t *tcpdatp, FILE *fp)
{
	char wkstr[4096];
	char buf[4096];
	char ostr[32*1024];

	dprintf2("### start GetPktData\n");
	strcpy(wkstr, "");
	strcpy(ostr, "");
	strcpy(buf, ibuf);
	contf = 0;                          /* V0.14-A */
	do {
		if (strlen(buf) >= 8 && buf[4] == ' ') { /* V0.15-C */
			/* 0000  00 01 02 03... */
			hex2str(&buf[6], wkstr);
			if (vf) printf("buf:[%s]=>wkstr:[%s]\n", buf, wkstr);
			strcat(ostr, wkstr);
			//if (strlen(ostr) > 512) break;
		} else {
			break;
		}
	} while(fgets(buf, sizeof(buf), fp));
	tcpdatp->pktdata = strdup(ostr);
	dprintf2("### end GetPktData\n");
}

void AddPortId(char *portid)
{
	int i;
	int found = 0;

	/* 後ろから検索し、なければ追加(後ろに同じものがある確立が高いため) */
	for (i = nportid-1; i>=0; i--) {
		if (!strcmp(portid, portids[i])) {
			found = 1;
			break;  /* 存在する */
		}
	}
	if (!found) {
		if (nportid < MAX_NPORTID) {
			//strcpy(portids[nportid], portid);
			portids[nportid] = strdup(portid);
			++nportid;
		} else {
			printf("Error: num of portid(%d) overflow\n", nportid);
		}
	}
}

void PrintTcpDat(tcpdata_t *tcpdatp)
{
	//if (tcpdatp->frmno == 7809) return;
	if (vf > 2) {
		printf("@@@@@ start\n"); fflush(stdout);
		printf("@@@@@ [%s]\n", tcpdatp->pktdata); fflush(stdout);
		printf("@@@@@ end\n"); fflush(stdout);
	}
	/* V0.16-A start */
	if (ipcnt) {
		if (ipcnt == 1) {
			/* allow specified ip */
			if (strcmp(tcpdatp->srcip, iplist[0]) && strcmp(tcpdatp->dstip, iplist[0])) {
				if (vf) fflush(stdout);
				return;
			}
		} else if (ipcnt == 2) {
			if (!strcmp(tcpdatp->srcip, iplist[0]) && !strcmp(tcpdatp->dstip, iplist[1]) ||
			    !strcmp(tcpdatp->srcip, iplist[1]) && !strcmp(tcpdatp->dstip, iplist[0])) {
				/* allow specified ip pair */
			} else {
				if (vf )fflush(stdout);
				return;
			}
		}
	}
	/* V0.16-A end   */

	/* No date time portid src(port) dst(port) protocol Seq Ack info pktdat */
	if (csvf) {                                   /* V0.16-A start */
		printf("%7d,%s,%s,%11s,%15s(%5d),%15s(%5d),Seq:%5u,Ack:%5u,%8s,%s",
		   tcpdatp->frmno,
		   tcpdatp->date,
		   tcpdatp->time,
		   tcpdatp->portid,
		   tcpdatp->srcip,
		   tcpdatp->srcport,
		   tcpdatp->dstip,
		   tcpdatp->dstport,
		   tcpdatp->seq,             /* option */
		   tcpdatp->ack,             /* option */
		   tcpdatp->proto,
		   tcpdatp->info);
	} else {                                      /* V0.16-A end */
		printf("%7d %s %s %11s %15s(%5d) %15s(%5d) Seq:%5u Ack:%5u %8s %s",
		   tcpdatp->frmno,
		   tcpdatp->date,
		   tcpdatp->time,
		   tcpdatp->portid,
		   tcpdatp->srcip,
		   tcpdatp->srcport,
		   tcpdatp->dstip,
		   tcpdatp->dstport,
		   tcpdatp->seq,             /* option */
		   tcpdatp->ack,             /* option */
		   tcpdatp->proto,
		   tcpdatp->info);
	}                                             /* V0.16-A */

	if (tcpdatp->pktdata) {
		if (csvf) {                               /* V0.16-A */
			printf(",[%s]\n", tcpdatp->pktdata);  /* V0.16-A */
		} else {                                  /* V0.16-A */
			printf(" [%s]\n", tcpdatp->pktdata);
		}                                         /* V0.16-A */
	} else {
		printf("\n");
	}
	if (vf) fflush(stdout);
}

void ReadTcpFile(char *fname)
{
	FILE *fp = NULL;
	char buf[4096];
	int  newdata;
	tcpdata_t tcpdat;
	
	memset(&tcpdat, 0, sizeof(tcpdat));

	if ((fp = fopen(fname, "r")) == NULL) {
	    perror(fname);
		exit(1);
	}

	newdata = 0;
    while(fgets(buf, sizeof(buf), fp)) {
		if (!strncmp(buf, "No.", 3)) {
			dprintf3("### read packet: %s", buf);   /* V0.15-A */
			newdata = 1;
			if (tcpdat.frmno > 0) {      /* 1回目は出力されないようにする */
				/* 前回データの登録 (for -port) */
				memcpy(&tcpdatas[cnt++], &tcpdat, sizeof(tcpdat));
				/* 前回データの表示 */
				if (!portf) PrintTcpDat(&tcpdat);
				memset(&tcpdat, 0, sizeof(tcpdat));
				if (cnt >= max_ntcpdata - 2) {
					printf("Error: too many tcpdata %d\n", cnt);
					break;
				}
			}
			continue;
		}
		if (newdata) {
			GetSummary(buf, &tcpdat);
			newdata = 0;
			continue;
		}
		if (!strncmp(buf, KEY_TCP, strlen(KEY_TCP))) {
			GetPortNo(buf, &tcpdat);
			if (tcpdat.srcport < tcpdat.dstport) {
				sprintf(tcpdat.portid, "%d-%d", tcpdat.srcport, tcpdat.dstport);
			} else {
				sprintf(tcpdat.portid, "%d-%d", tcpdat.dstport, tcpdat.srcport);
			}
			AddPortId(tcpdat.portid);
		}
		if (!strncmp(buf, KEY_PKTDATA, strlen(KEY_PKTDATA)) && tcpdat.pktdata == NULL) {
			GetPktData(buf, &tcpdat, fp);
		}
	}
	if (!portf) PrintTcpDat(&tcpdat);                   /* 最終データの表示 V0.14 */
	memcpy(&tcpdatas[cnt++], &tcpdat, sizeof(tcpdat));  /* for -port */

	if (fp) fclose(fp);
}

void DispByEachPortid()
{
	int i, j;
	printf("### Num of Portid: %d\n", nportid);
	for (i = 0; i<nportid; i++) {
		printf("{### Portid[%d] %s\n", i, portids[i]);
		for (j = 0; j<cnt; j++) {
			if (!strcmp(portids[i], tcpdatas[j].portid) &&
			    strcmp(tcpdatas[j].time, start_time) >= 0 &&
			    strcmp(tcpdatas[j].time, end_time)   <= 0 ) {
				PrintTcpDat(&tcpdatas[j]);
			}
		}
	}
}

void usage()
{
    printf("tcpan ver. %s\n", VER);
    printf("usage: tcpan [-port] [-raw] [-n] [-oldf] [-csv]\n");
	printf("             [-s <start_time>] [-e <end_time>]\n");
	printf("             [-ip <ip_addr1> [-ip <ip_addr2>]]\n");
	printf("             <wireshark_exp_file.txt>\n");
    printf("  -port: disp each communication port.\n");
    printf("  -raw : output raw packet data.\n");
    printf("  -ip  : filter by ip addr.\n");                   /* V0.16-A */
    printf("  -n   : allocates memory by num of data.\n");
    printf("  -s   : start time. (%s)\n", start_time);
    printf("  -e   : end time.   (%s)\n", end_time);
    printf("  -oldf: old input file format (no Length)\n");    /* V0.15-A */
    printf("  -csv : output csv format\n");                    /* V0.16-A */
    printf("  Wireshark Export Option\n");
    printf("    [File]=>[Export]=>[File]\n");
    printf("       [v]Packet summary line\n");
    printf("       [v]Packet details: [As displayed]\n");
    printf("       [v]Packet Bytes\n");
    printf("       [ ]Each packet on a new page\n");
    exit(1);
}

int main(int a, char *b[])
{
    int  i;
    char *fname = NULL;

    for (i = 1; i<a; i++) {
		/* V0.11-A start */
        if (!strcmp(b[i], "-raw")) {
            rawf = 1;
            continue;
        }
        if (!strcmp(b[i], "-n")) {
            nnf = 1;
            continue;
        }
		/* V0.11-A end   */
		/* V0.15-A start */
        if (!strcmp(b[i], "-oldf")) {
            oldf = 1;
            continue;
        }
		/* V0.15-A end   */
		/* V0.16-A start */
        if (!strcmp(b[i], "-ip") && a > i+1) {
			if (ipcnt >= 2) usage();
			strcpy(iplist[ipcnt++], b[++i]);
            continue;
        }
        if (!strcmp(b[i], "-csv")) {
            csvf = 1;
            continue;
        }
		/* V0.16-A end   */
        if (!strcmp(b[i], "-v")) {
            ++vf;
            continue;
        }
        if (!strcmp(b[i], "-port")) {
            portf = 1;
            continue;
        }
        if (!strcmp(b[i], "-s") && a > i+1) {
            start_time = b[++i];
            continue;
        }
        if (!strcmp(b[i], "-e") && a > i+1) {
            end_time = b[++i];
            continue;
        }
        if (!strcmp(b[i], "-h")) {
            usage();
        }
		if (b[i][0] == '-') {
			printf("unknown option: %s\n", b[i]);
            usage();
		}
		fname = b[i];
    } 

	if (fname == NULL) {
		usage();
	}

	/* V0.11-A start */
	max_ntcpdata = MAX_NTCPDATA;
	if (nnf) {
		max_ntcpdata = CountData(fname) + 2;
	}
	/* V0.11-A end   */

	tcpdatas = (tcpdata_t *)malloc(max_ntcpdata * sizeof(tcpdata_t));
	portids  = (char **)malloc(MAX_NPORTID);

	ReadTcpFile(fname);

	if (portf) {
		DispByEachPortid();
	}

    return 0;
}


/* vim:ts=4:sw=4:
 */
