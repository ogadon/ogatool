/*
 *  pat_disp.c
 *     disp .xbm pattern
 *
 *     2013/01/04 V0.10 by oga.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* describe target xbm info - start */
#include "alien0.xbm"
#include "alien1.xbm"
#include "alien2.xbm"
#include "alien3.xbm"
#include "ana1.xbm"
#include "ana2.xbm"
#include "ana3.xbm"
#include "ana4.xbm"
#include "ana5.xbm"
#include "keva0.xbm"
#include "keva1.xbm"
#include "keva2.xbm"
#include "keva3.xbm"
#include "kevb0.xbm"
#include "kevb1.xbm"
#include "kevb2.xbm"
#include "kevb3.xbm"
#include "kevc0.xbm"
#include "kevc1.xbm"
#include "kevc2.xbm"
#include "kevc3.xbm"
#include "paku0.xbm"
#include "paku1.xbm"
#include "paku2.xbm"
#include "paku3.xbm"
#include "ume1.xbm"
#include "ume2.xbm"
#include "ume3.xbm"
#include "ume4.xbm"
#include "zubo0.xbm"
#include "zubo1.xbm"
#include "xalien.xbm"

int width[] = {
	alien0_width,
	alien1_width,
	alien2_width,
	alien3_width,
	ana1_width,
	ana2_width,
	ana3_width,
	ana4_width,
	ana5_width,
	keva0_width,
	keva1_width,
	keva2_width,
	keva3_width,
	kevb0_width,
	kevb1_width,
	kevb2_width,
	kevb3_width,
	kevc0_width,
	kevc1_width,
	kevc2_width,
	kevc3_width,
	paku0_width,
	paku1_width,
	paku2_width,
	paku3_width,
	ume1_width,
	ume2_width,
	ume3_width,
	ume4_width,
	zubo0_width,
	zubo1_width
	/* xalien_width */
};
int height[] = {
	alien0_height,
	alien1_height,
	alien2_height,
	alien3_height,
	ana1_height,
	ana2_height,
	ana3_height,
	ana4_height,
	ana5_height,
	keva0_height,
	keva1_height,
	keva2_height,
	keva3_height,
	kevb0_height,
	kevb1_height,
	kevb2_height,
	kevb3_height,
	kevc0_height,
	kevc1_height,
	kevc2_height,
	kevc3_height,
	paku0_height,
	paku1_height,
	paku2_height,
	paku3_height,
	ume1_height,
	ume2_height,
	ume3_height,
	ume4_height,
	zubo0_height,
	zubo1_height
	/* xalien_height */
};

char *bits[] = {
	alien0_bits,
	alien1_bits,
	alien2_bits,
	alien3_bits,
	ana1_bits,
	ana2_bits,
	ana3_bits,
	ana4_bits,
	ana5_bits,
	keva0_bits,
	keva1_bits,
	keva2_bits,
	keva3_bits,
	kevb0_bits,
	kevb1_bits,
	kevb2_bits,
	kevb3_bits,
	kevc0_bits,
	kevc1_bits,
	kevc2_bits,
	kevc3_bits,
	paku0_bits,
	paku1_bits,
	paku2_bits,
	paku3_bits,
	ume1_bits,
	ume2_bits,
	ume3_bits,
	ume4_bits,
	zubo0_bits,
	zubo1_bits
	/* xalien_bits */
};

char *name[] = {
	"alien0.xbm",
	"alien1.xbm",
	"alien2.xbm",
	"alien3.xbm",
	"ana1.xbm",
	"ana2.xbm",
	"ana3.xbm",
	"ana4.xbm",
	"ana5.xbm",
	"keva0.xbm",
	"keva1.xbm",
	"keva2.xbm",
	"keva3.xbm",
	"kevb0.xbm",
	"kevb1.xbm",
	"kevb2.xbm",
	"kevb3.xbm",
	"kevc0.xbm",
	"kevc1.xbm",
	"kevc2.xbm",
	"kevc3.xbm",
	"paku0.xbm",
	"paku1.xbm",
	"paku2.xbm",
	"paku3.xbm",
	"ume1.xbm",
	"ume2.xbm",
	"ume3.xbm",
	"ume4.xbm",
	"zubo0.xbm",
	"zubo1.xbm",
	/* "xalien.xbm" */
};
/* describe target xbm info - end   */

/*
 *  get bit value 
 *
 *    IN  bytearr: byte array
 *    IN  bitpos : bit position
 *    OUT ret    : bit value
 */
int getbit(char *bytearr, int bitpos)
{
	int bytepos = bitpos / 8;
	int bitval  = 0x01 << (bitpos % 8);
	
	return (bytearr[bytepos] & bitval);
}
	
int main(int a, char *b[])
{
	int  i, ww, hh;
	int  numarr = sizeof(width)/sizeof(int);
	int  srcf   = 0;   /* output x68k splite pattern */
	char buf[1024];
	char *pt;

	for (i = 1; i < a; i++) {
		if (!strcmp(b[i], "-src")) {
			srcf = 1;
			continue;
		}
		if (!strcmp(b[i], "-h")) {
			printf("usage: pat_disp [-src]\n");
			printf("  -src : output x68k splite pattern.\n");
			return 1;
		}
	}

	for (i = 0; i < numarr; i++) {
		printf("/* No.%02d %s */\n", i, name[i]);
		strcpy(buf, name[i]);
		/* delete ".xbm" */
		pt = strchr(buf, '.');
		if (pt) {
			*pt = '\0';
		}
		if (srcf) printf("char %s[%d] = {\n", buf, width[i]*height[i]);
		for (hh = 0; hh < height[i]; hh++) {
			if (srcf) printf("    ");
			for (ww = 0; ww < width[i]; ww++) {
				if (getbit(bits[i], hh*width[i]+ww)) {
					if (srcf) {
						if (ww != 0) {
							printf(",");
						}
						printf(" %2d", 1);
					} else {
						printf("¡");
					}
				} else {
					if (srcf) {
						if (ww != 0) {
							printf(",");
						}
						printf(" %2d", 0);
					} else {
						printf(" ");
					}
				}
			}
			if (srcf) {
				if (hh == height[i] - 1) {
					printf("\n");
				} else {
					printf(",\n");
				}
			} else {
				printf("\n");
			}
		}
		if (srcf) printf("};\n");
		printf("\n");
	}
}

/* vim:ts=4:sw=4:
 */
