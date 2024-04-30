/*
 *   find2 : �t�@�C��/�f�B���N�g���̌��� (���{��Ή���)
 *           for WIN Only
 *
 *     2003/05/16 V0.10 by oga.
 *     2003/05/21 V0.11 fix find2 c:\ => c:\/xxx
 *     2009/04/04 V0.12 support -mtime
 *     2013/12/12 V0.13 support X68K & Linux
 *
 *   CFLAG
 *     Linux: -DX68K -DLINUX
 *
 *
 */
#ifdef _WIN32
#include <windows.h>
#else  /* X68K, LINUX */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#endif /* X68K, LINUX */
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

/* macros */
#define VER "0.13"
#define	IS_DOT(str)	(!strcmp(str,".") || !strcmp(str,".."))

#define dprintf		if (vf) printf
#define dprintf2	if (vf >= 2) printf

/* defines */
#define F_TYPE_DIR  0x01
#define F_TYPE_FILE 0x02

/* globals */
char *find_name = NULL;  /* -name  */
int  ftype = 0;          /* -ftype */
int  mtime = -1;         /* -mtime */
int  vf    = 0;          /* -v     */
int  depth = 0;
time_t cur_tt = 0;       /* current time_t  V0.12-A */

#ifdef LINUX
#define stricmp  strcasecmp
#endif

#if defined X68K || defined LINUX
int  errno2 = 0;

#define INVALID_HANDLE_VALUE  NULL
#define MAX_PATH              PATH_MAX
#define TRUE   1
#define FALSE  0

/* #define PDELM  "\\" */

typedef int  DWORD;
typedef int  BOOL;
typedef char CHAR;

typedef struct _DIR2 {
	DIR  *dirp;
	char path[MAX_PATH];
} DIR2, *HANDLE;

typedef struct _WIN32_FIND_DATA {
	DWORD       dwFileAttributes;
	DWORD       nFileSizeHigh;
	DWORD       nFileSizeLow;
	CHAR        cFileName[ MAX_PATH ];

	/* FILETIME    ftCreationTime;   */
	/* FILETIME    ftLastAccessTime; */
	/* FILETIME    ftLastWriteTime;  */
	/* DWORD       dwReserved0;      */
	/* DWORD       dwReserved1;      */
	/* CHAR        cAlternateFileName[ 16 ]; */
} WIN32_FIND_DATA;

/* available attr */
#define FILE_ATTRIBUTE_DIRECTORY     0x00000010  /* �f�B���N�g��     */
#define FILE_ATTRIBUTE_NORMAL        0x00000080  /* �ʏ�t�@�C��     */

/* not support attr */
#define FILE_ATTRIBUTE_READONLY      0x00000001  /* �ǂݎ���p���� */
#define FILE_ATTRIBUTE_HIDDEN        0x00000002  /* �B���t�@�C������ */
#define FILE_ATTRIBUTE_ARCHIVE 	     0x00000020  /* �A�[�J�C�u����   */
#define FILE_ATTRIBUTE_REPARSE_POINT 0x00000400  /* �֘A���p�[�X�|�C���g������ */
#define FILE_ATTRIBUTE_COMPRESSED    0x00000800  /* ���k�����B       */
#define FILE_ATTRIBUTE_ENCRYPTED     0x00004000  /* �Í��������B     */
#define FILE_ATTRIBUTE_OFFLINE       0x00001000  /* ���p�\�ł͂Ȃ� */

/* FindNextFile error reason */
#define ERROR_NO_MORE_FILES ENOENT;


HANDLE FindFirstFile(char *path, WIN32_FIND_DATA *pwfd)
{
	DIR    *dirp;
	DIR2   *dir2p;        /* DIR with path */
	struct dirent *dp;
	struct stat   stbuf;
	char   path2[MAX_PATH];

	/* delete wild card */
	strcpy(path2, path);
	if (path2[strlen(path2)-1] == '*') {
		path2[strlen(path2)-1] = '\0';
	}
	if (path2[strlen(path2)-1] == '/') {
		if (strlen(path2) >= 2 && path2[strlen(path2)-2] == ':') {
			/* if "a:/" ... don't delete "/" */
		} else {
			/* delete "/" */
			path2[strlen(path2)-1] = '\0';
		}
	}

	memset(pwfd, 0, sizeof(WIN32_FIND_DATA));

	if (vf) printf("opendir = [%s]\n", path2);
	dirp = opendir(path2);
	if (!dirp) {
		errno2 = errno;
		perror("opendir");
		printf("path = [%s]\n", path);
		return INVALID_HANDLE_VALUE;
	}

	dir2p = (DIR2 *)malloc(sizeof(DIR2));
	if (dir2p == NULL) {
		errno2 = errno;
		perror("malloc");
		return INVALID_HANDLE_VALUE;
	}

	dir2p->dirp = dirp;
	strcpy(dir2p->path, path2);

	if (FindNextFile(dir2p, pwfd) == FALSE) {
		/* EOF or Error */
		return dir2p;
	}
	return dir2p;
}

