/*
 *   du : �f�B���N�g���̒P�ʂ̃f�B�X�N�g�p�ʂ̕\��
 *
 *   �\���`��
 *     �u���b�N�x�[�X�f�B�X�N�g�p��KB [(�t�@�C���T�C�Y�x�[�X�g�p��KB)] Dir��
 *
 *            97/09/27 V1.00 by oga.
 *            00/06/12 V1.10 -o support
 *            00/07/17 V1.11 continue not when not accessible dir
 *            02/10/02 V1.12 -d (depth) support
 *            05/08/31 V1.13 support large size dir
 *            08/06/15 V1.14 support over 4gb file
 *            19/09/23 V1.15 skip access denied error, and expand size format
 *
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/* macros */
#define VER "1.15"
#define	IS_DOT(str)	(!strcmp(str,".") || !strcmp(str,".."))

#define dprintf		if (vf) printf
#define dprintf2	if (vf >= 2) printf

/* funcs */
void GetBlkSize(char *path, int *blkp);
void Du(char *path);

/* globals */
int df     = 0;			/* -d �w��t���O              */
int dlevel = 2;			/* -d �f�B���N�g���̐[��      */
int sf     = 0;			/* -s �w��t���O              */
int depth  = 0;			/* -s �p�f�B���N�g���[��      */
int af     = 0;			/* -a �w��t���O              */
int of     = 0;			/* -o �w��t���O              */
int vf     = 0;			/* -v �w��t���O              */
int blk;			/* �u���b�N�T�C�Y(byte)       */
unsigned int total  = 0;        /* file�T�C�Y�Ōv�Z�����T�C�Y */
unsigned int totalb = 0;	/* block�P�ʂŌv�Z�����T�C�Y  */

typedef struct u_long64 {
    u_int high;
    u_int low;
} u_long64;

u_long64 total64;       /* file�T�C�Y�Ōv�Z�����T�C�Y(64bit) */
u_long64 totalb64;      /* block�P�ʂŌv�Z�����T�C�Y(64bit)  */

/* x(64) <= x(64) + y(32) */
void add64(u_long64 *x, u_int y)
{
    u_int sv;

    sv = x->low;
    x->low += y;
    if (x->low < sv) {
       /* low���������Ȃ����猅�オ�蔭�� */
       ++(x->high);
    }
}

/* V1.14-A start */
/* ���32�r�b�g�ɒl�𑫂�      */
/* x(64) <= x(64) + y(32)*2^32 */
void add64high(u_long64 *x, u_int y)
{
    x->high += y;
}
/* V1.14-A end   */

/* x(64) <= x(64) - y(64) (x > y�ł��邱��) */
void sub64(u_long64 *x, u_long64 *y)
{
    if (x->low < y->low) {
       /* y.low���傫����Ό������蔭�� */
       --x->high;
    }
    x->low  = x->low  - y->low;
    x->high = x->high - y->high;
}

/*
 *   x ��0x3ff ffffffff �܂ł̂ݑΉ�(����u_int�Ɏ��܂�͈͂܂�)
 *
 */
void dev1024(u_long64 *x, u_int *ans)
{
    /* 1024�Ŋ���ꍇ */
    u_long64 wk;

    wk.high = x->high;
    wk.low  = x->low;

    wk.low  = wk.low  >> 10;
    wk.high = wk.high << 22;
    *ans = wk.high | wk.low;
}

void Usage()
{
    printf("du Verson %s  by oga.\n",VER);
    printf("usage: du [-a] [-s] [-d <depth>] [-o <size(MB)>] [dirnames]\n");
    printf("   -a: �t�@�C���T�C�Y�̍��v���\��\n");
    printf("   -s: �J�����g�f�B���N�g���̏��̂ݏo��\n");
    printf("   -d: �w�肵���[���܂ŏo��(1�ȏ�)\n");
    printf("   -o: �w��T�C�Y�ȏ�̏��̂ݏo��\n");
}

