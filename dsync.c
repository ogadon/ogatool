/*
 *  dsync : 2 directory sync tool
 *
 *    V1.00 98/05/14  by oga
 *    V1.01 04/10/06  for Windows
 *    V1.02 04/10/10  support test mode (-t)
 *    V1.03 04/10/12  support ignore time diff(-i)
 *    V1.04 05/08/19  support -f (for read only file), copy/warn/error count
 *    V1.05 05/08/25  support -d 
 *    V1.06 05/09/04  support copy time
 *    V1.07 05/09/12  stop on change drive error
 *    V1.08 06/01/08  exclusive option -d -m
 *    V1.09 07/01/07  support -e option
 *    V1.10 07/03/14  check mkdir error
 *    V1.11 07/03/15  support UNC path for win
 *    V1.12 07/04/24  check source file open error
 *    V1.13 07/04/29  support -l (logging) option 
 *    V1.14 07/10/26  disp start time
 *    V1.15 07/11/14  output FindFirstError to log
 *    V1.15.1 07/12/20  output FindNextFile error to log
 *    V1.16 08/04/05  support -s
 *    V1.17 09/11/21  fix bug
 *    V1.18 11/03/14  support -c (Same File Check in copy) for -t(test mode)
 *    V1.19 11/12/06  change default r/w buffer size for perf and support -b
 *    V1.20 11/12/09  support -ca compare all file for -t(test mode)
 *    V1.21 14/01/15  support -nr
 *    V1.22 14/05/31  change file compare method
 *
 *  Known Bug
 *    07/04/31 Long UNC(upper 256) => app error  (WinNT stat() unsupport?)
 *
 *  [dir1]      [dir2]
 *    file1
 *    file2       file2
 *    file3       file3
 *    file4
 *                file5
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#include <sys/utime.h>
#define ISKANJI(c) ((c >= 0x80 && c < 0xa0) || (c >= 0xe0 && c<0xff))
#define mode_t unsigned short
#else  /* !_WIN32 */
#include <dirent.h>
#include <unistd.h>
#include <utime.h>
#endif  /* _WIN32 */

#define VER "1.22"

#define	dprintf   if (vf) printf
#define	dprintf2  if (vf >= 2) printf
#define	dprintf3  if (vf >= 3) printf

/* V1.09-A start */
#define MAX_IGNORE 50
#ifdef _WIN32
#define FILECMP stricmp
#else
#define FILECMP strcmp
#define Sleep(x)  sleep((x)/1000)
#endif
/* V1.09-A end   */

typedef struct _f_list {
    struct _f_list *next;
    char *fname;
} f_list_t;

#ifdef _WIN32
/*
struct timeval {
    u_int tv_sec;
    u_int tv_usec;
};
*/

struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};

int gettimeofday(struct timeval *tv, struct timezone *tz);
#endif /* _WIN32 */

int  CopyFileX(char *f1, char *f2);
int  DirCopy(char *d1, char *d2);
void WaitForStart(int hh, int mm);          /* V1.16-A */

int rf  = 1;	      /* dir recursive flag    */
int fdf = 1;	      /* force dir flag        */
int delf= 0;	      /* delete flag           */
int ff  = 0;	      /* force copy flag       */
int mf  = 0;	      /* sync mutually flag    */
int tf  = 0;          /* test mode flag        */
int cf  = 0;          /* same file check flag  V1.18-A */
int caf = 0;          /* same file check (all) V1.20-A */
int vf  = 0;          /* verbose mode          */
int stf = 0;          /* daily start time      V1.16-A */
int logff= 0;         /* log output flat       V1.13-A */
int ignore_time = 2;  /* ignore time diff(sec) */
int cnt_warn    = 0;  /* warning count         */
int cnt_error   = 0;  /* error   count         */
int cnt_copy    = 0;  /* copy    count         */
int cnt_delete  = 0;  /* delete  count         */
int ig_cnt      = 0;  /* ignore file count     V1.09-A */
int bufsize     = 100*1024;  /* buffer size    V1.19-A */

char *ig_files[MAX_IGNORE]; /* ignore files    V1.09-A */
char *log_file = NULL;   /* log file name      V1.13-A */
char *cpbuf = NULL;      /* r/w buffer         V1.19-C */
FILE *logfp    = NULL;   /* log outpt desc     V1.13-A */

