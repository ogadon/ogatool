/*
 *  class : IP アドレスクラスチェック
 *
 *      97/09/09 V0.10 by oga
 *      13/12/08 V0.20 support private check
 *      14/01/12 V0.30 check CIDR
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLASS_A	0x80
#define CHECK_A	0x00

#define CLASS_B	0xc0
#define CHECK_B	0x80

#define CLASS_C	0xe0
#define CHECK_C	0xc0

#define PRIVATE "(private)"
#define CIDR    "(CIDR)"   /* V0.30-A */


main(a,b)
int a;
char *b[];
{
	int  ad;             /* 1st octet */
	int  ad2 = 0;        /* 2nd octet */
	int  vf  = 1;
	char *private = "";
	char *pt;

	if (a < 2) {
		printf("usage: class <IP_address>\n");
		exit(1);
	}
	ad = atoi(b[1]);
	pt = strchr(b[1], '.');
	if (pt) {
		++pt;
		ad2 = atoi(pt);
		if (vf) printf("1st.2nd = %d.%d\n", ad, ad2);
	}
	if ((ad & CLASS_A) == CHECK_A) {
		if (ad == 10) private = PRIVATE;
	    printf("%s is class A address %s\n", b[1], private);
	}
	if ((ad & CLASS_B) == CHECK_B) {
		if (ad == 172 && (ad2 >= 16 && ad2 <= 31)) private = PRIVATE;
	    printf("%s is class B address %s\n", b[1], private);
	}
	if ((ad & CLASS_C) == CHECK_C) {
		if (ad == 192 && ad2 == 168) private = PRIVATE;
		if (ad >= 194 && ad  <= 207) private = CIDR;         /* V0.30-A */
	    printf("%s is class C address %s\n", b[1], private);
	}
	return 0;
}

/* vim:ts=4:sw=4:
 */

