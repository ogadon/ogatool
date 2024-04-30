/*
 *  addimgdate : add file date to ImageXX.jpg file
 *
 *  2023/07/01 V0.10 by oga
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>

#define strcasecmp stricmp
#define S_IFDIR    _S_IFDIR
#else  /* Linux */
#include <dirent.h>
#include <unistd.h>
#include <sys/param.h>
#endif

#define dprintf if (vf) printf

/* globals */
int  vf   = 0;              /* -v   verbose          */
int  rf   = 0;              /* -r   recursive        */
int  tf   = 0;              /* -t   test mode        */
#ifdef _WIN32
int  igncase = 1;           /* -i   ignore case      */
#else
int  igncase = 0;           /* -i   ignore case      */
#endif

int compare(const void *arg1, const void *arg2)
{
    return strcmp( *(char**)arg1, *(char**)arg2 );
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

    dprintf("Start opendir\n");

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

    dprintf("Start readdir(0x%08x)\n",dirp);

    if (dirp->firstf) {
        dirp->hdir = FindFirstFile(dirp->path, &(dirp->direntry.wfd));
        if (dirp->hdir == INVALID_HANDLE_VALUE) {
            printf("readdir: FindFirstFile() error\n");
            return NULL;
        }
        dirp->firstf = 0;
    } else {
        status = FindNextFile(dirp->hdir, &(dirp->direntry.wfd));
        if (status == FALSE) {
            return NULL;   /* EOF or Error */
        }
    }
    return &(dirp->direntry);
}

int closedir(DIR *dirp)
{
    dprintf("Start closedir\n");
    if (dirp) {
        FindClose(dirp->hdir);
        free(dirp);
    }
    return 0;
}
#endif /* _WIN32 */

/* 
 *  get current dir files and sort
 *
 *  IN  : suffix : NULL : all files
 *                 str  : select by suffix str
 *  IN  : maxent : max entry size of files
 *  OUT : files  : filenames store area
 *  OUT : ret    : num of files
 */
int GetFiles(char *files[], char *suffix, int maxent)
{
    DIR           *dirp;
    struct dirent *dp;
    int           cnt = 0;

    if ((dirp = opendir(".")) == NULL) {
	perror("opendir");
	exit(1);
    }

    cnt = 0;
    while((dp=readdir(dirp)) !=NULL) {
	if (!strcmp(dp->d_name,".") || !strcmp(dp->d_name,"..")) {
            continue;
        }
        if (strlen(dp->d_name) > 99) {
            printf("too long filename (%s) skip.\n", dp->d_name);
            continue;
        }
        if (suffix) {
            if (igncase) {
                if (strcasecmp(&(dp->d_name[strlen(dp->d_name)-strlen(suffix)]), suffix)) {
                    dprintf("suffix not match (%s)\n", dp->d_name);
                    continue;
                }
            } else {
                if (strcmp(&(dp->d_name[strlen(dp->d_name)-strlen(suffix)]), suffix)) {
                    dprintf("suffix not match (%s)\n", dp->d_name);
                    continue;
                }
            }
        }
        if (maxent <= cnt) {
            printf("Too many files\n");
            break;
        }
        dprintf("strcpy(%s)\n", dp->d_name);
        files[cnt] = (char *)malloc(strlen(dp->d_name)+1);
        strcpy(files[cnt], dp->d_name);
	/* printf("%5d file = %s\n", i, dp->d_name); */
	++cnt;
    }
    closedir(dirp);

    /* sort */
    qsort(files, cnt, sizeof(char *), compare);

    return cnt;
}

/* 
 *  rename "Image*" files
 *
 *  IN  : curdir : current dir
 *  IN  : arf    : recursive flag
 */
void RenameFileR(char *curdir, int arf)
{
    DIR           *dirp;
    struct dirent *dp;
    struct stat   stbuf;
    struct tm     *wtm;
    char          fullpath[1025];
    char          fullpath_dst[1025];
    int           wcnt;

    dprintf("curdir: %s\n", curdir);

    if ((dirp = opendir(curdir)) == NULL) {
	perror(curdir);
	exit(1);
    }

    while((dp=readdir(dirp)) !=NULL) {
	if (!strcmp(dp->d_name,".") || !strcmp(dp->d_name,"..")) {
            continue;
        }
        if (strlen(dp->d_name) > 99) {
            printf("too long filename (%s) skip.\n", dp->d_name);
            continue;
        }

	sprintf(fullpath, "%s/%s", curdir, dp->d_name);

	if (stat(fullpath, &stbuf) == 0) {
	    if (stbuf.st_mode & S_IFDIR && arf) {
                 RenameFileR(fullpath, arf);
	    } else if ((stbuf.st_mode & S_IFREG) && !strncmp(dp->d_name, "Image", 5)) {
		wtm = localtime(&stbuf.st_mtime);
		sprintf(fullpath_dst, "%s/%02d%02d%02d_%s", curdir, 
		    wtm->tm_year+1900-2000,
		    wtm->tm_mon+1,
		    wtm->tm_mday,
		    dp->d_name);
		wcnt = 1;
		while (stat(fullpath_dst, &stbuf) == 0) {
		    /* if exist dst, try new name */
		    sprintf(fullpath_dst, "%s/%02d%02d%02d_%d_%s", curdir, 
		        wtm->tm_year+1900-2000,
		        wtm->tm_mon+1,
		        wtm->tm_mday,
		        wcnt++,
		        dp->d_name);
		}
		printf("rename %s => %s\n", fullpath, fullpath_dst);
		rename(fullpath, fullpath_dst);
	    }
	}
    }
    closedir(dirp);
}

int main(int a, char *b[])
{
    int  i;
    FILE *fp;
    char *prefix = "Image";     /* arg1 prefix           */

    char indexfile[2048];

    for (i = 1; i<a; i++) {
        if (!strcmp(b[i],"-h")) {
            printf("usage: addimgdate [-r] [<target_prefix(%s)>]\n", prefix);
            exit(1);
        }
        if (!strcmp(b[i],"-i")) {
            igncase = 1 - igncase;
            continue;
        }
        if (!strcmp(b[i],"-r")) {
            rf = 1;
            continue;
        }
        if (!strcmp(b[i],"-t")) {
            tf = 1;
            continue;
        }
        if (!strcmp(b[i],"-v")) {
            vf = 1;
            continue;
        }
    }


    RenameFileR(".", rf);

    exit(0);
}

/*
 * vim: ts=4 sw=4
 */