int main(a,b)
int a;
char *b[];
{
    char *fname[1024];
    int  i;
    int  fn = 0;

    memset(&total64,  0, sizeof(u_long64));
    memset(&totalb64, 0, sizeof(u_long64));

    for (i=1;i<a;i++) {
	if (!strncmp(b[i],"-a",2)) {
	    af = 1;
	    continue;
	}
	if (!strncmp(b[i],"-s",2)) {
	    sf = 1;
	    continue;
	}
	if (!strncmp(b[i],"-v",2)) {
	    ++vf;
	    continue;
	}
	if (!strcmp(b[i],"-d")) {   /* V1.12-A */
            if (++i < a) {
	        df = 1;
	        dlevel = atoi(b[i]);
            } else {
	        Usage();
		exit(1);
	    }
            if (dlevel < 1) {
	        Usage();
		exit(1);
	    }
	    continue;
	}
	if (!strcmp(b[i],"-o")) {
            if (++i < a) {
	        of = atoi(b[i]);
            } else {
	        Usage();
		exit(1);
	    }
	    continue;
	}
	if (!strncmp(b[i],"-",1)) {
	    Usage();
	    exit(1);
	}
	fname[fn++] = b[i];
    }
    if (!fn) {
        fname[fn++] = "";	/* �f�t�H���g�̓J�����g�f�B���N�g�� */
    }

    for (i = 0; i<fn; i++) {
        GetBlkSize(fname[i],&blk);
        printf("%d bytes per cluster.\n",blk);
        Du(fname[i]);
    }
    return 0;
}

/*
 *  GetBlkSize() : �w��p�X�̃N���X�^�T�C�Y�����߂�
 *
 *  IN  path  : �p�X��
 *
 *  OUT *blkp : �N���X�^�T�C�Y(byte)
 *              �p�X�����s���ȏꍇ��1��Ԃ�
 */
void GetBlkSize(char *path,int *blkp)
{
    int sect,bytes,free,totalsz;
    char wk[MAX_PATH];
    char *pp = wk;

    strcpy(wk,path);
    if (wk[1] == ':') {
        wk[2] = '/';
        wk[3] = '\0';
    } else {
        pp = NULL;		/* current drive */
    }
    if (GetDiskFreeSpace(pp,&sect,&bytes,&free,&totalsz) != TRUE) {
	printf("%s is unavailable\n",path);
        *blkp = 1;
        return;
    }
    *blkp = sect * bytes;		/* calc bytes/Cluster */
}

/*
 *  Du() : �w��p�X�ȉ��̃t�@�C���T�C�Y���v�����߂�
 *
 *  IN  path : �p�X��
 *
 *  OUT sz   : �t�@�C���T�C�Y���v
 *      szb  : �u���b�N�x�[�X�̃t�@�C���T�C�Y���v
 *
 */
