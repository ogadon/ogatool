/*
 *  readevt
 *     eventlogk�̃_���v
 *
 *     10/02/03 V0.10 by oga. (http://nienie.com/~masapico/api_ReadEventLog.html)
 *     10/02/08 V0.20 support DB,FW collaboration
 *     10/02/22 V0.30 support execute netstat -an
 *     10/02/26 V0.31 delete bksv access on -fw
 *     10/02/27 V0.32 Stop IIS
 *     10/02/27 V0.33 Stop WWW
 *     10/03/02 V0.34 Kill WinDump
 *     10/03/03 V0.35 Fix WWW Service Name
 *     10/03/04 V0.36 Fix -s option and support -w
 *     10/03/24 V0.37 expand message buffer
 *     10/04/09 V0.38 support no bkserver environment
 *     10/04/15 V0.39 change send message host
 *
 */

#ifdef _WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include "tlist_common.h"

#define VER             "0.39"
#define TARGET_WIN      "Wireshark: Capture from VMware Accelerated AMD PCNet Adapter"
#define TARGET_WIN_FW1  "Wireshark: Capture from Realtek RTL8168D/8111D Family PCI-E GBE NIC"
#define TARGET_WIN_FW2  "Wireshark: Capture from USB2.0 to Gigabit Ethernet Adapter"
//#define TARGET_WIN_DBG  "Microsoft Word"
#define TARGET_WIN_DBG  "�C�x���g �r���[�A"
#define TARGET_PROC_WINDUMP  "WinDump"

/* wait msec */
#define WAIT_VAL        (3*60*1000)
#define BKSV_DIR        "\\\\gaplus\\oga\\tool"     /* V0.39-C */
#define FW_DIR          "\\\\gaplus\\oga\\tool"     /* V0.39-C */

#define SERVICE_IIS     "IIS Admin Service"        
//#define SERVICE_WWW     "World Wide Web Publishing Service"  /* for -dbg */
#define SERVICE_WWW     "W3SVC"  /* for -dbg */

#define MAX_FCNT        40

int vf = 0;                /* -v         */
int csvf = 0;              /* -csv       */
int dbgf = 0;              /* -dbg debug */
int dbf  = 0;              /* -db  dbsv  */
int fwf  = 0;              /* -fw  F/W   */
int daemonf = 0;           /* -d         */
char *bksv_dir = BKSV_DIR; /* bksv_dir   */
char *fw_dir   = FW_DIR;   /* F/W dir    */
//char *service_iis = SERVICE_IIS;   /* IIS */
char *service_iis = SERVICE_WWW;   /* WWW */

/* event log check span (sec) */
int check_span = 1*3600;   /* 1h   */

/* check messsage */
char check_msg[4096];
char check_msg2[4096];

/* Group code */
char *grp_code = "com";

/* wait val */
int  wait_val = WAIT_VAL; 

int  max_fcnt = MAX_FCNT; 

/* for tlist API */
#define MAX_TASKS           512

BOOL        ForceKill = TRUE;
TASK_LIST   tlist[MAX_TASKS];

void Log(char *msg)
{
	FILE *fp = NULL;
	if ((fp = fopen("readevt.log", "a")) == NULL) {
		perror("readevt.log");
		return;
	}
	fprintf(fp, "%s\n", msg);
	if (fp) fclose(fp);
}

