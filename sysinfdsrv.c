/*
 *   sysinfdsrv.c
 *     System Information Service Daemon
 *
 *   2001/12/03 V1.00  by oga.  (MEM, LOAD)
 *   2002/02/03 V1.01  Keep Connection
 *   2013/02/25 V1.02  add socket close()
 *   2013/11/21 V1.10  support Win
 *   2013/12/01 V1.20  support Win Service (sysinfdsrv)
 *   2013/12/06 V1.21  fix minus load for win.
 *   2013/12/06 V1.22  add non block accept code
 *
 *   Protocol
 *      HELP
 *         Return Help String\n\n
 *
 *      GET_MEM
 *         Get Memory/Swap Info (KB)
 *         Total Free Shared Buffers Cached SwapTotal SwapFree
 *         ---------------------------------------------------------
 *         69860 1520 4 3008 57292 51400 27728\n\n
 *
 *      GET_LOAD
 *         Get Load Ave. Info
 *         1min 5min 15min
 *         ---------------------------------
 *         0.00 0.00 0.00\n\n
 *
 *   All Rights Reserved. Copyright (C) 2001,2002, Moritaka Ogasawara. 
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#include <winsock.h>
#else  // Linux
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#endif // Linux


/* Keep Connection V1.01-A */
#define KEEP_CONN
#define GET_LOG      // ログ取得時指定  

#ifndef TCP_PORT
#define TCP_PORT 9998
#endif

#define SIZE	1024

#ifndef SIGCLD
#define SIGCLD SIGCHLD
#endif

/* request keywords */
#define GET_MEM		"GET_MEM"
#define GET_LOAD	"GET_LOAD"
#define LOGIN		"LOGIN"
#define HELP		"HELP"
#define EXIT		"EXIT"

/* for help */
#define HELP_STR	"GET_MEM  GET_LOAD  HELP  EXIT\n\n"

/* 仮のdefine */

#ifdef _WIN32
#define strncasecmp strnicmp
#define CLOSE	    closesocket

#define OPT_REG     1
#define OPT_DEL     2

#define EXENAME           "sysinfdsrv.exe"
#define EXENAME_NOEXT     "sysinfdsrv"
#define SERVICE_NAME      "Sysinfd"
#define SERVICE_DISPNAME  "Sysinfd"
//#define SERVICE_DISPNAME  "System Information Servcie"

// 64bit
typedef DWORDLONG mem_uint; 

//typedef struct _procarg_t {
//    int    clsockfd;   /* client socket */
//    int    con;        /* start number */
//} procarg_t;

//
// for getCpuUsage()
//
#define Li2Double(x) ((double)((x).HighPart) * 4.294967296E9 + (double)((x).LowPart))

typedef struct
{
	  DWORD   dwUnknown1;
	  ULONG   uKeMaximumIncrement;
	  ULONG   uPageSize;
	  ULONG   uMmNumberOfPhysicalPages;
	  ULONG   uMmLowestPhysicalPage;
	  ULONG   uMmHighestPhysicalPage;
	  ULONG   uAllocationGranularity;
	  PVOID   pLowestUserAddress;
	  PVOID   pMmHighestUserAddress;
	  ULONG   uKeActiveProcessors;
	  BYTE    bKeNumberProcessors;
	  BYTE    bUnknown2;
	  WORD    wUnknown3;
} SYSTEM_BASIC_INFORMATION;

typedef struct
{
	  LARGE_INTEGER		liIdleTime;
	  DWORD			dwSpare[76];
} SYSTEM_PERFORMANCE_INFORMATION;

typedef struct
{
	  LARGE_INTEGER liKeBootTime;
	  LARGE_INTEGER liKeSystemTime;
	  LARGE_INTEGER liExpTimeZoneBias;
	  ULONG		uCurrentTimeZoneId;
	  DWORD     	dwReserved;
} SYSTEM_TIME_INFORMATION;

typedef LONG (WINAPI *PROCNTQSI)(UINT,PVOID,ULONG,PULONG);

PROCNTQSI NtQuerySystemInformation;


#else  // Linux
#define HANDLE  int
#define CLOSE	close

// 64bit
typedef unsigned long long int mem_uint; 
#endif // Linux

typedef struct _mem_t {
    mem_uint total;
    mem_uint free;
    mem_uint shared;
    mem_uint buffers;
    mem_uint cached;
    mem_uint swap_total;
    mem_uint swap_free;
} mem_t;

typedef struct _load_t {
    int load1m;   // load ave. 1min  x100   1.23→123
    int load5m;   // load ave. 5min  x100
    int load15m;  // load ave. 15min x100
} load_t;


/* globals */
int     vf        = 0;  /* 1:verbose mode */
int     port      = 0;  /* port number    */
int	sockfd    = 0;
int	proc_stop = 0;  /* stop request   */

/* func for windows porting */
#ifdef _WIN32
SERVICE_STATUS        SvcStatus;
SERVICE_STATUS_HANDLE hSvcStatus;

HANDLE         hThreadMain;
HANDLE         hStopEvent;
#endif // _WIN32

