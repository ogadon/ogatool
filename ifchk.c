/*
 *   #ifdef check tool                                 by Hyper halx.oga  
 *                                                                       
 *             usage : ifchk [filename] [-N<9>] [-h]         
 *                        N : ネストの最大値(デフォルトは9)            
 *                                                                     
 *                                                                     
 *                                      V1.00 Initial Revision.  92/??/??      
 *                                      V1.10 [-N] [-h] support. 94/03/07      
 *                                      V1.11 #elif, tab support.95/04/06      
 *                                                                    
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* #define NEST 9 */

#define VER "V1.11"

main(a,b)
int a;
char *b[];
{
	char    cc[500],endmsg[80];
	char    *fname=0;
	int     flag[50];
	int     c, pc, i, j, ofs, jj, rc;
	int     nest = 9;
	FILE *fpin;
	/* FILE *fpout; */


	rc = 0;
	for (i=0; i<nest+1 ; i++) {flag[i] = 0;}

	for (i=1; i<a; i++) {
		if (strcmp("-h",b[i])==0) {
			printf("usage : ifchk [filename] [-N<9>] [-h]\n");
			exit(0);
		}
		if (strncmp("-",b[i],1)==0) {
			nest = atoi(b[i]+1);
			if (nest == 0) {
				printf("usage : ifchk [filename] [-N<9>] [-h]\n");
				printf("Invalid argument -N : N > 0\n");
				exit(1);
			}
#ifdef DEBUG
			printf("nest = %d\n",nest);
#endif
		} else {
			fname = b[i];
		}
	}

	if (fname == 0) {
		fpin = stdin;
	} else {
		if ((fpin = fopen(fname,"r")) == NULL) {
			printf("Error : 入力ファイルエラー！\n");
       	    		printf("Usage : ifchk [filename] [-N<9>] [-h]\n");
			exit(1);
		} else {
			printf("\n         *********************************************************  \n"); 
			printf(  "              #ifdef checker %s ( %s )     by oga.\n",VER,fname); 
			printf(  "         *********************************************************  \n\n"); 
		}
	}
	/*
	if ((fpout = fopen(b[2],"w")) == NULL) {
		printf("Error : 出力ファイルエラー！\n");
		exit(1);
	}
	*/
	while ((c = getc(fpin)) != EOF) {
		i = nest+1;
		while (c != '\n' && c != EOF) {
			cc[i++] = c;
			c = getc(fpin);
		}
		cc[i] = '\n';

		/* printf("line read done....\n"); */
		for (j = 1; j <= nest ; j++) {
			if (flag[j] == 1) {
				cc[j] = '|';
			} else {
				cc[j] = ' ';
			}
		}

	    if (cc[nest+1] == '#') {
		for (ofs=1;cc[nest+1+ofs] == ' ' || cc[nest+1+ofs] == '\t'; ofs++) {}

		if (strncmp(&cc[nest+1+ofs],"if",2) == 0) {
			if (flag[nest] == 1) {
				printf("======= Error : #ifdef nest is too deep!\n");
				exit(1);
			}
			for (j = 1; j <= nest && flag[j] == 1 ; j++) {}
			flag[j] = 1;
			cc[j] = '+';
			for (jj = j+1; jj <= nest; jj++) {
				cc[jj] = '-';
			}
		}
		if (strncmp(&cc[nest+1+ofs],"else",4) == 0 ||
		    strncmp(&cc[nest+1+ofs],"elif",4) == 0 ) {
			for (j = nest; j > 0 && flag[j] == 0 ; j--) {}
			if (j <= 0) {
				printf("======= Error : #else is not reached. \n");
				++rc;
			} else {
				cc[j] = '+';
				for (jj = j+1; jj <= nest; jj++) {
					cc[jj] = '-';
				}
			}
		}
		if (strncmp(&cc[nest+1+ofs],"endif",5) == 0) {
			for (j = nest; j > 0 && flag[j] == 0 ; j--) {}
			if (j <= 0) {
				printf("======= Error : #endif is not reached. \n");
				++rc;
			} else {
				flag[j] = 0;
				cc[j] = '+';
				for (jj = j+1; jj <= nest; jj++) {
					cc[jj] = '-';
				}

			}
		}
	   }
		j = 1;
		while ((pc = cc[j++]) != '\n') {
			putchar(pc);
		}
		putchar('\n');
	}

	j = 0;
	for (i=0;i<nest+1;i++) {
		j += flag[i];
		}
	if (j > 0) {
		printf("======= Error : #ifdef not closed. \n");
		++rc;
		}

	if (rc == 0) {
		strcpy(endmsg,"OK\0");
	} else {
		strcpy(endmsg,"NG\0");
	}

	printf("\n******  %s! number of Errors is %d  ******\n\n",endmsg,rc);
	return rc;
}		
