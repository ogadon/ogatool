/*
 *  perfan
 * 
 *  ログ(hh:mm:ss.mmm形式の時刻を持つログ)から
 *  経過時刻を表示する。
 *
 *    99/11/30  V1.00 by oga
 *    99/11/30  V1.01 -sql support
 *    99/12/04  V1.02 -wco support
 *    99/12/09  V1.03 WinNT support
 *    00/06/08  V1.04 start/end diff support
 *    00/07/10  V1.05 support DbaConnectionImpl::commit() for TraceLevel10
 *    00/07/11  V1.06 support initialize(=CreateParamStatement),
 *                            DBPreparedStatment::GetResultSet(=execute) for TraceLevel10
 *    00/09/12  V1.07 -osql support
 *    00/10/04  V1.08 ave 1/10000 sec
 *    00/11/20  V1.09 add DBStatement::GetResultSet(=execute), 
 *                    -ex(for Level-1)support
 *                    -max
 *    00/03/29  V1.10 uname support (not available)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <strings.h>
#include <sys/utsname.h>
#endif

#define VER "V1.09"

/*
 *  Type Definition
 */
typedef struct hmsm {
    int hh;     /* hour        */
    int mm;     /* minute      */
    int ss;	/* sec         */
    int ms;	/* mili second */
} hmsm_t;

char *executeKey1 = "DBPreparedStatment::GetResultSet()";/* for TraceLevel=10 */
char *executeKey2 = "DBStatement::GetResultSet()";       /* for TraceLevel=10 */

/*
 *  MACROS
 */
/* execute    : maybe SELECT        */
/* #define IS_EXECUTE(buf)		strstr(buf,"::execute()") */
#define IS_EXECUTE(buf)		strstr(buf,executeKey1)   /* V1.09 */
#define IS_EXECUTE2(buf)	strstr(buf,executeKey2)   /* V1.09 */

/* next       : maybe Fetch         */
#define IS_NEXT(buf)		strstr(buf,"::next()")

/* execUpdate : maybe UPDATE/INSERT */
#define IS_EXECUPDATE(buf)	strstr(buf,"::execUpdate()")

/* commit     : COMMIT              */
/* #define IS_COMMIT(buf)	strstr(buf,"DbaTransactionImpl::commit()") */
#define IS_COMMIT(buf)		strstr(buf,"DbaConnectionImpl::commit()")

/* initialize : createParamStatement*/
/*#define IS_INIT(buf) strstr(buf,"DbaConnectionImpl::createParamStatement()")*/
#define IS_INIT(buf)		strstr(buf,"DbaParamStatementImpl::initialize()")

/* initialize : createStatement(TraceLeve=-1) V1.09 */
#define IS_CREATES(buf)		strstr(buf,"DbaConnectionImpl::createStatement()")


/* SQL ID */
#define ID_EXECUTE	0
#define ID_EXECUTE2	1
#define ID_NEXT		2
#define ID_EXECUPDATE	3
#define ID_COMMIT	4
#define ID_INITIALIZE	5
#define ID_CREATES	6
#define ID_MAX          7	/* array size          */

/*
 *  globals
 */
hmsm_t start_tms[ID_MAX];       /* enter time          */
int    ms_total[ID_MAX];	/* Total millisec      */
int    cnt[ID_MAX];		/* Total SQL           */
int    u_max[ID_MAX];		/* unit max                      V1.09 */
int    u_min[ID_MAX];		/* unit min                      V1.09 */
int    vf      = 0;		/* verbose             */
int    maxf    = 0;		/* -max : display max/min        V1.09 */
int    first   = 1;		/* first line of one command execute   */
int    com_cnt = 0;		/* command execute counter             */
int    cscnt   = 0;		/* for Duplicate createStatement V1.09 */
int    msec    = 0;		/* diff msec                     V1.09 */

hmsm_t com_start, com_end;      /* for command time         */
hmsm_t head_tms;      		/* for header change        */
hmsm_t oldtms;			/* previous time            */