void usage()
{
#if 0
    printf("usage: dsync [-r] [-f] [-i <sec>] [-m] [-v] <src_dir> <dest_dir>\n");
    printf("       -r : sync directories recursively.\n");
    printf("       -fd: make new directory forcibly.\n");
#else 
    /* -f, -r をデフォルトONとする */
    printf("dsync Ver %s\n", VER);
    printf("usage: dsync [{-d|-m}] [-f] [-nr] [-i <sec>] [-e <file_dir>] [-l <log_file>] [-s <daily_start_time(HH:MM)>] [-v] [-t [-c[a]]] <src_dir> <dest_dir>\n");
#endif
    printf("       -d : delete deleted file\n");
    printf("       -m : sync directories mutually. \n");
    printf("       -f : force copy read only file\n");
    printf("       -nr: sync no recursive\n");                /* V1.21-A */
    printf("       -i : ignore time difference (default:2(sec))\n");
    printf("       -e : ignore specific file or dir (max:%d)\n", MAX_IGNORE); /* V1.09-A */
    printf("       -s : daily start time\n");                 /* V1.16-A */
    printf("       -l : outpt log to file\n");                /* V1.13-A */
    printf("       -la: outpt log to file (append mode)\n");  /* V1.13-A */
    printf("       -b : read/write buffer size (default:%d(KB))\n", bufsize/1024);
    printf("       -t : test mode ON.\n");
    printf("       -c : compare file in copy. (test mode only)\n"); /* V1.18-A */
    printf("       -ca: compare all file. (test mode only)\n"); /* V1.18-A */
    printf("       -v : verbose mode ON.\n");
    exit(1);
}


#ifdef _WIN32
/* dirp->direntry */
struct dirent {
    char            *d_name;
    WIN32_FIND_DATA wfd;
};

/* dirp */
typedef struct _dir {
    HANDLE hdir;
    char   path[1024];     /* search condition for FindFirstFile */
    int    firstf;         /* first readdir flag                 */
    struct dirent direntry;
} DIR;


DIR *opendir(char *dir)
{
    DIR *dirp;

    dprintf3("## Start opendir\n");

    dirp = malloc(sizeof(DIR));
    if (!dirp) {
        return NULL;
    }
    memset(dirp, 0, sizeof(DIR));
    dirp->firstf = 1;
    dirp->direntry.d_name = dirp->direntry.wfd.cFileName;
    sprintf(dirp->path, "%s\\*", dir);   /* find all files */
    return dirp;
}

struct dirent *readdir(DIR *dirp)
{
    int status = 0;

    dprintf3("## Start readdir(0x%08x)\n",dirp);

    if (dirp->firstf) {
        dirp->hdir = FindFirstFile(dirp->path, &(dirp->direntry.wfd));
        if (dirp->hdir == INVALID_HANDLE_VALUE) {
            fprintf(logfp, "Error: readdir: FindFirstFile(%s) error\n", dirp->path); /* V1.15-C */
	    ++cnt_error;
            return NULL;
        }
        dirp->firstf = 0;
    } else {
        status = FindNextFile(dirp->hdir, &(dirp->direntry.wfd));
        if (status == FALSE) {
            /* V1.15.1-A start */
            if ((status = GetLastError()) != ERROR_NO_MORE_FILES) {
                fprintf(logfp, "Error: readdir: FindNextFile(%s) error %d\n",
                        dirp->path, status);
            }
            /* V1.15.1-A end   */
            return NULL;   /* EOF or Error */
        }
    }
    return &(dirp->direntry);
}

int closedir(DIR *dirp)
{
    dprintf3("## Start closedir\n");
    if (dirp) {
        FindClose(dirp->hdir);
        free(dirp);
    }
    return 0;
}
#endif /* _WIN32 */