void Log(char *msg)
{
    FILE   *logfp;
    char   *logpath;
    char   datetime[128];
    time_t tt;

#ifdef _WIN32
    logpath = "c:\\sysinfdsrv.log";
#else  // Linux
    logpath = "/tmp/sysinfdsrv.log";
#endif // Linux

    printf("%s\n", msg);

#ifdef GET_LOG
    if ((logfp = fopen(logpath, "a")) == NULL) {
	printf("log open error. errno = %d\n", errno);
	return;
    }

    tt = time(0);
    strftime(datetime, sizeof(datetime), "%Y/%m/%d %H:%M:%S", localtime(&tt));
    fprintf(logfp, "%s %s\n", datetime, msg);

    fclose(logfp);
#endif // GET_LOG
}

#ifdef _WIN32
/*
 *   READ(fd,buf,size)
 *
 *    OUT ret  : >= 0 success (read size)
 *             : -1 error
 */
int READ(HANDLE fd, void *buf, int count)
{
	int  rbytes = 0;
	BOOL ret;

	ret = ReadFile(
		fd,        // ファイルのハンドル
		buf,       // データバッファ
		count,     // 読み取り対象のバイト数
		&rbytes,   // 読み取ったバイト数
		NULL       // オーバーラップ構造体のバッファ
	);
	if (ret == FALSE) {
		return -1;
	}
	return rbytes;
}


/*
 *   WRITE(fd,buf,size)
 *
 *    OUT ret  : >= 0 success (write size)
 *             : -1   error
 */
int WRITE(HANDLE fd, void *buf, int count)
{
	int  wbytes = 0;
	BOOL ret;

	ret = WriteFile(
		fd,        // ファイルのハンドル
		buf,       // データバッファ
		count,     // 書き込み対象のバイト数
		&wbytes,   // 書き込んだバイト数
		NULL       // オーバーラップ構造体のバッファ
	);
	if (ret == FALSE) {
		return -1;
	}
	return wbytes;
}

// ntdll!NtQuerySystemInformation (NT specific!)
//
// The function copies the system information of the
// specified type into a buffer
//
// NTSYSAPI
// NTSTATUS
// NTAPI
// NtQuerySystemInformation(
//   IN   UINT   SystemInformationClass,   // information type
//   OUT  PVOID  SystemInformation,        // pointer to buffer
//   IN   ULONG  SystemInformationLength,  // buffer size in bytes
//   OUT  PULONG ReturnLength OPTIONAL     // pointer to a 32-bit
//					   // variable that receives
//					   // the number of bytes
//					   // written to the buffer
// );

#define SystemBasicInformation		0
#define SystemPerformanceInformation	2
#define SystemTimeInformation		3

/*
 *  getCpuUsage()
 *
 *    IN  interval: 利用率を計測するための間隔(msec)
 *    OUT      ret: CPU利用率 (%)
 */
int getCpuUsage(int interval)
{
    SYSTEM_PERFORMANCE_INFORMATION	SysPerfInfo;
    SYSTEM_TIME_INFORMATION		SysTimeInfo;
    SYSTEM_BASIC_INFORMATION	SysBaseInfo;
    double				dbIdleTime;
    double				dbSystemTime;
    LONG				status;
    LARGE_INTEGER		liOldIdleTime = {0,0};
    LARGE_INTEGER		liOldSystemTime = {0,0};

    int iCtl=0, percentage=0;

    NtQuerySystemInformation = (PROCNTQSI)GetProcAddress(
					GetModuleHandle("ntdll"),
					"NtQuerySystemInformation"
				);
    if (!NtQuerySystemInformation) {
	Log("GetProcAddress(NtQuerySystemInformation) error.");
	return -1;
    }

    // get number of processors in the system
    status = NtQuerySystemInformation(SystemBasicInformation,&SysBaseInfo,sizeof(SysBaseInfo),NULL);
    if(status != NO_ERROR) {
	Log("NtQuerySystemInformation1(SystemBasicInformation) error.");
	return -1;
    }

    //printf("\n getting CPU Usage\n");
    //while(!_kbhit())

    while(1) {
        // get new system time
        status = NtQuerySystemInformation(SystemTimeInformation,&SysTimeInfo,sizeof(SysTimeInfo),0);
        if(status!=NO_ERROR) {
	    Log("NtQuerySystemInformation2(SystemTimeInformation) error.");
            return -1;
	}

        // get new CPU's idle time
        status = NtQuerySystemInformation(SystemPerformanceInformation,&SysPerfInfo,sizeof(SysPerfInfo),NULL);
        if(status != NO_ERROR) {
	    Log("NtQuerySystemInformation3(SystemPerformanceInformation) error.");
            return -1;
	}

        // if it's a first call - skip it
        if(liOldIdleTime.QuadPart != 0) {
            // CurrentValue = NewValue - OldValue
            dbIdleTime   = Li2Double(SysPerfInfo.liIdleTime)     - Li2Double(liOldIdleTime);
            dbSystemTime = Li2Double(SysTimeInfo.liKeSystemTime) - Li2Double(liOldSystemTime);

            // CurrentCpuIdle = IdleTime / SystemTime
            dbIdleTime = dbIdleTime / dbSystemTime;

            // CurrentCpuUsage% = 100 - (CurrentCpuIdle * 100) / NumberOfProcessors
            dbIdleTime = 100.0 - dbIdleTime * 100.0 / (double)SysBaseInfo.bKeNumberProcessors + 0.5;

            //+ 0.5, result is same in task manager
            percentage= (UINT)(dbIdleTime);
	    if (percentage < 0) percentage = 0;  // V1.21-A

            //printf("\b\b\b\b%3d%%", percentage);
            if(iCtl > 0 ) return percentage;
        }

        // store new CPU's idle and system time
        liOldIdleTime   = SysPerfInfo.liIdleTime;
        liOldSystemTime = SysTimeInfo.liKeSystemTime;

        // wait one second
        Sleep(interval);
        iCtl++;
    }
    return 0;
}

