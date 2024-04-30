/*
 *  CPU使用率取得サンプル
 *
 *      01/12/16 V0.10 by oga.
 */

#include <windows.h>
#include <stdio.h>
//#include <winperf.h>  // in windows.h

#define dprintf if (vf) printf

void memdump(FILE *fp, unsigned char *buf, int size);

int vf = 0;

// 
//　(0) Get Titles DataBase for Objec, Counter Index
// 
int GetTitleDB()
{
    HKEY  hkey;		//レジストリキーを参照 
    DWORD MaxData;	//TiltesDataBaseの容量 
    FILE  *fp; 
    PSTR  TitlesDataBase;
    int   n;

    // Open Key
    RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
        "Software\\Microsoft\\Windows NT\\CurrentVersion\\Perflib\\009", 
        0, 
        KEY_READ, 
        &hkey); 

    // Get TitleDB size
    RegQueryInfoKey(hkey,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,&MaxData,NULL,NULL); 

    // データベースの領域確保 
    TitlesDataBase = (PSTR)malloc((MaxData+1) * sizeof(TCHAR)); 

    // データベースを取得 
    RegQueryValueEx(hkey, 
        (LPTSTR) "Counters", 
        NULL, 
        NULL, 
        (LPBYTE) TitlesDataBase, 
        &MaxData ); 

#if 0
    // TitlesDataBaseをファイルに出力 
    fp = fopen("TitlesDataBase.txt","w"); 
    for (n=0; n<(int)MaxData; n++) {
        if ((char)TitlesDataBase[n] == 0) {
            fprintf(fp,"\n"); 
        }
        fprintf(fp,"%c",TitlesDataBase[n]); 
    } 
    fclose(fp);  
#endif
    return 0;
}

//
//  (1)Get Data Block
//
PERF_DATA_BLOCK *GetPerfDataBlock()
{
     DWORD lpType;
     DWORD lpSize = 200000;
     PERF_DATA_BLOCK *pfdata;
     long  code;
     char  buf[1024];

     pfdata = (PERF_DATA_BLOCK *)malloc(lpSize);
     if (pfdata == NULL) {
         return NULL;
     }

     dprintf("RegQueryValueEx(HKEY_PERFORMANCE_DATA) start\n");
     if (code = RegQueryValueEx(HKEY_PERFORMANCE_DATA,
                     "Global",
		     0,
		     &lpType,
		     (LPBYTE)pfdata,
		     &lpSize) != ERROR_SUCCESS) {
         printf("RegQueryValueEx() failed. code=%d size=%d\n", code, lpSize);
	 if (code == ERROR_MORE_DATA) {
	     printf("MORE Data size!\n");
	     return NULL;
         }
     }
     dprintf("RegQueryValueEx(HKEY_PERFORMANCE_DATA) end\n");

     RegCloseKey(HKEY_PERFORMANCE_DATA);

     // 正しいデータが取得できたかチェック
     if (pfdata->Signature[0] == (WCHAR)'P' &&
         pfdata->Signature[1] == (WCHAR)'E' &&
         pfdata->Signature[2] == (WCHAR)'R' &&
         pfdata->Signature[3] == (WCHAR)'F') {
         // collect data get success
         dprintf("Perf Data get success. (%d) (%d)\n", 
	     				sizeof(PERF_DATA_BLOCK),
					lpSize);
         //memdump(stdout, (unsigned char *)pfdata, lpSize);
     } else {
         // Invalid data returnd
	 return NULL;
     }
     return pfdata;
}

void FreePerfDataBlock(PERF_DATA_BLOCK *pfdata)
{
    free(pfdata);
}

//
//  (2)Get Perf Object
//
PERF_OBJECT_TYPE *GetPerfObj(PERF_DATA_BLOCK *pfdata, int idx)
{
    int i;
    int found = 0;
    PERF_OBJECT_TYPE *pfobj; 

    if (idx < 1) {
        printf("GetPerfObj: Invalid Argument idx=%d\n", idx);
        return NULL;
    }
    // get 1st object
    pfobj = (PERF_OBJECT_TYPE *) ((PBYTE) pfdata + pfdata->HeaderLength);  
    for (i = 0; i < pfdata->NumObjectTypes; i++) {
        if (pfobj->ObjectNameTitleIndex == idx) {
	    found = 1;
	    break;        // found target object
        }
	pfobj = (PPERF_OBJECT_TYPE) ((PBYTE) pfobj + pfobj->TotalByteLength); 
    }
    if (found) {
        return pfobj;
    }
    printf("not found object (index = %d)\n", idx);
    return NULL;
}