int main(int a, char *b[])
{
    int  i;
    int  st_hh, st_mm;    /* 開始時刻         V1.16-A */
    char *dir1=NULL, *dir2=NULL;
    time_t tt;
    char datestr1[256];   /* start datetime */
    char datestr2[256];   /* end   datetime V1.14-A */

    memset(ig_files, 0, sizeof(ig_files)); /* V1.09-A */

    for (i = 1; i<a; i++) {
        if (!strncmp(b[i],"-r",2)) {
            rf = 1;
            continue;
        }
        if (!strncmp(b[i],"-nr",3)) {      /* V1.21-A */
            rf = 0;
            continue;
        }
        if (!strncmp(b[i],"-f",2)) {
            ff = 1;
            continue;
        }
        if (!strncmp(b[i],"-fd",3)) {
            fdf = 1;
            continue;
        }
        if (!strncmp(b[i],"-d",3)) {
            delf = 1;
            continue;
        }
        if (!strncmp(b[i],"-m",2)) {
            mf = 1;
            continue;
        }
        if (!strncmp(b[i],"-t",2)) {
            tf = 1;
            continue;
        }
        if (!strcmp(b[i],"-c")) {
            cf = 1;
            continue;
        }
        if (!strcmp(b[i],"-ca")) {  /* V1.19 */
            caf = 1;
            continue;
        }
        if (i+1 < a && !strncmp(b[i],"-i",2)) {
            ignore_time = atoi(b[++i]);
            continue;
        }
	/* V1.19-A start */
        if (i+1 < a && !strncmp(b[i],"-b",2)) {
            bufsize = atoi(b[++i]) * 1024;
	    if (bufsize == 0) bufsize = 4096;
	    printf("buffer size is %d KB.\n", bufsize/1024); 
            continue;
        }
	/* V1.19-A end   */
        if (i+1 < a && !strcmp(b[i],"-e")) {  /* V1.09-A start */
            ig_files[ig_cnt++] = strdup(b[++i]);
            continue;
        }                                     /* V1.09-A end   */
        /* V1.13-A start */
        if (i+1 < a && !strncmp(b[i], "-l", 2)) {
            logff = 1;
            if (!strcmp(b[i], "-la")) {
                logff = 2;  /* add mode */
            }
            log_file = b[++i];
            continue;
        }
        /* V1.13-A end   */
        /* V1.16-A start */
        if (i+1 < a && !strncmp(b[i], "-s", 2)) {
	    char *pt;
            stf = 1;
	    ++i;
            st_hh = atoi(b[i]);
	    pt = strchr(b[i], ':');
	    if (pt) {
		++pt;    /* skip ":" */
            	st_mm = atoi(pt);
	    } else {
		/* ":" なし */
		usage();
	    }
	    if (st_hh < 0 || st_hh > 23 || st_mm < 0 || st_mm > 59) {
		usage();
	    }
            continue;
        }
        /* V1.16-A end   */
        if (!strncmp(b[i],"-v",2)) {
            ++vf;
            continue;
        }
        if (!strncmp(b[i],"-h",2)) {
            usage();
        }
        if (dir1 == NULL) {
            dir1 = b[i];
            continue;
        }
        if (dir2 == NULL) {
            dir2 = b[i];
            continue;
        }
    } 

    if (!dir1 || !dir2) {
        usage();
    }
    if (delf && mf) {   /* V1.08-A */
        usage();
    }
    /* V1.13-A start */
    if (logff && log_file == NULL) {
        usage();
    }

    if (logff) {
        char *logmode = "w";

        if (logff == 2) {
            logmode = "a";
        }
	if ((logfp = fopen(log_file, logmode)) == NULL) {
	    logfp = stdout;
	}
    } else {
        logfp = stdout;
    }

    cpbuf = (char *)malloc(bufsize);
    if (cpbuf == NULL) {
        perror("malloc");
        return 1;
    }


    /* V1.16-A start */
    while(1) {
	if (stf) {
	    printf("dsync scheduled at %02d:%02d ...\n", st_hh, st_mm);
	    WaitForStart(st_hh, st_mm);
	}
	/* V1.16-A end   */
        tt = time(0);
        strftime(datestr1, sizeof(datestr1), "%Y/%m/%d %H:%M:%S", localtime(&tt));
        fprintf(logfp, "---- dsync start at %s ----\n", datestr1);
        if (logfp != stdout) printf("---- dsync start at %s ----\n", datestr1);
        /* V1.13-A end   */

        fprintf(logfp, "## Sync [%s] to [%s]  %s (ignore:%dsec)\n", dir1, dir2, tf?"(Test Mode)":"", ignore_time);

        fflush(logfp); /* V1.13-A */

        DirCopy(dir1, dir2);

        if (mf) {
    	    /* 逆方向にもsyncする */
    	    delf = 0;   /* 逆方向ではファイル削除はしない */
    	    DirCopy(dir2, dir1);
        }

        fprintf(logfp, "Result: Copy:%d  Delete:%d  Warning:%d  Error:%d  %s\n", 
    	   cnt_copy, cnt_delete, cnt_warn, cnt_error, tf?"(Test Mode)":"");
        if (logfp != stdout) {
    	    printf("Result: Copy:%d  Delete:%d  Warning:%d  Error:%d  %s\n", 
    	    cnt_copy, cnt_delete, cnt_warn, cnt_error, tf?"(Test Mode)":"");
        }

        /* V1.13-A start */
        tt = time(0);
        strftime(datestr2, sizeof(datestr2), "%Y/%m/%d %H:%M:%S", localtime(&tt));
        fprintf(logfp, "---- dsync end   at %s ----\n", datestr2);

        if (logfp && logfp != stdout) {
    	    printf("---- dsync end %s -> %s ----\n", datestr1, datestr2);
        }
        /* V1.13-A end   */

	/* V1.16-A start */
	if (!stf) {
	    break;   /* -s指定でなければ1回で終わり */
	}
	Sleep(60000);
    }
    /* V1.16-A end   */

    if (logfp && logfp != stdout) {
        fclose(logfp);
    }

    /* V1.09-A start */
    for (i = 0; i<ig_cnt; i++) {
        if (ig_files[i]) {
	    dprintf("## free [%s]\n", ig_files[i]);
	    free(ig_files[i]);
	}
    }
    /* V1.09-A end   */

    if (cnt_error) return 1;
    return 0;
}

/*
 *  tv_diff(tv1, tv2, tv3)
 *
 *  IN 
 *   tv1, tv2 : time value
 *
 *  OUT 
 *   tv3      : tv1 - tv2
 *
 */
