/********************************************************************
 *  comment delete filter                        by Hyper halx.oga  *
 *                                  199X.XX.XX   first version      *
 *                                  1994.09.20   add stdin/filename *
 *                                  1995.05.11   add error line     *
 *                                                                  *
 *             usage : cat filename | comdel                        *
 *                     comdel [filename]			    *
 ********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINEUP(z) if (z == '\n') ++line

main(a,b)
int a;
char *b[];
{
	FILE	*fpin;

	char    xx[3]; /* char    xx[2]; */
	int     c;
	int     i, j, line=1, saveline;

	if (a == 1) 
		fpin = stdin;
	else {
		if ((fpin = fopen(b[1],"r")) == NULL) {
			perror("comdel");
			exit(1);
		}

	}
	xx[2] = '\0';  /**/
	xx[0] = getc(fpin);
	LINEUP(xx[0]);
	xx[1] = getc(fpin);
	LINEUP(xx[1]);
	while (xx[1] != EOF) {
	    if (strcmp(xx,"/*") == 0 ) {
		saveline = line;
		putchar(xx[0]);
		putchar(xx[1]);
		while(strcmp(xx,"*/") != 0) {
		    xx[0] = xx[1];
		    if ((xx[1] = getc(fpin)) == EOF ) {
			putchar('\n');
		        if (a == 1)
		   	    fprintf(stderr, "*** Error : comment not closed. (line=%d) *** \n",saveline);
			else 
			    fprintf(stderr, "*** Error : comment not closed. (line=%d) *** (%s)\n",saveline,b[1]);
			exit(1);
		    }
	    	    LINEUP(xx[1]);
		}
	    }
	    putchar(xx[0]);
	    xx[0] = xx[1];
	    xx[1] = getc(fpin);
	    LINEUP(xx[1]);
	}
	putchar(xx[0]);
	exit(0);
}