#endif // !_WIN32

/* wait child process */
void reapchild()
{
#ifndef _WIN32
	wait(0);
	signal(SIGCLD,reapchild);
#endif
}

/* end process */
void sigint()
{
    Log("sysinfd interrupted.\n");
    if (sockfd) close(sockfd);
    exit(1);
}

#ifdef _WIN32
/*
 *  dwCtrlType
 *    CTRL_C_EVENT        : Ctrl+C
 *    CTRL_BREAK_EVENT    : Ctrl+Break
 *    CTRL_CLOSE_EVENT    : Close Console
 *    CTRL_LOGOFF_EVENT   : Logoff
 *    CTRL_SHUTDOWN_EVENT : Shutdown
 */
BOOL WINAPI CtrlProc(DWORD dwCtrlType)
{
    // Stop all calse
    Log("sysinfd interrupted2.\n");
    if (sockfd) closesocket(sockfd);
    WSACleanup();

    return FALSE;  // FALSE: NextHandler(END)    TRUE: not END
}


/* 
 *  InstallService()
 *    サービス制御マネージャ・データベースにサービスを登録する。 
 *
 *    IN  exefile: サービスで実行するexeファイル(完全パス) 
 */
void InstallService(SC_HANDLE hSCManager, char *exefile)
{
    SC_HANDLE hService;    // サービス・ハンドル

    /* サービスの登録 */
    hService = CreateService(
                hSCManager,                   // データベース・ハンドル
                SERVICE_NAME,                 // サービス名
                SERVICE_DISPNAME,             // サービス表示名
                SERVICE_ALL_ACCESS,           // 全アクセスの許可
                SERVICE_WIN32_OWN_PROCESS,    // 独立の Win32 サービス・プロセス
                SERVICE_DEMAND_START,         // 起動要求時に開始
                SERVICE_ERROR_NORMAL,         // 起動失敗時にメッセージを表示
                exefile,                      // 実行ファイル名
                NULL,                         // ロード順序グループなし
                NULL,                         // タグは不要
                NULL,                         // 依存グループなし
                NULL,                         // LocalSystem アカウントを使用
                NULL);                        // パスワードなし

    /* 後処理 */
    if (hService == NULL) {
        printf("CreateService(%ld) error.\n", GetLastError());
    } else {
        printf("Service [%s] registed.\n", SERVICE_NAME);
        CloseServiceHandle(hService);
    }
}

/* 
 *  RemoveServcie()
 *    サービス制御マネージャ・データベースからサービスを削除する。 
 */
void RemoveService(SC_HANDLE hSCManager)
{
    SC_HANDLE hService;    // サービス・ハンドル

    /* サービスのオープン */
    hService = OpenService(
                hSCManager,             // データベース・ハンドル
                SERVICE_NAME,           // サービス名
                SERVICE_ALL_ACCESS);    // 全アクセス・タイプを許可

    if (hService == NULL) {
        printf ("OpenService(%ld) error.\n", GetLastError());
        return;
    }

    /* サービスの削除 */
    if (! DeleteService(hService)) {
        printf ("DeleteService(%ld) error.\n", GetLastError());
    } else {
        printf ("Service [%s] deleted.\n", SERVICE_NAME);
    }

    /* サービス・ハンドルをクローズ */
    CloseServiceHandle(hService);
}

/*
 *  RegDelSerivce()
 *    サービスの登録/削除 
 *
 *  IN  opt : OPT_REG:サービス登録  OPT_DEL:サービス削除 
 *  IN  path: 登録するexeのパス (NULLの場合、カレントディレクトリ\自exe)
 *  OUT ret : 0: success <>0: failed
 */
