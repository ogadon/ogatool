/*
 *  tolower : change filename to lower case
 *
 *   2001/07/03 V1.00 by oga.
 *   2001/12/16 V1.01 -win support
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void ToLower(char *istr, char *ostr)
{
    int i = 0;
    while (istr[i] != '\0') {
        ostr[i] = tolower(istr[i]);
        ++i;
    }
    ostr[i] = '\0';
}

int main(int a, char *b[])
{
    int  i;
    int  winf = 0;		/* -win */
    char lowname[4096];
    char tmpname[4096];

    /* arg check */
    for (i = 1; i<a; i++) {
	if (!strcmp(b[i], "-h")) {
	    printf("usage: tolower [-win] [filename ...]\n");
	    exit(1);
	}
	if (!strcmp(b[i], "-win")) {
	    winf = 1;
	}
	ToLower(b[i], lowname);
	if (strcmp(b[i], lowname)) {
	    printf("rename %s => %s\n", b[i], lowname);
	    if (winf) {
	        strcpy(tmpname, lowname);
	        strcat(tmpname, ".tolower.tmp");
		/* FILENAME => filename.lowercase.tmp */
	        if (rename(b[i], tmpname) < 0) {
		    /* error */
	            printf("Error1: nochange %s\n", b[i]);
		} else {
		    /* filename.lowercase.tmp => filename */
	            if (rename(tmpname, lowname) < 0) {
		        /* error */
	                printf("Error2: nochange %s\n", b[i]);
		        /* filename.lowercase.tmp => FILENAME */
			rename(tmpname, b[i]);
		    }
		}
	    } else {
		/* FILENAME => filename */
	        rename(b[i], lowname);
	    }
	} else {
	    printf("nochange %s\n", b[i]);
	}
    }
}
