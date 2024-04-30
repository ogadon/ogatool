/*
 *	lsc.c : color ls
 *
 *			1995.08.20  V0.01	Initial Version. by ‚¨‚ªš
 *			1995.08.21  V0.02	support 3050RX
 *			1995.08.21  V0.03	support symbolic link. 
 *			2004.10.13  V0.04	support linux and fix link check
 *			2013.12.31  V0.05	fix no env prob
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include "cur.h"
#include <sys/stat.h>

#ifdef X68000
#include <sys/dos.h>
#include <sys/iocs.h>
#include <unistd.h>			
#else /* H3050RX */
#ifndef _WIN32
#include <curses.h>
#endif  /* !_WIN32 */
#include <sys/param.h>
#endif

#define MAX_ENT  2000
#define CALXY  	 x*y_max+y+off_y*x_max*y_max

int x_max = 0, y_max = 0, off_y = 0, max_file_len = 0;
int env_x = 0, env_y = 0;     /* V0.05-C */

char	dirf[MAX_ENT];

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

    dprintf2("## Start opendir\n");

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

    dprintf2("## Start readdir(0x%08x)\n",dirp);

    if (dirp->firstf) {
        dirp->hdir = FindFirstFile(dirp->path, &(dirp->direntry.wfd));
        if (dirp->hdir == INVALID_HANDLE_VALUE) {
            printf("Error: readdir: FindFirstFile() error\n");
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
    dprintf2("## Start closedir\n");
    if (dirp) {
        FindClose(dirp->hdir);
        free(dirp);
    }
    return 0;
}
#endif /* _WIN32 */

main(a,b)
int a;
char *b[];
{
	int	c, i=0, j;
	int	x=0,y=0,cnt=0, f_aster=0, del_f = 0, vi_f = 0;
	int	af=0;
	DIR	*dirp;
	int	move_dir_f = 0;			/* 94.11.02 for move dir */
	struct	dirent *dp;
	struct	stat statbuf;
	char 	*dirn = ".";
	char	*file[MAX_ENT];
#ifdef X68000
	char	wkf[PATH_MAX+1];
	char	buf[PATH_MAX+1];
#else /* H3050RX */
	char	wkf[MAXPATHLEN+1];
	char	buf[MAXPATHLEN+1];
#endif

	/* check arg */
	for (i=1; i<a ; i++) {
		if (!strcmp(b[i],"-a")) {
			af = 1;
			continue;
		}
		dirn = b[1];
	}

#ifdef X68000
	if (_iocs_crtmod(-1) == 16) {
		env_x = 96;				/* 96 */
		env_y = 31;
	} else {
		env_x = 64;
		env_y = 31;
		if (x > x_max-1) x=x_max-1;
	}
	/*
	if (i = atoi(getenv("COLUMNS"))) {
		env_x = i - 1;
		if (x > x_max-1) x=x_max;
	}
	if (i = atoi(getenv("LINES"))) {
		env_y = i-1;
	}
	*/
#else
	if (getenv("COLUMNS")) {     /* V0.05-A */
		env_x = atoi(getenv("COLUMNS"));
	}
	if (getenv("LINES")) {       /* V0.05-A */
		env_y = atoi(getenv("LINES"));
	}
	if (env_x == 0)
		env_x = 80;
	if (env_y == 0)
		env_y = 24;
#endif
	for (i = 0; i< MAX_ENT; i++) {
		file[i] = (char *)0;
	}

	if ((dirp = opendir(dirn)) == 0) {
		perror("");
		exit(1);
	}
	i=0;
        while((dp=readdir(dirp)) !=NULL){
		if (!strcmp(dp->d_name,".") || !strcmp(dp->d_name,".."))
			continue;
		if (!af && !strncmp(dp->d_name,".",1))
			continue;
		file[i] = (char *)malloc(strlen(dp->d_name)+2);
		strcpy(file[i],dp->d_name);	/* set basename */
		if (max_file_len < strlen(file[i]))
			max_file_len = strlen(file[i]);
		strcpy(wkf,dirn);
		strcat(wkf,"/");
		strcat(wkf,dp->d_name);
#ifdef DEBUG
		printf("name = %s\n",wkf);	/* DEBUG */
#endif

#ifdef linux
		lstat(wkf,&statbuf);  /* for link check V0.04-C */
#else
		stat(wkf,&statbuf);
#endif
		dirf[i] = ' ';
#if defined (X68000) || defined (linux)  /* V0.04-C */
		if (statbuf.st_mode & S_IFDIR ) { 
#else
		if (statbuf.st_mode & _S_IFDIR ) {  /* } */
#endif
			strcat(file[i],"/");
			dirf[i] = '/';
		}
#ifdef X68000
		else if ((statbuf.st_mode & S_IFMT) == S_IFLNK) { 
#else
		else if (S_ISLNK(statbuf.st_mode)) { /*} I don't know link check*/
#endif
			strcat(file[i],"@");
			dirf[i] = '@';
		}
		else if (statbuf.st_mode & (S_IXUSR + S_IXGRP + S_IXOTH )) { 
			strcat(file[i],"*");
			dirf[i] = '*';
		}

#ifdef DEBUG
		printf("mode = %o, %s %d\n",statbuf.st_mode,file[i],statbuf.st_nlink);
#endif
		i++;
		if (i >= MAX_ENT) {
			printf("filer : ƒGƒ“ƒgƒŠƒI[ƒo[ƒtƒ[  ");
			printf("(MAX is %d)\n",MAX_ENT);
#ifndef X68000
			sleep(1);
#endif
			break;
		}
    	}
    	closedir(dirp);

    	max_file_len += 3;
	x_max = env_x / max_file_len;
    	xsort(file,i);
	y_max = (i-1) / x_max + 1;

	dispall(file);

	freex(file); 
	return 0;
}

disp(file,x,y,sw)
char **file;
int x,y,sw;
{
	char fmt[100];

#ifdef X68000
	sprintf(fmt,"%%-%ds[33m",max_file_len);
	switch (sw) {
	    case '*':	/* Às‰Â”\ƒtƒ@ƒCƒ‹ */
		printf("[32m"); /* ‰©F•\¦  */
		break;
	    case '/':	/* ƒfƒBƒŒƒNƒgƒŠ */
		printf("[31m"); /* …F•\¦  */
		break;
  	    case '@':	/* ƒVƒ“ƒ{ƒŠƒbƒNƒŠƒ“ƒN */
  		printf("[37m"); /* ‘¾”’F•\¦  */
  		break;
	    default :
		printf("[33m"); /* ”’F•\¦  */
		break;
	}
#else  /* for H3050RX mjpterm */
	sprintf(fmt,"%%-%ds[0m",max_file_len);
	/* 1:Blue  2:Red  4:Green */
	switch (sw) {
	    case '*':	/* Às‰Â”\ƒtƒ@ƒCƒ‹ */
		printf("[35m"); /* …F•\¦  */
		break;
	    case '/':	/* ƒfƒBƒŒƒNƒgƒŠ */
		printf("[32m"); /* Ô•\¦  */
		break;
	    case '@':	/* ƒVƒ“ƒ{ƒŠƒbƒNƒŠƒ“ƒN */
		printf("[33m"); /* ƒ}ƒ[ƒ“ƒ^•\¦  */
		break;
	    default :
		printf("[0m");  /* ƒm[ƒ}ƒ‹•\¦ */
		break;
	}
#endif
	printf(fmt,file);
}

dispall(file)
char *file[];
{
	int i,j;

	for (i=0; i<y_max ; i++) {
		for (j=0; j<x_max ; j++) {
			if (file[j*y_max+i])
				disp(file[j*y_max+i],j,i,dirf[j*y_max+i]);
			else
				break;
		}
		if (x_max*max_file_len != env_x)
			printf("\n");
	}
	if (x_max*max_file_len == env_x)
	printf("\n");
}
freex(file)
char *file[];
{
	int i=0;
	while (file[i])
		free(file[i++]);

}

/* 
 *	nŒÂ‚Ì”z—ñfile‚Éƒ|ƒCƒ“ƒg‚³‚ê‚Ä‚¢‚é•¶š—ñ‚ğ•À‚×•Ï‚¦‚éB
 *
 *						V1.00 by oga.
 */
xsort(file,n)
char *file[];
int n;
{
	char *wk;
	int wk2;
	int i,j;

	for (i=1;i<n;i++) {
		for (j=0;j<n-i;j++) {
			if (strcmp(file[j],file[j+1]) > 0 ) {
				wk 	  = file[j];
				file[j]   = file[j+1];
				file[j+1] = wk;
				wk2	  = dirf[j];
				dirf[j]   = dirf[j+1];
				dirf[j+1] = wk2;
			}
		}
	}
}

/* vim:ts=8:sw=8:
 */
