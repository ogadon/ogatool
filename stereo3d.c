/*
 *  stereo3d.c
 *
 *    2002/03/03 V1.00 by oga.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RAND(x)  ((rand() & 0xffff)*(x)/(RAND_MAX & 0xffff))

void usage()
{
    printf("usage: stereo3d <filename>\n");
    exit(1);
}

int main(int a, char *b[])
{
	float sw = 40.0;
	float dw = 78.0;
	float hdw = dw / 2.0;
	float w = 20.0;
	float d = .2;
	float maxxl;
	float x;
	float xl;
	float y;
	float xr;
	float h = 1.0;
	int   i, len;
	char  s[256];
	char  cc[4];
	char  buf[256];
	char  c;
	int   wk;
	char  *ss = "abcdefghijklmnopqrstuvwxyz0123456789!#$%^&*()-=\\[];'`,./";

	char  *filename = NULL;
	FILE  *fp;

	for (i=1; i<a; i++) {
	    if (!strcmp(b[i], "-h")) {
	        usage();
	    }
	    filename = b[i];
	}
	if (filename == NULL) {
	    usage();
	}

	srand(time(0));

        if ((fp = fopen(filename, "r")) == NULL) {
	    perror(filename);
	    exit(1);
	}

	memset(buf, 0, sizeof(buf));
	printf("...............................#.........#.................\n");
	while (fgets(buf, sizeof(buf), fp)) {
	    if (buf[strlen(buf)-1] == 0x0a) {
	        buf[strlen(buf)-1] = '\0';
	    }
	    if (buf[strlen(buf)-1] == 0x0d) {
	        buf[strlen(buf)-1] = '\0';
	    }
	    len = strlen(buf);
	    memset(&buf[len], ' ', 100-len);
	    xr    = -hdw;
	    y     = h * 1;
	    maxxl = -999.0;
	    strcpy(s, "");
	    while (xr < hdw) {
		x = xr * (1.0+y) - y*w/2.0;
		i = (int)(x/(1.0 + h) + sw/2.0) + 1;
		if (i < 1) {
		    c = 0;
		} else {
		    c = (buf[i-1] == ' ')? 0 : buf[i-1]-'0';
		}
		// printf("d=%.2f c=%3d ", d, c);
		y = h - (d * (float)c);
		xl = (float)xr - w*y/(1+y);
		// printf("x=%.2f i=%3d c=%3d y=%.2f xl=%.2f\n", x, i, c, y, xl);
		if (xl < -hdw || xl >= hdw || xl <= maxxl) {
		    c = ss[RAND(strlen(ss))];
		} else {
		    c = s[(int)(xl + hdw)];
		    maxxl = xl;
		}
		sprintf(cc, "%c", c);
		strcat(s, cc);
		xr++;
	    }
	    printf("%s\n", s);
	}
}


