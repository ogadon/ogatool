/*
 *  strftime
 *
 *   07/03/13 V1.01 support -brew
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void usage()
{
    printf("usage : strftime { [-brew] <time_t> | -r <yymmddhhmmss> [-summer] }\n");
    printf("        -brew : based 1980/01/06\n");   /* V1.01-A */
    exit(1);
}

main(a,b)
int a;
char *b[];
{
	struct tm *tmp;
	time_t tt;
	char   buf[1024];
	int    rf   = 0;     /* -r      */
	int    smf  = 0;     /* -summer */
	int    brf  = 0;     /* -brew   */
	char   *arg = NULL;  /* <time_t> or <yymmddhhmmss> */
	int    i;

	for (i = 1; i<a; i++) {
	    if (!strcmp(b[i], "-r")) {
	        rf = 1;
		continue;
	    }
	    if (!strcmp(b[i], "-h")) {
	        usage();
		continue;
	    }
	    if (!strcmp(b[i], "-summer")) {
	        smf = 1;
		continue;
	    }
	    if (!strcmp(b[i], "-brew")) {
	        brf = 1;
		continue;
	    }
	    arg = b[i];
	}

	if (arg == NULL) {
            usage();
        }

	/* arg check */
	if (brf && rf) {
            usage();
	}

	if (rf == 0) {
            /* arg is time_t value */
	    /*tt  = atoi(b[1]);*/
	    tt = strtoul(arg, (char **)NULL, 0);
	    /* V1.01-A start */
	    if (brf) {
	        tt += 3657*3600*24;  /* 10”N‚Æ5“ú(365.25x8+365x2+5) */
	    }
	    /* V1.01-A start */
	    tmp = localtime(&tt);
	    if (tmp == NULL) {
                printf("invalid value\n");
                exit(1);
            }
	    printf("%04d/%02d/%02d %02d:%02d:%02d\n",
			tmp->tm_year+1900,
			tmp->tm_mon+1,
			tmp->tm_mday,
			tmp->tm_hour,
			tmp->tm_min,
			tmp->tm_sec);
	    printf("summer = %d\n",tmp->tm_isdst);

	    strftime(buf, sizeof(buf), "strftime: %Y/%m/%d %H:%M:%S\n", tmp);
	    printf(buf);
	} else {
	    /* -r yymmddhhmmss */
	    char   tmbuf[5];
	    time_t curtt;
	    struct tm tm;

	    strcpy(buf, arg);
            curtt = time(0); 	/* Œ»İ‚Ì•b       */

            /* “ú•tŠÔ‚©‚çstruct tm ‚ğì¬ */
            tmbuf[2] = '\0';  
            strncpy(tmbuf,&buf[0], 2);	/* yy */
            tm.tm_year = (atoi(tmbuf) < 95) ? atoi(tmbuf)+100 : atoi(tmbuf);
            strncpy(tmbuf,&buf[2], 2);	/* mm */
            tm.tm_mon  = atoi(tmbuf)-1;
            strncpy(tmbuf,&buf[4], 2);	/* dd */
            tm.tm_mday = atoi(tmbuf);
            strncpy(tmbuf,&buf[6], 2);	/* hh */
            tm.tm_hour = atoi(tmbuf);
            strncpy(tmbuf,&buf[8], 2);	/* mm */
            tm.tm_min  = atoi(tmbuf);
            strncpy(tmbuf,&buf[10],2);	/* ss */
            tm.tm_sec  = atoi(tmbuf);

	    if (smf) {
                tm.tm_isdst= 1;		/* summer time */
		printf("summer time.\n");
	    } else {
                tm.tm_isdst= 0;		/* not summer time */
	    }

	    printf("%04d/%02d/%02d %02d:%02d:%02d\n",
			tm.tm_year+1900,
			tm.tm_mon+1,
			tm.tm_mday,
			tm.tm_hour,
			tm.tm_min,
			tm.tm_sec);

            tt = mktime(&tm);
            printf("time_t ... current:%d / specified:%d\n",curtt,tt);

	    strftime(buf, sizeof(buf), "strftime: %Y/%m/%d %H:%M:%S\n", &tm);
	    printf(buf);
	}

}

/* vim:ts=8:sw=8:
 */