#ifdef _WIN32
char *head_key = "Microsoft";   /* -hk: header kerword */
#else  /* !_WIN32 */
char *head_key = "HP-UX";       /* -hk: header kerword */
#endif /* !_WIN32 */

void usage()
{
    printf("usage: perfan filename {-k <key_word> [-k <keyword> ...] | -a [-t] }\n");
    printf("                       [-p <print_pos(90/25)>]\n");
    printf("                       [-hk <header_keyword(HP-UX/*SQL*)>]\n");
    printf("                       [-sql]\n");
    printf("                       [-osql]\n");
    printf("                       [-wco [-max] [-ex]]\n");
    printf("       -k   : search keyword\n");
    printf("       -a   : search all line\n");
    printf("       -t   : add time only for '(HH:MM:SS.mm)'\n");
    printf("       -p   : print position(90:WCO log/25:SQL trace\n");
    printf("       -hk  : header keyword(HP-UX:WCO log/*SQL*:SQL trace\n");
    printf("       -sql : for HiRDB  SQL trace\n");
    printf("       -osql: for Oracle SQL trace\n");
    printf("       -wco : for WCO perf analyze (unnecessary -k, -a)\n");
    printf("       -ex  : for TraceLevel=-1 log (execute time accuracy up)\n");
    printf("       -max : display max/min\n");
}

/*
 *  get_diff_times(hmsm_t *old, hmsm_t *new, hmsm_t *diff)
 *
 *  FUNC     : get difference of two hmsm_t (old-new)
 *
 *  IN  old  : old hmsm_t
 *      new  : new hmsm_t
 *      diff : diff hmsm_t (old - new)
 *  OUT ret  : 0 success
 */
int get_diff_times(hmsm_t *old, hmsm_t *new, hmsm_t *diff)
{
    int new1, old1, diff1;

    new1 = new->hh*3600000 + new->mm*60000 + new->ss*1000 + new->ms;
    old1 = old->hh*3600000 + old->mm*60000 + old->ss*1000 + old->ms;

    if (new1 - old1 < 0) {
	new1 += 3600000;
    }
    diff1 = new1 - old1;

    diff->hh = diff1/3600000;
    diff1 -= (3600000 * diff->hh);
    diff->mm = diff1/60000;
    diff1 -= (60000 * diff->mm);
    diff->ss = diff1/1000;
    diff1 -= (1000 * diff->ss);
    diff->ms = diff1 % 1000;

    return 0;
}

/*
 *  add_times(hmsm_t *t1, hmsm_t *t2, hmsm_t *addt)
 *
 *  FUNC     : add two hmsm_t (t1+t2) to addt
 *
 *  IN  t1   : hmsm_t
 *      t2   : hmsm_t
 *      addt : add result
 *  OUT ret  : 0 success
 */
int add_times(hmsm_t *t1, hmsm_t *t2, hmsm_t *addt)
{
    int tt1, tt2, add1;

    tt1 = t1->hh*3600000 + t1->mm*60000 + t1->ss*1000 + t1->ms;
    tt2 = t2->hh*3600000 + t2->mm*60000 + t2->ss*1000 + t2->ms;

    add1 = tt1 + tt2;

    addt->hh = add1/3600000;
    add1 -= (3600000 * addt->hh);
    addt->mm = add1/60000;
    add1 -= (60000 * addt->mm);
    addt->ss = add1/1000;
    add1 -= (1000 * addt->ss);
    addt->ms = add1 % 1000;

    return 0;
}

/* 
 *  IN  : buf : include "HH:MM:SS.mmm" str
 *  OUT : tms : store tms to HH,MM,SS,mmm
 *        str : "HH:MM:SS.mmm"
 *        ret : !=0  : success (return "HH:MM:SS.mmm" pos ptr on buf)
 *              NULL : false (not include "HH:MM:SS.mmm" str)
 */