void tv_diff(tv1,tv2,tv3)
struct timeval *tv1,*tv2,*tv3;
{
    if (tv1->tv_usec < tv2->tv_usec) {
	tv3->tv_sec  = tv1->tv_sec  - tv2->tv_sec - 1;
	tv3->tv_usec = tv1->tv_usec + 1000000 - tv2->tv_usec;
    } else {
	tv3->tv_sec  = tv1->tv_sec  - tv2->tv_sec;
	tv3->tv_usec = tv1->tv_usec - tv2->tv_usec;
    }
}

#ifdef _WIN32
int gettimeofday(struct timeval *tv, struct timezone *tz)
{
    	SYSTEMTIME syst;

        //GetSystemTime(&syst);   // UTC
        GetLocalTime(&syst);
	tv->tv_sec  = syst.wHour * 3600 +
       		      syst.wMinute *60 +
        	      syst.wSecond;
	tv->tv_usec = syst.wMilliseconds * 1000;
	return 0;
}
#endif /* _WIN32 */

/* 
 *  get current dir files and sort
 *
 *  IN  : char *dir      : target dir
 *  IN  : int  sortsw    : 0:no sort  1:sort
 *  OUT : char **files[] : *files[] address
 *  OUT : ret    : num of files
 */
int GetFiles(char *dir, char **filesp[], int sortsw)
{
    DIR           *dirp;
    struct dirent *dp;
    int           cnt = 0;

    *filesp = NULL;

    /* 1: count files */
    if ((dirp = opendir(dir)) == NULL) {
	perror("opendir");
	fprintf(logfp, "Error: Directory [%s] open error.\n", dir);
        ++cnt_error;
	return 0;
    }

    cnt = 0;
    while((dp=readdir(dirp)) !=NULL) {
	if (!strcmp(dp->d_name,".") || !strcmp(dp->d_name,"..")) {
	    /* ".", ".."は無視する */
            continue;
        }
	++cnt;
    }
    closedir(dirp);

    if (cnt == 0) {
        /* no file */
        return 0;
    }

    *filesp = (char **)malloc(sizeof(char *)*(cnt+1)); /* 1個多く確保 */
    memset(*filesp, 0, sizeof(char *)*(cnt+1));

    /* 2: make file list */
    if ((dirp = opendir(dir)) == NULL) {
	perror("opendir");
	fprintf(logfp, "Error: Directory [%s] open error.\n", dir);
        ++cnt_error;
	return 0;
    }

    cnt = 0;
    while((dp=readdir(dirp)) !=NULL) {
	if (!strcmp(dp->d_name,".") || !strcmp(dp->d_name,"..")) {
	    /* ".", ".."は無視する */
            continue;
        }
        (*filesp)[cnt] = (char *)malloc(strlen(dp->d_name)+1);
        strcpy((*filesp)[cnt], dp->d_name);
	/* printf("%5d file = %s\n", i, dp->d_name); */
	++cnt;
    }
    closedir(dirp);

    return cnt;
}

void FreeFiles(char **filep[])
{
    int cnt = 0;
    if (filep == NULL || *filep == NULL) {
	fprintf(logfp, "Error: filep is null.\n");
	exit(1);
    }

    while ((*filep)[cnt] != NULL) {
        free((*filep)[cnt]);
	++cnt;
    }
    free(*filep);
    *filep = NULL;
}

/* 
 *  rm -r dir
 *
 *  IN  : dir : delete target dir
 *
 */
void DeleteDirR(char *dir)
{
    DIR           *dirp;
    struct dirent *dp;
    struct stat   stbuf;
    char          fullpath[2049];

    dprintf("## DeleteDirR: dir: %s/\n", dir);

    if ((dirp = opendir(dir)) == NULL) {
	perror(dir);
	fprintf(logfp, "Error: DeleteDirR: Directory [%s] open error.\n", dir);
	return;
    }

    while((dp=readdir(dirp)) !=NULL) {
	if (!strcmp(dp->d_name,".") || !strcmp(dp->d_name,"..")) {
            continue;
        }
	sprintf(fullpath, "%s/%s", dir, dp->d_name);

	if (stat(fullpath, &stbuf) == 0) {
	    if (stbuf.st_mode & S_IFDIR) {
                 DeleteDirR(fullpath);
	    } else {
		 if (tf) {
                     dprintf("## TEST: DeleteDirR: Delete file %s\n", fullpath);
		 } else {
                     unlink(fullpath);
                     dprintf("## DeleteDirR: Delete file %s\n", fullpath);
		 }
	    }
	}

    }
    closedir(dirp);

    if (tf) {
        dprintf("## TEST: DeleteDirR: Delete dir %s/\n", dir);
    } else {
        rmdir(dir);
        dprintf("## DeleteDirR: Delete dir %s/\n", dir);
    }

    return;
}

/*
 *  指定ディレクトリ下を比較し、d2のみにあるファイルを消去する
 *
 */