//
//  KillProc
//
//  IN  pname - process name
//
int KillProc(char *apname)
{
	int    i;
	int    rval = 0;
	char   pname[MAX_PATH];
	char   tname[PROCESS_SIZE+5];
	char   *p;
	DWORD  numTasks;
	DWORD  ThisPid;
	TASK_LIST_ENUM te;

	// �啶���ɕϊ�   
	strcpy(pname, apname);
	strupr(pname);

	printf("kill %s ...\n", pname);

    //
    // Obtain the ability to manipulate other processes
    //
    EnableDebugPriv();

#if 0
    if (pid) {
		// pid�̏ꍇ   
        tlist[0].dwProcessId = pid;
        if (KillProcess( tlist, TRUE )) {
            printf( "process #%d killed\n", pid );
            return 0;
        } else {
            printf( "process #%d could not be killed\n" );
            return 1;
        }
    }
#endif

    //
    // get the task list for the system
    //
    numTasks = GetTaskList( tlist, MAX_TASKS );

    //
    // enumerate all windows and try to get the window
    // titles for each task
    //
    te.tlist = tlist;
    te.numtasks = numTasks;
    GetWindowTitles( &te );

    ThisPid = GetCurrentProcessId();

	if (vf) printf("numTasks = %d\n", numTasks);
    for (i=0; i<numTasks; i++) {
        //
        // this prevents the user from killing KILL.EXE and
        // it's parent cmd window too
        //
        if (ThisPid == tlist[i].dwProcessId) {
            continue;
        }
        if (MatchPattern( tlist[i].WindowTitle, "*KILL*" )) {
            continue;
        }

        tname[0] = 0;
		
		if (vf) printf("Process[%3d] %4d = %s (%s) %d\n", i, tlist[i].dwProcessId, tlist[i].ProcessName, tlist[i].WindowTitle, strlen(tlist[i].ProcessName));
        strcpy( tname, tlist[i].ProcessName );
        p = strchr( tname, '.' );
        if (p) {
            p[0] = '\0';
        }
        if (MatchPattern( tname, pname )) {
            tlist[i].flags = TRUE;
        } else if (MatchPattern( tlist[i].ProcessName, pname )) {
            tlist[i].flags = TRUE;
        } else if (MatchPattern( tlist[i].WindowTitle, pname )) {
            tlist[i].flags = TRUE;
        }
    }

    for (i=0; i<numTasks; i++) {
        if (tlist[i].flags) {
            if (KillProcess( &tlist[i], ForceKill )) {
                printf( "process #%d [%s] killed\n", tlist[i].dwProcessId, tlist[i].ProcessName );
            } else {
                printf( "process #%d [%s] could not be killed\n", tlist[i].dwProcessId, tlist[i].ProcessName );
                rval = 1;
            }
        }
    }

    return rval;
}

//
//  �R�}���h�t���p�X����  
//
void Which(char *cmd, char *fullpath) 
{
	char *pathp;
	char path[2048];
	int  pathlen;
	int  i, j;
	int  found = 0;
	int  comsize = 0;
	struct stat stbuf;

	strcpy(fullpath, "");

	pathp   = getenv("PATH");
	pathlen = strlen(pathp);

	i = 0;
	while (i < pathlen) {
		j = 0;
		while (pathp[i] != ';' && i < pathlen) { /* path[] = "/usr/bin" */
			path[j] = pathp[i];
			if (path[j] == '\\') {
				path[j] = '/';
			}
			++i;
			++j;
		}
		i++;
		path[j] = '\0';

		strcat(path, "/");               /* path[] = "/usr/bin/"       */
		strcat(path, cmd);               /* path[] = "/usr/bin/com"    */
	    
		comsize = strlen(path);
		strcpy(&path[comsize], ".exe");  /* path[] = "/usr/bin/com.exe"*/
		if (stat(path, &stbuf) == 0) {
			found = 1;
			break;
		}
		//strcpy(&path[comsize], ".com");  /* path[] = "/usr/bin/com.com"*/
		//if (stat(path, &stbuf) == 0) {
		//	found = 1;
		//	break;
		//}
	}
	if (found) {
		strcpy(fullpath, path);
	}
}