int RegDelService(int opt, char *path)
{
    
    DWORD       Operation = 0;    // 処理指定子
    DWORD       errNum = 0; 
    char        exepath[4096];
    struct stat stbuf;
    SC_HANDLE   hSCManager;
    

    if (path) {
        strcpy(exepath, path);
    } else {
        getcwd(exepath, sizeof(exepath)-1);
        strcat(exepath, "\\");
        strcat(exepath, EXENAME);
	if (vf) printf("exepath = [%s]\n", exepath);
    }

    if (stat(exepath, &stbuf)) {
	printf("file not found. (%s)\n", exepath);
	return -1;
    }

    
    // サービス制御マネージャ・データベースをオープン 
    hSCManager = OpenSCManager(
                    NULL,                      // ローカル・システム
                    NULL,                      // デフォルトのデータベース
                    SC_MANAGER_ALL_ACCESS);    // 全アクセス・タイプを許可
    if (hSCManager == (SC_HANDLE)NULL) {
        errNum = GetLastError();
        printf("OpenSCManager(%ld) error.\n", errNum);
        return errNum;
    }

    switch (opt) {
      case OPT_REG:
        InstallService(hSCManager, exepath);
        break;
      case OPT_DEL:
        RemoveService(hSCManager);
        break;
      default:
        printf("RegDelSerivce: unknown option [%d]\n", opt);
        break;
    }

    /* データベースをクローズ */
    CloseServiceHandle(hSCManager);

    return 0;
}
#endif  // _WIN32


char *get_item(char *buf, char *sep, int pos, char *outbuf)
{
    int i;
    char *pt;
    char *p;
    char wk[4096];
    char msglog[4096];

    strcpy(wk,buf);      /* strtok()はbufを破壊するためコピーして利用する */

    for (i = 0; i<pos; i++) {
	if (i == 0) {
	    pt = (char *)strtok(wk,sep);
	} else {
	    pt = (char *)strtok(NULL,sep);
	}
	if (pt == NULL) break;
    }
    if (pt == NULL) {
	sprintf(msglog, "Out of item(%s) pos(%d).",buf,pos);
	Log(msglog);
	/* 結果領域クリア */
	strcpy(outbuf,"");
	return NULL;
    }

    strcpy(outbuf, pt);

    /* cut tail space */
    p = &outbuf[strlen(outbuf)-1];	/* last char */
    while (*p == ' ' || *p == 0x0a) --p;
    *(p+1) = '\0';

    return outbuf;
} /* get_item */


/*
 *   fd から読み込んだ内容をnewsockfdに書き込む
 *
 *   not used
 */
void PUT_DATA(int fd, int newsockfd)
{
	char c[4096];
	int size;
        while((size = recv(fd, c, SIZE, 0)) != 0) {
            send(newsockfd, c, size, 0);
	}
}


/*
 *   Help(fd)
 *      send HELP
 *
 *    IN  fd   : output fd
 *    OUT ret  : 0  success
 *             : -1 error
 */
int Help(int fd)
{
    if (vf) printf("%s", HELP_STR);
    send(fd, HELP_STR, strlen(HELP_STR), 0);
    return 0;
}

#ifdef _WIN32
/*
 *   GetMemInfo(mem_t *memdat) for Win
 *
 *    OUT memdat : memory data
 *        ret  : 0 : success
 *               -1: error
 */
int GetMemInfo(mem_t *memdat)
{
    MEMORYSTATUSEX mst;                          // WDF11502-C
    int    i;
    int    ret;
    int    loadavg;

    mst.dwLength = sizeof(MEMORYSTATUSEX);       // WDF11502-A

    // メモリ情報取得
    // upper 4GB
    GlobalMemoryStatusEx(&mst);
    memdat->total      = mst.ullTotalPhys/1024;
    memdat->free       = mst.ullAvailPhys/1024;
    memdat->shared     = 0;
    memdat->buffers    = 0;
    memdat->cached     = 0;
    memdat->swap_total = mst.ullTotalPageFile/1024;
    memdat->swap_free  = mst.ullAvailPageFile/1024;

    if (vf) {
        printf("cur_mem=%I64u\n", 
            memdat->total - memdat->free - memdat->buffers - memdat->cached);
    }

    return 0;
}

/*
 *   GetLoadInfo(mem_t *memdat) for Win
 *
 *    OUT loaddat : load data
 *        ret     : 0 : success
 *                  -1: error
 */
int GetLoadInfo(load_t *loaddat)
{
    int usage = 0;

    usage = getCpuUsage(100);

    loaddat->load1m  = usage;
    loaddat->load5m  = usage;
    loaddat->load15m = usage;

    return 0;
}

#else  // Linux
/*
 *   GetMemInfo(mem_t *memdat) for Linux
 *
 *    OUT memdat : memory data
 *        ret    : 0 : success
 *                 -1: error
 */
