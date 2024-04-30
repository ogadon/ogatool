/*
 *     HEX <=> Decimal　変換プログラム
 *
 *        usage : hexdec { 0x...(16進数) | ...(10進数) }
 *
 *     2001/02/03 V1.03 fix 10=>16 bug
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

main(a,b)
int a;
char *b[];
{
    unsigned long ans;

    if (b[1] == 0) {
        printf(" usage : hexdec { 0x...(16進数) | ...(10進数) } \n");
	exit(0);
    }
	
    if (strncmp(b[1],"0x",2) == 0 ||
        strchr(b[1],'a') ||
        strchr(b[1],'b') ||
        strchr(b[1],'c') ||
        strchr(b[1],'d') ||
        strchr(b[1],'e') ||
        strchr(b[1],'f')) {
	if (strncmp(b[1],"0x",2) == 0) {
		ans = strtoul(b[1],(char **)NULL,0);
	} else {
		char buf[50];
		memcpy(buf,"0x",2);
		strcpy(&buf[2],b[1]);
		ans = strtoul(buf,(char **)NULL,0);
	} 
        printf("  16進数   10進数\n");
        printf("  0x%x => %u ",ans,ans);
	if (ans & 0x80000000) {
	        printf("(%d)\n",ans);
	} else {
		printf("\n");
	}
    } else {
        printf("  10進数   16進数\n");
        ans = strtoul(b[1],(char **)NULL,0);    /* V1.01 */
        printf("%8u => 0x%x\n",ans,ans);        /* V1.01 */
    }
}
