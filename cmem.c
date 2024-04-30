/*
 *  cmem : Display Memory Info (for console)
 *
 *    2001/12/09 V1.00 based xmem V0.33 by oga.
 *    2005/09/11 V1.01 support Linux 2.6.x
 *    2022/05/22 V1.02 add description to usage
 *    2024/02/05 V1.03 support ext meminfo file
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>

#define VER	"Ver 1.03"
#define S_MEM	"Memory"
#define S_SWAP	"Swap"

/* constants */
#define OFFS	8		/* string area   */

#define dprintf if (vf) printf

/* globals */
int	vf = 0;		             /* verbose flag */
char    *meminfo = "/proc/meminfo";  /* meminfo file V1.03-A */

#ifdef COMMENT
   (MB)0        10        20        30        40        50        60        70
Mem  82|######***|*******==|=========|=========|===......|.........|.........|
Swp  50|###......|.........|.........|.........|
Lod 1.2|###......|.........|.........|.........|
#endif /* COMMENT */

unsigned long MyColor();
void get_memdata();

#ifdef HIUX
typedef struct _MEMORYSTATUS {
    unsigned int dwLength;		/* not used on UNIX */
    unsigned int dwMemoryLoad;		/* not used on UNIX */
    unsigned int dwTotalPhys;		/* �S  ����������   */
    unsigned int dwAvailPhys;		/* �󂫕���������   */
    unsigned int dwTotalPageFile;	/* �S  �X���b�v�e�� */
    unsigned int dwAvailPageFile;	/* �󂫃X���b�v�e�� */
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
    dprintf("start:%d end:%d\n", spoint, length);

    for (i = spoint; i<=spoint + length; i++) {
        buf[i] = chr;
    }
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
	dprintf("Memory : %d %d %d %d %d %d\n",mem->total,mem->used,
			mem->free,mem->shared,mem->buf,mem->cache);
	dprintf("Swap   : %d %d %d\n",mem->swap_total,mem->swap_used,mem->swap_free);
#endif
}

#else /* Linux */
/*
 *  get load average info
 *
 *  OUT : loadave (loadave * 100)
 *  OUT : ret     (0:success  -1:error)
 */
void get_loadave(int *loadave)
{
    FILE *fp;
    char buf[1024];
    char wk[1024];

#ifdef HIUX
    *loadave = -99;          /* not support */    
#endif

    if (!(fp = fopen("/proc/loadavg","r"))) {
	perror("fopen /proc/loadavg");
	*loadave = -1;       /* error */
	return;
    }
    fscanf(fp, "%s", buf);   /* get loadave(1min) */
    fclose(fp);

    /* 1.23 => 123 */
    buf[1] = buf[2];
    buf[2] = buf[3];
    buf[3] = '\0';
    *loadave = atoi(buf);

    dprintf("LoadAve. : %d\n", *loadave);

    return;
}

/* 
 *  get memory information from /proc/meminfo for Linux
 * 
 *  unit is KB
 */