int GetMemInfo(mem_t *memdat)
{
    char  buf[2048];
    char  wk[2048];
    FILE  *fp;
    char  *path = "/proc/meminfo";
    //ulong total, freem, shared, buffer, cache, swtotal, swfree;

    memset(memdat, 0, sizeof(mem_t));

    //total = freem = shared = buffer = cache = swtotal = swfree = 0;

    if (!(fp = fopen(path, "r"))) {
        sprintf(buf,"GetMem: fopen(%s) error errno=%d\n", path, errno);
	printf(buf);
        //send(fd, buf, strlen(buf), 0);
	return -1;
    }
    while (fgets(buf, sizeof(buf), fp)) {
        if (!strncmp(buf, "MemTotal:", strlen("MemTotal:"))) {
	    memdat->total   = atoi(get_item(buf, " ", 2, wk));    /* 1 */

	} else if (!strncmp(buf, "MemFree:", strlen("MemFree:"))) {
	    memdat->free    = atoi(get_item(buf, " ", 2, wk));    /* 2 */

	} else if (!strncmp(buf, "MemShared:", strlen("MemShared:"))) {
	    memdat->shared  = atoi(get_item(buf, " ", 2, wk));  /* 3 */

	} else if (!strncmp(buf, "Buffers:", strlen("Buffers:"))) {
	    memdat->buffers = atoi(get_item(buf, " ", 2, wk));  /* 4 */

	} else if (!strncmp(buf, "Cached:", strlen("Cached:"))) {
	    memdat->cached  = atoi(get_item(buf, " ", 2, wk));  /* 5 */

	} else if (!strncmp(buf, "SwapTotal:", strlen("SwapTotal:"))) {
	    memdat->swap_total = atoi(get_item(buf, " ", 2, wk));  /* 6 */

	} else if (!strncmp(buf, "SwapFree:", strlen("SwapFree:"))) {
	    memdat->swap_free  = atoi(get_item(buf, " ", 2, wk));  /* 7 */
	}
    }
    fclose(fp);

    return 0;
}

/*
 *   GetLoadInfo(mem_t *memdat) for Linux (現在使用せず /proc/loadavgから取得)
 *
 *    OUT loaddat : load data
 *        ret     : 0 : success
 *                  -1: error
 */
int GetLoadInfo(load_t *loaddat)
{
    loaddat->load1m  = 0;
    loaddat->load5m  = 0;
    loaddat->load15m = 0;

    return 0;
}
#endif // Linux

/*
 *   GetMem(fd)
 *
 *    IN  fd   : output fd
 *    OUT ret  : 0  success
 *             : -1 error
 */
int GetMem(int fd)
{
    char  buf[2048];
    int   ret;
    mem_t memdat;

    memset(&memdat, 0, sizeof(mem_t));

    ret = GetMemInfo(&memdat);  // no error
    if (ret < 0) return ret;

#ifdef _WIN32
    sprintf(buf, "%I64u %I64u %I64u %I64u %I64u %I64u %I64u\n\n", 
                  memdat.total, 
		  memdat.free, 
		  memdat.shared, 
		  memdat.buffers, 
		  memdat.cached, 
		  memdat.swap_total, 
		  memdat.swap_free);
#else  // Linux
    sprintf(buf, "%llu %llu %llu %llu %llu %llu %llu\n\n", 
                  memdat.total, 
		  memdat.free, 
		  memdat.shared, 
		  memdat.buffers, 
		  memdat.cached, 
		  memdat.swap_total, 
		  memdat.swap_free);
#endif // Linux

    if (vf) printf("%s", buf);

    return send(fd, buf, strlen(buf), 0);
}

/*
 *   GetLoad(fd)
 *
 *    IN  fd   : output fd
 *    OUT ret  : 0  success
 *             : -1 error
 */
int GetLoad(int fd)
{
    char buf[2048];

#ifdef _WIN32
    load_t loaddat;

    memset(&loaddat, 0, sizeof(load_t));

    GetLoadInfo(&loaddat);

    sprintf(buf, "%1.2f %1.2f %1.2f\n\n", 
	    ((float)loaddat.load1m)/100, 
	    ((float)loaddat.load5m)/100, 
	    ((float)loaddat.load15m)/100);

#else  // Linux
    FILE *fp;
    char *path = "/proc/loadavg";
    char *pt;

    if (!(fp = fopen(path, "r"))) {
        sprintf(buf,"GetLoad: fopen(%s) error errno=%d\n", path, errno);
	printf(buf);
        send(fd, buf, strlen(buf), 0);
	return -1;
    }
    if (fgets(buf,sizeof(buf),fp) == NULL) {
        sprintf(buf,"GetLoad: fgets(%s) error errno=%d\n", path, errno);
	printf(buf);
        send(fd, buf, strlen(buf), 0);
        fclose(fp);
	return -1;
    }
    fclose(fp);

    /* "0.00 0.00 0.00 2/42 494"  => "0.00 0.00 0.00" */
    pt = strchr(buf, ' ');
    ++pt;
    pt = strchr(pt, ' ');
    ++pt;
    pt = strchr(pt, ' ');
    *pt = '\n';
    ++pt;
    *pt = '\n';
    ++pt;
    *pt = '\0';
#endif // Linux

    if (vf) printf("%s", buf);

    return send(fd, buf, strlen(buf), 0);
}

/*
 *   ErrorMsg(fd,str)
 *      send error message
 *
 *    IN  fd   : output fd
 *        str  : request string
 *    OUT ret  : 0  success
 *             : -1 error
 */
int ErrorMsg(int fd, char *str)
{
    char buf[2048];

    sprintf(buf,"%s request could not understand.\n",str); 

    if (vf) printf("%s", buf);

    return send(fd,buf,strlen(buf), 0) ;
}

/* 
 *  メッセージ受信と処理
 *
 *    IN  sfd : accept fd
 *    OUT ret : 0  success
 *            : -1 error
 */
