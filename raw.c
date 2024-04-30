/*
 *   raw I/O sample
 *
 *     07/03/25 V0.10 by oga.
 *
 */
#include <stdio.h>
#include <windows.h>
#include <winioctl.h>

/*
 *  Dump Data
 *
 */
#define READ_SIZE (1024)
void DumpData(unsigned char *buf, int isize)
{
    int c, xx, addr = 0;
    int f=0, f2=0;
    int kflag = 0;
    int fflag = 0;  /* -f flush */
    int rev   = 0;  /* 1:dpの出力結果を元のファイルに戻す */
    int size  = 0;
    int work;
    int pos   = 0;  /* for read  */
    int opos  = 0;  /* for write */
    unsigned char obuf[1024]; /* for write */
    char asc[17];

    printf("Location: +0       +4       +8       +C       /0123456789ABCDEF\n");

#if 0
    /***** (original getc(first)) *****/
    size = fread(buf, 1, READ_SIZE, fp);
    pos  = 0;
    if (size <= 0) {
        c = EOF;
    } else {
        c = buf[pos++];
    }
    /**********************************/
#else
    /* dummy read */
    if (isize < READ_SIZE) {
        size = isize;
    } else {
        size = READ_SIZE;
    }
    if (isize == 0) {
        c = EOF;
    } else {
        c = buf[pos++];
    }
#endif

    opos = 0;
    while(c != EOF) {
	xx = 0;
	strcpy(asc,"                ");
	printf("%08x: ",addr);
	f = 0;
	while(c != EOF && xx < 16) {
	    if (fflag) {
	        printf("%02x",c);          /* これが実行時間の60% */
	        fflush(stdout);            /* 2.2倍くらい遅くなる */
	    } else {
	        work = c >> 4;
	        obuf[opos++] = ((work > 9)?work-10+'a':work+'0');
	        work = c & 0x0f;
	        obuf[opos++] = ((work > 9)?work-10+'a':work+'0');
	    }

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
	    } else if (c >= 127 ) {
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
	    if ((xx % 4) == 3) {
	        if (fflag) {
	            printf(" ");
		} else {
	            obuf[opos++] = ' ';
		}
	    }
	    ++xx;
	    ++addr;

            /***** (original getc(next))  10%up *****/
	    if (pos >= size) {
#if 0
                size = fread(buf, 1, READ_SIZE, fp);
                pos  = 0;
                if (size <= 0) {
                    c = EOF;
                } else {
                    c = buf[pos++];
                }
#else
                /* dummy read */
                if (isize < pos + READ_SIZE) {
                    size = isize - pos;
                }
                if (isize == 0) {
                    c = EOF;
                } else {
                    c = buf[pos++];
                }
#endif
	    } else {
                c = buf[pos++];
	    }
	    /****************************************/

	    if (f == 1 && xx >= 16) {
		asc[xx++] = c;
		f = 0;
		f2 = 1;
	    }
	}
	if (!fflag) {
	    obuf[opos] = '\0';
	    printf("%s", obuf);
	    opos = 0;
	}

	while (xx <16) {
		printf("  ");
		if ((xx % 4) == 3) printf(" ");
		++xx;
	}
	asc[xx]='\0';

	printf("/%16s \n",asc);
    }
}

int RawRead(char *drive, int offset, int size)
{
    HANDLE hDev;
    BOOL   bRet;
    unsigned char *buf;
    DWORD  readsize = 0;
    DWORD  bytesReturnd = 0;
    char   path[128];
    int    ret;
    FILE   *fp;

    buf = (unsigned char *)malloc(4096);
    if (buf == NULL) {
        printf("malloc error\n");
	exit(1);
    }
    memset(buf, 0, 4096);

    sprintf(path, "\\\\.\\%s", drive);
    //strcpy(path, drive);  /* DEBUG */
    printf("path=[%s]\n", path);

#if 0
    fp = fopen(path, "rb");
    if (fp == NULL) {
	perror(path);
	exit(1);
    }
    fread(buf, 1, size, fp);
    DumpData(buf, size);
    fclose(fp);
    exit(1);
#endif

    /* Open Raw Device */
#if 0
    hDev = CreateFile(path, 0, 0, NULL, 0,
                      FILE_FLAG_DELETE_ON_CLOSE, NULL);
#else
    hDev = CreateFile(path,
                GENERIC_READ,           /* 読込み指定         */
                FILE_SHARE_READ |       /* 複数プロセスのアクセス許可 */
                FILE_SHARE_WRITE,
                NULL,                   /* セキュリティ属性なし */
                OPEN_EXISTING,          /* 既存 or 新規ファイルオープン */
                FILE_ATTRIBUTE_NORMAL,  /* ファイル属性なし     */
                NULL);                  /* テンプレートファイルなし   */
#endif
    if (hDev == INVALID_HANDLE_VALUE) {
        printf("CreateFile Error: error=%d\n", GetLastError());
	exit(1);
    }

#if 0  /* いる? */
    /* Volume Lock */
    ret = DeviceIoControl(hDev,
		    FSCTL_LOCK_VOLUME,
		    NULL,
		    0,
		    NULL,
		    0,
		    &bytesReturnd,
		    NULL);
    if (ret == 0) {
        printf("DeviceIoControl Error: error=%d\n", GetLastError());
	exit(1);
    }
#endif

    /* Read Data */
    bRet = ReadFile(hDev, buf, size, &readsize, NULL);
    if (bRet == FALSE) {
        printf("ReadFile Error: error=%d\n", GetLastError());
        CloseHandle(hDev);
	exit(1);
    }
    DumpData(buf, size);
    CloseHandle(hDev);
    return 0;
}

void usage()
{
    printf("usage: raw <drive>\n");
    exit(1);
}

int main(int a, char *b[])
{
    int  i;
    char *drive = NULL;

    for (i = 1; i < a; i++) {
	if (!strcmp(b[i], "-h")) {
	    usage();
	}
	drive = b[i];
    }
    if (drive == NULL) {
        usage();
    }

    RawRead(drive, 0, 512);  /* sectorサイズ単位でないとInvalid Argとなる */
    return 0;
}

