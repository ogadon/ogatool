/*
 *  xpstat : デーモン起動状態監視ツール
 *
 *      defailt  : for Linux
 *      -DH3050R : for HI-UX/WE2 and HP-UX
 *
 *	V1.00  96/10/30  by oga.
 *	V1.01  96/10/30  -fn support and wait time adjust
 *	V1.02  96/10/31  -fs, -n support
 *	V1.03  96/11/06  .xpstat comment, -x, -y, -l  support
 *	V1.04  14/05/01  fix open argument
 *
 */
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdlib.h>
#include <string.h>

#define VER		"1.04"
#define MAXPRC		64
#define BARLEN		12
#define MAXBARLEN	30
#define LOADTITLE	"Load ave."
#define AVEKEY		"average: "

/* process info */
typedef struct pst {
	char procname[256];	/* process name		*/
	char name[256];		/* display process name */
	char active;		/* 0:inactive  1:active */
	char cnt;		/* bar length		*/
} ps_t;

/* globals */
Display *d;
Window  w;
GC      ggreen,gwhite,gblack,gred,gskblue,gyellow;
int	xx, yy;
int	refresh = 0;		/* -r spcify flag (black refresh option) */
int	kaif    = 0;		/* -k spcify flag (kainuma option)       */
int	vuef    = 0;		/* -vue spcify flag (for VUE option)     */
int	lf      = 0;		/* -l spcify flag (load average)         */
int	nf      = 0;		/* -n spcify flag (number of procs)      */
int	loadave = 0;		/* load average */
char	fontn[256];		/* font name	*/
char	tmpf[256];		/* tmp filename */

int	BX = 10;	/* mini : 8  */
int	BY = 15;	/* mini : 12 */

void usage()
{
    printf("\n");
    printf(" xpstat Ver %s\n",VER);
    printf("\n");
    printf("   usage : xpstat [{ -f <proc_file> | procname1 procname2 ...}]\n");
    printf("\n");
    printf("   # <proc_file> is following format.\n");
    printf("   procname1[:name1]\n");
    printf("   procname2[:name2]\n");
    printf("       :        :   \n\n");
    printf("   # null line is a separator\n");
    printf("   # default <proc_file> is $HOME/.xpstat\n");
    printf("       -n   : level meter is number of processes\n");
    printf("       -x   : level meter block width\n");
    printf("       -y   : level meter block height\n");
    printf("       -l   : display load average\n");
    printf("       -fn  : set font name<%s>\n",fontn);
    printf("       -fs  : set font size<7>\n");
#if 0
    printf("       -vue : string color blue\n");
    printf("       -k   : kainuma option (display only one block)\n");
    printf("       -r   : refresh/always fill black\n");
#endif
    printf("\n Copyright(C)1996, Moritaka Ogasawara. All Rights Reserved.\n\n");
}

/*
 *   監視するプロセス名ファイル(.xpstat)を読み込む
 */
void getprocs(prc,num,filen,maxlen)
ps_t *prc;
int  *num, *maxlen;
char *filen;
{
	FILE *fp;
	int  i,j;
	char buf[1024];

	if (!(fp = fopen(filen,"r"))) {
	    sprintf(buf,"read %s",filen);
	    perror(buf);
	    usage();
	    exit(1);
	}
	while (fgets(buf,sizeof(buf),fp)) {
	    if (buf[0] == '#') {
		/* skip comment line */
	    	continue;
	    }
	    if (buf[strlen(buf)-1] == 0xa) {
	    	buf[strlen(buf)-1] = '\0';
	    }
	    i=0;
	    while (buf[i] != ':' && buf[i] != '\0') {
	    	prc[*num].procname[i] = buf[i];
		i++;
	    }
	    j=0;
	    if (buf[i] == ':') {
	        i++;
	        while (buf[i] != ':' && buf[i] != '\0') {
	    	    prc[*num].name[j] = buf[i];
		    i++;
		    j++;
	        }
	    } else {
	    	strcpy(prc[*num].name, prc[*num].procname);
	    }
            if (*maxlen < strlen(prc[*num].name)) {
            	*maxlen = strlen(prc[*num].name);
            }
#ifdef DEBUG
	    fprintf(stderr,"procname=[%s]\t/ name=[%s]\n",
	    				prc[*num].procname,prc[*num].name);
#endif
	    ++(*num);
	}
	fclose(fp);
}