char *get_times(char *buf, hmsm_t *tms, char *str)
{
    char *pt = NULL;

    if (vf >= 2)printf("get_times : buf=[%s]\n", buf);

    memset(tms, 0, sizeof(hmsm_t));
    strcpy(str, "");			/* HH:MM:SS.mmm */

    if ((pt = strchr(buf,':')) && *(pt+3) == ':' && *(pt+6) == '.') {
	pt -= 2;			/* point to top of "HH:..."    */
	strncpy(str, pt, 12);		/* copy "HH:MM:SS.mmm" to str  */
	str[12] = '\0';			/* terminator                  */
	if (!isdigit(str[0])) {
	    /* skip header's  hh:mm:ss.sss */
            if (vf >= 2) printf("get_time() return NULL [%s]\n", str);
	    return NULL;
	}
	tms->hh = atoi(&str[0]);
	tms->mm = atoi(&str[3]);
	tms->ss = atoi(&str[6]);
	tms->ms = atoi(&str[9]);
        if (vf >= 2) {
	    printf("time = [%s]  %02d:%02d:%02d.%03d\n",str,
				tms->hh,tms->mm,tms->ss,tms->ms);
	}
	return pt;  /* success */
    }
    if (vf >= 2) printf("get_time() return NULL\n");
    return NULL; /* false */
}

/*
 *  Oracle SQL Trace用
 *    "tim=xxxxxx" がある場合、先頭に、"HH:MM:SS.mmm "を付加する。
 *     (print_times()に渡すための前処理) 
 *  
 *  IN  : buf    : .... tim=xxxxxxxxxx ...
 *                 (xxxxxは1/100秒単位)
 *  OUT : obuf   : HH:MM:SS.mm0 <buf>      HH:MM:SSは本当の時刻ではないので注意
 *        ret    : 0 : 無変換("tim="なし)
 */
int tim2hms(char *buf, char *obuf)
{
    int  tim;
    int  hh,mm,ss,mmm;
    char *pt;

    if (pt = strstr(buf, "tim=")) {
        tim = atoi(&pt[5]);            /* "tim=xyyyyyyyy" のyyyy部分を数値に */
        /* printf("DEBUG: tim=%d\n",tim); */
        hh = tim/100/3600;
        mm = (tim - hh*100*3600)/100/60;
        ss = (tim - hh*100*3600 - mm*100*60)/100;
        mmm= (tim % 100) * 10;
        sprintf(obuf,"%02d:%02d:%02d.%03d %s", hh%24, mm, ss, mmm, buf);
        /* printf("DEBUG: %s\n",obuf); */
        return 1;
    }

    strcpy(obuf, buf);   /* そのまま返す */
    return 0;
}

/*
 *  bufから時刻部分を取出し、前回との差分時刻を出力する。 for WCO log or other
 *  
 *  IN  : buf    : ....19:11:08.536 ....
 *  OUT : stdout : 19:11:08.536 (00:00:00.000) 文字列
 */
int print_times(char *buf, int pos)
{
    hmsm_t tms, diff;
    char timstr[1024];

    if (get_times(buf, &tms, timstr) == NULL) {
	return -1;
    }

    get_diff_times(&oldtms, &tms, &diff);

    if (first) {
	printf("%s (00:00:00.000) ", timstr);
	first = 0;
    } else {
	printf("%s (%02d:%02d:%02d.%03d) ", timstr,
		diff.hh, diff.mm, diff.ss, diff.ms);
    }
    memcpy(&oldtms, &tms, sizeof(hmsm_t));
    printf("%s\n", &buf[pos]);
    
    return 0;
}

/* V1.01 */
/*
 *  bufから時刻部分2個を取出し、差分時刻を出力する。 for SQL trace
 * 
 *  IN  : buf    : ....19:11:08.536 .. 19:11:08.630 ....
 *  OUT : stdout : 19:11:08.536 (00:00:00.000) 文字列
 */
