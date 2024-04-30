/*
 *   ht : %nn%nn%nn => <0xnn><0xnn><0xnn>...
 *        =nn=nn=nn => <0xnn><0xnn><0xnn>...
 *
 *     2001/04/25 V1.01 fix memset  by oga.
 *     2007/05/11 V1.02 support QUOTED-PRINTABLE
 *     2009/06/02 V1.03 exit on fopen error 
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

main(a,b)
int a;
char *b[];
{
	FILE *fp;
	int c;
	int i;
	char buf[10];

	memset (buf, 0, sizeof(buf));

	if (a == 1) {
		fp = stdin;
	} else {
		if ((fp = fopen(b[1],"rb")) == 0) {
			perror(b[0]);
			exit(1);
		}
	}

	c = getc(fp);
	while (c != EOF) {
		if (c == '%' || c == '=') {  /* V1.02-C */
			strcpy(buf,"0x");
			buf[2]= getc(fp);
			buf[3]= getc(fp);
			c = strtol(buf,(char **)NULL,0);
		}
		putchar(c);
		/* printf("DATA=[0x%08x]\n",c); */
		c = getc(fp);
	}
}
