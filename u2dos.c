/* 
 *    u2dos [filename] : UNIX形式の改行コードをDOS形式の改行に変換する。
 *                       0x0a => 0x0d 0x0a
 *       96.04.07 V1.00 by oga 
 *       07.05.13 V1.01 fix bug
 *       11.01.01 V1.02 fix empty line bug
 *       13.01.03 V1.03 support Win(VC/MinGW)
 *       13.02.20 V1.04 fix MinGW prob. and add usage
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

int vf = 0;             /* -v: verbose */

void disp_str(char *str)
{
	char *pt = str;
	while (*pt != '\0') {
		printf("%02x ", (unsigned char)(*pt));
		++pt;
	}
	printf("\n");
}

int main(int a, char *b[])
{
	int f = 0;          /* kanji 1st */
	int c;
	int i;
	FILE *fp;
	char buf[1024*10];  /* V1.01-A */
	int  len;           /* V1.01-A */
	char *fname = NULL;

	for (i = 1; i < a; i++) {
		if (!strcmp(b[i], "-h")) {
			printf("usage: u2dos [<unix_text>]\n");
			return 1;
		}
		if (!strcmp(b[i], "-v")) {
			++vf;
			continue;
		}
		fname = b[i];
	}

	if (fname) {
		if ((fp = fopen(fname, "rb")) == 0) {
			perror("fopen");
			return -1;
		}
	} else {
		fp = stdin;
	}

#if 0 /* defined(MINGW) */
	/* 0x0dが2重に出力されるのを防止。 再現せず。不要だった? */
	setmode(fileno(stdout), O_BINARY);   /* V1.03-A */
#endif

#if 1  /* V1.01-C */
	while (fgets(buf, sizeof(buf), fp)) {
		if (vf) disp_str(buf);
		len = strlen(buf);
		/* 最後が0x0d以外-0x0aの場合のみ変換 */
		if (len > 0 && buf[len-1] == 0x0a) {
		    if (len == 1 || (len > 1 && buf[len-2] != 0x0d)) {  /* V1.02-C */
		        buf[len-1] = 0x0d;
		        buf[len]   = 0x0a;
		        buf[len+1] = '\0';
		    }
		}
		if (vf) disp_str(buf);
		printf("%s", buf);
	}
#else
	while ((c = getc(fp)) != EOF) {
		if (!f && c == 0x0a) {
			putchar(0x0d);
			putchar(0x0a);
			continue;
		}
		if (!f && (c & 0x80)) {
			f = 1;
		} else {
			f = 0;
		}
		putchar(c);
	}
#endif
	return 0;
}