//
//  ExecCmd() : �R�}���h�����s����  
//
//  IN  cmd     : ���s����R�}���h  <command> 
//  IN  comargs : �R�}���h������    <command> <args> ...
//  OUT ret     : >= 0 success (command exit code)
//                  -1 error
// 
DWORD ExecCmd(char *cmd, char *comarg)
{
	BOOL                status;
	STARTUPINFO         si;
	PROCESS_INFORMATION pi;

	HANDLE	hProc;   /* Process Handle */
	DWORD   pst;
	DWORD   st = 0;  /* success */

	/* make command line */
	memset(&si, 0, sizeof(si));
	si.cb  = sizeof(si);
	status = CreateProcess(cmd,     /* ���s���W���[����            */
	                       comarg,  /* �R�}���h���C��              */
	                       NULL,    /* �v���Z�X�̃Z�L�����e�B����  */
	                       NULL,    /* �X���b�h�̃Z�L�����e�B����  */
	                       FALSE,   /* �n���h�����p�����Ȃ�        */
	                       0,       /* fdwCreate                   */
	                       NULL,    /* ���u���b�N�̃A�h���X      */
	                       NULL,    /* �J�����g�f�B���N�g��(�e��)  */
	                       &si,     /* STARTUPINFO�\����           */
	                       &pi);    /* PROCESS_INFORMATION�\����   */
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
			//printf("exec_cmd: GetExitCodeProcess error(%d)\n", GetLastError());
			st = 0;		/* error */
		} else {
			st = pst;
		}
	}

	status = CloseHandle(hProc);
	if(status != TRUE){
		//printf("CloseHandle error(%d).\n", GetLastError());
	}
	return st;
}

/* web server������������ */
/*
 *  msg : "": �ʒm�t�@�C��0�o�C�g��  
 */
void SendBKSVMsg(char *msg)
{
	FILE *fp = NULL;
	char *mode = "a";
	char fname[2048];

	if (strlen(msg) == 0) {
		mode = "w";
	}

	if (!fwf) {
		/* bkserver (Web, DB) */
		sprintf(fname, "%s\\%s.sendmsg.txt", bksv_dir, grp_code);
		if ((fp = fopen(fname, mode)) == NULL) {
			perror(fname);
			return;
		}
		fprintf(fp, "%s", msg);
		if (fp) fclose(fp);
	}

	/* DB�T�[�o��FW�̃t�@�C���͏������Ȃ� */
	if (dbf) return;

	if (!dbf) {
		/* firewall (Web, FW) */
		sprintf(fname, "%s\\fw.sendmsg.txt", fw_dir);
		if ((fp = fopen(fname, mode)) == NULL) {
			perror(fname);
			return;
		}
		fprintf(fp, "%s", msg);
		if (fp) fclose(fp);
	}
}

/* bkserver�o�R�Ŕ����ʒm�����邩�`�F�b�N���� */
int FileSVChk()
{
	struct stat stbuf;
	char   fname[2048];
	int    ret;

	sprintf(fname, "%s\\%s.sendmsg.txt", bksv_dir, grp_code);
	ret = stat(fname, &stbuf);
	if (ret == 0 && stbuf.st_size > 5) {
		return 1;  /* Web�T�[�o����̃C�x���g�ʒm���� */
	}
	return 0;      /* Web�T�[�o����̃C�x���g�ʒm�Ȃ� */
}

/* F/W�ɔ����ʒm�����邩�`�F�b�N���� */
int FWChk()
{
	struct stat stbuf;
	char   fname[2048];
	int    ret;

	sprintf(fname, "%s\\fw.sendmsg.txt", fw_dir);
	ret = stat(fname, &stbuf);
	if (ret == 0 && stbuf.st_size > 5) {
		return 1;  /* Web�T�[�o����̃C�x���g�ʒm���� */
	}
	return 0;      /* Web�T�[�o����̃C�x���g�ʒm�Ȃ� */
}


/* �����ݕ�����̎擾 */
char **GetArgs(const EVENTLOGRECORD *pBuf)
{
	char *cp;
	WORD ArgCount;
	char **Args = NULL;

	if(pBuf->NumStrings == 0) return NULL;

	/* �������X�g���擾 */
	Args = GlobalAlloc(GMEM_FIXED, sizeof(char *) * pBuf->NumStrings);
	cp = (char *)pBuf + (pBuf->StringOffset);

	for(ArgCount=0; ArgCount<pBuf->NumStrings; ArgCount++) {
		Args[ArgCount] = cp;
		cp += strlen(cp) + 1;
	}
	return Args;
}