void Du(char *path)
{
    HANDLE fh;
    WIN32_FIND_DATA wfd;
    unsigned int  subblk;
    char   wk[MAX_PATH];
    unsigned int    tsv;
    unsigned int    tsvb;

    u_long64        tsv64;   /* V1.13-A */
    u_long64        tsvb64;  /* V1.13-A */
    u_long64        work64;  /* V1.13-A */
    unsigned int    ans;     /* V1.13-A */

    int    i;
    int    ast = 0;	/* 1:�A�X�^���X�N�t�� */
    
    depth++;
    tsv    = total;
    tsvb   = totalb;

    memcpy(&tsv64,  &total64,  sizeof(u_long64));  /* V1.13-A */
    memcpy(&tsvb64, &totalb64, sizeof(u_long64));  /* V1.13-A */

    strcpy(wk,path);	/* copy path */
    if (strlen(path) && path[strlen(wk)-1] != '/') {
        if (path[strlen(path)-1] == '*') {
            ast = 1;
            for (i = strlen(path)-1; i >= 0; i--) {
		if (path[i] == '/' || path[i] == '\\') {
                    if (i) path[i] = '\0';
		    break;
		}
                path[i] = '\0';
	    }
            dprintf("xxx path=%s\n",path);
            /* wk   = "/dir*"   */
            /* path = ""        */
        } else {
            strcat(wk,"/*");	/* add /*     */
            /* wk   = "dir/*"   */
        }
    } else {
         strcat(wk,"*");	/* add *      */
         /* wk   = "*"   */
    }

    dprintf("Du: Enter path=[%s] search=[%s]\n",path,wk);
    
    fh = FindFirstFile(wk,&wfd);
    if (fh == INVALID_HANDLE_VALUE) {
	int lastst;
	lastst = GetLastError();
	/* V1.15-C start */
        if (vf >= 2 || lastst != ERROR_ACCESS_DENIED) {
	    printf("du: FindFirstFile(%s) Error code=%d\n",wk,lastst);
	}
	/* V1.15-C end   */
        /* exit(1); */
	depth--; /* V1.11-A */
	return;  /* V1.11-C */
    }
    if (!IS_DOT(wfd.cFileName)) {
        total  += wfd.nFileSizeLow;
        totalb += (((wfd.nFileSizeLow + blk - 1)/blk)* blk);

        add64(&total64, wfd.nFileSizeLow);                             /*V1.13-A*/
        add64high(&total64, wfd.nFileSizeHigh);                        /*V1.14-A*/
        add64(&totalb64,(((wfd.nFileSizeLow + blk - 1)/blk)* blk));    /*V1.13-A*/
        add64high(&totalb64, wfd.nFileSizeHigh);                       /*V1.14-A*/

        dprintf2("First: file = %-16s  size hi=%u lo=%u (hi=%u lo=%u)\n",wfd.cFileName,
		wfd.nFileSizeHigh, wfd.nFileSizeLow,
                wfd.nFileSizeHigh, ((wfd.nFileSizeLow + blk - 1)/blk)* blk);
	if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            dprintf("[%s] is DIR\n",wfd.cFileName);
            strcpy(wk,path);            /* restore base path */
	    if (strlen(wk) && wk[strlen(wk)-1] != '/') strcat(wk,"/");
            strcat(wk,wfd.cFileName);	/* make new path     */
	    Du(wk);
	}
    }
    while(FindNextFile(fh,&wfd)) {
        if (IS_DOT(wfd.cFileName)) {
            continue;
        }
        total += wfd.nFileSizeLow;
        totalb += (((wfd.nFileSizeLow + blk - 1)/blk)* blk);

        add64(&total64, wfd.nFileSizeLow);                             /*V1.13-A*/
        add64high(&total64, wfd.nFileSizeHigh);                        /*V1.14-A*/
        add64(&totalb64,(((wfd.nFileSizeLow + blk - 1)/blk)* blk));    /*V1.13-A*/
        add64high(&totalb64, wfd.nFileSizeHigh);                       /*V1.14-A*/

        dprintf2("First: file = %-16s  size hi=%u lo=%u (hi=%u lo=%u)\n",wfd.cFileName,
		wfd.nFileSizeHigh, wfd.nFileSizeLow,
                wfd.nFileSizeHigh, ((wfd.nFileSizeLow + blk - 1)/blk)* blk);
	if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            dprintf("[%s] is DIR\n",wfd.cFileName);
            strcpy(wk,path);            /* restore base path */
	    if (strlen(wk) && wk[strlen(wk)-1] != '/') strcat(wk,"/");
            strcat(wk,wfd.cFileName);	/* make new path     */
	    Du(wk);
	}
    }
    FindClose(fh);
    depth--;
    if (sf && depth > 1) {
        /* -s �w��Ő[��2�ȏ�Ȃ�\�����Ȃ� */
        return;
    }
    /* V1.12-A start */
    if (df && depth > dlevel) {
        /* -d �w��̐[���𒴂������͕̂\�����Ȃ� */
        return;
    }
    /* V1.12-A end   */
    if (!ast) {
        memcpy(&work64, &totalb64, sizeof(u_long64));    /* V1.13-A */
        sub64(&work64, &tsvb64);                         /* V1.13-A */
        dev1024(&work64, &ans);                          /* V1.13-A */
        /* if (!of || ((totalb-tsvb)/1024/1024) >= of) { }*/
        if (!of || (ans/1024) >= of) {                   /* V1.13-C */
            if (df) {
                printf("%2d: ", depth);
            }
#ifdef OLD
            printf("%7d",(totalb-tsvb)/1024);
#else
            printf("%9d", ans);                          /* V1.13-C V1.15-C */
#endif
            if (af) {
#ifdef OLD
                printf(" (%7d)",(total-tsv)/1024);
#else
                memcpy(&work64, &total64, sizeof(u_long64));     /* V1.13-A */
                sub64(&work64, &tsv64);                          /* V1.13-A */
                dev1024(&work64, &ans);                          /* V1.13-A */
                printf(" (%9d)", ans);                           /* V1.13-C V1.15-C */
#endif
            }
            printf(" %s\n",path);
	}
    }

} /* End Du() */

// vim:ts=8 
