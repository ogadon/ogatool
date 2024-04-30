/*
 *  tbl2csv
 *
 *  HTMLのテーブル部分をCSVファイルにする。
 *
 *      98/12/13 by oga
 *      99/01/28 fix1
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int a, char *b[])
{
    int  i;
    int  vf = 0;		/* verbose */
    char *filename = NULL;
    FILE *fp;
    char *pt;
    int  table = 0;		/* TABLE中か */
    int  intd = 0;              /* <TD>中か  */
    char buf[4096];

    for (i = 1; i<a; i++) {
        if (!strncmp(b[i],"-v",2)) {
            vf = 1;
            continue;
        }
        if (!strncmp(b[i],"-h",2)) {
            printf("usage: tbl2csv [<filename>]\n");
            exit(1);
        }
        filename = b[i];
    }

    if (filename) {
	if (!(fp = fopen(filename,"r"))) {
		perror("fopen");
		exit(1);
	}
    } else {
        fp = stdin;
    }

    while (fgets(buf,sizeof(buf),fp)) {
        if (buf[strlen(buf)-1] == 0x0a) {
            buf[strlen(buf)-1] = '\0';
        }
        if (buf[strlen(buf)-1] == 0x0d) {
            buf[strlen(buf)-1] = '\0';
        }
        if ((pt = (char *)strstr(buf,"<TABLE")) || 
                (pt = (char *)strstr(buf,"<table"))) {
	    table = 1;
	    pt = (char *)strstr(pt, ">");
	    if (vf) printf("## TABLE ON!!\n");
        } else {
            pt = &buf[0];
        }
        if (table) {
            for ( ; *pt != '\0' ; ++pt) {
                if (!strncasecmp(pt,"<tr",3)) {
                    printf("\n");
		    while (*pt != '>') ++pt;
	            if (vf) printf("## TR !!\n");
                    continue;
                }
                if (!strncasecmp(pt,"<td",3)) {
                    if (intd) {
                        printf("\",\"");
                    } else {
                        printf(",\"");
		    }
                    intd = 1;			/* TDの中にいる */
		    while (*pt != '>') ++pt;
	            if (vf) printf("## TD ON!!\n");
                    continue;
                }
                if (!strncasecmp(pt,"</td",4)) {
                    if (intd) {
                        printf("\"");
                    }
                    intd = 0;			/* TDから外れた */
		    while (*pt != '>') ++pt;
	            if (vf) printf("## TD OFF!!\n");
                    continue;
                }
                if (intd) {
                    if (*pt == '<') {
	                if (vf) printf("## SKIP TAG !! [");
                        while (*pt != '>' && *pt != '\0') {
	                    if (vf) putchar(*pt);
                            ++pt;  /* TDの中のタグは無視する */
                        }
	                if (vf) printf("%c]\n",*pt);
                    } else { 
			/* タグに囲まれていないものは表示する */
                        if (*pt != '>' && *pt != '\0') {
                            printf("%c",*pt);
			}
                    }
                }
            }
        }
        if ((pt = (char *)strstr(buf,"</TABLE")) || 
                (pt = (char *)strstr(buf,"</table"))) {
	    table = 0;
	    pt = (char *)strstr(pt, ">");
            if (intd) {
                 printf("\"");
            }
            printf("\n");
	    if (vf) printf("## TABLE OFF!!\n");
        }
    }
    if (fp != stdin) {
        fclose(fp);
    }
}