int print_times2(char *buf, int pos)
{
    hmsm_t tms, diff;
    char timstr[1024], timstr_end[1024];
    char *pt = NULL;

    if ((pt = get_times(buf, &oldtms, timstr)) == NULL) {
	return -1;
    }

    if (get_times(pt+12, &tms, timstr_end) == NULL) {
	return -1;
    }

    get_diff_times(&oldtms, &tms, &diff);

    printf("(%02d:%02d:%02d.%03d) ",
		diff.hh, diff.mm, diff.ss, diff.ms);
    printf("%s\n", &buf[pos]);
    
    return 0;
}

/*
 *  PrintResult(char *str, int id, int total)
 *
 *  FUNC      : print one result
 *  IN  str   : print prefix string
 *      id    : SQL ID
 *      total : command total sec for percent display
 */
void PrintResult(char *str, int id, int total)
{
    /* printf("DEBUG : ms_total[%d]=%d\n",id,ms_total[id]); */
    printf("%s %9.3f(sec)/%7d(times)  ave:%6.4f(sec) %5.1f%%\n", str,
		((float)ms_total[id])/1000 , cnt[id],
		cnt[id]?((float)(ms_total[id]))/1000/cnt[id]:0,
		((float)ms_total[id]*100)/total);
    if (maxf && cnt[id]) {
        printf("                                                    max:%4.3f min:%4.3f\n",
		((float)(u_max[id]))/1000,
		((float)(u_min[id]))/1000);
    }
}

/*
 *  PutAllResult()
 *
 *  FUNC      : print(flush) all result
 */
void PutAllResult()
{
    int sql_total = 0, com_total;
    int id;
    hmsm_t com_diff;

    /* get SQL Total time */
    for (id = 0; id < ID_MAX; id++) {
        sql_total += ms_total[id];
    }

    /* get Command Total time */
    get_diff_times(&com_start, &com_end, &com_diff);
    com_total = GetMilliSec(&com_diff);

    /* print result */
    printf("\n############  WCO Perf Analyze %s Result[%d]  #########\n", VER, com_cnt);
    PrintResult("execute(SELECT1)    ", ID_EXECUTE,         com_total);
    PrintResult("execute(SELECT2)    ", ID_EXECUTE2,        com_total);
    PrintResult("next(FETCH)         ", ID_NEXT,            com_total);
    PrintResult("execUpdate(INST/DEL)", ID_EXECUPDATE,      com_total);
    PrintResult("commit(COMMIT)      ", ID_COMMIT,          com_total);
    PrintResult("createParamStatement", ID_INITIALIZE,      com_total);
    PrintResult("createStatement     ", ID_CREATES,         com_total);
    printf("---------------------------------------------------\n");
    printf("SQL     TOTAL     %12.3f(sec)  %5.1f%%\n", 
		((float)sql_total)/1000, ((float)sql_total*100)/com_total);

    printf("No SQL  TOTAL     %12.3f(sec)  %5.1f%%\n", 
		((float)com_total-sql_total)/1000, 
		((float)(com_total-sql_total)*100)/com_total);
    printf("---------------------------------------------------\n");
    printf("Command TOTAL     %12.3f(sec)  %5.1f%%\n", 
		((float)com_total)/1000, ((float)com_total*100)/com_total);
    printf("\n");

    /* reset counters */
    memset(&head_tms,     0, sizeof(head_tms));
    memset(&start_tms[0], 0, sizeof(start_tms));
    memset(&ms_total[0],  0, sizeof(ms_total));
    memset(&cnt[0],       0, sizeof(cnt));
    memset(&com_start,    0, sizeof(com_start));
    memset(&com_end,      0, sizeof(com_end));

    first = 1;   /* reset first line      */
    cscnt = 0;
    ++com_cnt;   /* command execute count */
}

/*
 *   GetMilliSec(hmsm_t *tms)
 *
 *   FUNC    : convert hmsm_t to millisecond
 *   IN  tms : hmsm_t
 *   OUT ret : millisecond
 */
