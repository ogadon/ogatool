#include <stdio.h>
main(a,b)
int a;
char *b[];
{
	int c;
	FILE *fp;

	if (a == 1) {
		fp = stdin;
	} else {
		if ((fp = fopen(b[1],"r")) == 0) {
			perror(b[0]);
		}
	}

	c = skip_sep(fp);
	while (c != EOF) {
		while ((c != EOF && isalnum(c)) || c == '.' || c == '_') {
			putchar(c);
			c = getc(fp);
		}
		putchar('\n');
		c = skip_sep(fp);
	}

}
skip_sep(fp)
FILE *fp;
{
	int c;

	c = getc(fp);
	while(c != EOF && isalnum(c) == 0)
		c = getc(fp);
		
	return c;
}