/*
 *   レベルメータを表示する。 
 */
void dispbar(x,y,len)
int x,y,len;
{
	int i, barlen;

	if (nf) {
	    barlen = MAXBARLEN;
	} else {
	    barlen = BARLEN;
	}

	for (i = 0; i<barlen; i++) {
	    if (i < len) {
	        if (i < 9) {
	            /* green level */
	            XFillRectangle(d,w,ggreen,x+i*BX, y, BX-2, BY-2);
		    if (kaif) break;	/* 1個だけ表示 */
	        } else {
	            /* red zone */
	            XFillRectangle(d,w,gred,x+i*BX, y, BX-2, BY-2);
	        }
	    } else {
	        XFillRectangle(d,w,gblack,x+i*BX, y, BX-2, BY-2);
		if (kaif) break;	/* 1個だけ表示 */
	    } 
	}
}

/*
 *    監視プロセス情報を表示する
 */
void dispall(prc,num,strarea)
ps_t *prc;
int  num,strarea;
{
	int i, nfbk;

	if (refresh) {
	    /* fill black for VUE */
	    XFillRectangle(d,w,gblack,0,0,xx,yy);
	}

	if (lf) {
	    /* display load average line */
	    XDrawString(d, w, gyellow, 3, BY, LOADTITLE, strlen(LOADTITLE));
	    nfbk = nf;
	    nf = 1;		/* MAXBARLEN 使用 */
	    dispbar(strarea,2,(loadave*3)/100 +1);
	    nf = nfbk;
	}
	for(i = 0; i<num; i++) {
	    if (strlen(prc[i].procname)) {
	        /* draw process name */
	        XDrawString(d, w, gwhite, 3, (i+lf)*BY+BY,
	        			prc[i].name,strlen(prc[i].name));
		if (nf) {
		    /* length is number of procs */
	            dispbar(strarea,(i+lf)*BY+2,prc[i].active);/* disp meter */
		} else {
	            dispbar(strarea,(i+lf)*BY+2,prc[i].cnt);   /* disp meter */
		}
	    } else {
	        /* draw separate line */
	        XFillRectangle(d, w, gskblue, 3, (i+lf)*BY+BY/2+2,
	        				strarea+BX*MAXBARLEN-6,1);
	    }
	}
	XFlush(d);
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
		return((unsigned long)-1);
}

/* 
 *    get load average
 *
 *    IN  : buf ... uptime output
 *    OUT : ret ... load average x 100
 */
int get_loadave(buf)
char *buf;
{
	char *pt;
	char wkbuf[16];
	int  i=0;

	pt = (char *)strstr(buf,AVEKEY);/* search "average: " */

	pt += strlen(AVEKEY);		/* move pointer to X.XX */
	while (*pt != ' ') {
	    if (*pt != '.') {		/* skip . */
	        wkbuf[i] = *pt;
		i++;
	    }
	    pt++;
	}
	wkbuf[i] = '\0';

#ifdef DEBUG
	fprintf(stderr,"load ave = %s\n",wkbuf);
#endif
	return(atoi(wkbuf));
}

void sigcatch()
{
	unlink(tmpf);
	exit(1);
}

/*
 *   MAIN routine
 */
