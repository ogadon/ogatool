/*
 *     xpkt : �p�P�b�g�̗��ʕ\��
 *
 *             V1.00  1998.05.30    by Hyper Halx.f oga.
 *             V1.01  1998.11.01    fix 1000000 packet bug
 *             V1.02  2000.01.05    support -title
 *             V1.03  2000.12.22    support value display
 *             V1.04  2001.02.03    fix invalid value when if add/delete
 *             V1.05  2001.05.02    support Linux 2.2.x 
 *             V1.06  2003.05.12    fix ibyte,obyte overflow
 */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <stdlib.h>

#define VER	"Ver 1.06"
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

GC gc[NCOLOR];
XEvent	report;

int	vf = 0;		/* verbose flag */

unsigned long MyColor(Display *, char *);
int get_pktdata();
int get_bar(int);
char *get_item(char *, char *, int, char *);

#ifdef HIUX
void GlobalMemoryStatus(MEMORYSTATUS *);
int ExecOpenStdout(char *, int *);
int ReadLine(char *, int , int );
#endif /* HIUX */

main(a,b)
int a;
char *b[];
{
	Display *d;
	Window  w;
	XSetWindowAttributes att;
	Font    font;
	int i;
	int x_size = 0, y_size = 0;  /* current window size */
	int x_home = 10, y_home = 10;
	int x0, y0;
	int xbuf, xused, xfree, xcache;
	int wait = 1;		/* 5sec */
	int width;
	char	name[128],wk[128];
	char	value[1024];    /* V1.03        */
	char	*pt;
	char	*title = NULL;  /* V1.02        */
	int	UNIT = 50;	/* 1 scale      */
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
		"10G"	/* 6 */
		"100G"	/* 7 */
	};

	u_long  color[NCOLOR];
	char	*cname[NCOLOR] = {
		"black",	/* 0 */
		"red",		/* 1 */
		"blue",		/* 2 */
		"magenta",	/* 3 */
		"green",	/* 4 */
		"cyan",		/* 5 */
		"yellow",	/* 6 */
		"white"		/* 7 */
	};
	enum col {BLACK, RED, BLUE, MAGENTA, GREEN, CYAN, YELLOW, WHITE};

	d = XOpenDisplay(NULL);

	for (i = 1; i<a ; i++) {
		if (!strncmp(b[i],"-h",2)) {
			/*printf("usage : xpkt [-update <time>] [-geometry <X>x<Y>+<X>+<Y>]\n"); */
			printf("usage : xpkt [-update <time(1)>]\n");
			printf("             [-scale <scale_width>]\n");
			printf("             [-title <title>]\n");
			printf("             [-nonum]\n");
			exit(1);
		}
		if (!strncmp(b[i],"-g",2) && a > i) {
			strcpy(name,b[++i]);
			if (pt = strtok(name,"x")) x_size = atoi(pt);
			if (pt = strtok(NULL,"+")) y_size = atoi(pt);
			if (pt = strtok(NULL,"+")) x_home = atoi(pt);
			if (pt = strtok(NULL,"+")) y_home = atoi(pt);
			continue;
		}
		if (!strncmp(b[i],"-u",2) && a > i) {
			wait = atoi(b[++i]);		/* update time */
			continue;
		}
		if (!strncmp(b[i],"-s",2) && a > i) {
			UNIT = atoi(b[++i]);		/* scale width */
			continue;
		}
		if (!strncmp(b[i],"-v",2)) {
			vf = 1;				/* verbose */
			continue;
		}
		if (!strcmp(b[i],"-title") && a > i) {
			title = b[++i];			/* title V1.02 */
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

	if (!x_size) {
	    x_size = OFFS + 4*UNIT  + RM;	/* 1MB/sec�܂�OK */
	}
	if (!y_size) {
	    y_size = 20*ifnum +LH;
	}

	for (i = 0; i<NCOLOR; i++) {
	    color[i] = MyColor(d,cname[i]);	   /* create color id     */
        }

        if (vf) {
            printf("x=%d, y=%d, width=%d, height=%d\n",
            				x_home,y_home,x_size,y_size);
        }

	w = XCreateSimpleWindow(d, RootWindow (d,0),
					x_home,y_home,  /* home x,y         */ 
					x_size,y_size,  /* window size x,y  */
					1,              /* border_width */
					1,              /* border       */
					0   );          /* background   */

	att.override_redirect = 0;         /* Window Manager ������� ... 0 */
                                           /*                    �Ȃ� ... 1 */

	XChangeWindowAttributes(d,w,CWOverrideRedirect,&att);
	XMapWindow(d,w);

	for (i = 0; i<NCOLOR; i++) {
	    gc[i]    = XCreateGC(d,w,0,0);         /* create gc           */
	    XSetForeground(d,gc[i], color[i]);     /* set fg color to gc  */
	    XSetBackground(d,gc[i], color[BLACK]); /* set bg color to gc(black)*/
	    if (i == RED) {
                /* load font and set */
                /* font = XLoadFont(d, "5x10"); */
                font = XLoadFont(d, "7x13");
                XSetFont(d,gc[i],font);
	    }
	}

	/* Title Name */
	if (title) {
	    sprintf(name,"%s - xpkt %s",title,VER); /* V1.02 */
	} else {
	    sprintf(name,"xpkt %s",VER);
	}
	XStoreName(d,w,name);
	XSelectInput(d,w, ButtonPressMask|ExposureMask);

	while (1){
	    memcpy(pktold, pktdat, sizeof(pktold));	/* pktold <= pktdat */
	    ifnum = get_pktdata(pktdat);

	    y0    = LH;
	    width = (y_size-TM-LH)/(ifnum*2);

	    XFillRectangle(d,w,gc[BLACK],0,0,x_size,y_size);

	    for (i = 0; i<ifnum; i++) {
	    	/* �O���t�`�� */
		if (pktdat[i].mtu > 0) {    /* V1.05-A */
                    /* Linux -2.0.x (packets) */
	            ibyte = pktdat[i].mtu * (pktdat[i].ipkt - pktold[i].ipkt);
		    obyte = pktdat[i].mtu * (pktdat[i].opkt - pktold[i].opkt);
		} else {                /* V1.05-A */
                    /* Linux 2.2.x- (bytes)   V1.05-A */
	            ibyte = (pktdat[i].ipkt - pktold[i].ipkt);
		    obyte = (pktdat[i].opkt - pktold[i].opkt);
		}                       /* V1.05-A */
		ilen = get_bar(ibyte)*UNIT/10;
		olen = get_bar(obyte)*UNIT/10;
		dprintf("### ibyte=%d ilen=%d obyte=%d olen=%d\n",ibyte,ilen,obyte,olen);

		/* interface name */
		/* pktdat[i].name[strlen(pktdat[i].name)-1] = '\0'; V1.04-D */
		XDrawImageString(d, w, gc[WHITE], 5, y0+TM+LH+(i*width*2)-4, 
			pktdat[i].name, strlen(pktdat[i].name));

		/* receive packet bar */
		XFillRectangle(d,w,gc[GREEN], OFFS, y0+TM+(i*width*2),   
							ilen,width-2);

		/* transmit packet bar */
		XFillRectangle(d,w,gc[YELLOW],OFFS, y0+TM+(i*width*2)+width,
							olen,width-2);

                if (!nonumf) {
                    /* receive packet value V1.03 */
                    if (ibyte) {
                        sprintf(value, "%d", ibyte/1024);
                        XDrawImageString(d, w, gc[RED], OFFS+2,
                                         y0+TM+LH+(i*width*2)-7,
                                         value, strlen(value));
                    }

                    /* transmit packet value V1.03 */
                    if (obyte) {
                        sprintf(value, "%d", obyte/1024);
                        XDrawImageString(d, w, gc[RED], OFFS+2,
                                         y0+TM+LH+(i*width*2)-7+width,
                                         value, strlen(value));
                    }
                }

	    }

	    /* �ڐ� */
	    for (i = 0; i<7; ++i) {
		XFillRectangle(d,w,gc[BLACK],
		    OFFS+UNIT*(i+1),
		    y0+TM,
		    1,
		    y0*6+TM);

	        XDrawImageString(d, w, gc[WHITE],
	 	    OFFS+UNIT*(i+1)-8, 10, unit[i], strlen(unit[i]));
	    }

	    XFlush(d);

	    /* ��ʂ��N���b�N���ꂽ�� */
	    if (XCheckMaskEvent(d,ButtonPressMask,&report) == True) {
		printf("xpkt %s  by Moritaka Ogasawara.\n",VER);
	    }

	    /* ���T�C�Y */
	    if (XCheckMaskEvent(d,ExposureMask,&report) == True) {
		Window root;
		int x,y;
		unsigned int width,height, border, depth;

		XGetGeometry(d,w,&root,&x,&y,&width,&height,&border,&depth);
		if (x_size != width || y_size != height) {
			x_size = width;
			y_size = height;
			XClearWindow(d,w);
		}
	    }

	    sleep(wait);
	}
}

unsigned long MyColor(display,color)
Display *display;
char *color;
{
	Colormap cmap;
	XColor c0,c1;
	int code;

	cmap = DefaultColormap(display,0);

	code = XAllocNamedColor(display,cmap,color,&c1,&c0);
	if (code)
		return(c1.pixel);
	else
		return(-1);
}

#ifdef HIUX
/* 
 *  get memory information from /etc/sysinfo command for HIUX
 *
 */
void get_memdata(mem)
mem_t *mem;
{
	int  *fd;
	char buf[1024];
	char *pt;
	FILE *fp;
	MEMORYSTATUS memst;

	GlobalMemoryStatus(&memst);
	mem->total  = memst.dwTotalPhys/1024;
	mem->free   = memst.dwAvailPhys/1024;
	mem->used   = (mem->total - mem->free);
	mem->shared = 0;
	mem->buf    = 0;
	mem->swap_total = memst.dwTotalPageFile/1024;
	mem->swap_free  = memst.dwAvailPageFile/1024;
	mem->swap_used  = mem->swap_total - mem->swap_free;

#ifdef DEBUG
	printf("Memory : %d %d %d %d %d %d\n",mem->total,mem->used,
			mem->free,mem->shared,mem->buf,mem->cache);
	printf("Swap   : %d %d %d\n",mem->swap_total,mem->swap_used,mem->swap_free);
#endif
}

#else /* Linux */

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
 *   10���ȏ�̏ꍇ��9���݂̂ɕύX����
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
 *    OUT      ret  : ���������C���^�t�F�[�X�e�[�u�������i�[����B
 */
int get_pktdata(if_t pkt[])
{
	FILE *fp;
	char buf[1024],wk[1024];
	char *pt;
	int  i,j,found;

	dprintf("## get_pktdata start\n");

	/* memset(pkt, 0, sizeof(if_t)*NDAT);  V1.04-D */
	for (i = 0; i<NDAT; i++) {          /* V1.04-A */
	    pkt[i].active = 0;              /* V1.04-A */
	}                                   /* V1.04-A */

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
	                pkt[i].active = 1;    /* V1.04-A */
	                break;
	            }
	            i++;
	        }
	        if (!found) {
	            /* �܂����o�^�Ȃ�o�^ */
	            strcpy(pkt[i].name, wk);
	    	    if (get_item(buf,"\t",9,wk)) pkt[i].mtu = atoi(wk);
		    /* V1.05-A start */
		    if (pkt[i].mtu == 0) {
			pkt[i].mtu = -1;
		    }
#if 0
		    if (pkt[i].mtu == 0) {
			dprintf("NAME[%s] MTU[%d]\n", pkt[i].name, pkt[i].mtu);
			get_mtu(pkt[i].name, &pkt[i].mtu);
		    }
		    dprintf("NAME[%s] MTU[%d]\n", pkt[i].name, pkt[i].mtu);
#endif
		    /* V1.05-A end   */
	            pkt[i].active = 1;        /* V1.04-A */
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
	            /* /proc/net/route�ɂ���if�̂ݐݒ� */
		    if (strstr(wk,pkt[i].name)) {
		        pt = strchr(buf, ':');     /* V1.01 */
		        if (pt) *pt = ' ';         /* V1.01 */
			if (pkt[i].mtu > 0) {      /* V1.05-A */
			    /* Linux -2.0.x (packets) */
	    		    if (get_item(buf," ",2,wk)) pkt[i].ipkt = atoi(wk);
	    		    if (get_item(buf," ",7,wk)) pkt[i].opkt = atoi(wk);
			} else {                   /* V1.05-A */
			    /* Linux 2.2.x- (bytes)   V1.05-A */
			    /* V1.06-C start */
	    		    /* if (get_item(buf," ",2,wk))  pkt[i].ipkt = atoi(wk); */
	    		    /* if (get_item(buf," ",10,wk)) pkt[i].opkt = atoi(wk); */
	    		    if (get_item(buf," ",2,wk)) {
			        to9digit(wk);
			        pkt[i].ipkt = atoi(wk);
			    }
	    		    if (get_item(buf," ",10,wk)) {
			        to9digit(wk);
			        pkt[i].opkt = atoi(wk);
			    }
			    /* V1.06-C end   */
			}                          /* V1.05-A */
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
	    		pkt[i].active,          /* V1.04-A */
	    		pkt[i].mtu,
	    		pkt[i].ipkt,
	    		pkt[i].opkt);
	    }
	    i++;
	}
	dprintf("## get_pktdata end\n");
	return i;
}
#endif /* Linux */

#ifdef HIUX
/* 
 *   ExecOpenStdout(path)					A3UAD001
 *   
 *       path�Ŏw�肳�ꂽ�R�}���h�����s���A���̃R�}���h�̕W���o�͂�
 *       �t�@�C���f�B�X�N���v�^��Ԃ��B
 *   
 *   IN  path : ���s����R�}���h�̃t���p�X
 *   OUT ret  : ���� : ���s�����R�}���h�̕W���o�͂̃t�@�C���f�B�X�N���v�^�B
 *              ���s : -1
 *       err  : ���s���Aerrno���ݒ肳���
 * 
 *   (��) ExecOpenStdout()�����������ꍇ�A�Ăяo�����ŕԂ��ꂽ
 *        �t�@�C���f�B�X�N���v�^��close()���邱�ƁB
 */
int ExecOpenStdout(char *path , int *err)
{
    int pfd1[2];   /* [0]:for read  [1]:for write */
    int pid;

    /* �p�C�v�쐬 */
    if (pipe(pfd1) < 0) {
	*err = errno;
	return -1;	/* pipe create error */
    }

    if (!(pid = fork())) {
	/* �q�v���Z�X(���s�R�}���h��) */
	close(1);       /* stdout �����                        */
	dup(pfd1[1]);   /* �R�}���h��stdout�� pfd1[1]�Ɋ��蓖�Ă� */
	close(pfd1[0]); /* �ǂݍ��ݗp��pfd1[0]�����            */
	execlp(path, path,(char *)0); /* �R�}���h���s(�����Ȃ�)   */
	/* not reached */
    } else if (pid < (pid_t)0) {
	*err = errno;
	return -1; 	/* fork error */
    }

    /* �e�v���Z�X(�Ăяo����) */
    close(pfd1[1]);		/* �������ݗp��pfd1[0]�����   */

    return pfd1[0];		/* �t�@�C���f�B�X�N���v�^��Ԃ�  */
}

/* 
 *   ReadLine(buf, size, fd)					A3UAD001
 *   
 *       �t�@�C���f�B�X�N���v�^fd�����s���̕��������o���B
 *       fgets()�̃t�@�C���f�B�X�N���v�^��
 *       �ő�size-1�����ǂݍ���
 *   
 *   IN  size : ������i�[�̈�̍ő�T�C�Y
 *       fd   : �ǂݍ��ݑΏۃt�@�C���f�B�X�N���v�^
 *   OUT buf  : ��s���̕�����
 *       ret  : �ǂݍ��񂾃T�C�Y
 *              ���s(EOF) : -1
 * 
 *   (��) ExecOpenStdout()�����������ꍇ�A�Ăяo�����ŕԂ��ꂽ
 *        �t�@�C���f�B�X�N���v�^��close()���邱�ƁB
 */
int ReadLine(char *buf, int size, int fd)
{
    int cnt = 0;
    int len = 0;

    while (len = read(fd,&buf[cnt],1)) {	/* 1�o�C�g�Âǂ� */
	if (buf[cnt] == '\n') {
	    break;
	}
	++cnt;
	if (cnt >= size-1) {
	    break;
	}
    }
    buf[cnt] = '\0';
#ifdef XDEBUG
    printf( "OUTPUT:[%s] cnt=%d len=%d\n",buf,cnt,len);
#endif
    if (len < 0) {
        printf("command output read error. code=%d",errno);
	return -1;
    }
    if (len == 0) return -1;	/* EOF */
    return cnt;
}
#endif  /* HIUX */

/*
 *  char *get_item(buf,sep,pos,outbuf)
 *
 *     buf����sep�ŋ�؂�ꂽpos�Ԗڂ̍��ڂ�outbuf�ɃR�s�[���ĕԂ��܂��B
 *     ���ڒ��̑O��̃X�y�[�X�����͍폜����܂�
 *     buf��1024�����܂łł�
 *          sep��","���w�肷��ƁA",,"�͈�̋�؂蕶���Ƃ��ĔF������܂�
 *     
 *     IN   buf    : �؂�o�����̕�����(���ڂ�sep�ŋ�؂��Ă���)
 *          pos    : 1�ȏ�̒l (1���ŏ��̍���)
 *          sep    : ���ڂ̋�؂蕶���ł� " ", ","�Ȃ�
 *     OUT  outbuf : �w�荀�ڂ̕�����(���ڒ��̌�̃X�y�[�X/���s�͍폜)
 *          ret    : ������  outbuf
 *                   ���s��  (char *)0
 */
char *get_item(char *buf, char *sep, int pos, char *outbuf)
{
 int i;
 char *pt = NULL;
 char *p;
 char wk[1024];

	strcpy(wk,buf);      /* strtok()��buf��j�󂷂邽�߃R�s�[���ė��p���� */

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
		/* ���ʗ̈�N���A */
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
 *  �ΐ����ۂ����������߂� 
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

	j -= 30;	/* 10KB��10�Ƃ��� */

	if (j <= 0) {
	    j = 1;
	}
	dprintf("ret = %d\n",j);

	return j;
}
