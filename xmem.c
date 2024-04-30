/*
 *     xmem : メモリの使用状況を表示 
 *
 *             V0.10  96.08.31    by Hyper Halx.f oga.
 *             V0.20  97.02.14    Support Linux2.0.X "cached:"
 *             V0.21  97.05.16    Support HI-UX/WE2   flag is HIUX
 *             V0.30  97.06.07    change out lokking
 *             V0.31  99.05.07    delete MB when over 64MB
 *             V0.32  01.02.12    support every 20M,50M scale
 *             V0.33  01.10.11    support -title option
 *             V0.34  05.09.11    support Linux2.6.X
 *
 */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#define VER	"Ver 0.34"
#define S_MEM	"Memory"
#define S_SWAP	"Swap"

/* constants */
#define RM	10		/* right margine */
#define TM	 5		/* top margine   */
#define OFFS	45		/* string area   */

unsigned long MyColor();
void get_memdata();

#ifdef HIUX
typedef struct _MEMORYSTATUS {
    unsigned int dwLength;		/* not used on UNIX */
    unsigned int dwMemoryLoad;		/* not used on UNIX */
    unsigned int dwTotalPhys;		/* 全  物理メモリ   */
    unsigned int dwAvailPhys;		/* 空き物理メモリ   */
    unsigned int dwTotalPageFile;	/* 全  スワップ容量 */
    unsigned int dwAvailPageFile;	/* 空きスワップ容量 */
    unsigned int dwTotalVirtual;	/* not used on UNIX */
    unsigned int dwAvailVirtual;	/* not used on UNIX */
} MEMORYSTATUS;	/* for GlobalMemoryStatus */

void GlobalMemoryStatus(MEMORYSTATUS *);
int ExecOpenStdout(char *, int *);
int ReadLine(char *, int , int );
char *get_item(char *, char *, int, char *);
#endif /* HIUX */

typedef struct mem {
	int total;		/* total memory size <KB>           */
	int used;		/* used memory size <KB>            */
	int free;		/* free memory size <KB>            */
	int shared;		/* shared memory size <KB>          */
	int buf;		/* filesystem used cache size <KB>  */
	int cache;		/* cache size(V0.20 for Linux 2.0.X)*/
	int swap_total;		/* total swap size <KB>             */
	int swap_used;		/* used swap size <KB>              */
	int swap_free;		/* free swap size <KB>              */
} mem_t;

/*
union { 
    XEvent	report;
    XKeyEvent	key;
} report;
*/
XEvent	report;