int GetMilliSec(hmsm_t *tms)
{
    return(tms->hh*3600*1000 + tms->mm*60*1000 + tms->ss*1000 + tms->ms);
}

/* V1.02 */
/*
 *   AnalyzeWcoLog(char *buf)
 *
 *   IN  buf : WCO Log 1 line
 *   OUT ret : 0  success
 *             -n failed
 */
int AnalyzeWcoLog(char *buf)
{
    char *ex = NULL, *up = NULL,  *cm = NULL, *nx = NULL;
    char *in = NULL, *ex2 = NULL, *cs = NULL;                 /* V1.06,V1.09 */
    char *pt;
    char wkstr[2048];
    int  id = -1; 
    hmsm_t wktms, diff;

    /* get time */
    if (!(pt = get_times(buf, &wktms, wkstr))) {
	return -2;
    }

    /* skip header */
    if (strstr(buf, head_key)) {
	if (GetMilliSec(&head_tms) && memcmp(&head_tms, &wktms, sizeof(wktms))) {
	    /* if head_tms != 0 && change header time then flush results */
	    PutAllResult();
	}
	printf("%s\n", buf);
        memcpy(&head_tms, &wktms, sizeof(hmsm_t));
	return -1;
    }

    /* get command start time */
    if (first) {
        memcpy(&com_start, &wktms, sizeof(hmsm_t));
	first = 0;
    }
    /* get command end time   */
    memcpy(&com_end, &wktms, sizeof(hmsm_t));

    if (!(nx = IS_NEXT(buf))       && !(ex = IS_EXECUTE(buf)) && 
        !(ex2= IS_EXECUTE2(buf))   && !(cs = IS_CREATES(buf)) && 
	!(up = IS_EXECUPDATE(buf)) && !(cm = IS_COMMIT(buf))  &&
	!(in = IS_INIT(buf))
	) {
	return -1;
    }

    if (nx) {
	id = ID_NEXT;
    } else if (ex) {
	/* execute(1) */
	id = ID_EXECUTE;
    } else if (ex2) {
	/* execute(2) */
	id = ID_EXECUTE2;
    } else if (up) {
	/* execUpdate() */
	id = ID_EXECUPDATE;
    } else if (cm) {
	/* commit() */
	id = ID_COMMIT;
    } else if (in) {
	/* initialize(createParamStatement) */
	id = ID_INITIALIZE;
    } else if (cs) { /* V1.09 */
	/* createStatement(createStatement) Level=-1 only */
	id = ID_CREATES;
    } else {
	printf("Error : Invalid Line [%s]\n",buf);
	return -3;
    }

    if (strstr(buf,"enter ") || strstr(buf,"call ")) {
	/* enter or call */
	if (cs) {
	    /* for duplicate enter (createStatement) */
	    if (cscnt == 0) {
	        memcpy(&start_tms[id], &wktms, sizeof(hmsm_t));
	    }
	    ++cscnt;
	} else {
	    memcpy(&start_tms[id], &wktms, sizeof(hmsm_t));
	}
	if (vf) {
		printf("##                   %s\n", buf);
	}
    } else if (strstr(buf,"exit ") || strstr(buf,"return ")) {
	if (cs) {
	    --cscnt;
	    if (cscnt > 0) {
		return 0;
	    } else if (cscnt < 0) {
		cscnt = 0;
	        printf("Error : exit with no enter createStatement [%s]\n",buf);
		return -6;
	    }
	}
        /* exit or return */
	if (!GetMilliSec(&start_tms[id])) {
	    printf("Error : exit with no enter [%s]\n",buf);
	    return -4;
	}
	get_diff_times(&start_tms[id], &wktms, &diff);
	msec = GetMilliSec(&diff);
	ms_total[id] += msec;
	if (u_max[id] < msec) {
	    u_max[id] = msec;
	}
	if (u_min[id] > msec) {
	    u_min[id] = msec;
	}
	++cnt[id];
	if (vf) {
	    printf("## %02d:%02d:%02d:%03d (%d) %s\n",
			   diff.hh, diff.mm, diff.ss, diff.ms, cnt[id], buf);
	}
	memset(&start_tms[id], 0, sizeof(hmsm_t));   /* clear enter time */
    } else {
	printf("Error : no exit or enter [%s]\n",buf);
	return -5;
    }

    return 0;
}


