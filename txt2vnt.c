/*
 *   txt2vnt
 *
 *     09/06/02 V0.10 by oga.
 *     09/06/21 V0.11 fix bug 
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void disp_header()
{
    char work_date[256];    // Date
    char work_dt[256];      // Date Time   yyyymmddTOnnnnnZ
    time_t tt;
    struct tm *tmp;
    
    tt  = time(0);
    tmp = localtime(&tt);

    printf("BEGIN:VNOTE\r\n");
    printf("VERSION:1.1\r\n");
    // DCREATED:20090507T031900Z
    // LAST-MODIFIED:20090507T031900Z
    strftime(work_date, sizeof(work_date), "%Y%m%d", tmp);
    sprintf(work_dt, "%sTO%dZ", work_date, tmp->tm_hour*3600 + tmp->tm_min*60 + tmp->tm_sec);
    printf("DCREATED:%s\r\n", work_dt);
    printf("LAST-MODIFIED:%s\r\n", work_dt);
    printf("BODY;CHARSET=SHIFT_JIS;ENCODING=QUOTED-PRINTABLE:");
}

void disp_footer()
{
    printf("CATEGORIES:%s\r\n", "");
    printf("END:VNOTE\r\n");
}

void usage()
{
    printf("usage: txt2vnt [txt_file]\n");
}

int main(int a, char *b[])
{
    int  i;
    int  vf = 0;		/* verbose */
    int  cnt = 0;
    unsigned int c;
    char *filename = NULL;
    FILE *fp;

    for (i = 1; i<a; i++) {
        if (!strcmp(b[i],"-h")) {
            usage();
        }
        if (!strcmp(b[i],"-v")) {
            vf = 1;
            continue;
        }
        //if (i+1 < a && !strcmp(b[i],"-x")) {
        //    xx = atoi(b[++i]);
        //    continue;
        //}
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

    disp_header();
    cnt = 0;
    while ((c = getc(fp)) != EOF) {
        ++cnt;
        if (cnt >= 20) {  // 20çsÇ≈â¸çs 
            //printf("\r\n\t");
            printf("=\r\n");
            cnt = 0;
        }

        if (isprint(c)) {
            putchar(c);
        } else {
            printf("=%02x", c);
        }
    }
    if (cnt) printf("\r\n");
    disp_footer();

    fclose(fp);
        
}