int DirCheckDelete(char *d1, char *d2)
{
    DIR           *dp;
    struct dirent *dent;
    struct stat   stbuf;
    struct stat   stbuf2;
    char          buf[2048];
    char          wkd1[512],wkd2[512];
    time_t	  time1,time2;
    int           fnum = 0;
    int           i = 0;
    char          **files;

    dprintf2("## dir1=[%s], dir2=[%s]\n",d1,d2);

    if (stat(d2, &stbuf)) {
        /* コピー先ディレクトリなし(チェック不要) */
	return 0;
    }

    if (!(stbuf.st_mode & S_IFDIR)) {
        /* d2がディレクトリでない(チェック不要) */
	return 0;
    }

    /* make file list for d2 */
    fnum = GetFiles(d2, &files, 1);
    if (fnum == 0) return 0;  /* file なし */

    /* check file & delete */
    for (i = 0; i<fnum; i++) {
    	sprintf(wkd1,"%s/%s", d1, files[i]);
    	sprintf(wkd2,"%s/%s", d2, files[i]);
	dprintf2("## FileList[%d] : %s\n", i, files[i]);

        /* d2 にあるファイルが d1 にあるかチェック */
	if (stat(wkd1, &stbuf)) {
            /* d2 にあるファイルが d1 に存在しないので削除する */
            if (tf) {
	        /* テストモード */
                fprintf(logfp, "## TEST: Delete %s\n", wkd2);
	        ++cnt_delete;
            } else {
                if (!stat(wkd2, &stbuf2)) {
                    if (stbuf2.st_mode & S_IFDIR) {
                        /* ディレクトリならばそれ以下全て削除 */
                        fprintf(logfp, "Delete %s/\n", wkd2);
			DeleteDirR(wkd2);
                    } else {
                        fprintf(logfp, "Delete %s\n", wkd2);
	                unlink(wkd2);
		    }
                    ++cnt_delete;
		} else {
	            fprintf(logfp, "Error: Cannot get file status [%s]\n", wkd2);
                    ++cnt_error;
		}
	    }
	}
    }

    FreeFiles(&files);

    return 0;
}

/* V1.18-A start */
/*
 *  IN  : fname1, fname2  compare files
 *  OUT : ret 0: same file
 *            1: different file
 *            2: fname1(src) not exist
 *            3: fname2(dst) not exist
 *
 */
int Cmp(char *fname1, char *fname2)
{
    FILE *fp1, *fp2;
    int  c1, c2;
    int  differ = 0;
    int  i;
    int  chrcnt = 1;
    int  line   = 1;
    int  printcnt = 1;   /* defferent print count */

    if (!(fp1 = fopen(fname1,"rb"))) {
	if (errno != ENOENT) perror(fname1);
	return 2;
    }
    if (!(fp2 = fopen(fname2,"rb"))) {
	if (errno != ENOENT) perror(fname2);
	fclose(fp1);
	return 3;
    }
    while (1) {
        c1 = getc(fp1);
        c2 = getc(fp2);
	if (c1 == EOF || c2 == EOF) {
	    if (c1 == c2) {
	        /* match */
	        break;
	    }
	    //printf("%s: EOF on %s\n", b[0], (c1 == EOF)?fname1:fname2);
            differ = 1;
	    break;
	}
	if (c1 != c2) {
	    // printf("%s %s differ: [0x%02x/0x%02x] char %d, line %d\n", 
	    //     fname1, fname2, c1, c2, chrcnt, line); 
            differ = 1;
	    if (--printcnt <= 0) {
                break;
	    }
	}

	++chrcnt;
	if (c1 == 0x0a) {
	    ++line;
	}
    }
    fclose(fp1);
    fclose(fp2);

    return differ;
}
/* V1.18-A end   */

/*
 *  指定ディレクトリ以下のファイルをコピーする。
 *
 *    d1 : src  dir
 *    d2 : dest dir
 */