/* �\�[�X�����烂�W���[�������擾 */
BOOL GetModuleNameFromSourceName(
	const char *SourceName, 
	const char *EntryName, 
	char *ExpandedName /* 1000�o�C�g�̃o�b�t�@ */)
{
	DWORD lResult;
	DWORD ModuleNameSize;
	char ModuleName[1000];
	HKEY hAppKey = NULL;
	HKEY hSourceKey = NULL;
	BOOL bReturn = FALSE;

	/* Application���O�p�̃��W�X�g���L�[���I�[�v�� */
	lResult = RegOpenKeyEx(
		HKEY_LOCAL_MACHINE,
		"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application",
		0,
		KEY_READ,
		&hAppKey);

	if(lResult != ERROR_SUCCESS) {
		printf("registry can not open.\n");
		goto Exit;
	}

	/* �\�[�X�̏�񂪊i�[����Ă��郌�W�X�g�����I�[�v�� */
	lResult = RegOpenKeyEx(
		hAppKey,
		SourceName,
		0,
		KEY_READ,
		&hSourceKey);

	if(lResult != ERROR_SUCCESS) goto Exit;

	ModuleNameSize = 1000;

	/* �\�[�X���W���[�������擾 */
	lResult = RegQueryValueEx(
		hSourceKey,
		"EventMessageFile",
		NULL,
		NULL,
		ModuleName,
		&ModuleNameSize);

	if(lResult != ERROR_SUCCESS) goto Exit;

	/* ���ϐ���W�J */
	ExpandEnvironmentStrings(ModuleName, ExpandedName, 1000);

	/* ����I�� */
	bReturn = TRUE;

Exit: /* �㏈�� */
	if(hSourceKey != NULL) RegCloseKey(hSourceKey);
	if(hAppKey != NULL) RegCloseKey(hAppKey);

	return bReturn;
}




/* ���b�Z�[�W�̕\�� */
BOOL DispMessage(
	const char *SourceName, 
	const char *EntryName,
	const char **Args, 
	DWORD MessageId,
	char  *msgbuf,
	int   dispf)
{
	BOOL bResult;
	BOOL bReturn = FALSE;
	HANDLE hSourceModule = NULL;
	char SourceModuleName[1000];
	char *pMessage = NULL;

	/* �\�[�X���W���[�������擾 */	
	bResult = GetModuleNameFromSourceName(SourceName, EntryName, SourceModuleName);
	if(!bResult) goto Exit;

	/* �\�[�X���W���[�������[�h */
	hSourceModule = LoadLibraryEx(
		SourceModuleName,
		NULL,
		DONT_RESOLVE_DLL_REFERENCES | LOAD_LIBRARY_AS_DATAFILE);

	if(hSourceModule == NULL) goto Exit;

	/* ���b�Z�[�W���쐬 */
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_ARGUMENT_ARRAY,
		hSourceModule,
		MessageId,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPSTR)&pMessage,
		0,
		(va_list *)Args);

	/* ����I�� */
	bReturn = TRUE;

Exit: /* �㏈�� */
	if(pMessage != NULL) {
		if (dispf) printf("%s", pMessage);
		if (msgbuf) {
			strcpy(msgbuf, pMessage);
		}
	} else {
		if (dispf) printf("(%d)\n", MessageId);
	}

	if(hSourceModule != NULL) FreeLibrary(hSourceModule);
	if(pMessage != NULL) LocalFree(pMessage);

	return bReturn;
}


/*
 *   �C�x���g���O�̓ǂݎ��
 *   OUT ret : -1  error
 *              0  success nop
 *              1  success Exsit Specified Event
 */
