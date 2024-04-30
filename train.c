/*
 *  train
 *
 *    2001/11/18 V0.10 by oga.
 *    2003/02/06 V0.20 support -f option
 *    2005/12/08 V0.30 support multi timetable & comment
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define dprintf if (vf) printf

/* globals */
int timetable[1000];   /* sec from 00:00:00   */
int vf = 0;            /* verbose flag (-v)   */
char title[1024] = ""; /* title of time table */
char *type[1000];      /* comment of train    */


/*
 *  IN  path  : path of .timetable 
 *  IN  tblno : timetable number (0-n)
 *
 *  .timetable format
 *  -----------------------
 *  # timetable1
 *  [title1]
 *  0:1,10(Exp),25
 *      :
 *  23:5,30,45
 *  # timetable2
 *  [title2]
 *  0:1,12,27(Exp)
 *      :
 *  23:5,32,47
 *  -----------------------
 */
int ReadTimeTable(char *path, int tblno)
{
    char buf[4096]; /* each line         */
    char wk[1024];  /* for train comment */
    int  hh, mm;
    int  nent = 0;
    int  ii;
    char *pt;
    FILE *fp = NULL;
    int  found = 0;
    int  tbl_cnt = 0;
    
    strcpy(title, "");

    if ((fp = fopen(path, "r")) == NULL) {
        perror(path);
	return -1;
    }
    nent = 0;
    while (fgets(buf,sizeof(buf),fp)) {
        if (buf[strlen(buf)-1] == 0x0a) {
            buf[strlen(buf)-1] =  '\0';
	}
        if (buf[strlen(buf)-1] == 0x0d) {
            buf[strlen(buf)-1] =  '\0';
	}
	if (buf[0] == '[') {
	    if (found) {
	        break;  /* next timetable */
	    }
	    dprintf("tbl_cnt(%d)-tblno(%d)\n", tbl_cnt, tblno);
	    if (tbl_cnt != tblno) {
	        ++tbl_cnt;
	        continue;
	    }
	    strcpy(title, buf);
	    found = 1;
	    continue;
	}

        if (found == 0 || strlen(buf) == 0 || buf[0] == '#' || strchr(buf,':') == NULL) {
	    continue;
	}

        hh = atoi(buf);
	pt = strchr(buf,':');
	++pt;
	while (*pt != '\0') {
	    mm = atoi(pt);
	    dprintf("%3d %02d:%02d\n", nent, hh, mm);
	    timetable[nent] = hh*3600 + mm*60;

	    /* pt = strchr(pt,','); */

	    while (isdigit(*pt)) ++pt;
	    ii = 0;
	    while (*pt != ',' && *pt != '\0') {
	        wk[ii++] = *pt;
	        ++pt;
	    }
            wk[ii] = '\0';
            type[nent] = strdup(wk);

	    ++nent;
	    /* if (pt == NULL) break; */
	    if (*pt == '\0') break;
	    ++pt;
	}
    }

    if (fp) fclose(fp);

    dprintf("DEBUG: tblno:%d  nent:%d\n", tblno, nent);

    return nent;  /* num of entry */
}

int GetCurSec(char *timestr)
{
   time_t tt;
   struct tm *ptm;

   tt = time(0);
   ptm = localtime(&tt);

   if (timestr) sprintf(timestr, "%02d:%02d:%02d", ptm->tm_hour,
                                                   ptm->tm_min,
			                           ptm->tm_sec);

   return (ptm->tm_hour*3600 + ptm->tm_min*60 + ptm->tm_sec);
}

int main(int a, char *b[])
{
    int  i;
    int  nent;
    int  cursec;
    int  hh, mm;
    int  linef;
    char strcurtime[128];
    char path[1024];
    char *fname = NULL;
    int  fmin = 60;
    int  tbl;

    /* arg check */
    for (i = 1; i<a; i++) {
	if (!strncmp(b[i],"-v",2)) {
	    vf = 1;
	    continue;
	}
	if (!strncmp(b[i],"-f",2) && i+1 < a) {
	    fname = b[++i];
	    continue;
	}
	if (!strncmp(b[i],"-m",2) && i+1 < a) {
	    fmin = atoi(b[++i]);
	    continue;
	}
	if (!strncmp(b[i],"-a",2)) {
	    fmin = 60*24;  /* 24hr */
	    continue;
	}
	if (!strncmp(b[i],"-h",2)) {
	    printf("usage: train [-f <time_table_file>] [{-m <minutes> | -a}]\n");
	    printf("       -f : time table file (default:~/.timetable)\n");
	    printf("       -m : forward minutes (default:60)\n");
	    printf("       -a : all train\n");
	    exit(1);
	}
    }

    if (fname == NULL) {
        sprintf(path, "%s/.timetable", 
	              (getenv("HOME") == NULL)?"":getenv("HOME"));
	fname = path;
    }
    if (fmin <= 0) {
        fmin = 60;
    }

    cursec = GetCurSec(strcurtime);

    tbl = 0;
    while (1) {
      nent = ReadTimeTable(fname, tbl);
      if (nent <= 0) {
          /* printf("no data.\n"); */
          break;
      }

      if (tbl > 0) printf("\n");
      printf("%s\n", title);   /* print title */
      linef = 0;
      for (i = 0; i < nent; i++) {
        /* display -10min - +60(fmin)min */
        if (cursec - 10*60 <= timetable[i] &&
	    timetable[i] <= cursec + fmin*60) {
	    hh = timetable[i]/3600;
	    mm = (timetable[i] - hh*3600)/60;
	    if (cursec < timetable[i]) {
	        if (linef == 0) {
	            linef = 1;
		    printf("------ %s\n", strcurtime);  /* cur time */
		}
	        printf("%02d:%02d (%2d min)  %s\n", hh, mm, 
		                  (timetable[i]-cursec)/60, type[i]);
	    } else {
	        printf("%02d:%02d\n", hh, mm);
	    }
	}
      }

      ++tbl;
    }

}