int DirCopy(char *d1, char *d2)
{
    DIR           *dp;
    struct dirent *dent;
    struct stat   stbuf;
    struct stat   stbuf2;
    char          buf[2048];
    char          wkd1[512],wkd2[512];
    time_t	  time1,time2;
    int           cmp;                             /* V1.22-A */

    dprintf2("## dir1=[%s], dir2=[%s]\n",d1,d2);

    if (delf) {
        /* なくなったファイル削除オプションあり */
        DirCheckDelete(d1, d2);
    }

    if (!(dp = opendir(d1))) {
	perror("opendir");
	fprintf(logfp, "Error: Directory [%s] open error.\n", d1);
        ++cnt_error;
	exit(1);
    }
    if (stat(d2, &stbuf)) {
        /* コピー先ディレクトリなし */
	if (!fdf) {
	    /* 強制オプションがない場合 */
	    printf("Warning: %s directory not exist.\n",d2);
	    printf("         Make dir(m) make dir All(a) Cancel(c) Exit(e)?: ");
            buf[sizeof(buf)-1] = '\0';
	    fgets(buf, sizeof(buf)-1, stdin);
	    switch (buf[0]) {
	      case 'a' :
	        fdf = 1;	/* 以後強制的にdir作成 */
	        break;
	      case 'c' :
	        closedir(dp);
	        return 0;	/* このディレクトリ以下はやらない */
	      case 'e' :
	        exit(1);	/* コマンド中止 */
	        break;
	      default:
	        break;
	    }

	}
        if (tf) {
	    fprintf(logfp, "## TEST: make new directory [%s]\n",d2);
	} else {
	    fprintf(logfp, "make new directory [%s]\n",d2);
	    mkdirp(d2);
	}
    }

    while (dent = readdir(dp)) {
	if (!strcmp(dent->d_name,".") || !strcmp(dent->d_name,"..")) {
	    /* ".", ".."は無視する */
	    continue;
	}

	/* V1.09-A start */
	if (ig_files[0]) {
	    /* -e optioneあり */
	    int i;
	    int igf = 0;
	    for (i = 0; i<ig_cnt; i++) {
                if (!FILECMP(dent->d_name, ig_files[i])) {
		    /* 無視するファイルであれば無視 */
	            if (vf) fprintf(logfp, "## %s/%s copy skipped. (ignore file)\n", d1, dent->d_name);
		    igf = 1;
		    break;
		}
	    }
	    if (igf) continue;
	}
	/* V1.09-A end   */

    	sprintf(wkd1,"%s/%s", d1, dent->d_name);
    	sprintf(wkd2,"%s/%s", d2, dent->d_name);

	dprintf2("## file : [%s]\n",wkd1);

#ifdef _WIN32
    	stat(wkd1, &stbuf);
#else  /* !_WIN32 */
    	lstat(wkd1, &stbuf);  /* リンクをトレースしない */
#endif /* !_WIN32 */
	time1 = stbuf.st_mtime;

    	if (stat(wkd2, &stbuf2)) {
	    /* ファイルがない場合は、作るように0を入れる */
	    time2 = 0;
	} else {
	    time2 = stbuf2.st_mtime;
	}

        /* ディレクトリは時刻に関係なくチェック */
    	if (stbuf.st_mode & S_IFDIR) {
            /* ディレクトリ */
	    if (rf) {
               /* -rの場合、ディレクトリだったらリカーシブコール */
	       DirCopy(wkd1, wkd2);
	   }
	} else if (time2 + ignore_time < time1) {
	    /* ファイルの場合コピー先が古い場合のみコピーする(ignore_time分の誤差は許容) */
#ifndef _WIN32
	    if ((stbuf.st_mode & S_IFLNK) == S_IFLNK) {
                /* シンボリックリンク */
	        fprintf(logfp, "## %s copy skipped. (Symbolic link)\n", wkd1);
            } else
#endif /* !_WIN32 */
	    if (stbuf.st_mode & S_IFREG) {
                /* ファイル */
		if (tf) {
		    char wkmsg[256];
		    strcpy(wkmsg, "");

		    /* V1.18-A V1.20-C start */
		    if (cf || caf) {
		        cmp = Cmp(wkd1, wkd2);                 /* V1.22-A */
		        if (cmp == 0) {                        /* V1.22-C */
		            /* コピー対象のものはsameでも表示する */
		            strcpy(wkmsg, " (same)");
		        } else if (cmp == 1) {                 /* V1.22-A */
		            strcpy(wkmsg, " (different)");     /* V1.20-A */
		        } else if (cmp == 2) {                 /* V1.22-A */
		            strcpy(wkmsg, " (src not exist)"); /* V1.20-A */
		        } else if (cmp == 3) {                 /* V1.22-A */
		            strcpy(wkmsg, " (dst not exist)"); /* V1.20-A */
		        }
		    }
	            fprintf(logfp, "## TEST: Copy %s to %s%s\n", wkd1, wkd2, wkmsg);
		    /* V1.18-A V1.20-C end   */
                    ++cnt_copy;
		} else {
	            CopyFileX(wkd1, wkd2);
		}
            } else {
                /* その他ファイル */
	        fprintf(logfp, "## %s copy skipped.\n", wkd1);
	    }
	} else if (tf && caf && (stbuf.st_mode & S_IFREG)) {
	    if (cmp = Cmp(wkd1, wkd2)) {                                               /* V1.22-C */
	        if (cmp == 2) {                                                        /* V1.22-C */
	            fprintf(logfp, "## TEST: %s does not exist.\n", wkd1);             /* V1.22-C */
	        } else if (cmp == 3) {                                                 /* V1.22-C */
	            fprintf(logfp, "## TEST: %s does not exist.\n", wkd2);
	        } else {                                                               /* V1.22-C */
	            fprintf(logfp, "## TEST: %s and %s is different.\n", wkd1, wkd2);  /* V1.20-A */
	        }                                                                      /* V1.22-C */
	    } else {
	        /* ファイルが同じ場合は-vの場合のみ出力する */
	        if (vf) fprintf(logfp, "## TEST: %s and %s is same.\n", wkd1, wkd2);  /* V1.20-A */
	    }
	}
    }
    closedir(dp);
    return 0;
}