BOOL FindNextFile(HANDLE dir2p, WIN32_FIND_DATA *pwfd)
{
	struct dirent *dp;
	struct stat   stbuf;
	char   wkpath[MAX_PATH];

	if (vf) printf("FindNextFile: start readdir(%s)\n", dir2p->path);  /* debug */

	memset(pwfd, 0, sizeof(WIN32_FIND_DATA));

	if (vf) printf("FindNextFile: ----\n");  /* debug */

	dp = readdir(dir2p->dirp);
	if (!dp) {
		/* end of entry */
		errno2 = ERROR_NO_MORE_FILES;
		return FALSE;
	}
	strcpy(pwfd->cFileName, dp->d_name);
	strcpy(wkpath, dir2p->path);
	strcat(wkpath, "/");
	strcat(wkpath, dp->d_name);
	if (vf) printf("FindNextFile: start stat(%s)\n", wkpath);  /* debug */
	if (stat(wkpath, &stbuf) == 0) {
		if (stbuf.st_mode & S_IFDIR) {
			pwfd->dwFileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
			if (vf >= 2) printf("# %s is dir. 0x%08x\n", wkpath, pwfd->dwFileAttributes);
		} else {
			/* �Ƃ肠�����f�B���N�g���ȊO��NORMAL�ɂ����� */
			pwfd->dwFileAttributes |= FILE_ATTRIBUTE_NORMAL;
			if (vf >= 2) printf("# %s is file. 0x%08x\n", wkpath, pwfd->dwFileAttributes);
		}
		pwfd->nFileSizeHigh = 0;
		pwfd->nFileSizeLow  = stbuf.st_size;
	}
	return TRUE;
}

BOOL FindClose(HANDLE dir2p)
{
	closedir(dir2p->dirp);   /* void */
	free(dir2p);

	return TRUE;
}

int GetLastError()
{
	return errno2;
}

#endif /* X68K || LINUX */

void Usage()
{
    printf("find Verson %s  by oga.\n",VER);
    printf("usage: find [<path>] [<opt>]\n");
    printf("  <opt>\n");
    printf("    -type {d|f}   : dir or file only\n");
    printf("    -name <name>  : name match (ignore case)\n");
    printf("    -mtime <n>    : file's data was last modified n*24 hours ago.\n");
    /*printf("    -mmin <n>     : file's data was last modified n*24 hours ago.\n"); */
}

/*
 *  CheckMtime() : �w��p�X��mtime���`�F�b�N����
 *
 *  IN  path: mtime�`�F�b�N�Ώۃt�@�C��/�f�B���N�g���p�X
 *  OUT ret:  1: mtime �Ώ�  0: mtime��Ώ�
 *
 */
int CheckMtime(char *path, int mtime)
{
    struct stat stbuf;

    if (stat(path, &stbuf) < 0) {
        printf("find2: stat(%s) Error errno=%d\n",path, errno);
	return 0; /* �G���[���͔�ΏۂƂ��� */
    }
    dprintf("CheckMtime: %u <= %u < %u %s\n", 
	    cur_tt + mtime*3600*24,
	    stbuf.st_mtime,
	    cur_tt + (mtime + 1)*3600*24,
	    path);
    if (cur_tt + (mtime - 1)*3600*24 < stbuf.st_mtime
	&& stbuf.st_mtime <= cur_tt + mtime*3600*24 ) {
	return 1;
    }
    return 0;
}

/*
 *  Find() : �w��p�X�ȉ��̃t�@�C������������
 *
 *  IN  path : �������[�g�p�X��
 *
 *  OUT sz   : �t�@�C���T�C�Y���v
 *      szb  : �u���b�N�x�[�X�̃t�@�C���T�C�Y���v
 *
 */