int ReadLog(void)
{
	DWORD BufSize;
	DWORD ReadBytes;
	DWORD NextSize;
	BOOL bResult;
	DWORD i;
	char *cp;
	char *pSourceName;
	char *pComputerName;
	HANDLE hEventLog = NULL;
	EVENTLOGRECORD *pBuf = NULL;
	char **Args = NULL;
    char buf1[128];
    char buf2[128];
    char msgbuf[4096*4];
    char wkmsg[4096*4];
	time_t tt;
	int  dispf = 1;
	int  ret = 0;

	/* �A�v���P�[�V�����C�x���g���O�̃I�[�v�� */
	hEventLog = OpenEventLog(NULL, "Application");

	if(hEventLog == NULL) {
		printf("event log can not open.\n");
		return -1;
	}

	for(;;) {
		/* �C�x���g���O�̃T�C�Y�擾 */
		BufSize = 1;
		pBuf = GlobalAlloc(GMEM_FIXED, BufSize);

		bResult = ReadEventLog(
			hEventLog,
			EVENTLOG_FORWARDS_READ | EVENTLOG_SEQUENTIAL_READ,
			0,
			pBuf,
			BufSize,
			&ReadBytes,
			&NextSize);

		if(!bResult && GetLastError() != ERROR_INSUFFICIENT_BUFFER) break;

		GlobalFree(pBuf);
		pBuf = NULL;

		/* �o�b�t�@���蓖�� */
		BufSize = NextSize;
		pBuf = GlobalAlloc(GMEM_FIXED, BufSize);

		/* �C�x���g���O�̓ǂݎ�� */
		bResult = ReadEventLog(
			hEventLog,
			EVENTLOG_FORWARDS_READ | EVENTLOG_SEQUENTIAL_READ,
			0,
			pBuf,
			BufSize,
			&ReadBytes,
			&NextSize);

		if(!bResult) break;

		tt = time(0);

		/* ���ԓ��Ȃ�\�� / -a(check_span:0) �Ȃ�S���\�� */
		if (pBuf->TimeWritten > tt - check_span || check_span == 0) {
			dispf = 1;  /* �\���Ώ�(���ԓ�) */
		} else {
			dispf = 0;  /* �\���ΏۊO */
		}

		/* �ǂݎ�����C�x���g�̕\�� */
#if 0
		//printf("���R�[�h�ԍ�: %d\n", pBuf->RecordNumber);
		//printf("��������: %s", ctime(&pBuf->TimeGenerated));
		//printf("�������ݎ���: %s", ctime(&pBuf->TimeWritten));
	    //printf("��������: %s  / �������ݎ���: %s\n", buf1, buf2);
#endif
		strcpy(msgbuf, "");
		strftime(buf1, sizeof(buf1), "%y/%m/%d %H:%M:%S", localtime(&pBuf->TimeGenerated));
		strftime(buf2, sizeof(buf2), "%y/%m/%d %H:%M:%S", localtime(&pBuf->TimeWritten));

		cp = (char *)pBuf;
		cp += sizeof(EVENTLOGRECORD);

		pSourceName = cp;
		cp += strlen(cp)+1;

		pComputerName = cp;
		cp += strlen(cp)+1;

		if ((dispf && (daemonf == 0 || vf)) || vf >= 2) {
			printf("���R�[�h�ԍ�: %05d\n", pBuf->RecordNumber);
			printf("����: %s  �C�x���gID: %08x\n", buf2, pBuf->EventID);

			printf("�C�x���g�̎��: ");
			switch(pBuf->EventType) {
				case EVENTLOG_SUCCESS:          printf("����\n"); break;
				case EVENTLOG_ERROR_TYPE:       printf("�G���[\n"); break;
				case EVENTLOG_WARNING_TYPE:     printf("�x��\n"); break;
				case EVENTLOG_INFORMATION_TYPE: printf("���\n"); break;
				case EVENTLOG_AUDIT_SUCCESS:    printf("�č�����\n"); break;
				case EVENTLOG_AUDIT_FAILURE:    printf("�č����s\n"); break;
				default:                        printf("�s��\n"); break;
			}

			printf("�\�[�X��: %s\n", pSourceName);
			printf("�R���s���[�^��: %s\n", pComputerName);

			/* �J�e�S���̕\�� */
			printf("�񎟃J�e�S��: ", pBuf->EventCategory);
			DispMessage(pSourceName, "CategoryMessageFile", NULL, pBuf->EventCategory, NULL, 1);

			/* ���b�Z�[�W�̕\�� */
			Args = GetArgs(pBuf);

			printf("���b�Z�[�W: ");
			DispMessage(pSourceName, "EventMessageFile", Args, pBuf->EventID, msgbuf, 1);

			if(Args != NULL) {
				GlobalFree(Args);
				Args = NULL;
			}

			/* �ŗL�f�[�^�̕\�� */
			if(pBuf->DataLength > 0 && csvf == 0) {
				printf("�ŗL�f�[�^: ");
				for(i=0; i<pBuf->DataLength; i++) printf("%02x ", *(((unsigned char *)pBuf)+(pBuf->DataOffset)+i));
				printf("\n");
			}

			printf("\n");
		} else {
			/* ���b�Z�[�W�̕\�� */
			Args = GetArgs(pBuf);

			DispMessage(pSourceName, "EventMessageFile", Args, pBuf->EventID, msgbuf, 0);

			if(Args != NULL) {
				GlobalFree(Args);
				Args = NULL;
			}
		}
		if (dispf && strstr(msgbuf, check_msg)) {
			printf("found [%s]: %s %s\n", check_msg, buf2, msgbuf);
			sprintf(wkmsg, "found [%s]: %s %s\n", check_msg, buf2, msgbuf);
			Log(wkmsg);
			if (dbf == 0) SendBKSVMsg(wkmsg);   /* WebSV => BKSV => DBSV */
			ret = 1;   /*�@���ԓ��ɖړI�̃��b�Z�[�W���� */
		}
		if (dispf && strstr(msgbuf, check_msg2)) {
			printf("found [%s]: %s %s\n", check_msg2, buf2, msgbuf);
			sprintf(wkmsg, "found [%s]: %s %s\n", check_msg, buf2, msgbuf);
			Log(wkmsg);
			if (dbf == 0) SendBKSVMsg(wkmsg);   /* WebSV => BKSV => DBSV */
			ret = 1;   /*�@���ԓ��ɖړI�̃��b�Z�[�W���� */
		}

		/* �o�b�t�@��� */
		GlobalFree(pBuf);
		pBuf = NULL;
	}

	if(hEventLog != NULL) CloseEventLog(hEventLog);
	return ret;
}


