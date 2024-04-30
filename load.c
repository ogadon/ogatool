//
//  load.c  CPU使用率を取得する。 (for NT)
//
//      98/12/24  by oga.
//

#include <windows.h>
#include <stdio.h>

#define dprintf	printf

//#define SYSBUF_SIZE	192
#define SYSENT_SIZE	32		// sysentの数
#define CPU_MAX		64		// 最大サポートCPU数

typedef struct _sysent {
	hyper user;
	hyper kernel;
	hyper base;
	hyper unknown[3];
} sysent;

hyper Base[CPU_MAX];
hyper User[CPU_MAX];
hyper Kernel[CPU_MAX];

BOOL  first = TRUE;

int (*NtQuerySystemInformation)(int, hyper [], int, int);

//hyper sysbuf[SYSBUF_SIZE];
sysent sysbuf[SYSENT_SIZE];

int Init()
{
    int i;
    HMODULE  hMod = GetModuleHandle("NTDLL.DLL");

    memset(sysbuf, 0, sizeof(sysbuf));
    dprintf("size of sysbuf=%d\n",sizeof(sysbuf));

    if (hMod) {
        NtQuerySystemInformation = 
			(int (*)(int, hyper[], int, int))GetProcAddress(hMod, "NtQuerySystemInformation");
	for (i=0; i<CPU_MAX; i++) {
	    Base[i] = User[i] = Kernel[i] = (hyper)0;
            printf("User = %d\n",User[i]);
	}
	return 0;
    } else {
	return -1;
    }
}

//
//   IN : int rate[nProcessor]   ... 取得する領域のアドレス(プロセッサ数分
//        int nProcessor         ... 取得対象プロセッサの数
//
int GetCpuLoad(int rate[], int nProcessor)
{
    int i;
    hyper user, kernel, base;

    dprintf("NtQuerySystemInformation\n");
    NtQuerySystemInformation(sizeof(hyper), (hyper *)sysbuf, sizeof(sysbuf), 0);
    dprintf("NtQuerySystemInformation End.\n");

    for (i=0; i<nProcessor; i++) {
        dprintf("Get diff. %d - %d\n",sysbuf[i].user, User[i]);
	// Get differential value
        user   = sysbuf[i].user                      - User[i];
        dprintf("Get diff 2.\n");
        dprintf("Get diff. %d - %d - %d\n",sysbuf[i].kernel, sysbuf[i].user, Kernel[i]);
        kernel = sysbuf[i].kernel - sysbuf[i].user   - Kernel[i];
        dprintf("Get diff 3.\n");
        base   = sysbuf[i].base   - sysbuf[i].kernel - Base[i];

        dprintf("Save cur value.\n");
	// Save current value
	User[i]   = sysbuf[i].user;
	Kernel[i] = sysbuf[i].kernel - sysbuf[i].user;
	Base[i]   = sysbuf[i].base   - sysbuf[i].kernel;
	if (first) {
	    // 初回コール時は0%とする。
	    rate[i] = 0;
	} else {
            dprintf("Calc rate.\n");
	    user = 100 - ((user*100)/base);
	    kernel = (kernel*100)/base;
	    rate[i] = (user + kernel)/2;
	}
    }

    first = FALSE;
    dprintf("GetCpuLoad Return.\n");
    return 0;
}

int main(int a, char *b[])
{
    int rate[64], nProcessor = 1;

    memset(rate, 0, sizeof(rate));

    Init();

    while(1) {
        GetCpuLoad(rate, nProcessor);
	printf("CPU Load : %d%%", rate[0]);
	Sleep(1000);
    }
}