void Find(char *path)
{
    HANDLE fh;
    WIN32_FIND_DATA wfd;
    unsigned int  subblk;
    char   wk[MAX_PATH];
    char   fullpath[MAX_PATH];   /* V0.12-A */
    int    tsv;
    int    tsvb;
    int    i;
    int    ast = 0;	/* 1:�A�X�^���X�N�t�� */
    int    name_match = 0;
    int    dispf = 1;   /* 1: �\���Ώ�        V0.12-A */
    char   *pt;
    
    depth++;
    if (vf) printf("[%d]Find start.\n", depth);  /* debug */

    /* �f�B���N�g���\�� */
    if ((ftype & F_TYPE_FILE) == 0) {
	/* V0.12-A start -mtime */
	if (mtime >= 0) {
	    if (CheckMtime(path, mtime) == 0) {
		dispf = 0;
	    }
	}
	/* V0.12-A end   -mtime */

        /* -type f�łȂ��ꍇ���Ώ� */
        pt = strrchr(path, '/');
        if (pt) ++pt;  /* pt = basename <path> */
        if (find_name) {
            /* -name�w�肪�����āA�f�B���N�g��������v������\�� */
            if ( pt && !stricmp(pt, find_name)) {
	        if (dispf) printf("%s\n",path);  /* disp dir */
	    }
        } else {
            /* -name�w�肪�Ȃ��ꍇ�͑S���\�� */
	    if (dispf) printf("%s\n",path);  /* disp dir */
        }
    }

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

    dprintf("find2: Enter path=[%s] search=[%s]\n",path,wk);
    
    /* first dir entry */
    fh = FindFirstFile(wk,&wfd);
    if (fh == INVALID_HANDLE_VALUE) {
        printf("find2: FindFirstFile(%s) Error code=%d\n",wk,GetLastError());
        /* exit(1); */
	depth--; /* V1.11-A */
	return;  /* V1.11-C */
    }
    if (!IS_DOT(wfd.cFileName)) {
        name_match = 1;
        if (find_name && stricmp(wfd.cFileName, find_name)) {
	    /* -name�w�肪�����āA���O����v���Ȃ��ꍇ�͕\�����Ȃ� */
	    name_match = 0;
	}

	if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            dprintf("[%s] is DIR\n",wfd.cFileName);
            strcpy(wk,path);            /* restore base path */
	    if (strlen(wk) && wk[strlen(wk)-1] != '/') strcat(wk,"/");
            strcat(wk,wfd.cFileName);	/* make new path     */
	    Find(wk);
	} else {
	    /* �t�@�C���̏ꍇ�����\��(-type d�w��̂Ȃ��ꍇ) */
            if ((ftype & F_TYPE_DIR) == 0) {
	        if (name_match && dispf) printf("%s/%s\n",path, wfd.cFileName);  /* disp file, dir */
	    }
        }
    }
    
    /* since the 2nd dir entry */
    while(FindNextFile(fh,&wfd)) {
        if (IS_DOT(wfd.cFileName)) {
            continue;
        }
	/* V0.12-A start -mtime */
	dispf = 1;
	if (mtime >= 0) {
	    sprintf(fullpath, "%s/%s", path, wfd.cFileName);
	    if (CheckMtime(fullpath, mtime) == 0) {
		dispf = 0;
	    }
	}
	/* V0.12-A end   -mtime */

        name_match = 1;
        if (find_name && stricmp(wfd.cFileName, find_name)) {
	    /* -name�w�肪�����āA���O����v���Ȃ��ꍇ�͕\�����Ȃ� */
	    name_match = 0;
	}

	if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            dprintf("[%s] is DIR\n",wfd.cFileName);
            strcpy(wk,path);            /* restore base path */
	    if (strlen(wk) && wk[strlen(wk)-1] != '/') strcat(wk,"/");
            strcat(wk,wfd.cFileName);	/* make new path     */
	    Find(wk);
    	    if (vf) printf("[%d]Find return.\n", depth);  /* debug */
	} else {
	    /* �t�@�C���̏ꍇ�����\��(-type d�w��̂Ȃ��ꍇ) */
            if ((ftype & F_TYPE_DIR) == 0) {
	        if (name_match && dispf) printf("%s/%s\n",path, wfd.cFileName);  /* disp file, dir */
	    }
        }
    }
    if (vf) printf("[%d]Find end.\n", depth);  /* debug */
    FindClose(fh);
    if (vf) printf("[%d]Find end. (FindClose end)\n", depth);  /* debug */

    depth--;

    return;

} /* End Find() */

int main(int a, char *b[])
{
    char *fname = ".";
    int  i;
    int  fn = 0;

    cur_tt = time(0);   /* get current time_t at command start  V0.12-A */

    for (i=1;i<a;i++) {
	if (!strcmp(b[i],"-type") && i+1 < a) {
	    ++i;
            if (!strcmp(b[i], "d")) {
	        ftype |= F_TYPE_DIR;
	    } else if (!strcmp(b[i], "f")) {
	        ftype |= F_TYPE_FILE;
	    } else {
	        Usage();
	        exit(1);
	    }
	    continue;
	}
	if (!strcmp(b[i],"-name") && i+1 < a) {
	    ++i;
	    find_name = b[i];
	    continue;
	}
	if (!strcmp(b[i],"-mtime") && i+1 < a) {
	    ++i;
	    mtime = atoi(b[i]);
	    continue;
	}
	if (!strcmp(b[i],"-print")) {
	    continue;             /* -print�͖��� */
	}
	if (!strncmp(b[i],"-v",2)) {
	    ++vf;
	    continue;
	}
	if (!strncmp(b[i],"-",1)) {
	    Usage();
	    exit(1);
	}
	fname = b[i];
    }

    if (strlen(fname) > 0 && fname[strlen(fname)-1] == '/') {
        fname[strlen(fname)-1] = '\0';
    }
    if (strlen(fname) > 1 && 
        ((unsigned char)fname[strlen(fname)-2]) < 0x80 &&
        fname[strlen(fname)-1] == '\\') {
	/* ������2�o�C�g�ڂłȂ�'\'��������폜 */
        fname[strlen(fname)-1] = '\0';
    }
    Find(fname);  /* find ���s */
    return 0;
}

/* vim:ts=8:sw=4:
 */


