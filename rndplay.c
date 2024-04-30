/*
 *   rndplay : ランダムプレイ  
 *
 *     2006/04/25 V0.10 by oga.
 *     2006/05/14 V0.20 support -t option
 */
#ifdef _WIN32
#include <windows.h>
#endif /* _WIN32 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* macros */
#define VER "0.20"
#define	IS_DOT(str)	(!strcmp(str,".") || !strcmp(str,".."))
#define RAND(x)     	((rand() & 0xffff)*(x)/(RAND_MAX & 0xffff))

#define dprintf		if (vf) printf
#define dprintf2	if (vf >= 2) printf

/* defines */
#define F_TYPE_DIR  0x01
#define F_TYPE_FILE 0x02

#define SUFF_MP3   ".mp3"
#define PLAYER_MP3 "mpg123"    /* mp3 player     without .exe */
#define PLAYER_MS  "wmplayer"  /* WinMediaPlayer without .exe */


/* globals */
//char *find_name = NULL;  /* -name  */
//int  ftype = 0;          /* -ftype */
int  vf    = 0;            /* -v     */
int  tf    = 0;            /* -t     */
int  depth = 0;
int  cur_list_size = 0;    /* path list size   */
char **pathlist = NULL;    /* path list        */
int  num_ent    = 0;       /* num of all songs */
int  gnum       = 10;      /* 演奏する曲数     */

void Usage()
{
    printf("rndplay Verson %s  by oga.\n",VER);
    printf("usage: rndplay [<path(.)>] [-s <suffix(mp3)>] [-n <num(10)>] [-t]\n");
    printf("       -t : print ID3 tag\n");
}

/*
 *  AddPathList() : パスリストにパスを追加する  
 *
 *  IN  path : 追加するパス名  
 *
 *  OUT num_ent : リスト数  
 *
 */
void InitPathList()
{
    cur_list_size = 1000;
    pathlist = (char **)malloc(sizeof(char *) * cur_list_size);
    memset(pathlist, 0, sizeof(char *) * cur_list_size);
}

/*
 *  AddPathList() : パスリストにパスを追加する  
 *
 *  IN  path : 追加するパス名  
 *
 *  OUT num_ent : リスト数  
 *
 */
void AddPathList(char *path)
{
    char **newlist;

    dprintf("add list %s\n", path);
    pathlist[num_ent++] = strdup(path);
    if (num_ent >= cur_list_size) {
        /* pathlistの拡張 */
        cur_list_size += 1000;
        newlist = (char **)malloc(sizeof(char *) * (cur_list_size));
	memcpy(newlist, pathlist, sizeof(char *) * (cur_list_size-1000));
	free(pathlist);
	pathlist = newlist;
    }
}

/*
 *  Find() : 指定パス以下のファイルを検索する
 *
 *  IN  path : 検索ルートパス名
 *      suff : 検索するファイルの拡張子  
 *
 *  OUT sz   : ファイルサイズ合計
 *      szb  : ブロックベースのファイルサイズ合計
 *
 */
void Find(char *path, char *suff)
{
    HANDLE fh;
    WIN32_FIND_DATA wfd;
    unsigned int  subblk;
    char   wk[MAX_PATH];
    char   wkmp3[MAX_PATH];
    int    tsv;
    int    tsvb;
    int    i;
    int    ast = 0;	/* 1:アスタリスク付き */
    int    len;
    int    name_match = 0;
    char   *pt;
    
    depth++;

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
            dprintf2("xxx path=%s\n",path);
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

    dprintf2("Find: Enter path=[%s] search=[%s]\n",path,wk);
    
    fh = FindFirstFile(wk,&wfd);
    if (fh == INVALID_HANDLE_VALUE) {
        printf("Find: FindFirstFile(%s) Error code=%d\n",wk,GetLastError());
        /* exit(1); */
	depth--; /* V1.11-A */
	return;  /* V1.11-C */
    }
    if (!IS_DOT(wfd.cFileName)) {
        name_match = 0;
	len = strlen(wfd.cFileName);
        if (len > 3 && !stricmp(&wfd.cFileName[len-strlen(suff)], suff)) {
	    /* mp3ファイルならば有効 */
	    name_match = 1;
	}

	if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            dprintf2("[%s] is DIR\n",wfd.cFileName);
            strcpy(wk,path);            /* restore base path */
	    if (strlen(wk) && wk[strlen(wk)-1] != '/') strcat(wk,"/");
            strcat(wk,wfd.cFileName);	/* make new path     */
	    Find(wk, suff);
	} else {
	    /* mp3ファイルの場合リスト追加 */
	    if (name_match) {
	        sprintf(wkmp3, "%s/%s",path, wfd.cFileName);
		AddPathList(wkmp3);
	    }
        }
    }
    while(FindNextFile(fh,&wfd)) {
        if (IS_DOT(wfd.cFileName)) {
            continue;
        }

        name_match = 0;
	len = strlen(wfd.cFileName);
        if (len > 3 && !stricmp(&wfd.cFileName[len-strlen(suff)], suff)) {
	    /* -name指定があって、名前が一致しない場合は表示しない */
	    name_match = 1;
	}

	if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            dprintf2("[%s] is DIR\n",wfd.cFileName);
            strcpy(wk,path);            /* restore base path */
	    if (strlen(wk) && wk[strlen(wk)-1] != '/') strcat(wk,"/");
            strcat(wk,wfd.cFileName);	/* make new path     */
	    Find(wk, suff);
	} else {
	    /* mp3ファイルの場合リスト追加 */
	    if (name_match) {
	        sprintf(wkmp3, "%s/%s",path, wfd.cFileName);
		AddPathList(wkmp3);
	    }
        }
    }
    FindClose(fh);

    depth--;

} /* End Find() */