//
//  (3)Get Perf Instance Def
//
//     idx : 0-
//
PERF_INSTANCE_DEFINITION *GetPerfInstance(PERF_OBJECT_TYPE *pfobj, int idx)
{
    int i;
    int found = 0;
    PERF_INSTANCE_DEFINITION *pfins; 

    if (idx < 0) {
        printf("GetPerfInstance: Invalid Argument idx=%d\n", idx);
        return NULL;
    }
    // get 1st object
    pfins = (PERF_INSTANCE_DEFINITION *)((PBYTE) pfobj + pfobj->DefinitionLength);  
    for (i = 0; i < pfobj->NumInstances; i++) {
        if (i == idx) {
            dprintf("DEBUG3: instance %d\n", i);
	    found = 1;
	    break;
	}
	pfins = (PPERF_INSTANCE_DEFINITION)((PBYTE)pfins + pfins->ByteLength);  
    }
    if (found) {
        return pfins;
    }
    printf("not exist instance (index = %d)\n", idx);
    return NULL;
}

//
//  (4)Get Perf Counter Def
//
PERF_COUNTER_DEFINITION *GetPerfCnt(PERF_OBJECT_TYPE *pfobj, int idx)
{
    int i;
    int found = 0;
    PERF_COUNTER_DEFINITION *pfcnt; 

    if (idx < 1) {
        printf("GetPerfCnt: Invalid Argument idx=%d\n", idx);
        return NULL;
    }
    // get 1st object
    pfcnt = (PERF_COUNTER_DEFINITION *) ((PCHAR) pfobj + pfobj->HeaderLength);
    for (i = 0; i < pfobj->NumCounters; i++) {
        if (pfcnt->CounterNameTitleIndex == idx) {
	    found = 1;
	    break;  // found target counter
        }
	pfcnt = (PERF_COUNTER_DEFINITION *)((PCHAR) pfcnt + pfcnt->ByteLength); 
    }
    if (found) {
        return pfcnt;
    }
    printf("not found counter (index = %d)\n", idx);
    return NULL;
}

//
//  (5)Get Counter Value
//
void *GetPerfCntValue(PERF_INSTANCE_DEFINITION *pfins, 
                      PERF_COUNTER_DEFINITION  *pfcnt)
{
    PERF_COUNTER_BLOCK *pfcntblk;
    void *data;

    pfcntblk = (PERF_COUNTER_BLOCK *)((PBYTE)pfins + pfins->ByteLength);  
    data = (void*) ((PBYTE)pfcntblk + pfcnt->CounterOffset);  
    return data;
}

//
//  Get Perf Value (DWORD)
//
DWORD GetPerfValue(int obj_idx, int ins_no, int cnt_idx)
{
    PERF_DATA_BLOCK  *pfdata;
    PERF_OBJECT_TYPE *pfobj;
    PERF_INSTANCE_DEFINITION *pfins;
    PERF_COUNTER_DEFINITION *pfcnt;
    DWORD *pvalue;
    DWORD value;

    pfdata = GetPerfDataBlock();
    if (pfdata == NULL) {
        exit(1);
    }
    pfobj = GetPerfObj(pfdata, obj_idx);      // Object Index
    if (pfobj == NULL) {
        exit(1);
    }
    pfins = GetPerfInstance(pfobj, ins_no);   // Instance Number
    if (pfins == NULL) {
        exit(1);
    }
    pfcnt = GetPerfCnt(pfobj, cnt_idx);       // Counter Index
    if (pfcnt == NULL) {
        exit(1);
    }
    pvalue = (DWORD *)GetPerfCntValue(pfins, pfcnt);  // Get Counter Value
    value = *pvalue;

    FreePerfDataBlock(pfdata);

    return value;
}