main(a,b)
int a;
char *b[];
{
	Display *d;
	Window  w;
	GC      gc_use, gc_buf, gc_free,gc_black, gc_scale, gc_cache, gc_str;
	XSetWindowAttributes att;
	unsigned long black, cuse, cbuf, cfree, cscale, ccache, cstr;
	int i;
	int x_size = 0, y_size = 0;  /* current window size */
	int x_home = 10, y_home = 10;
	int x0, y0;
	int xbuf, xused, xfree, xcache;
	int xsused, xsfree;
#ifdef HIUX
	int wait = 1;		/* 5sec */
#else  /* HIUX */
	int wait = 1;		/* 1sec */
#endif /* HIUX */
	int width;
	char	name[128],wk[128];
	char    title[512];     /* -title V0.33 */
	char	*pt;
	mem_t	mem;
	int	vf = 0;		/* verbose flag */
	int	UNIT = 0;	/* 1 scale      */

	strcpy(title, "");      /* V0.33-A */
	
	d = XOpenDisplay(NULL);

	for (i = 1; i<a ; i++) {
		if (!strncmp(b[i],"-h",2)) {
			printf("usage : xmem [-update <time>] [-geometry <X>x<Y>+<X>+<Y>]\n");
			printf("             [-scale <scale_width>]\n");
			printf("             [-title <title>\n");
			exit(1);
		}
		if (!strncmp(b[i],"-g",2) && a > i) {
			strcpy(name,b[++i]);
			if (pt = strtok(name,"x")) x_size = atoi(pt);
			if (pt = strtok(NULL,"+")) y_size = atoi(pt);
			if (pt = strtok(NULL,"+")) x_home = atoi(pt);
			if (pt = strtok(NULL,"+")) y_home = atoi(pt);
		}
		if (!strncmp(b[i],"-u",2) && a > i) {
			wait = atoi(b[++i]);		/* update time */
		}
		if (!strncmp(b[i],"-s",2) && a > i) {
			UNIT = atoi(b[++i]);		/* scale width */
		}
		if (!strncmp(b[i],"-title",6) && a > i) {
			strcpy(title, b[++i]);		/* title V0.33-A */
		}
		if (!strncmp(b[i],"-v",2) && a > i) {
			vf = 1;				/* verbose */
		}
	}

	get_memdata(&mem);
	/* mem.total = 8*1024; */
	if (!UNIT) {
	    UNIT = (250 - OFFS - RM)/((mem.total-1)/10240 + 1) ;
	    if (UNIT > 100) UNIT=100;
	}
	if (!x_size) {
	    i = (mem.total-1)/10240 + 1;
	    if (i < 2) i = 2;
	    x_size = OFFS + i * UNIT  + RM;
	}
	if (!y_size) {
	    y_size = 55+TM;
	}
	if (vf) printf("mem.total=%d, x_size=%d, UNIT=%d\n",mem.total,x_size,UNIT);

	cuse   = MyColor(d,"red");
	cbuf   = MyColor(d,"green");
	cfree  = MyColor(d,"blue");
	cscale = MyColor(d,"white");
	ccache = MyColor(d,"yellow");
	cstr   = MyColor(d,"yellow");
	black  = MyColor(d,"Black");

	w = XCreateSimpleWindow(d, RootWindow (d,0),
					x_home,y_home,  /* home x,y         */ 
					x_size,y_size,  /* window size x,y  */
					1,              /* border_width */
					1,              /* border       */
					0   );          /* background   */

	att.override_redirect = 0;         /* Window Manager 介入あり ... 0 */
                                           /*                    なし ... 1 */

	XChangeWindowAttributes(d,w,CWOverrideRedirect,&att);
	XMapWindow(d,w);

	gc_use   = XCreateGC(d,w,0,0);           /* create gc   */
	gc_buf   = XCreateGC(d,w,0,0);           /* create gc   */
	gc_free  = XCreateGC(d,w,0,0);           /* create gc   */
	gc_scale = XCreateGC(d,w,0,0);           /* create gc   */
	gc_cache = XCreateGC(d,w,0,0);           /* create gc   */
	gc_str   = XCreateGC(d,w,0,0);           /* create gc   */
	gc_black = XCreateGC(d,w,0,0);           /* create gc   */

	XSetForeground(d,gc_use  ,cuse);         /* set color to gc  */
	XSetForeground(d,gc_buf  ,cbuf);         /* set color to gc  */
	XSetBackground(d,gc_buf  ,black);        /* set back color to gc  */
	XSetForeground(d,gc_free ,cfree);        /* set color to gc  */
	XSetForeground(d,gc_scale,cscale);       /* set color to gc  */
	XSetForeground(d,gc_cache,ccache);       /* set color to gc  */
	XSetBackground(d,gc_scale,black);        /* set back color to gc  */
	XSetForeground(d,gc_str  ,cstr);         /* set color to gc  */
	XSetBackground(d,gc_str  ,black);        /* set back color to gc  */
	XSetForeground(d,gc_black,black);        /* set color to gc  */

        if (strlen(title)) {
	    sprintf(name,"%s - xmem %s",title, VER);
	} else {
	    sprintf(name,"xmem %s",VER);
	}
	XStoreName(d,w,name);
	XSelectInput(d,w, ButtonPressMask|ExposureMask);

	while (1){
		get_memdata(&mem);

		y0    = (y_size-TM)/7;
		width = (y_size-TM)*2/7;

		xbuf   = (UNIT * mem.buf)/10240;
		xcache = (UNIT * mem.cache)/10240;
		xused  = (UNIT * mem.used)/10240 - xbuf - xcache;
		xfree  = (UNIT * mem.free)/10240;

		xsused = (UNIT * mem.swap_used)/10240;
		xsfree = (UNIT * mem.swap_free)/10240;

		/* disp mem data */
		XFillRectangle(d,w,gc_use,  OFFS,                  y0+TM,xused,width);
		XFillRectangle(d,w,gc_cache,OFFS+xused,            y0+TM,xcache,width);
		XFillRectangle(d,w,gc_buf,  OFFS+xused+xcache,     y0+TM,xbuf, width);
		XFillRectangle(d,w,gc_free, OFFS+xused+xcache+xbuf,y0+TM,xfree,width);
		if (mem.cache) {
		    /* Linux 2.0.X format */
		    sprintf(wk,"Memory (%.1fM/%.1fM/%.1fM/%.1fM) ",
					(float)(mem.used-mem.buf-mem.cache)/1024,
					(float)mem.cache/1024,
					(float)mem.buf  /1024,
					(float)mem.free /1024);
		} else {
#ifdef HIUX
		    /* HIUX format */
		    sprintf(wk,"Memory (%.1fM/%.1fM) ",
					(float)mem.used /1024,
					(float)mem.free /1024);
#else  /* HIUX */
		    /* Linux 1.2.13 format */
		    sprintf(wk,"Memory (%.1fM/%.1fM/%.1fM) ",
					(float)(mem.used-mem.buf-mem.cache)/1024,
					(float)mem.buf  /1024,
					(float)mem.free /1024);
#endif /* HIUX */
		}
#ifdef V020
		XDrawImageString(d, w, gc_scale,
			OFFS+xused+xcache+xbuf+xfree+5, y0*3-3, wk, strlen(wk));
#else /* V030 */
		XDrawImageString(d, w, gc_str,
			5, y0*3-3+TM, S_MEM, strlen(S_MEM));
#endif /* V020 */

		/* disp swap data */
		XFillRectangle(d,w,gc_use,OFFS,y0*4+TM,xsused,width);
		XFillRectangle(d,w,gc_free,OFFS+xsused,y0*4+TM,xsfree,width);
		sprintf(wk,"Swap (%.1fM/%.1fM)",
			(float)mem.swap_used/1024,
			(float)mem.swap_total/1024);
#ifdef V020
		XDrawImageString(d,w,gc_scale,OFFS+xsused+xsfree+5,y0*6,
								wk,strlen(wk));
#else /* V030 */
		XDrawImageString(d, w, gc_str,
			5, y0*6+TM, S_SWAP, strlen(S_SWAP));
#endif /* V020 */

		/* 目盛 */
		for (i = 0; i<100; ++i) {
#ifdef V020
			XFillRectangle(d,w,gc_scale,
			    OFFS+UNIT*i,
			    y0*3+y0/4+1,
			    2,
			    y0/2);
#else /* V030 */
			XFillRectangle(d,w,gc_black,
			    OFFS+UNIT*(i+1),
			    y0+TM,
			    1,
			    y0*6+TM);

			if (mem.total/1024 <= 64) {  /* V0.31 */
		            sprintf(wk,"%dM",(i+1)*10);  /* V0.33 */
			} else {
		            sprintf(wk,"%d",(i+1)*10);
			}

			/* V0.32-A start */
			if (UNIT < 10) {
			    if (((i+1) % 5) != 0) continue;
			} else if (UNIT < 20) {
			    if (((i+1) % 2) != 0) continue;
			}
			/* V0.32-A end */

		        XDrawImageString(d, w, gc_scale,
		 	    OFFS+UNIT*(i+1)-8, 10, wk, strlen(wk));
		        
#endif /* V020 */
		}

		XFlush(d);

		/* 画面をクリックされたら */
		if (XCheckMaskEvent(d,ButtonPressMask,&report) == True) {
			printf("xmem %s  by Moritaka Ogasawara.\n",VER);
		}

		/* リサイズ */
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
/* 
 *  get memory information from /proc/meminfo for Linux
 * 
 *  unit is KB
 */
void get_memdata(mem)
mem_t *mem;
{
    FILE *fp;
    char buf[1024];
    char *pt;

    if (!(fp = fopen("/proc/meminfo","r"))) {
        perror("fopen /proc/meminfo");
        exit(1);
    }
    fgets(buf,sizeof(buf),fp);	/* skip header   */

    if (strstr(buf, "total:")) {
        /* Linux 2.0～2.4 */
	fgets(buf,sizeof(buf),fp);	/* read memdata  */
	strtok(buf," ");		/* skip Mem:     */
	mem->total  = atoi(strtok(NULL," "))/1024;
	mem->used   = atoi(strtok(NULL," "))/1024;
	mem->free   = atoi(strtok(NULL," "))/1024;
	mem->shared = atoi(strtok(NULL," "))/1024;
	mem->buf    = atoi(strtok(NULL," "))/1024;
	if (pt = strtok(NULL," ")) {			/* V0.20 */
		mem->cache = atoi(pt)/1024;
	} else {
		mem->cache = 0;
	}

	fgets(buf,sizeof(buf),fp);	/* read swapdata */
	strtok(buf," ");		/* skip Mem:     */
	mem->swap_total = atoi(strtok(NULL," "))/1024;
	mem->swap_used  = atoi(strtok(NULL," "))/1024;
	mem->swap_free  = atoi(strtok(NULL," "))/1024;
    } else {
        /* Linux 2.6～ */
	mem->shared = 0;
	do {
	    if (!strncmp(buf, "MemTotal:", strlen("MemTotal:"))) {
	        mem->total = atoi(&buf[strlen("MemTotal:")]); /* KB */
		continue;
	    } else if (!strncmp(buf, "MemFree:", strlen("MemFree:"))) {
	        mem->free = atoi(&buf[strlen("MemFree:")]); /* KB */
		continue;
	    } else if (!strncmp(buf, "Buffers:", strlen("Buffers:"))) {
	        mem->buf = atoi(&buf[strlen("Buffers:")]); /* KB */
		continue;
	    } else if (!strncmp(buf, "Cached:", strlen("Cached:"))) {
	        mem->cache = atoi(&buf[strlen("Cached:")]); /* KB */
		continue;
	    } else if (!strncmp(buf, "SwapTotal:", strlen("SwapTotal:"))) {
	        mem->swap_total = atoi(&buf[strlen("SwapTotal:")]); /* KB */
		continue;
	    } else if (!strncmp(buf, "SwapFree:", strlen("SwapFree:"))) {
	        mem->swap_free = atoi(&buf[strlen("SwapFree:")]); /* KB */
		continue;
	    }
	} while (fgets(buf, sizeof(buf), fp));

	mem->used = mem->total - mem->free;
	mem->swap_used = mem->swap_total - mem->swap_free;
    }

    fclose(fp);

#ifdef DEBUG
    printf("%d %d %d %d %d %d\n",mem->total,mem->used,
			mem->free,mem->shared,mem->buf,mem->cache);
    printf("%d %d %d\n",mem->swap_total,mem->swap_used,mem->swap_free);
#endif
}
#endif /* Linux */

#ifdef HIUX
/* 
 *   GlobalMemoryStatus(mem)  ... Win32 GlobalMemoryStatus Emulation  A3UAD001
 *   
 *   IN  mem : MEMORYSTATUS *
 */
void GlobalMemoryStatus(MEMORYSTATUS *mem)
{
    int  sysfd;				/* file descriptor */
    int  st;				/* err code        */
    char *phys  = "physical memory is";	/* 物理メモリ      */
    char *freem = "free memory";	/* 空きメモリ      */
    char buf[2048];
    char wk[2048];

    memset(mem,0,sizeof(MEMORYSTATUS));

    /* 
     *  メモリ情報取得 (/etc/sysdef) 
     */
    if ((sysfd = ExecOpenStdout("/etc/sysdef" , &st)) < 0) {
        /* /etc/sysdef 起動失敗 */
        printf("/etc/sysdef execute error. code=%d",st);
    } else {
      /* /etc/sysdef 起動成功 */
      while(ReadLine(buf,sizeof(buf),sysfd) >= 0) {
	if (strstr(buf,phys)) {
	    if (get_item(buf," ",4,wk)) {
		mem->dwTotalPhys = ((unsigned int)atoi(wk)) * 1024;
	    }
	} else if (strstr(buf,freem)) {
	    if (get_item(buf," ",10,wk)) {
		mem->dwAvailPhys = ((unsigned int)atoi(wk)) * 1024;
	    }
	}
      }
      if (st = close(sysfd)) {
        printf("/etc/sysdef stdout close error. code=%d",st);
      }
    }

    /* 
     *  スワップ情報取得 (/etc/swapinfo) 
     */
    if ((sysfd = ExecOpenStdout("/etc/swapinfo" , &st)) < 0) {
        /* /etc/swapinfo 起動失敗 */
        printf("/etc/swapinfo execute error. code=%d",st);
	return;
    } else {
      /* /etc/swapinfo 起動成功 */
      while(ReadLine(buf,sizeof(buf),sysfd) >= 0) {
	if (!strncmp(buf,"dev",        strlen("dev"))      ||
	        !strncmp(buf,"fs",     strlen("fs"))       ||
	        !strncmp(buf,"localfs",strlen("localfs"))  ||
	        !strncmp(buf,"network",strlen("network"))  ) {
	    if (get_item(buf," ",2,wk)) {
		/* AVAIL */
		mem->dwTotalPageFile += ((unsigned int)atoi(wk)) * 1024;
	    }
	    if (get_item(buf," ",4,wk)) {
		/* FREE  */
		mem->dwAvailPageFile += ((unsigned int)atoi(wk)) * 1024;
	    }
	}
      }
      if (st = close(sysfd)) {
        printf("/etc/swapinfo stdout close error. code=%d", st);
      }
    }

    return;
}
/* 
 *   ExecOpenStdout(path)					A3UAD001
 *   
 *       pathで指定されたコマンドを実行し、そのコマンドの標準出力の
 *       ファイルディスクリプタを返す。
 *   
 *   IN  path : 実行するコマンドのフルパス
 *   OUT ret  : 成功 : 実行したコマンドの標準出力のファイルディスクリプタ。
 *              失敗 : -1
 *       err  : 失敗時、errnoが設定される
 * 
 *   (注) ExecOpenStdout()が成功した場合、呼び出し側で返された
 *        ファイルディスクリプタをclose()すること。
 */
int ExecOpenStdout(char *path , int *err)
{
    int pfd1[2];   /* [0]:for read  [1]:for write */
    int pid;

    /* パイプ作成 */
    if (pipe(pfd1) < 0) {
	*err = errno;
	return -1;	/* pipe create error */
    }

    if (!(pid = fork())) {
	/* 子プロセス(実行コマンド側) */
	close(1);       /* stdout を閉じる                        */
	dup(pfd1[1]);   /* コマンドのstdoutを pfd1[1]に割り当てる */
	close(pfd1[0]); /* 読み込み用のpfd1[0]を閉じる            */
	execlp(path, path,(char *)0); /* コマンド実行(引数なし)   */
	/* not reached */
    } else if (pid < (pid_t)0) {
	*err = errno;
	return -1; 	/* fork error */
    }

    /* 親プロセス(呼び出し側) */
    close(pfd1[1]);		/* 書き込み用のpfd1[0]を閉じる   */

    return pfd1[0];		/* ファイルディスクリプタを返す  */
}

/* 
 *   ReadLine(buf, size, fd)					A3UAD001
 *   
 *       ファイルディスクリプタfdから一行分の文字列を取出す。
 *       fgets()のファイルディスクリプタ版
 *       最大size-1だけ読み込む
 *   
 *   IN  size : 文字列格納領域の最大サイズ
 *       fd   : 読み込み対象ファイルディスクリプタ
 *   OUT buf  : 一行分の文字列
 *       ret  : 読み込んだサイズ
 *              失敗(EOF) : -1
 * 
 *   (注) ExecOpenStdout()が成功した場合、呼び出し側で返された
 *        ファイルディスクリプタをclose()すること。
 */
int ReadLine(char *buf, int size, int fd)
{
    int cnt = 0;
    int len = 0;

    while (len = read(fd,&buf[cnt],1)) {	/* 1バイトづつ読む */
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
 char *pt;
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
#endif  /* HIUX */