/*
 *  Which() : PATHからコマンドを探す 
 *
 *  IN  command : コマンド名 
 *
 *  IN/OUT fullpath : フルパスコマンド (IN:MAX_PATHサイズ以上)
 *  OUT    ret  :  0: command found
 *                -1: command not found
 *
 */
int Which(char *command, char *fullpath)
{
	char path[2048];         /* stat path */
	char *pathenvp;
	char pathp[2048];        /* ENV path */
	int pathlen, comsize;
	int i, j;
	struct stat statb;
	int allf = 0;
	int found = 0;

	/* fullpath指定など、存在したらそれを返す */
	if (stat(command, &statb) == 0) {
	    strcpy(fullpath, command);
	    return 0;
	}

	strcpy(fullpath, "");

	dprintf2("command=%s\n",command);

	strcpy(pathp,".;");
	pathenvp="";
	pathenvp = (char *)getenv("PATH");

	dprintf2("PATH=%s\n",pathenvp);

	if (pathenvp) strcat(pathp,pathenvp);
	pathlen = strlen(pathp);

	dprintf2("path=%s\n",pathp);

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

		dprintf2("path=[%s]\n",path);

		strcat(path,"/");               /* path[] = "/usr/bin/"       */
		strcat(path,command);           /* path[] = "/usr/bin/com"    */
	    
		dprintf2("check path=%s\n",path);
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
		return -1;
	}
	return 0;
}

/*
 *  Play() : playする  
 *
 *  IN  playno : Play Sequence
 *      player : playerのパス 
 *      opt    : playerのオプション 
 *      list   : 曲リスト 
 *      num    : 曲番号  
 *  OUT ret    : 0 success
 *              -1 error
 */
int Play(int playno, char *player, char *opt, char **list, int num)
{
	BOOL 			status;
	STARTUPINFO 		si;
	PROCESS_INFORMATION 	pi;

	HANDLE	hProc;   /* Process Handle */
	int     pst;
	int     st = 0;  /* success */
	char    comarg[MAX_PATH];

        /* PrintId3(list[num]); */

        /* make command line */
	sprintf(comarg, "\"%s\" %s \"%s\"",player, opt, list[num]);
	dprintf("DEBUG: Play: %s %s\n", player, comarg);
	if (tf) {
	    printf("\n");  /* 見やすくするように */
	}
	printf("## No.%d/%d: [%d/%d]%s\n", 
	         playno+1, gnum,
	         num, num_ent, 
  	         list[num]);

	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
        status = CreateProcess(player,	/* 実行モジュール名            */
			comarg, 	/* コマンドライン              */
			NULL,		/* プロセスのセキュリティ属性  */
			NULL,		/* スレッドのセキュリティ属性  */
			FALSE,		/* ハンドルを継承しない        */
			0, 		/* fdwCreate                   */
			NULL,		/* 環境ブロックのアドレス      */
			NULL,		/* カレントディレクトリ(親同)  */
                        &si,            /* STARTUPINFO構造体           */
                        &pi);           /* PROCESS_INFORMATION構造体   */
        if(status != TRUE){
            printf("exec_cmd: CreateProcess returned=%d\n",GetLastError());
            return -1;	/* error */
        }

	hProc = pi.hProcess;
	status=CloseHandle(pi.hThread);
	if(status != TRUE){
	    printf("CloseHandle error(%d).\n", GetLastError());
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

	status=CloseHandle(hProc);
	if(status != TRUE){
		printf("CloseHandle error(%d).\n", GetLastError());
	}
	return 0;
}

int main(int a, char *b[])
{
    char *fname = ".";
    int  i;
    int  fn = 0;
    char player[MAX_PATH];
    char suffix[MAX_PATH];
    char *opt = NULL;               /* player option */
    time_t     tt;
#ifdef _WIN32
    SYSTEMTIME syst;
#endif /* _WIN32 */

    strcpy(player, "");
    strcpy(suffix, SUFF_MP3);

    for (i=1;i<a;i++) {
	if (i+1 < a && !strncmp(b[i],"-n",2)) {
	    gnum = atoi(b[++i]);
	    continue;
	}
	if (i+1 < a && !strncmp(b[i],"-s",2)) {
	    sprintf(suffix, ".%s", b[++i]);
	    continue;
	}
	if (!strncmp(b[i],"-t",2)) {
	    tf = 1;     /* print ID3 tag */
	    continue;
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
	/* 漢字の2バイト目でない'\'だったら削除 */
        fname[strlen(fname)-1] = '\0';
    }

    InitPathList();       /* パスリストの初期化 */
    Find(fname, suffix);  /* find(mp3) 実行 */

#ifndef _WIN32
    /* Windowsではなぜかいまいち */
    tt = time(0);
#else  /* _WIN32 */
    GetLocalTime(&syst);
    tt = syst.wHour * 3600 * 1000 +
         syst.wMinute * 60 * 1000 +
         syst.wSecond      * 1000 +
	 syst.wMilliseconds;
#endif /* _WIN32 */

    srand(tt); /* 乱数系列初期化 */

    if (!stricmp(suffix, SUFF_MP3) && Which(PLAYER_MP3, player)) {
        printf("%s not found\n", PLAYER_MP3);
    }

    /* set option */
    if (strlen(player)) {
        /* for mpg123   */
        if (tf) {
            opt = "";               /* print title  */
	} else {
            opt = "-q";             /* quiet mode   */
	}
    } else {
	opt = "/prefetch:6 /Play";  /* for WMPlayer */
    }

    if (strlen(player) == 0 && Which(PLAYER_MS, player)) {
        printf("%s not found\n", PLAYER_MS);
    }

    for (i = 0; i < gnum; i++) {
        Play(i, player, opt, pathlist, RAND(num_ent));
    }

    return 0;
}



