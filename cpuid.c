/*
 *  CPUID for x86 Processor
 *
 *     1998/01/02  V0.01 by oga
 *     1999/01/18  V0.02 AMDデータシート反映 support 3DNow!
 *     2001/02/19  V0.03 support Windows
 */

#include <stdio.h>
#include <string.h>

char CPUID[] = { 0x0f, 0xa2 };	/* 意味なし */
unsigned int  gEAX,gEBX,gECX,gEDX;

/*
 *    int値を文字列に変換
 */
void DispRegChr(unsigned int x)
{
    int i;

    for (i = 0; i<4; i++) {
        putchar(x & 0xff);
        x >>= 8;
    }
}

int main(int a, char *b[])
{
    int family, model, step, wk;
    int vf = 0;

    if (a > 1 && !strcmp(b[1],"-a")) {
        vf = 1;
    }

    /* 
     *   CPUID実行(EAX=0) 
     */
#ifdef _WIN32
    __asm {
        xor     eax, eax
	_emit   0x0f
	_emit   0xa2
	mov     gEAX,eax
	mov     gEBX,ebx
	mov     gECX,ecx
	mov     gEDX,edx
    }
#else
    __asm__ __volatile__(
        "movl  $0,%eax\n\t"             /* %eax <= 0x80000000 */
        ".byte 0x0f,0xa2\n\t"           /* CPUID exec         */
        "movl  %eax,gEAX\n\t"            /* %eax => EAX        */
        "movl  %ebx,gEBX\n\t"            /* %ebx => EBX        */
        "movl  %ecx,gECX\n\t"            /* %ecx => ECX        */
        "movl  %edx,gEDX");              /* %edx => EDX        */
#endif
    printf("\ncpuid Ver 1.03  by oga.\n");
    if (vf) printf("  CPUID command max param = %d\n",gEAX);
    printf("  Vendor ID   : ");
    DispRegChr(gEBX);	/* VendorID1 */
    DispRegChr(gEDX);	/* VendorID2 */
    DispRegChr(gECX);	/* VendorID3 */
    printf("\n");

    if (gEAX == 0) {
        return 0;
    }

    /* 
     *   CPUID実行(EAX=1)  for Family ID
     */
#ifdef _WIN32
    __asm {
        mov     eax,0x00000001
	_emit   0x0f
	_emit   0xa2
	mov     gEAX,eax
	mov     gEBX,ebx
	mov     gECX,ecx
	mov     gEDX,edx
    }
#else
    __asm__ __volatile__(
        "movl  $0x00000001,%eax\n\t"    /* %eax <= 0x00000001 */
        ".byte 0x0f,0xa2\n\t"           /* CPUID exec         */
        "movl  %eax,gEAX\n\t"            /* %eax => EAX        */
        "movl  %ebx,gEBX\n\t"            /* %ebx => EBX        */
        "movl  %ecx,gECX\n\t"            /* %ecx => ECX        */
        "movl  %edx,gEDX");              /* %edx => EDX        */
#endif
    step = gEAX & 0xf;
    gEAX >>= 4;
    model = gEAX & 0xf;
    gEAX >>= 4;
    family = gEAX & 0xf;
    printf("  Family ID   : %d  (i%d86)\n",family,family);
    printf("  Model ID    : %d\n"         ,model);
    printf("  Stepping ID : %d\n"         ,step);
    printf("  ----------------------------------\n");

    /* 
     *   CPUID実行(EAX=1) for 3DNow
     */
#ifdef _WIN32
    __asm {
        mov     eax,0x80000001
	_emit   0x0f
	_emit   0xa2
	mov     gEAX,eax
	mov     gEBX,ebx
	mov     gECX,ecx
	mov     gEDX,edx
    }
#else
    __asm__ __volatile__(
        "movl  $0x80000001,%eax\n\t"    /* %eax <= 0x80000001 */
        ".byte 0x0f,0xa2\n\t"           /* CPUID exec         */
        "movl  %eax,gEAX\n\t"            /* %eax => EAX        */
        "movl  %ebx,gEBX\n\t"            /* %ebx => EBX        */
        "movl  %ecx,gECX\n\t"            /* %ecx => ECX        */
        "movl  %edx,gEDX");              /* %edx => EDX        */
#endif
    if (vf) printf("  EDX         : %08x\n"       ,gEDX);
    wk = gEDX & 1;
    printf("  CPU Contains a FPU         : %s\n",wk?"Yes":"No");
    gEDX >>= 1;
    wk = gEDX & 1;
    if (vf) printf("  Enhanced Virtual 8086 mode : %s\n",wk?"Yes":"No");
    gEDX >>= 1;
    wk = gEDX & 1;
    if (vf) printf("  I/O Breakpoints            : %s\n",wk?"Yes":"No");
    gEDX >>= 1;
    wk = gEDX & 1;
    if (vf) printf("  Page size extensions       : %s\n",wk?"Yes":"No");
    gEDX >>= 1;
    wk = gEDX & 1;
    if (vf) printf("  Time stamp counter TSC     : %s\n",wk?"Yes":"No");
    gEDX >>= 1;
    wk = gEDX & 1;
    if (vf) printf("  iPentium-style MSRs        : %s\n",wk?"Yes":"No");
    gEDX >>= 1;
    wk = gEDX & 1;
    if (vf) printf("  PAE(or Reserved)           : %s\n",wk?"Yes":"No");
    gEDX >>= 1;
    wk = gEDX & 1;
    printf("  Machine check exception    : %s\n",wk?"Yes":"No");
    gEDX >>= 1;
    wk = gEDX & 1;
    printf("  CMPXCHG8B instruction      : %s\n",wk?"Yes":"No");
    gEDX >>= 1;
    wk = gEDX & 1;
    if (vf) printf("  CPU contains a local APIC  : %s\n",wk?"Yes":"No");
    gEDX >>= 3;
    wk = gEDX & 1;
    if (vf) printf("  Memory Type Register       : %s\n",wk?"Yes":"No");
    gEDX >>= 1;
    wk = gEDX & 1;
    if (vf) printf("  PGE(Global Paging)         : %s\n",wk?"Yes":"No");
    gEDX >>= 1;
    wk = gEDX & 1;
    if (vf) printf("  MCA(or Reserved)           : %s\n",wk?"Yes":"No");
    gEDX >>= 1;
    wk = gEDX & 1;
    if (vf) printf("  CMOV                       : %s\n",wk?"Yes":"No");
    gEDX >>= 8;
    wk = gEDX & 1;
    printf("  MMX(TM) Technology         : %s\n",wk?"Yes":"No"); /* bit23 */
    gEDX >>= 8;
    wk = gEDX & 1;
    printf("  3D Now!(TM) Technology     : %s\n",wk?"Yes":"No"); /* bit31 */

    return 0;
}