void main(a,b)
int a;
char *b[];
{
        FILE *fp;
        int fd;
        int num= 0, i, j, strarea = 0;
        int ff = 0;			/* -f spcify flag */
        int fnsize = 7;			/* font width     */
        int x_home=0,y_home=0;
        char buf[1024], filen[256];
        char *pt;
        ps_t prc[MAXPRC];
        struct timeval  tv, tv2, tv3;
        struct timezone tz;

	/* for Window */
	Font	font;
        XSetWindowAttributes att;
        unsigned long green,red,skblue,yellow,white,black;

	memset(prc,0,sizeof(ps_t)*MAXPRC);
	gettimeofday(&tv2,&tz);		/* init */

	/* default font name */
#ifdef H3050R
	strcpy(fontn,"7x13bold");
#else  /* Linux */
	strcpy(fontn,"7x14bold");
#endif

	/* get args */
        for (i=1; i<a; i++) {
            if (!strncmp(b[i],"-h",2)) {
            	usage();
        	exit(1);
            }
            if (!strcmp(b[i],"-f")) {
            	ff = 1;
            	i++;
            	strcpy(filen,b[i]);
            	continue;
            }
	    if (!strncmp(b[i],"-g",2) && a > i) {
		strcpy(buf,b[++i]);
		if (pt =(char *) strtok(buf,"+"))  x_home = atoi(pt);
		if (pt =(char *) strtok(NULL,"+")) y_home = atoi(pt);
		/* printf("x=%d / y=%d\n",x_home,y_home); */
		continue;
	    }
            if (!strncmp(b[i],"-r",2)) {
		refresh = 1;	/* VUE用にいちいち黒く塗る(注)ちかちかする*/
		continue;
            }
            if (!strncmp(b[i],"-k",2)) {
		kaif = 1;	/* 1 個のみ表示 海沼の好み */
		continue;
            }
            if (!strncmp(b[i],"-v",2)) {
		vuef = 1;	/* 文字の色を青にする for VUE */
		continue;
            }
            if (!strcmp(b[i],"-fn")) {
		if (++i < a) {
		    strcpy(fontn,b[i]);
		} else {
		    printf("specify font name!\n");
		}
		continue;
            }
            if (!strcmp(b[i],"-fs")) {
		if (++i < a) {
		    fnsize = atoi(b[i]);
		} else {
		    printf("specify font size!\n");
		}
		continue;
            }
            if (!strcmp(b[i],"-l")) {
		lf = 1;		/* load average 表示 */
		continue;
            }
            if (!strcmp(b[i],"-n")) {
		nf = 1;		/* レベルメータはプロセス数 */
		continue;
            }
            if (!strcmp(b[i],"-x")) {
		if (++i < a) {
		    BX = atoi(b[i]);
		} else {
		    printf("specify block width!\n");
		}
		continue;
            }
            if (!strcmp(b[i],"-y")) {
		if (++i < a) {
		    BY = atoi(b[i]);
		} else {
		    printf("specify block height!\n");
		}
		continue;
            }
            strcpy(prc[num].procname,b[i]);
            strcpy(prc[num].name,b[i]);
            if (strarea < strlen(prc[num].name)) {
            	strarea = strlen(prc[num].name);
            }
            num++;
            /* clear arg area */
            memset(b[i],0,strlen(b[i]));
        }

	/* read proc name */
	if (!ff) {
	    sprintf(filen,"%s/.xpstat",getenv("HOME"));
	} 
	if (!num) {
	    getprocs(prc,&num,filen,&strarea);
	}
	if (lf) {
	    if (strarea < strlen(LOADTITLE)) {
		strarea = strlen(LOADTITLE);
	    }
	}
	strarea = strarea*fnsize+7;	/* proc name area length */

	d = XOpenDisplay(NULL);

	green  = MyColor(d,"green");
	red    = MyColor(d,"red");
	skblue = MyColor(d,"orange");
	yellow = MyColor(d,"yellow");

	if (vuef) {
	    white  = MyColor(d,"blue");
	} else {
	    white  = MyColor(d,"white");
	}
	black  = MyColor(d,"Black");

	xx = strarea+BX*BARLEN+3;
	yy = num*BY+5;

	w = XCreateSimpleWindow(d, RootWindow (d,0),
			x_home,y_home, 			   /* home x,y        */
			strarea+BX*BARLEN+3, (num+lf)*BY+5,/* window size x,y */
			1,              		   /* border_width    */
			1,              		   /* border          */
			0   );          		   /* background      */

	att.override_redirect = 0;         /* Window Manager 介入あり ... 0 */
                                           /*                    なし ... 1 */

	sprintf(buf,"xpstat %s",VER);
	XStoreName(d,w,buf);

	XChangeWindowAttributes(d,w,CWOverrideRedirect,&att);
	XMapWindow(d,w);

	ggreen   = XCreateGC(d,w,0,0);           /* create gc   */
	gred     = XCreateGC(d,w,0,0);           /* create gc   */
	gwhite   = XCreateGC(d,w,0,0);           /* create gc   */
	gblack   = XCreateGC(d,w,0,0);           /* create gc   */
	gskblue  = XCreateGC(d,w,0,0);           /* create gc   */
	gyellow  = XCreateGC(d,w,0,0);           /* create gc   */

	XSetForeground(d,ggreen  ,green);        /* set color to gc  */
	XSetForeground(d,gred    ,red);          /* set color to gc  */
	XSetForeground(d,gwhite  ,white);        /* set color to gc  */
	XSetBackground(d,gwhite  ,black);        /* set back color to gc  */
	XSetForeground(d,gblack  ,black);        /* set color to gc  */
	XSetForeground(d,gskblue ,skblue);       /* set color to gc  */
	XSetForeground(d,gyellow ,yellow);       /* set color to gc  */

	/* load font and set */
	font = XLoadFont(d, fontn);
	XSetFont(d,gwhite,font);
	XSetFont(d,gyellow,font);

	XSelectInput(d,w, ButtonPressMask|ExposureMask);

	/* set tmp filename */
	sprintf(tmpf,"/tmp/pstmp%d",tv2.tv_usec);

	/* set signal handler */
        signal(SIGINT, sigcatch);
        signal(SIGQUIT,sigcatch);
        signal(SIGTRAP,sigcatch);
        signal(SIGABRT,sigcatch);
        signal(SIGTERM,sigcatch);
        signal(SIGPIPE,sigcatch);	/* close button */

        while(1) {
            /* ブロセスデータ採取 */
	    if ((fd = open(tmpf,O_CREAT|O_RDWR|O_TRUNC, 0666)) < 0) {
		perror("open pstmp");
		exit(1);
	    }
	    close(1);		/* close stdout(1) */
	    dup(fd);		/* stdout(1) redirect to tmpfile */

	    if (lf) {
		/* get load average */
	        system("uptime");
	    }
#ifdef H3050R
	    system("ps -ef");
#else  /* linux */
	    system("ps ax");
#endif
	    fchmod(fd,0666);	/* mode set to rw-rw-rw- */
	    close(fd);		/* close tmpfile         */

	    /* プロセス起動状態チェック */
	    if (!(fp = fopen(tmpf,"r"))) {
	    	perror("fopen pstmp");
	    	exit(1);
	    }

	    for (i=0; i<num; i++) {
		prc[i].active = 0;
	    }
	    if (lf) {
		/* get load average */
	    	fgets(buf,sizeof(buf),fp);
		loadave = get_loadave(buf);
	    }
	    while (fgets(buf,sizeof(buf),fp)) {
	    	for (i=0; i<num; i++) {
		    if (strstr(buf,"xpstat")) {
			continue;	/* H3050R cannot clear args */
		    }
		    if (pt =(char *) strstr(buf,prc[i].procname)) {
		        int c1,c2;

			/* 前後が区切り文字であることのチェック */
		        c1 = pt[strlen(prc[i].procname)];
		        c2 = *(--pt);
		    	if ((c1 == ' ' || c1 == 0xa || c1 == '\0') &&
		    	    (c2 == '/' || c2 == ' ')) {
		    	    ++prc[i].active;	/* process is active */
		    	}
		    }
	    	}
	    }
	    fclose(fp);

	    /* 起動/停止状態によりBARの長さを変更 */
	    for (j=0; j<3; j++) {
	    	for (i=0; i<num; i++) {
	    	    if (prc[i].active) {
	    	    	(prc[i].cnt < BARLEN)? prc[i].cnt++ : 0 ;
	    	    } else {
	    	    	(prc[i].cnt > 0)? prc[i].cnt-- : 0 ;
	    	    }
		}

		/* ちょっと待ってみよう */
		gettimeofday(&tv3,&tz);
		tv.tv_sec  = 0;
		/* tv.tv_usec = 300000;		/* 0.3sec        */

		/* 待ち時間調整 */
		if (tv2.tv_usec < tv3.tv_usec) {
		    tv.tv_usec = 300000 - (tv3.tv_usec-tv2.tv_usec);
		} else {
		    tv.tv_usec = 300000 - (1000000+tv3.tv_usec-tv2.tv_usec);
		}
		if (tv.tv_usec < 0) {
		    tv.tv_usec = 1;
		}

		select(0,0,0,0,&tv);		/* wait a little */	
		gettimeofday(&tv2,&tz);

		/* BAR 表示 */
		dispall(prc,num,strarea);
	    }
	}
}

/* vim:ts=8:sw=8:
 */