/* port from findwin start */
void GetControlList(HWND hWin, int level)
{
	HWND hCntl = NULL;
	char classname[4096];
	char ctrltext[4096];
	char spc[1024];
	int  ret;

	++level;
	strcpy(spc, "                                                          ");
	spc[level-1] = '\0';
#if 0
	if (level > 1) {
		spc[level-1] = '+';
	}
#endif

	while (1) {
		hCntl = FindWindowEx(hWin, hCntl, NULL, NULL);
		if (!hCntl) {
			break;
		}
		GetClassName(hCntl, classname, sizeof(classname));
		ctrltext[0] = '\0';
		ret = GetWindowText(hCntl, ctrltext, sizeof(ctrltext));
		if (ret == 0) {
			ret = GetLastError();
			if (ret != ERROR_SUCCESS) {
				sprintf(ctrltext, "GetWindowText Error(%d)", ret);
			}
		}
		printf("%sClass:%-20s  Text:[%s]\n", spc, classname, ctrltext);
#if 0
		/* replace other window text */
		if (!strcmp(ctrltext, "0.391")) {
			SetWindowText(hCntl, "0.222");
			printf("Set 0.222!!\n");
			/* Send Redraw Message to Message Queue */
			SendMessage(hWin, WM_PAINT, 0, 0);
		}
#endif
		GetControlList(hCntl, level);
	}
}


/*
 *  find window and send message
 * 
 *  IN  WinTitle: Window Title
 *  IN  evid    : send event ID
 */
int SendMsg2Win(char *WinTitle, int evid)
{
	HWND hWin      = NULL;
	int  i;
	int  af     = 0;            /* all flag      */
	int  evf    = 0;            /* send evt flag */

	/* send message mode */
	evf = 1;
	/* evid = WM_CLOSE; */

	if (af) {
		/* find all window */
		GetControlList(NULL, 0);
	} else {
		/* find specific window */
		hWin = FindWindow(NULL, WinTitle);
		if (hWin) {
			char classname[4096];                                     /* V0.11-A */

			printf("[%s] Window Handle = 0x%08x\n", WinTitle, hWin);

			GetClassName(hWin, classname, sizeof(classname));         /* V0.11-A */
			printf("Class:%-20s  Text:[%s]\n", classname, WinTitle);  /* V0.11-A */
			GetControlList(hWin, 1);                                  /* V0.11-C */
		} else {
			printf("Not found [%s].\n", WinTitle);
		}
		if (evf) {
			printf("Send Message(%d) to [%s].\n", evid, WinTitle);
			SendMessage(hWin, evid, 0, 0);
		} 
	}
	return 0;
}
/* port from findwin end */