int GetAndProc(int sfd)
{
	char buf[SIZE];
	char sndbuf[SIZE];
	int  size;
	int  st = 0;

	/* get request */
	if (vf) printf("recv...\n");
	size = recv(sfd, buf, SIZE, 0);
	if (size < 0) {
	    perror("read");
	    return -1;
	}

	/* delete CR/LF */
	if (buf[size-2] == 0x0d && buf[size-1] == 0x0a) {
	    buf[size-2] = '\0';
	} else if (buf[size-1] == 0x0a) {
	    buf[size-1] = '\0';
	} else {
	    buf[size] = '\0';
	}

	/* dispatch request process */
	if (!strncasecmp(buf,HELP,strlen(HELP))) {
	    /* HELP */
    	    if (vf) printf("HELP\n");
	    st = Help(sfd);
	} else if (!strncasecmp(buf, EXIT, strlen(EXIT))) {
	    /* EXIT */
	    if (vf) printf("exit.\n");
	    st = -1;
	} else if (!strncasecmp(buf, GET_MEM, strlen(GET_MEM))) {
	    /* GET_MEM */
    	    if (vf) printf("GET_MEM\n");
	    st = GetMem(sfd);
	} else if (!strncasecmp(buf, GET_LOAD, strlen(GET_LOAD))) {
	    /* GET_LOAD */
    	    if (vf) printf("GET_LOAD\n");
	    st = GetLoad(sfd);
	} else {
	    st = ErrorMsg(sfd,buf);
	}
	return st;
}

#ifdef _WIN32
int ThreadGetAndProc(void *parm)
{
    int ret;
    int newsockfd = (int)parm;

#ifdef KEEP_CONN
    do {
#endif
    	if (vf) printf("Start GetAndProc...\n");
    	ret = GetAndProc(newsockfd);
#ifdef KEEP_CONN
    } while (ret >= 0);  // while not "exit" command
#endif

    if (vf) printf("close socket...\n");
    closesocket(newsockfd);

    if (vf) printf("Thread End.\n");
    return 0;
}
#endif  // _WIN32

int ServiceMainFunc()
{
	int 	newsockfd;
	int     clilen, childpid;
	struct 	sockaddr_in cli_addr,serv_addr;
	int 	fd;
	char 	*sendfile;
	int	con = 0;
	int     ret  = 0;
	char    msglog[4096];
	fd_set  rfds;           /* V1.22-A for select  */
	struct  timeval tmout;  /* V1.22-A for select  */

#ifdef _WIN32
	WSADATA WsaData;
	HANDLE  thd;            /* thread handle  */
	DWORD   threadID = 0;   /* thread ID      */
#endif

	Log("ServiceMainFunc Start ...");
#ifdef _WIN32
	if (ret = WSAStartup(0x0101, &WsaData)) {
	    sprintf(msglog, "WSAStartup Error: code = %d\n", ret);
	    Log(msglog);
	    exit(1);
	}
#endif // _WIN32

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(-1);
	} 

	memset((char *)&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons((short)port);

	if (vf) printf("wait on port (%d)\n", port);

	if(bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
		perror("bind");
		exit(-1);
	} 


	if (vf) printf("listen...\n");
	if(listen(sockfd, 5) < 0){
		perror("listen");
		exit(-1);
	}

	while (1) {
	    // V1.22-A start
	    if (vf) printf("check incoming...\n");
	    tmout.tv_sec  = 0;         // timeout sec
	    tmout.tv_usec = 200*1000;  // timeout usec  (0.2sec)
	    while (1) {
	         FD_ZERO(&rfds);
	         FD_SET(sockfd, &rfds);
	         ret = select(0, &rfds, 0, 0, &tmout);
		 //printf("select return %d\n", ret);
		 if (ret > 0) break;   // timeout:0 incoming:>0
		 if (proc_stop) break; // service stop request
	    }
	    if (proc_stop) break;      // service stop request
	    // V1.22-A end

	    if (vf) printf("Start accept...\n");
	    clilen = sizeof(cli_addr);
	    newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr,
				&clilen);
	    if(newsockfd < 0){
	        if (errno == EINTR) {
	            if (vf) printf("accept interrupted\n");
	            continue;
	        }
		perror("accept");
		exit(-1);
	    }
	
#ifdef _WIN32
	    //procarg_t procarg;

	    //procarg.clsockfd = newsockfd;
	    //procarg.con      = con;
	    
	    if (vf) printf("Create New Thread. (%d)\n", con);

            thd = CreateThread(
                        NULL,
                        0,
                        (LPTHREAD_START_ROUTINE)ThreadGetAndProc,
                        (void *)newsockfd,
                        0,
                        &threadID);
            if (thd == NULL) {
                sprintf(msglog, "CreateThread Error (%d)\n", GetLastError());
		Log(msglog);
                return -1;
            }

#else  // Linux
	    if (fork() == 0) {
	        /* this is child process */
	        close(sockfd);     /* close server socket */
	        if (vf) printf("accept %d pid=%d\n", con, getpid());

	        /* サーバ処理 */	
#ifdef KEEP_CONN
                while (1) {
		    /* もらったデータに対する処理 */
	            if (GetAndProc(newsockfd) < 0) {
		        if (vf) printf("Connection closed.\n");
	    		close(newsockfd);   /* close accept sockfd V1.02-A */
	                exit(0);
		    }
		}
#else  // KEEP_CONN
	        GetAndProc(newsockfd);      /* もらったデータに対する処理 */
    		close(newsockfd);           /* close accept sockfd V1.02-A */
	        exit(0);
#endif // !KEEP_CONN
	    } 
	    close(newsockfd);               /* close accept sockfd */
#endif // Linux

	    con++;
        } // accept while loop

	Log("ServiceMainFunc End ...");
	return 0;
}

