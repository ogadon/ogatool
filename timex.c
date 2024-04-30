/*
 *   timex for win32
 *
 *         1999/08/04  V1.00  by oga.
 *         2000/08/31  V1.01  stderr(-e) support
 *         2002/04/22  V1.02  support .cmd command
 */
#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <stdlib.h>

int which(char *command, char *fullpath)
{
	char path[2048];         /* stat path */
	char *pathenvp;
	char pathp[2048];        /* ENV path */
	int pathlen, comsize;
	int i, j;
	struct stat statb;
	int allf = 0;
	int found = 0;
	int vf = 0;

	/* fullpath指定など、存在したらそれを返す */
	if (stat(command, &statb) == 0) {
	    strcpy(fullpath, command);
	    return 0;
	}

	strcpy(fullpath, "");

	if (vf) printf("command=%s\n",command);

	strcpy(pathp,".;");
	pathenvp="";
	pathenvp = (char *)getenv("PATH");

	if (vf) printf("PATH=%s\n",pathenvp);

	if (pathenvp) strcat(pathp,pathenvp);
	pathlen = strlen(pathp);

	if (vf) printf("path=%s\n",pathp);

	i = 0;

	while (i < pathlen) {
		j = 0;
		while (pathp[i] != ';' && i < pathlen) { /*path[]="/usr/bin"*/
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
		strcat(path,command);           /* path[] = "/usr/bin/com"    */
	    
		if (vf) printf("check path=%s\n",path);
		comsize = strlen(path);
#if 0
		/* timexでは.bat未サポート */
		strcpy(&path[comsize],".bat");  /* path[] = "/usr/bin/com.bat"*/
		if (stat(path,&statb) == 0) {
			/* printf("%s\n", path); */
			strcpy(fullpath, path);
			if (allf) found = 1;
			else return 0;
		}
#endif
		strcpy(&path[comsize],".exe");  /* path[] = "/usr/bin/com.exe"*/
		if (stat(path,&statb) == 0) {
			/* printf("%s\n", path); */
			strcpy(fullpath, path);
			if (allf) found = 1;
			else return 0;
		}
		strcpy(&path[comsize],".com");  /* path[] = "/usr/bin/com.com"*/
		if (stat(path,&statb) == 0) {
			/* printf("%s\n", path); */
			strcpy(fullpath, path);
			if (allf) found = 1;
			else return 0;
		}
		strcpy(&path[comsize],".cmd");  /* path[] = "/usr/bin/com.cmd"*/
		if (stat(path,&statb) == 0) {
			/* printf("%s\n", path); */
			strcpy(fullpath, path);
			if (allf) found = 1;
			else return 0;
		}
	}
	if (!found) {
		printf("command %s not found.\n",command);
		return 1;
	}
	return 0;
}

/*
 *   exec command
 *
 *   IN : 
 *   OUT: 0: success  -1:error
 */
int exec_cmd(int a, char *b[])
{
	BOOL 			status;
	STARTUPINFO 		si;
	PROCESS_INFORMATION 	pi;
	FILE                    *ofp = stdout;

	HANDLE	hProc;				/* Process Handle */
	int	pst;
	int	st = 0;	/* success */
	char	command[2048];
	char	comargs[2048];

	int     i;
    	int 	start,end;
	int     point = 1;

	if (a >= 2 && !strcmp(b[point], "-e")) { /* V1.01 */
	    ofp = stderr;
	    ++point;
	}

	if (a < point+1) {
	    /* printf("Missing command\n"); */
	    printf("usage: timex [-e] <command string>\n");
	    printf("       -e : result output to stderr\n");
	    exit(1);
	}


	which(b[point++], command);		/* 完全パスに変換 */

	strcpy(comargs,"");
	for (i = point; i<a; i++) {
	    strcat(comargs," ");
	    strcat(comargs,b[i]);		/* args */
	}

	printf("%s%s\n", command, comargs);

	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
    	start = clock();			/* start point */
        status = CreateProcess(command,	/* 実行モジュール名            */
			comargs, 	/* コマンドライン              */
			NULL,		/* プロセスのセキュリティ属性  */
			NULL,		/* スレッドのセキュリティ属性  */
			FALSE,		/* ハンドルを継承しない        */
			0, 		/* fdwCreate                   */
			NULL,		/* 環境ブロックのアドレス      */
			NULL,		/* カレントディレクトリ(親同)  */
                        &si,            /* STARTUPINFO構造体           */
                        &pi);           /* PROCESS_INFORMATION構造体   */
        if(status != TRUE){
            printf("exec_cmd: CreateProcess returned=%d",GetLastError());
            return -1;	/* error */
        }

	hProc = pi.hProcess;
	status=CloseHandle(pi.hThread);
	if(status != TRUE){
	    printf("CloseHandle error(%d).", GetLastError());
	}

	/*
	 * wait for command end.
	 */
	if (WaitForSingleObject(hProc, INFINITE) != WAIT_FAILED) {
	    /* process end!! */
	    if (!GetExitCodeProcess(hProc, &pst)) {
		printf("exec_cmd: GetExitCodeProcess error(%d)\n",
						GetLastError());
		st = 0;		/* error */
	    } else {
	        st = pst;
	    }

	}
        end   = clock();		/* end point */

	status=CloseHandle(hProc);
	if(status != TRUE){
		printf("CloseHandle error(%d).", GetLastError());
	}

	/* disp time */
	fprintf(ofp,"\n-------------------\n");
        fprintf(ofp,"real : %.2f sec\n", ((float)(end-start))/CLOCKS_PER_SEC);
	fprintf(ofp,"-------------------\n");

	return pst;
}

int main(int a, char *b[])
{
    int st;

    st = exec_cmd(a,b);

    return st;
}