/*
 *    IN service   service name
 *    IN op        operation  "start" or "stop"
 */
void CtrlService(char *service_name, char *op)
{
	char  netcmd[2048];
	char  cmd[2048];
	char  comarg[2048];
	DWORD ret  = 0;

	Which("cmd", cmd);      /* search cmd.exe */
	Which("net", netcmd);   /* search net.exe */
	// cmd /C "C:/Windows/system32/net.exe start "IIS Admin Service" /y"
	sprintf(comarg, "%s /C \"%s %s \"%s\" /y\"", "cmd", netcmd, op, service_name);
	printf("%s\n", comarg);
	ret = ExecCmd(cmd, comarg);
	if (ret != 0) {
		printf("%s failed. ret=%d\n", netcmd, ret);
	}

}

void usage()
{
	printf("readevt Ver %s\n", VER);
	printf("usage: readevt [-csv | -d] [{-db|-fw}] [-a] [-s <span(sec)>] [func_key]\n");
	printf("               [-c <num_of_file>]\n");
	printf("       -d: daemon\n");
	printf("       -a: check all event\n");
	printf("       -c: num of netstatXXX.txt default: %d\n", max_fcnt);
	printf("       -s: check span(sec) default: %d\n", check_span);
	printf("       -w: loop wait time (msec) default: %d\n", wait_val);
	printf("ex)    K-DB (001): readevt -d kai -db\n");
	printf("       K-Web(002): readevt -d kai\n");
	printf("       Y-DB (003): readevt -d yo -db\n");
	printf("       Y-Web(004): readevt -d yo\n");
	printf("       F/W       : readevt -d -fw\n");
	exit(1);
}