/*
 *  void memdump(fp, buf, size)
 *
 *  bufから、size分を fp にヘキサ形式で出力する
 *
 *  <出力例>
 *  Location: +0       +4       +8       +C       /0123456789ABCDEF
 *  00000000: 2f2a0a20 2a205265 76697369 6f6e312e //#. # Revision1.
 *  00000010: 31203936 2e30372e 30312074 616b6173 /1 96.07.01 takas
 *  00000020: 6869206b 61696e75 6d610a20 2a2f0a2f /hi kainuma. #/./
 *
 *  IN  fp   : 出力ファイルポインタ (標準出力の場合はstdoutを指定)
 *      buf  : ダンプメモリ先頭アドレス
 *      size : ダンプサイズ
 *
 */
void memdump(FILE *fp, unsigned char *buf, int size)
{
	int c, xx, addr = 0, i;
	int f=0, f2=0;
	int kflag = 0;		/* 漢字出力 */
	int pos   = 0;
	char asc[17];
	
	/* ヘッダ出力 */
	fprintf(fp,
	  "Location: +0       +4       +8       +C       /0123456789ABCDEF\n");

	c = buf[pos++];   /* c = getc(infp); */
	while(pos <= size) {
	    xx = 0;
	    strcpy(asc,"                ");
	    fprintf(fp,"%08x: ",addr);
	    f = 0;
	    while(pos <= size && xx < 16) {
		fprintf(fp,"%02x",c);
		if (c < 32) {
		    if (f) {
			asc[xx] = c;
			if (c == 10 || c == 13) {
			    asc[xx] = '.'; /* 暫定 */
			}
		    } else {
			asc[xx] = '.';
		    }
		    f = 0;
		} else if (c > 127 ) {
		    if (kflag) {
		        if (f) {
			    asc[xx] = c;
			    f = 0;
			} else {
			    asc[xx] = c;
			    f = 1;
			}
		    } else {
			asc[xx] = '.';
		    }
		} else {
		    asc[xx] = c;
		    f = 0;
		}
		if (xx == 0 && f2) {
		    asc[xx] = '.';
		    f = 0;
		    f2 = 0;
		}
		if ((xx % 4) == 3) fprintf(fp," ");
		++xx;
		++addr;
		c = buf[pos++];   /* c = getc(infp); */
		if (f == 1 && xx >= 16) {
		    asc[xx++] = c;
		    f = 0;
		    f2 = 1;
		}
	    }
	    while (xx <16) {
		fprintf(fp,"  ");
		if ((xx % 4) == 3) fprintf(fp," ");
		++xx;
	    }
	    asc[xx]='\0';
	    fprintf(fp,"/%16s \n",asc);
	}
}

unsigned int GetCurMilliSec()
{
    SYSTEMTIME syst;

    GetLocalTime(&syst);
    return (unsigned int)(syst.wHour   * 3600000 +
                          syst.wMinute * 60000 +
                          syst.wSecond * 1000 +
			  syst.wMilliseconds);
}

int main(int a, char *b[])
{
    DWORD value;
    DWORD oldvalue = 0;
    DWORD usage;
    int wait_msec = 1000;
    int msec, oldmsec;
    int diff_msec;
    int i;

    for (i = 1; i < a; i++) {
        if (!strcmp(b[i], "-v")) {
	    ++vf;
	    continue;
	}
    }

    // GetTitleDB();

    // main
    while (1) {
        value = GetPerfValue(238,   // 238:"Processor" Object Index
                               0,   // 1: 2nd Instance (CPU 0)
		               6);  // 6:"% Processpr Time" Counter Index
        dprintf("GetPerfValue value=%u\n", value);
	msec = GetCurMilliSec();
	if (oldvalue) {
	    diff_msec = msec - oldmsec;
	    if (diff_msec < 0) {
	        diff_msec = 24*3600*1000 - oldmsec + msec;
	    }
	    usage = 100 - (value - oldvalue)/(100 * diff_msec);
	    printf("CPU usage = %3d%%\n", usage);
	}
	oldvalue = value;
	oldmsec  = msec;
        Sleep(wait_msec);
    }

    return 0;
}