void get_memdata(mem_t *mem)
{
    FILE *fp;
    char buf[1024];
    char *pt;

    if (!(fp = fopen(meminfo, "r"))) {      /* V1.03-C */
    	sprintf(buf, "fopen %s", meminfo);  /* V1.03-C */
        perror(buf);                        /* V1.03-C */
        exit(1);
    }
    fgets(buf,sizeof(buf),fp);	/* skip header   */

    if (strstr(buf, "total:")) {
        /* Linux 2.0�`2.4 */
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
        /* Linux 2.6�` */
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

    dprintf("Mem :t%d u%d f%d s%d b%d c%d\n",mem->total,mem->used,
		mem->free,mem->shared,mem->buf,mem->cache);
    dprintf("Swap:%d %d %d\n",mem->swap_total,mem->swap_used,mem->swap_free);
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
    char *phys  = "physical memory is";	/* ����������      */
    char *freem = "free memory";	/* �󂫃�����      */
    char buf[2048];
    char wk[2048];

    memset(mem,0,sizeof(MEMORYSTATUS));

    /* 
     *  ���������擾 (/etc/sysdef) 
     */
    if ((sysfd = ExecOpenStdout("/etc/sysdef" , &st)) < 0) {
        /* /etc/sysdef �N�����s */
        printf("/etc/sysdef execute error. code=%d",st);
    } else {
      /* /etc/sysdef �N������ */
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
     *  �X���b�v���擾 (/etc/swapinfo) 
     */
    if ((sysfd = ExecOpenStdout("/etc/swapinfo" , &st)) < 0) {
        /* /etc/swapinfo �N�����s */
        printf("/etc/swapinfo execute error. code=%d",st);
	return;
    } else {
      /* /etc/swapinfo �N������ */
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
 char *pt;
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
#endif  /* HIUX */


int main(int a, char *b[])
{
	unsigned long black, cuse, cbuf, cfree, cscale, ccache, cstr;
	int i;
	int x_home = 10, y_home = 10;
	int x0, y0;
	int xbuf, xused, xfree, xcache, xtotal;
	int xsused, xsfree, xstotal;
#ifdef HIUX
	int wait = 1;		/* 5sec */
#else  /* HIUX */
	int wait = 1;		/* 1sec */
#endif /* HIUX */
	char	name[128],wk[128];
	char	*pt;
	mem_t	mem;            /* memory info  */
	int     loadave;        /* load average */
	int	UNIT = 0;	/* 1 scale      */
	int     maxval;
	char	buf[1024];

	for (i = 1; i<a ; i++) {
		if (!strncmp(b[i],"-h",2)) {
			printf("usage : cmem [-update <time>] [-memfile <meminfo_file>]\n");
			printf("        # : used\n");
			printf("        * : cached\n");
			printf("        = : buffers\n");
			printf("        . : free\n");
			exit(1);
		}
		/* V1.03-A start */
		if (!strncmp(b[i],"-m",2) && a > i) {
			meminfo = b[++i];		/* meminfo file*/
		}
		/* V1.03-A end   */
		if (!strncmp(b[i],"-u",2) && a > i) {
			wait = atoi(b[++i]);		/* update time */
		}
		if (!strncmp(b[i],"-s",2) && a > i) {
			UNIT = atoi(b[++i]);		/* scale width */
		}
		if (!strncmp(b[i],"-v",2) && a > i) {
			vf = 1;				/* verbose */
		}
	}

	dprintf("start get_memdata()\n");
	get_memdata(&mem);
	dprintf("start get_loadave()\n");
	get_loadave(&loadave);

	maxval = (mem.total > mem.swap_total)? mem.total : mem.swap_total;

	/* calc UNIT (1,2,5,10,20,50,100,200,500,...) */
	if (!UNIT) {
	    i = 1;
	    while (1) {
	        if (maxval/1024 < i*7) {
		    UNIT = i;
		    break;
		} else if (maxval/1024 < i*7*2) {
		    UNIT = i*2;
		    break;
		} else if (maxval/1024 < i*7*5) {
		    UNIT = i*5;
		    break;
		} 
		i = i*10;
	    }
	}
	dprintf("max(mem,swap)=%d, UNIT=%d\n", maxval, UNIT);

        if (loadave >= 0) {
	    /* support loadave */
	    printf("\n\n\n\n");
	} else {
	    /* not support loadave */
	    printf("\n\n\n");
	}

	while (1){
		get_memdata(&mem);
		get_loadave(&loadave);

		xbuf   = mem.buf*10/1024/UNIT;
		xcache = mem.cache*10/1024/UNIT;
		xused  = (mem.used-mem.buf-mem.cache)*10/1024/UNIT;
		xfree  = mem.free*10/1024/UNIT;
		xtotal = mem.total*10/1024/UNIT;

		xsused = mem.swap_used*10/1024/UNIT;
		xstotal= mem.swap_total*10/1024/UNIT;

		dprintf("total:%d used:%d cache:%d buf:%d free:%d\n",
		         xtotal, xused, xcache, xbuf, xfree);

                if (loadave >= 0) {
                    /* cursor 4 up ("<ESC>M" cursor 1up) */
	            if (!vf) printf("%cM%cM%cM%cM",27,27,27,27);
		} else {
                    /* cursor 3 up ("<ESC>M" cursor 1up) */
	            if (!vf) printf("%cM%cM%cM",27,27,27);
		}

                /* disp scale1    */
		printf("    (MB)0 ");
		for (i = 1; i<8; i++) {
		    if (UNIT >= 20 && i == 7) {
		        /* max 79char */
		        printf("      %-4d", i*UNIT);
		    } else {
		        printf("      %-4d", i*UNIT);
		    }
		}
		printf("\n");


		/* disp mem data  */
	        memset(buf, 0, sizeof(buf));
		CFill(buf, ' ', 0, 70);
		CFill(buf, '.', 0, xtotal);             /* total   */
		CFill(buf, '#', 0, xused);              /* used    */
		CFill(buf, '*', xused+1, xcache);       /* cached  */
		CFill(buf, '=', xused+xcache+1, xbuf);  /* buffers */
		for (i = 0; i<8; i++) {
		    buf[i*10] = '|';
		}
		printf("Mem%5d%s\n", mem.used/1024, buf);

		/* disp swap data  */
	        memset(buf, 0, sizeof(buf));
		CFill(buf, ' ', 0, 70);
		CFill(buf, '.', 0, xstotal);
		CFill(buf, '#', 0, xsused);
		for (i = 0; i<8; i++) {
		    buf[i*10] = '|';
		}
		printf("Swp%5d%s\n", mem.swap_used/1024, buf);

		/* disp loadave data  */
	        memset(buf, 0, sizeof(buf));
		CFill(buf, ' ', 0, 70);
		CFill(buf, '#', 0, loadave/10);
		for (i = 0; i<(loadave/100)+2; i++) {
		    buf[i*10] = '|';
		}
		printf("Load%4.1f%s\n", ((float)loadave)/100, buf);

		sleep(wait);
	}
}