int main(a,b)
int a;
char *b[];
{
    char *fname = NULL;
    FILE *fp;
    int  i;
    int  pos = 90;		/* -p   : 表示開始位置        */
    int  af     = 0;		/* -a   : 全表示              */
    int  wcf    = 0;		/* -wco : WCO性能解析          V1.02 */
    int  sqlf   = 0;		/* -sql : HiRDB  SQLトレース用 V1.01 */
    int  osqlf  = 0;		/* -osql: Oracle SQLトレース用 V1.07 */
    int  tf     = 0;		/* -t   : (時刻)合計           V1.04 */
    int  exf    = 0;		/* -ex  : for TraceLevel=10    V1.09 */
    int  keycnt = 0;
    int  rtn;
    char *keyword[200];         /* -k   : キーワード格納      */
    char buf[4096];
    char newbuf[4096];
    hmsm_t totaltms;		/* total time for -t V1.04    */
    hmsm_t ttms;		/* wk tms for -t     V1.04    */

#if 0
#ifndef _WIN32
    struct utsname unbuf;

    uname(&unbuf);
    head_key = unbuf.sysname;
#endif
#endif /* 0 */

    memset(keyword,       0, sizeof(keyword));
    memset(&oldtms,       0, sizeof(oldtms));
    /* V1.02 start */
    memset(&start_tms[0], 0, sizeof(start_tms));
    memset(&ms_total[0],  0, sizeof(ms_total));
    memset(&u_max[0],     0, sizeof(u_max));
    memset(&cnt[0],       0, sizeof(cnt));
    memset(&head_tms,     0, sizeof(head_tms));
    memset(&com_start,    0, sizeof(com_start));
    memset(&com_end,      0, sizeof(com_end));
    /* V1.02 end */
    memset(&totaltms,     0, sizeof(oldtms));    /* V1.04 */

    for (i = 0; i < ID_MAX; i++) {
        u_min[i] = 99999;
    }

    /* arg check */
    for (i = 1; i<a; i++) {
	if (!strcmp(b[i],"-v")) {
	    vf++;
	    continue;
	}
	if (!strcmp(b[i],"-a")) {
	    af = 1;
	    continue;
	}
	if (!strcmp(b[i],"-t")) {
	    tf = 1;
	    continue;
	}
	if (!strcmp(b[i],"-wco")) {
	    wcf = 1;
	    continue;
	}
	if (!strcmp(b[i],"-sql")) {
	    sqlf = 1;
	    pos = 25;
	    head_key = "*SQL*";
	    continue;
	}
	if (!strcmp(b[i],"-osql")) {
	    osqlf = 1;
	    pos = 13;
	    head_key = "*** SESSION ID:(";
	    continue;
	}
	if (!strcmp(b[i],"-p")) {
	    i++;
	    pos = atoi(b[i]);
	    continue;
	}
	if (!strcmp(b[i],"-hk")) {
	    i++;
	    head_key = (char *)strdup(b[i]);
	    continue;
	}
	if (!strcmp(b[i],"-k")) {
	    i++;
	    keyword[keycnt] = (char *)strdup(b[i]);
	    keycnt++;
	    continue;
	}
	if (!strcmp(b[i],"-ex")) {  /* V1.09 */
	    /* for TraceLevel=-1 */
	    exf = 1;
            executeKey1 = "DbaParamStatementImpl::execute()";
            executeKey2 = "DbaStatementImpl::execute()";
	    continue;
	}
	if (!strcmp(b[i],"-max")) {  /* V1.09 */
	    /* disp max/min */
	    maxf = 1;
	    continue;
	}
	if (!strcmp(b[i],"-h")) {
	    usage();
	    exit(1);
	}
	fname = b[i];
    }

    /* open file */
    if (!wcf && (af == 0 && keyword[0] == NULL) ) {
	usage();
	exit(1);
    }

    if (fname == NULL || !strcmp(fname, "-")) {
	fprintf(stderr, "input from stdin!!\n");
	fp = stdin;
    } else {
	if ((fp = fopen(fname,"r")) == 0) {
	    perror("fopen");
	    exit(1);
	}
    }
    while (fgets(buf,sizeof(buf),fp)) {
	if (buf[strlen(buf)-1] == 0x0a) {
	    buf[strlen(buf)-1] = '\0';
	}
	if (buf[strlen(buf)-1] == 0x0d) {
	    buf[strlen(buf)-1] = '\0';
	}

	if (wcf) { /* V1.02 */
	    /* for WCO Log analyze for perf */
	    AnalyzeWcoLog(buf);
	    continue;
	}

	/* ヘッダキーワードチェック */
	if (strstr(buf, head_key)) {
	    hmsm_t wktms;
	    char   wkstr[2048];

            if (!(get_times(buf, &wktms, wkstr))) {
	        first = 1;	/* reset */
	        printf("%s\n", buf);
	        continue;
            }
	    if (GetMilliSec(&head_tms) && memcmp(&head_tms, &wktms, sizeof(wktms))) {
	        /* if head_tms != 0 && change header time then reset first */
	        first = 1;	/* reset */
	    }
	    printf("%s\n", buf);
            memcpy(&head_tms, &wktms, sizeof(hmsm_t));
	    if (tf) {
       	        memset(&totaltms, 0, sizeof(hmsm_t)); /* V1.04 */
	    }
	    continue;
	}

	if (tf) {  /* V1.04 */
	    char *pt;
	    char   wkstr2[2048];
    	    hmsm_t addtms;

	    if (pt = strchr(buf,'(')) {
                if (get_times(pt, &ttms, wkstr2)) {
		    add_times(&ttms, &totaltms, &addtms);
		    printf("%s\n", buf);
            	    memcpy(&totaltms, &addtms, sizeof(hmsm_t));
                }
	    }
	    /* 時刻を合計するだけ */
	    continue;
	}

	/* 時刻表示キーワードチェック */
	if (af) {
	    /* 全行表示対象 */
	    if (sqlf) {
		/* HiRDB SQL trace */
	        print_times2(buf,pos);
	    } else if (osqlf) {
		/* Oracle SQL trace */
		tim2hms(buf,newbuf);
	        print_times(newbuf,pos);
	    } else {
		/* WCO log */
	        print_times(buf,pos);
	    }
	} else {
	    for (i = 0; keyword[i]; i++) {
	        if (vf > 2) printf("key=[%s]\n",keyword[i]);
	        if (strstr(buf,keyword[i])) {
		    if (sqlf) {
			/* HiRDB SQL trace */
		        rtn = print_times2(buf,pos);
	            } else if (osqlf) {
		        /* Oracle SQL trace */
		        tim2hms(buf,newbuf);
	                rtn = print_times(newbuf,pos);
		    } else {
			/* WCO log */
		        rtn = print_times(buf,pos);
		    }
		    /* V1.07 start */
		    if (rtn < 0) { /* print_times*()で表示されなかった */
		        /* -k 指定のものは、時刻がなくても表示されるようにする) */
		        printf("%s\n", buf);
		    }
		    /* V1.07 end */
		    break;
	        }
	    }
	}
    }

    if (fp != stdin) fclose(fp);
    if (tf) {
	printf("### Total (%02d:%02d:%02d.%03d)\n",
		totaltms.hh, totaltms.mm, totaltms.ss, totaltms.ms);
    }

    if (wcf) {
	PutAllResult();
    }

    return 0;
}