int main(int a, char *b[]) 
{
	int   i;
	char  *target_win = TARGET_WIN;
	int   cnt  = 0;
	int   fcnt = 0;      /* netstatXXX.txt counter */
	DWORD ret  = 0;
	char  cmd[2048];
	char  netstatcmd[2048];
	char  comarg[2048];

	//strcpy(check_msg,  "EA000004");  /* Yosan Network Error */
	strcpy(check_msg,  "DBNMPNTW");
	strcpy(check_msg2, "DBMSSOCN");

	for (i = 1; i<a; i++) {
		if (!strcmp(b[i], "-h")) {
			usage();
		}
		if (!strcmp(b[i], "-csv")) {
			csvf = 1;
			continue;
		}
		if (!strcmp(b[i], "-v")) {
			++vf;
			continue;
		}
		if (!strcmp(b[i], "-d")) {
			daemonf = 1;   /* ���[�v���ă`�F�b�N���郂�[�h */
			continue;
		}
		if (!strcmp(b[i], "-db")) {
			/* ����DB�T�[�o */
			dbf = 1;       /* bkserver�o�R�Œ�~�ʒm���󂯂� */
			continue;
		}
		if (!strcmp(b[i], "-fw")) {
			/* ����F/W�}�V�� */
			fwf = 1;       /* F/W�ւ̒�~�ʒm���󂯂�        */
			continue;
		}
		if (!strcmp(b[i], "-dbg")) { 
			dbgf = 1;     /* �f�o�b�O�p(for bally) */
			target_win = TARGET_WIN_DBG;
			wait_val   = 3*1000;  /* (3sec) */
			strcpy(check_msg, "Active Directory");   /* Unicode�Ŏw��v����? */
			bksv_dir   = "\\\\galaga\\oga";
			fw_dir     = "\\\\galaga\\oga";
			check_span = 8*3600;    /* 8h */
			service_iis = SERVICE_WWW; 
			continue;
		}
		if (!strcmp(b[i], "-dbg2")) {
			dbgf = 2;     /* �f�o�b�O�p(for honban) */
			target_win = TARGET_WIN_DBG;
			wait_val   = 5*1000;  /* (5sec) */
			strcpy(check_msg, "�T�[�r�X���J�n");   /* Unicode�Ŏw��v����? */
			//bksv_dir   = "\\\\galaga\\oga";
			//fw_dir     = "\\\\galaga\\oga";
			check_span = 120;    /* 120sec */
			continue;
		}
		if (!strcmp(b[i], "-a")) {
			check_span = 0;   /* �S�C�x���g�\�� (default: latest 4h) */
			continue;
		}
		if (!strcmp(b[i], "-s") && a>i+1) {
			check_span = atoi(b[++i]);    /* sec */
			continue;
		}
		if (!strcmp(b[i], "-w") && a>i+1) {
			wait_val = atoi(b[++i]);    /* msec */
			continue;
		}
		grp_code = b[i];
	}

#ifdef _WIN32
	// search netstat.exe command path
	Which("netstat", netstatcmd);
	Which("cmd", cmd);
	//strcpy(cmd, "cmd");
#endif

	/* print options */
	printf("Check Evt Msg1: [%s]\n",    check_msg);
	printf("Check Evt Msg2: [%s]\n",    check_msg2);
	printf("Check Evt Span: latest %d sec\n",  check_span);
	if (fwf) {
		printf("Target Win FW1: [%s]\n",    TARGET_WIN_FW1);
		printf("Target Win FW2: [%s]\n",    TARGET_WIN_FW2);
	} else {
		printf("Target Win    : [%s]\n",    target_win);
	}
	printf("Sleep Time    : %d msec\n", wait_val);
	printf("Send Bksv Dir : %s\n",      bksv_dir);
	printf("Send FW Dir   : %s\n",      fw_dir);
	printf("Group name    : %s\n",      grp_code);
	printf("netstat       : %s\n",      netstatcmd);
	printf("IIS           : %s\n",      service_iis);

	//if (dbf == 0) SendBKSVMsg("");  /* clear */
	SendBKSVMsg("");  /* clear */

	while (1) {
#ifdef _WIN32
		// V0.12-A start
		// Exec netstat -an
		//sprintf(comarg, "%s -an", netstatcmd);
		// Exec cmd /C netstat -an
		sprintf(comarg, "%s /C \"%s -an > netstat%03d.txt\"", "cmd", netstatcmd, fcnt);
		//printf("%s: [%s]\n", cmd, comarg);
		++fcnt;
		if (fcnt >= max_fcnt) {
			fcnt = 0;
		}

		//ret = ExecCmd(netstatcmd, comarg);
		ret = ExecCmd(cmd, comarg);
		if (ret != 0) {
			printf("%s failed. ret=%d\n", netstatcmd, ret);
		}
		// V0.12-A end
#endif

		/*  Web�Ŕ���          DB�ɒʒm����                FW�ɒʒm���� */
		if (ReadLog() == 1 || (dbf && FileSVChk() == 1) || (fwf && FWChk() == 1)) {
			// 2��close����  
			if (fwf) {
				KillProc(TARGET_PROC_WINDUMP);   /* WinDump���~�߂� */
				printf("close1 %s", TARGET_WIN_FW1);
				SendMsg2Win(TARGET_WIN_FW1, WM_CLOSE);
				Sleep(2*60*1000);            /* 2min wait */
				printf("close2 %s", TARGET_WIN_FW1);
				SendMsg2Win(TARGET_WIN_FW2, WM_CLOSE);
			} else {
				KillProc(TARGET_PROC_WINDUMP);   /* WinDump���~�߂� */
				if (!dbf) {
					// Stop Web Server  
					CtrlService(service_iis, "stop");
				}
				printf("close1 %s", target_win);
				SendMsg2Win(target_win, WM_CLOSE);
				Sleep(2*60*1000);  /* 2min wait */
				printf("close2 %s", target_win);
				SendMsg2Win(target_win, WM_CLOSE);
			}
			break;
		}
		if (daemonf == 0) {
			// ���[�v���[�h�łȂ��ꍇ��1����s���ďI���    
			break;
		}

		Sleep(wait_val);   /* wait 3min */
		//Sleep(3*1000);       /* wait 3sec */
		printf("%d -------------------------------------------\n", ++cnt);
	}
	return 0;

}


