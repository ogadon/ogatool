/*
 *   括弧対応 check tool                                 by Hyper halx.oga  
 *                                                                       
 *             usage : ifchk2 [filename] [-N<9>] [-h]         
 *                       -N : ネストの最大値(デフォルトは9)            
 *                                                                     
 *                                                                     
 *                                      V1.00 Initial Revision.  94/03/16      
 *                                      V1.10 expand line.       95/10/24      
 *                                                                    
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* #define NEST 9 */

main(a,b)
int a;
char *b[];
{
	char    cc[500],endmsg[80];
	char    *fname=0;
	int     flag[50];
	int     c, pc, i, j, k, ofs, jj, rc, x;
	int     nest = 9;
	int     keyf = 0;
	FILE *fpin;
	/* FILE *fpout; */


	rc = 0;
	for (i=0; i<nest+1 ; i++) {flag[i] = 0;}

	for (i=1; i<a; i++) {
		if (strcmp("-h",b[i])==0) {
			printf("usage : ifchk2 [filename] [-N<9>] [-h]\n");
			exit(0);
		}
		if (strncmp("-",b[i],1)==0) {
			nest = atoi(b[i]+1);
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
			printf("\n         **************************************************  \n"); 
			printf(  "              括弧対応チェッカ ( %s )     by oga.\n",fname); 
			printf(  "         **************************************************  \n\n"); 
		}
	}
	/*
	if ((fpout = fopen(b[2],"w")) == NULL) {
		printf("Error : 出力ファイルエラー！\n");
		exit(1);
	}
	*/
	while ((c = getc(fpin)) != EOF) {
		if (c > 128 || c < 0)
			continue;
		i = nest+1;
		while (c != '\n' && c != EOF) {          /*  get 1 line  */
		        /* add tab process V1.10 */
			if (c == '\t') {
		            for (x = i-nest-1 - ((i-nest-1)/8)*8; x < 8 ; x++) {
		    	      cc[i++] = ' ';        /* add space */
		            }
			} else {
			    cc[i++] = c;
			}
			c = getc(fpin);
		}
		cc[i] = 0;

		/* printf("line read done....\n"); */
		for (j = 1; j <= nest ; j++) {
			if (flag[j] == 1) {
				cc[j] = '|';
			} else {
				cc[j] = ' ';
			}
		}

	    switch (ck_key_word(&cc[nest+1])) {
	    	case 1 :         /*  '{'     */
				if (flag[nest] == 1) {
					printf("======= Error : '{ }' nest is too deep!\n");
					exit(1);
				}
				for (j = 1; j <= nest && flag[j] == 1 ; j++) ;
				flag[j] = 1;
				cc[j] = '+';
				for (jj = j+1; jj <= nest; jj++) {
					cc[jj] = '-';
				}
				keyf = 1;
				break;
	    	case 2 :         /*  else      */
	    	case 5 :         /*  } {       */
	    	case 7 :         /*  } else {  */
				for (j = nest; j > 0 && flag[j] == 0 ; j--) ; 
				if (j <= 0) {
					printf("======= Error : else is not reached. \n");
					++rc;
				} else {
					cc[j] = '+';
					for (jj = j+1; jj <= nest; jj++) {
						cc[jj] = '-';
					}
				}
				keyf = 1;
				break;
	    	case 4 :         /*  '}'        */
				for (j = nest; j > 0 && flag[j] == 0 ; j--) ;
				if (j <= 0) {
					printf("======= Error : '}' is not reached. \n");
					++rc;
				} else {
					flag[j] = 0;
					cc[j] = '+';
					for (jj = j+1; jj <= nest; jj++) {
						cc[jj] = '-';
					}

				}
				keyf = 1;
			 	break;
			 default :
			 	break;
		}
		if (keyf) {
			k = nest+1;
			while (cc[k] != 0 && cc[k] == ' ') {
				cc[k] = '-';
				k++;
			}
			keyf = 0;
		}
		printf("%s\n",&cc[1]);
	}

	j = 0;
	for (i=0;i<nest+1;i++) {
		j += flag[i];
		}
	if (j > 0) {
		printf("======= Error : '}' not closed. \n");
		++rc;
		}

	if (rc == 0) {
		strcpy(endmsg,"OK\0");
	} else {
		strcpy(endmsg,"NG\0");
	}

	printf("\n******  %s! number of Errors is %d  ******\n\n",endmsg,rc);
}		

int ck_key_word(cc)
char *cc;
{
	int i = 0, ret = 0;

	while (cc[i] != 0) {
		if (cc[i] == '{') {
			ret |= 1;
		}
		if (strncmp(&cc[i],"else",4) == 0) 
			ret |= 2;
		if (cc[i] == '}') {
			ret |= 4;
		}
		i++;
	}
	return ret;
}