#ifdef _WIN32
int chdrvdir(char *dir)
{
    int ret;
    int drv;
    char buf[2048];

    dprintf3("##DBG: chdir start! [%s]\n",dir);
	
    getcwd(buf, sizeof(buf));

    if (dir[1] == ':') {
        /* ドライブ付きの場合 */
	drv = tolower(dir[0]) - 'a' + 1;

	dprintf3("##DBG: drv = %d\n",drv);

	if (ret = _chdrive(drv)) {
	    perror("chdrive");
	    /* return ret; */
	    exit(1); /* 続行不能 V1.07-C */
	}
	dprintf3("##DBG: chdir = %s\n",&dir[2]);
	return chdir(&dir[2]);
    }
    return chdir(dir);
}
#endif /* _WIN32 */

/*
 *  指定されたパスのディレクトリ部分のディレクトリを作成する
 *
 *  path = aaa/bbb
 *
 *  mkdir -p aaa/bbb
 */
int mkdirp(char *apath)
{
    char *pt;
    char cwd[2048];
    unsigned char path[2048];
    char wk[2048];
    int  i;
    int  kf = 0;
    int  ret;

    strcpy(path, apath);

#ifdef _WIN32
    /* '\\'を'/'に変更 */
    for (i = 0; i < strlen(path); i++) {
        if (ISKANJI(path[i])) {
	    /* SJIS 1バイト目なら、次もスキップ */
	    ++i;
	    continue;
	}
	if (path[i] == '\\') {
	    path[i] = '/';
	}
    }
#endif /* _WIN32 */

    dprintf3("### mkdirp=[%s]\n",path);

    getcwd(cwd, sizeof(cwd));

    dprintf3("###   pwd=[%s]\n",cwd);

    /* path = /aaa/bbb/test.html */
#ifdef _WIN32
    /* V1.11-A start */
    if (path[0] == '/' && path[1] == '/') {
        /* UNC path */
        pt = &path[2];
        pt = strchr(pt, '/');
        if (pt == NULL) {
	    fprintf(logfp, "### Error: Invalid UNC path (%s)\n", path);
            exit(1);            /* "//host" */
        }
        ++pt;
        pt = strchr(pt, '/');
        if (pt == NULL) {
	    fprintf(logfp, "### Error: Invalid UNC path (%s)\n", path);
            exit(1);            /* "//host/share" */
        }

        /* "//host/share/" */

        *pt = '\0';
        if (chdir(path) < 0) {  /* cd "//host/share" */
	    fprintf(logfp, "### mkdirp: chdir(%s) error. errno=%d\n", path, errno);
            exit(1);
        }

        ++pt;
        strcpy(wk, pt);         /* "//host/share/" 以降の部分をコピー */
    } else
    /* V1.11-A end   */
#endif /* _WIN32 */
    if (path[0] == '/') {
	/* フルパスの場合 */
        chdir("/");
        strcpy(wk, &path[1]);
#ifdef _WIN32
    } else if (path[1] == ':' && path[2] == '/') {
	/* Win ドライブ付きフルパスの場合 */
        chdrvdir(path);
        chdir("/");
        strcpy(wk, &path[3]);
#endif /* _WIN32 */
    } else {
	/* カレントディレクトリから */
        strcpy(wk,path);
    }
    strcat(wk,"/");
    pt = (char *)strrchr(wk,'/');
    if (!pt) {
	/* ディレクトリ部分なし */
	return 0;
    }
    *pt = '\0';
    /* wk = aaa/bbb */

    pt = (char *)strtok(wk,"/");
    while (pt) {
	dprintf3("###   mkdir %s\n",pt);
	ret = mkdir(pt, 0775);
	if (ret < 0 && errno != EEXIST) {  /* V1.10 */
	    fprintf(logfp, "### mkdirp: mkdir error. errno=%d\n", errno);
	    exit(1);  /* 中断 */
	}
	chdir(pt);
	pt = (char *)strtok(NULL,"/");
    }

#ifdef _WIN32
    chdrvdir(cwd);
#else  /* !_WIN32 */
    chdir(cwd);
#endif /* _WIN32 */
    return 0;
}

/*
 *   CopyFileX : ファイルのコピー
 *
 *     ret : 0 : 成功
 *           1 : 失敗
 */