#ifdef _WIN32
/*
 *  AbortService()
 *    サービスの起動中断
 *      (ServiceMain()からコールされる)
 */
void AbortService()
{
    Log("Abort Service.\n");
    //if (sockfd) closesocket(sockfd);
    //WSACleanup();

    exit(1);
}

/*
 *  SerivceCtrl()
 *    サービス制御ハンドラ
 * 
 *    IN  CtrlCode: 制御コード  
 */
void ServiceCtrl(DWORD CtrlCode)
{
    DWORD State = SERVICE_RUNNING;    // サービス状態
    DWORD ChkPoint = 0;               // チェック・ポイント値
    DWORD Wait = 0;                   // 処理待機時間（ミリ秒）

    /* 制御コード別にディスパッチ */
    switch(CtrlCode) {
    /* サービスの休止 */
    case SERVICE_CONTROL_PAUSE:
	Log("ServiceCtrl: SERVICE_CONTROL_PAUSE");
        if (SvcStatus.dwCurrentState == SERVICE_RUNNING) {
            SuspendThread(hThreadMain);
            State = SERVICE_PAUSED;
        }
        break;

    /* 休止サービスの再開 */
    case SERVICE_CONTROL_CONTINUE:
	Log("ServiceCtrl: SERVICE_CONTROL_CONTINUE");
        if (SvcStatus.dwCurrentState == SERVICE_PAUSED) {
            ResumeThread(hThreadMain);
            State = SERVICE_RUNNING;
        }
        break;

    /* サービスの停止 */
    case SERVICE_CONTROL_STOP:
	Log("ServiceCtrl: SERVICE_CONTROL_STOP");
        State           = SERVICE_STOP_PENDING;
        ChkPoint        = 1;
        Wait            = 3000;
        SetEvent(hStopEvent);
        break;

    /* サービス状態の更新 */
    case SERVICE_CONTROL_INTERROGATE:
	Log("ServiceCtrl: SERVICE_CONTROL_INTERROGATE");
        break;

    /* 無効な制御コード */
    default:
        break;
    }
    /* 更新されたサービス状態をサービス制御マネージャに通知 */
    ReportStatus(State, ChkPoint, Wait);
}

/*
 *  ReportStatus()
 *    サービス制御マネージャ(SCM)に状態通知  
 * 
 *  IN  State   : サービス状態  
 *  IN  ChkPoint: チェック・ポイント値  
 *  IN  Wait    : 処理待機時間(msec)
 * 
 */
BOOL ReportStatus(DWORD State, DWORD ChkPoint, DWORD Wait)
{
    BOOL  status;
    DWORD AcceptControl = (SERVICE_ACCEPT_PAUSE_CONTINUE |
                           SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN); 
    char  msglog[4096];

    /* サービス情報の設定 */
    SvcStatus.dwCurrentState = State;
    if (State == SERVICE_START_PENDING) {
        SvcStatus.dwControlsAccepted = 0;
    } else {
        SvcStatus.dwControlsAccepted = AcceptControl;
    }
    SvcStatus.dwCheckPoint   = ChkPoint;
    SvcStatus.dwWaitHint     = Wait;

    /* サービス情報を通知 */
    status = SetServiceStatus(hSvcStatus, &SvcStatus);
    if (!status){
        sprintf(msglog, "SetServiceStatus(%ld) error.", GetLastError());
	Log(msglog);
        AbortService();
    }
    return status;
}

/*
 *  ServiceMain()
 *    Windowsサービスのメイン関数  
 *      (1)SCMへの制御ハンドラ(CtrlHandler)登録 
 *      (2)サーバスレッド(ServiceMainFunc)の生成 
 *      (3)サービス停止イベント待ち  
 *      (4)終了処理  
 */
void ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv)
{
    DWORD dwErrorNumber;
    DWORD dwThreadID;
    char  msglog[4096];

    /* 制御ハンドラ関数の登録 */
    hSvcStatus = RegisterServiceCtrlHandler(
                    SERVICE_NAME,
                    (LPHANDLER_FUNCTION)ServiceCtrl);
    if (hSvcStatus == (SERVICE_STATUS_HANDLE)NULL) {
        dwErrorNumber = GetLastError();
        sprintf(msglog, "Can't regist CtrlHandler. ret = %d", dwErrorNumber);
	Log(msglog);
        return;
    }

    //dwErrorNumber = GetFnameFromRegistry("ServiceErrorLog"); 

    /* サービス状態に依存しないサービス情報の設定 */
    SvcStatus.dwServiceType             = SERVICE_WIN32_OWN_PROCESS;
    SvcStatus.dwWin32ExitCode           = NO_ERROR;
    SvcStatus.dwServiceSpecificExitCode = 0;

    /* サービス起動中であることをサービス制御マネージャに通知 */
    Log("ServiceMain: report status pending 1");
    if (!ReportStatus(SERVICE_START_PENDING, 1, 8000)) return;

    /* サービス停止を指示するためのイベントを作成 */
    hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (hStopEvent == NULL) {
        sprintf(msglog, "CreateEvent(%ld) error.", GetLastError());
	Log(msglog);
        AbortService();
        return;
    }

    /* サービス停止用イベントの作成をサービス制御マネージャに通知 */
    Log("ServiceMain: report status pending 2 / CreateThread(ServiceMainFunc)");
    if (!ReportStatus(SERVICE_START_PENDING, 2, 8000)) return;

    /* サービス提供用スレッドの作成 */
    hThreadMain = CreateThread(NULL, 0,
                        (LPTHREAD_START_ROUTINE)ServiceMainFunc,
                        NULL, 0, &dwThreadID);
    if (hThreadMain == (HANDLE)NULL) {
        sprintf(msglog, "CreateThread(%ld) error.", GetLastError());
	Log(msglog);
        AbortService();
        return;
    }

    /* サービス・スレッドの実行開始をサービス制御マネージャに通知 */
    Log("ServiceMain: report status (running)");
    if (!ReportStatus(SERVICE_RUNNING, 0, 1000)) return;

    /* "hStopEvent" がシグナル状態になるまで待機 */
    WaitForSingleObject(hStopEvent, INFINITE);

    Log("ServiceMain: stop event received.");

    proc_stop = 1;   // V1.22-A
    //Sleep(1000);     // V1.22-A
    WaitForSingleObject(hThreadMain, INFINITE);
    Log("ServiceMain: Main Thread End.");

    /* socketのクローズ */
    closesocket(sockfd);
    WSACleanup();

    /* サービスの停止処理 */
    Log("ServiceMain: report status (stopped)");
    if (!ReportStatus(SERVICE_STOPPED, 0, 1000)) {
        Log("ReportStatus(SERVICE_STOPPED) error.");
        return;
    }

    Log("ServiceMain: end.");

    if (hStopEvent != NULL) {
        CloseHandle(hStopEvent);
    }

    return;
}
#endif // _WIN32


/*   [Linux]
 *   main()
 *   +-ServiceMainFunc()
 *     +-accept()
 *     +-fork()
 *       +-GetAndProc()
 *
 *   [Win]
 *   main()
 *   +-StartServiceCtrlDispatcher(DispatchTable[ServiceMain])) 
 *
 *   ServiceMain()
 *   +-エラー時AbortService()
 *   +-CreateThread(ServiceMainFunc)
 *   +-WaitForSingleObject(hStopEvent, INFINITE); 停止イベント待ち
 *
 *   ServiceMainFunc()
 *     +-accept()
 *     +-CreateThread(ThreadGetAndProc)
 *
 *   ThreadGetAndProc()
 *       +-GetAndProc()
 *
 */
int main(int a, char *b[])
{
    int  i;
    int  regdel = 0;
    int  ret    = 0;
    char msglog[4096];

#ifdef _WIN32
    DWORD errNum;
    SERVICE_TABLE_ENTRY DispatchTable[] = {
        {SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain},
        {NULL, NULL} };    // 最後

    SetConsoleCtrlHandler(CtrlProc, TRUE);
#endif

    /* pars args */
    for (i = 1; i<a; i++) {
            if (!strncmp(b[i],"-h",2)) {
		printf("usage : %s [-v] [<portnum>]\n", EXENAME_NOEXT);
#ifdef _WIN32
		printf("        %s {-regsrv|-delsrv}\n", EXENAME_NOEXT);
#endif
		exit(1);
            }
            if (!strncmp(b[i],"-v",2)) {
		vf = 1;
		continue;
            }
#ifdef _WIN32
            if (!strcmp(b[i],"-regsrv")) {
		regdel = OPT_REG;
		continue;
            }
            if (!strcmp(b[i],"-delsrv")) {
		regdel = OPT_DEL;
		continue;
            }
#endif
            port = atoi(b[i]);
    }

    if (!port) {
        port = TCP_PORT;
    }

#ifndef _WIN32
    signal(SIGCLD, reapchild);
    signal(SIGINT, sigint);
#endif

#ifdef _WIN32
    if (regdel) {
        ret = RegDelService(regdel, NULL);
        return ret;
    }

    // メインスレッドをサービス制御マネージャに接続
    if (!StartServiceCtrlDispatcher(DispatchTable)) {
        errNum = GetLastError();
        sprintf(msglog, "StartServiceCtrlDispatcher(%ld) error.", errNum);
	Log(msglog);
    }
#else  // Linux
    ServiceMainFunc();
#endif // Linux


	return 0;
}

/* vim:ts=8:sw=4:
 */


