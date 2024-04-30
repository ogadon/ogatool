/*
 *  ctrain
 *
 *    2005/12/11 V0.10 by oga.
 *    2005/12/12 V0.11 support -l
 */

#if 0
30                 |20                 |10                 |0 東戸塚
             ◇    |       ◇          |                   +------------+
            □□□ |      □□□       |                   |  Station   |
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
            20:30         15:05

             <>    |       <>          |                   +------------+
            [][][] |      [][][]       |                   |   東戸塚   |
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
            20:30         15:05

----+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8
------------------------------------------------------------------------------
#endif 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#define sleep(x)  Sleep(((x)*1000))
#endif

#define dprintf if (vf) printf
#define ISKANJI(c) ((c >= 0x80 && c < 0xa0) || (c >= 0xe0 && c<0xff))
#define SCRN_ORGX   50       /* screen origin X */
#define SCRN_LINES   7       /* screen lines    */
#define TRAIN1    "<>"       /* train char 1 */
/*#define TRAIN2    "□□□"   /* train char 2 */
#define TRAIN2    "[][][]"   /* train char 2 */

/* globals */
int vf = 0;            /* verbose flag (-v)   */
int lf = 0;            /* list    flag (-l)   */
int timetable[1000];   /* sec from 00:00:00   */
char title[1024] = ""; /* title of time table */
char *type[1000];      /* comment of train    */
unsigned char scrn[SCRN_LINES][200]; /* screen buffer */


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

/* 指定位置(0～)の文字がSJISの2文字目かをチェック */
int Is2ndChar(unsigned char *str, int pos)
{
    int i;
    int kf = 0;


    /* 1個前の文字がSJISの1文字目かをチェックする */
    for (i = 0; i<pos; i++) {
        if (kf == 0 && ISKANJI(str[i])) {
	    kf = 1;
	} else {
	    kf = 0;
	}
    }
    dprintf("Is2ndChar: pos:%d result:%d\n", pos, kf);
    return kf;
}

/* 漢字区切りで指定した長さに縮める */
void TruncWStr(unsigned char *str, int len)
{
    /* 簡易版 */
    if (Is2ndChar(str, len-1)) {   /* Is2ndChar() */
        str[len] = '\0';
    } else {
        str[len-1] = '\0';
    }
}

void InitScrnBg()
{
    memset(scrn, ' ', sizeof(scrn));
    sprintf(&scrn[0][SCRN_ORGX],
      "30min              |20min              |10min              |0 %s", title);
    strcpy(&scrn[1][SCRN_ORGX] ,"                   |                   |                   +------------+                          ");
    strcpy(&scrn[2][SCRN_ORGX] ,"                   |                   |                   |   Station  |                          ");
    strcpy(&scrn[3][SCRN_ORGX] ,"+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
}

int main(int a, char *b[])
{
    int  i;
    int  nent;
    int  cursec;
    int  hh, mm;     /* time table value */
    char strcurtime[128];
    char path[1024];
    char *fname   = NULL;
    int  fmin     = 33;  /* disp start before 33 minutes */
    int  tbl      = 0;   /* timetable number */
    int  interval = 1;   /* update interval  */
    int  rsec     = 0;   /* sec to reach     */

    char wkbuf[1024];

    /* arg check */
    for (i = 1; i<a; i++) {
	if (!strncmp(b[i],"-v",2)) {
	    vf = 1;
	    continue;
	}
	if (!strncmp(b[i],"-l",2)) {
	    lf = 1;
	    continue;
	}
	if (!strncmp(b[i],"-f",2) && i+1 < a) {
	    fname = b[++i];
	    continue;
	}
	if (!strncmp(b[i],"-t",2) && i+1 < a) {
	    tbl = atoi(b[++i]);
	    continue;
	}
	if (!strncmp(b[i],"-u",2) && i+1 < a) {
	    interval = atoi(b[++i]);
	    if (interval == 0) {
	        interval = 1;
	    }
	    continue;
	}
	if (!strncmp(b[i],"-h",2)) {
	    printf("usage: ctrain [-f <timetable_file>] [-t <n>] [-u <sec>] [-l]\n");
	    printf("       -f : timetable file   (default:~/.timetable)\n");
	    printf("       -t : timetable number (default: 0)\n");
	    printf("       -l : timetable list\n");
	    printf("       -u : update interval  (default: 1 sec)\n");
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

    /* time table list */
    if (lf) {
        i = 0;
        while (ReadTimeTable(fname, i) > 0) {
	    printf("%02d:%s\n", i, title);
	    ++i;
	}
        return 0;
    }

    nent = ReadTimeTable(fname, tbl);
    if (nent <= 0) {
        printf("no data.\n");
        return 0;
    }

    printf("\n\n\n\n\n\n\n");

    while (1) {
      InitScrnBg();   /* 画面バッファ初期化 */
      cursec = GetCurSec(strcurtime);

      for (i = nent-1; i >= 0; i--) {
        /* display -10min ～ +60(fmin)min */
        if (cursec - 10*60 <= timetable[i] &&
	    timetable[i] <= cursec + fmin*60) {
	    hh = timetable[i]/3600;
	    mm = (timetable[i] - hh*3600)/60;
	    rsec = timetable[i]-cursec;

	    /* cur time */
	    strncpy(&scrn[0][SCRN_ORGX+60-strlen(strcurtime)-3], strcurtime, strlen(strcurtime));

	    /* train */
	    strncpy(&scrn[1][SCRN_ORGX+60-(rsec/30)-strlen(TRAIN2)+1], TRAIN1, strlen(TRAIN1));
	    strncpy(&scrn[2][SCRN_ORGX+60-(rsec/30)-strlen(TRAIN2)],   TRAIN2, strlen(TRAIN2));

	    /* time to reach */
	    if (rsec > 0) {
	        sprintf(wkbuf, "%02d:%02d", rsec/60, rsec % 60);
	        strncpy(&scrn[4][SCRN_ORGX+60-(rsec/30)-strlen(TRAIN2)+1], wkbuf, strlen(wkbuf));
	    }

	    /* time to arrive */
	    /*sprintf(wkbuf, "%02d:%02d発", hh, mm); */
	    sprintf(wkbuf, "%02d:%02ddp", hh, mm);
	    strncpy(&scrn[5][SCRN_ORGX+60-(rsec/30)-strlen(TRAIN2)+1], wkbuf, strlen(wkbuf));

	    /* type of train */
	    strncpy(&scrn[6][SCRN_ORGX+60-(rsec/30)-strlen(type[i])], type[i], strlen(type[i]));
	}
      }

      printf("%cM%cM%cM%cM%cM%cM%cM", 27,27,27,27,27,27,27);
      for (i = 0; i < SCRN_LINES; i++) {
          strncpy(wkbuf, &scrn[i][SCRN_ORGX], 79);
	  TruncWStr(wkbuf, 79);
          printf("%s\n", wkbuf);
      }
      sleep(interval);
    }
}



