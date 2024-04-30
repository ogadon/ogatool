/*
 *  mkindex : make indexNNN.html
 *
 *  2001/01/27 V1.00 by oga
 *  2002/08/20 V1.01 NUM_IMGS 25 => 40
 *  2002/09/24 V1.02 support -r
 *  2003/08/01 V1.03 support .html
 *  2004/10/20 V1.04 support -w
 *  2004/10/24 V1.05 support image click
 *  2006/01/03 V1.06 fix title bug
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#define MAXENT 10000
#define NUM_IMGS 40

/* globals */
int  vf   = 0;              /* -v   verbose          */
int  rf   = 0;              /* -r   recursive        */
int  wf   = 0;              /* -w   image width      */
int  gcnt = 0;              /* file count for -r     */
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
 *  get all files and sort
 *
 *  IN  : suffix : NULL : all files
 *                 str  : select by suffix str
 *  IN  : maxent : max entry size of files
 *  OUT : files  : filenames store area
 *  OUT : ret    : num of files
 */
int GetFilesR(char *files[], char *suffix, int maxent, char *curdir)
{
    DIR           *dirp;
    struct dirent *dp;
    struct stat   stbuf;
    char          fullpath[1025];

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
	    if (stbuf.st_mode & S_IFDIR) {
                 GetFilesR(files, suffix, maxent, fullpath);
	    }
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
        if (maxent <= gcnt) {
            printf("Too many files\n");
            break;
        }
        dprintf("strcpy(%s)\n", fullpath);
        files[gcnt] = (char *)malloc(strlen(fullpath)+1);
        strcpy(files[gcnt], fullpath);
	/* printf("%5d file = %s\n", i, fullpath); */
	++gcnt;
    }
    closedir(dirp);

    return gcnt;
}

void PutBackForward(FILE *fp, int backno, int fwno, int maxno)
{
    int i;

    fprintf(fp, "<a href=\"index%03d.html\">Page %03d</a>\n", backno, backno); 
    fputs(" <=> \n", fp);
    fprintf(fp, "<a href=\"index%03d.html\">Page %03d</a><br>\n", fwno, fwno);
    
    for (i = 1; i <= maxno; i++) {
        fprintf(fp, "<a href=\"index%03d.html\">%d</a>\n", i, i);
    }
}

int main(int a, char *b[])
{
    int  i;
    int  cnt;                   /* num of files          */
    int  fcnt;                  /* index file counter    */
    int  imgcnt;                /* image counter         */
    FILE *fp;
    char *suffix = NULL;        /* arg1 suffix           */
    int  num     = 0;           /* arg2 number of images */

    char *files[MAXENT];
    char indexfile[2048];

    memset(files, 0, sizeof(files));

    for (i = 1; i<a; i++) {
        if (!strcmp(b[i],"-h")) {
#ifdef _WIN32
            printf("usage: mkindex [-r] [-w <size>] [{gif | <jpg> | etc..} [<n(%d)>]]\n", NUM_IMGS);
#else
            printf("usage: mkindex [-i] [-r] [-w <img_width>] [{gif | <jpg> | etc..} [<n(%d)>]]\n", NUM_IMGS);
#endif
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
        if (i+1 < a && !strcmp(b[i],"-w")) {
            wf = atoi(b[++i]);
            continue;
        }
        if (!strcmp(b[i],"-v")) {
            vf = 1;
            continue;
        }
        if (!suffix) {
            suffix = b[i];
        }
        if (!num) {
            num = atoi(b[i]);
        }
    }

    if (!num) num = NUM_IMGS;
    if (!suffix) suffix = "jpg";

    if (rf) {
        gcnt = 0;
        cnt = GetFilesR(files, suffix, MAXENT, ".");
        /* sort */
        qsort(files, gcnt, sizeof(char *), compare);
    } else {
        cnt = GetFiles(files, suffix, MAXENT);
    }

    dprintf("start print\n");
    printf("suffix       : %s\n", suffix);
    printf("num of files : %d\n", cnt);

    if (cnt == 0) {
        printf("==> no target files.\n");
	exit(1);
    }

    fcnt   = 1;             /* file count    */
    imgcnt = 1;             /* image counter */
    while (1) {
        sprintf(indexfile, "index%03d.html", fcnt);
        if ((fp = fopen(indexfile, "w")) == NULL) {
            perror("fopen");
            exit(1);
        }
        printf("make %s ...\n", indexfile);

        /* title */
	fprintf(fp, "<html>\n<head><title>Page %03d</title></head>\n<body>\n", fcnt);
        fprintf(fp, "<h1><font color=\"#ff0000\">Page %03d</font></h1>\n", 
                    fcnt);                             /* current page number */

        /* back/forward link (top) */
        PutBackForward(fp, (fcnt == 1)?(cnt-1)/num+1:fcnt-1,
                           (fcnt == (cnt-1)/num+1)?1:fcnt+1,
			   (cnt-1)/num+1);
        fputs("<hr>\n", fp);

        /* images */
        for (i = 0; i<num; i++) {
            if (imgcnt > cnt) break;
            if (files[imgcnt-1]) {
	        if (strstr(suffix, "htm") || strstr(suffix, "HTM")) {  /*V1.03*/
                    fprintf(fp, "<a href=\"%s\">[%d] %s</a><hr>\n",    /*V1.03*/
                                files[imgcnt-1], imgcnt, files[imgcnt-1]);
	        } else {                                               /*V1.03*/
                    fprintf(fp, "<a href=\"%s\" target=\"_blank\">", files[imgcnt-1]); /*V1.05*/
		    if (wf) {                                          /*V1.04*/
                        fprintf(fp, "<img src=\"%s\" width=\"%d\" border=0></a>[%d] %s<hr>\n", 
                                files[imgcnt-1], wf, imgcnt, files[imgcnt-1]);
		    } else {
                        fprintf(fp, "<img src=\"%s\" border=0></a>[%d] %s<hr>\n", 
                                files[imgcnt-1], imgcnt, files[imgcnt-1]);
		    }
	        }                                                      /*V1.03*/
                free(files[imgcnt-1]);
                ++imgcnt;
            }
        }

        /* back/forward link (bottom) */
        PutBackForward(fp, (fcnt == 1)?(cnt-1)/num+1:fcnt-1,
                           (fcnt == (cnt-1)/num+1)?1:fcnt+1,
			   (cnt-1)/num+1);
	fprintf(fp, "</body>\n</html>\n");
        fclose(fp);
        if (imgcnt > cnt) break;
        ++fcnt;
    }

    exit(0);
}