int CopyFileX(char *f1, char *f2)
{
    FILE *fp1, *fp2;
    int sz, sz2;
    int all  = 0;
    int msec = 0;                       /* V1.05-A */
    struct stat stbuf, stbuf2;
    struct utimbuf utbuf;
    mode_t destmod;                     /* V1.04-A */
    struct timeval tvs, tve, tvd;       /* V1.05-A */
    int ret;                            /* V1.12-A */

    fprintf(logfp, "Copy %s to %s\n",f1,f2);
    /* V1.13-A stgart */
    if (logfp != stdout) {
        fflush(logfp);
        printf("Copy %s to %s\n",f1,f2);
    }
    /* V1.13-A end */

    /* コピー後元ファイルの時間を設定 */
    /* utime() */
    memset(&stbuf, 0, sizeof(stbuf));   /* V1.12-A */
    memset(&stbuf2, 0, sizeof(stbuf2)); /* V1.12-A */
    ret = stat(f1, &stbuf);
    /* V1.12-A start */
    if (ret < 0) {
        perror("CopyFileX: stat1");
	fprintf(logfp, "Error: Source file %s stat error\n", f1);
        ++cnt_error;
	return 1;
    }
    /* V1.12-A start */
    utbuf.actime  = stbuf.st_atime;	/* Access Time */
    utbuf.modtime = stbuf.st_mtime;	/* Modify Time */

    gettimeofday(&tvs, NULL);           /* V1.05-A */

    if (!(fp1 = fopen(f1,"rb"))) {
	perror("CopyFileX:fopen1");
	fprintf(logfp, "Error: Source file %s open error\n", f1);
        /* V1.12-A start */
        destmod = stbuf.st_mode;      /* File Mode */
        fprintf(logfp, "### Source file mode : 0%o\n", destmod);
        /* V1.12-A end   */
        ++cnt_error;
	return 1;
    }

    if (!(fp2 = fopen(f2,"wb"))) {

        /* V1.04-A start */
	if (ff && errno == EACCES) {
            stat(f2, &stbuf2);
            destmod = stbuf2.st_mode;       /* File Mode        */
#ifdef _WIN32
	    chmod(f2, destmod | _S_IWRITE); /* 0200             */
#else
	    chmod(f2, destmod | S_IWUSR);   /* S_IWGRP/S_IWOTH  */
#endif
	    fprintf(logfp, "Warning: Destination file %s mode change(+w)\n", f2);
	    ++cnt_warn;
            fp2 = fopen(f2,"wb");
	}
        /* V1.04-A end   */

	if (fp2 == NULL) {
	    perror("CopyFileX:fopen2");
	    fprintf(logfp, "Error: Destination file %s open error\n", f2);

            /* V1.04-A start */
            stat(f2, &stbuf2);
            destmod = stbuf2.st_mode;      /* File Mode */
            fprintf(logfp, "### Destination file mode : 0%o\n", destmod);
            ++cnt_error;
            /* V1.04-A end   */

	    fclose(fp1);
	    return 1;
	}
    }

    while (sz = fread(cpbuf,1,bufsize,fp1)) {   /* V1.19-C */
        dprintf3("read size = %d\n", sz);
	if ((sz2 = fwrite(cpbuf,1,sz,fp2)) != sz) {
	    fprintf(logfp, "fwrite(%s) incompleted. (%d/%d)\n",f2,sz2,sz); /* V1.17-C */
	    exit(1);
	}
	all += sz2;
    }

    if (all != stbuf.st_size) {
        fprintf(logfp, "fwrite(%s) incompleted. (%d/%d)\n",f2,all,stbuf.st_size);
	exit(1);
    }

    fclose(fp2);
    fclose(fp1);

    /* V1.05-A start */
    gettimeofday(&tve, NULL);

    tv_diff(&tve, &tvs, &tvd);
    msec = tvd.tv_sec*1000 + tvd.tv_usec/1000;
    if (stbuf.st_size >= 1024*1024) {
        fprintf(logfp, " (Size:%dKB  Time:%d.%03d sec  %dKB/s)\n", 
	       stbuf.st_size/1024, tvd.tv_sec, tvd.tv_usec/1000,
	       msec?(((stbuf.st_size/1024)*1000)/msec):0);
        if (logfp != stdout) {
            printf(" (Size:%dKB  Time:%d.%03d sec  %dKB/s)\n", 
	       stbuf.st_size/1024, tvd.tv_sec, tvd.tv_usec/1000,
	       msec?(((stbuf.st_size/1024)*1000)/msec):0);
        }
    }
    /* V1.05-A end   */

    /* 時刻変更 */
    if (utime(f2, &utbuf)) {
	perror("CopyFileX: utime");
	fprintf(logfp, "Error: utime(%s) error.\n",f2);
        ++cnt_error;
	/* 続行 */
    }

    ++cnt_copy;
    return 0;
}

/* V1.16-A start */
/*
 *  指定時刻まで待つ  
 *
 *    IN  hh: 時
 *    IN  mm: 分
 *
 */
void WaitForStart(int hh, int mm)
{
    time_t    tt;
    struct tm *tm;

    while(1) {
	tt = time(0);
        tm = localtime(&tt);
	if (hh == tm->tm_hour && mm == tm->tm_min) {
	    /* 時分一致 */
	    break;
        }
	Sleep(1000);
    }

    return;
}
/* V1.16-A end   */

/* vim:ts=8:sw=8:
 */
