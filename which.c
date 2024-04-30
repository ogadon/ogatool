/*
 *  which <command_name> : 実行されるコマンドの位置を求める
 *
 *    1995.02.02  V0.10 Initial Revision  by oga.
 *    1995.02.05  V0.20 add .r
 *    1995.07.08  V0.30 -all support.
 *    1995.10.30  V0.40 porting to DOS.
 *    1999.02.02  V0.41 add verbose
 *    2009.12.31  V0.42 add .dll and change PATH env buf length
 *    2012.12.25  V0.43 merge soruce
 *
 *	compile option
 *		X680x0 : -DX68K
 *		DOS    : -DX68K -DDOS
 */

#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>

int vf = 0;		/* V0.41 verbose */

usage()
{
	printf("usage : which <command_name> [-a]\n");
	exit(1);
}

main(a,b)
int a;
char *b[];
{
	char path[2048];         /* stat path V0.41-C */
	char *pathenvp;
	char pathp[4096];        /* ENV path  V0.41-C */
	char *filename = NULL;  /* filename   V0.41-A */
	int pathlen, comsize;
	int i, j;
	struct stat statb;
	int allf = 0;
	int found = 0;
	int vf    = 0;          /* 1:verbose  V0.41-A */

	/* V0.41-C start */
	for (i = 1; i<a; i++) {
		if (!strcmp(b[i],"-h")) {
			usage();
		}
		if (!strcmp(b[i],"-a")) {
			allf = 1;
			continue;
		}
		if (!strcmp(b[i],"-v")) {
			++vf;
			continue;
		}
		filename = b[i];
	}
	
	if (filename == NULL) {
		usage();
	}
	/* V0.41-C end */

#ifdef X68K
	strcpy(pathp,".;");
#ifdef DOS
	pathenvp="";
	pathenvp = (char *)getenv("PATH");
#else  /* DOS */
	pathenvp = (char *)getenv("path");
#endif /* DOS */

	if (vf) printf("PATH=%s\n",pathenvp);

#else  /* H3050 */
	pathp[0]='\0';
	pathenvp = (char *)getenv("PATH");
#endif
	if (pathenvp) strcat(pathp,pathenvp);
	pathlen = strlen(pathp);

	if (vf) printf("path=%s\n",pathp);

	i = 0;

	while (i < pathlen) {
		j = 0;
#ifdef X68K
		while (pathp[i] != ';' && i < pathlen)  /* path[] = "/usr/bin" */
#else
		while (pathp[i] != ':' && i < pathlen)  /* path[] = "/usr/bin" */
#endif
		{
			path[j] = pathp[i];
			if (path[j] == '\\')
				path[j] = '/';
			++i;
			++j;
		}
		i++;
		path[j] = '\0';

		if (vf) printf("path=[%s]\n",path);

		strcat(path,"/");               /* path[] = "/usr/bin/"       */
		strcat(path,filename);          /* path[] = "/usr/bin/com"    */
	    
		if (vf) printf("check path=%s\n",path);
#ifndef X68K
		if (stat(path,&statb) == 0) {   /* path[] = "/usr/bin/com"   */
			find(path);
			if (allf) found = 1;
			else exit(0);
		}
#else  /* X68K */
		comsize = strlen(path);
#ifndef DOS
		strcpy(&path[comsize],".x");    /* path[] = "/usr/bin/com.x"  */
		if (stat(path,&statb) == 0) {
			find(path);
			if (allf) found = 1;
			else exit(0);
		}
		strcpy(&path[comsize],".r");    /* path[] = "/usr/bin/com.r"  */
		if (stat(path,&statb) == 0) {
			find(path);
			if (allf) found = 1;
			else exit(0);
		}
		strcpy(&path[comsize],".sh");   /* path[] = "/usr/bin/com.sh" */
		if (stat(path,&statb) == 0) {
			find(path);
			if (allf) found = 1;
			else exit(0);
		}
#endif  /* !DOS */
		strcpy(&path[comsize],".bat");  /* path[] = "/usr/bin/com.bat"*/
		if (stat(path,&statb) == 0) {
			find(path);
			if (allf) found = 1;
			else exit(0);
		}
#ifdef DOS
		strcpy(&path[comsize],".exe");  /* path[] = "/usr/bin/com.exe"*/
		if (vf) printf("find [%s]\n",path);
		if (stat(path,&statb) == 0) {
			find(path);
			if (allf) found = 1;
			else exit(0);
		}
		strcpy(&path[comsize],".com");  /* path[] = "/usr/bin/com.com"*/
		if (stat(path,&statb) == 0) {
			find(path);
			if (allf) found = 1;
			else exit(0);
		}

		/* V0.42-A start */
		strcpy(&path[comsize],".dll");  /* path[] = "/usr/bin/dllname.dll" */
		if (stat(path,&statb) == 0) {
			find(path);
			if (allf) found = 1;
			else exit(0);
		}
		/* V0.42-A end   */
#endif /* DOS */
#endif /* X68K */
	}
	if (!found) {
		printf("command %s not found.\n",filename);
		exit(1);
	}
	exit(0);
}

find(path)
char *path;
{
	printf("%s\n",path);
	if (vf) printf("### found [%s]\n",path);
	return 0;
}


