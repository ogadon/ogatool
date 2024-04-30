/*
 *  tcpdmpan
 *    analyze tcpdump result
 *
 *    usage: tcpdump |tcpdmpan
 *
 *  2001/10/09 V1.00 by oga.
 *  2001/10/11 V1.01 support color and delta size
 *  2001/10/16 V1.02 display current num of entries, and bug fix
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#define MAXCONENT 1000
#define dprintf if (vf) printf

typedef struct _con_t {
    char src[256];
    char dest[256];
    long size;     /* total win size */
    int  delta;    /* delta win size */
} con_t;

int vf     = 0;
int lines  = 24-3; /* default window size if no LINE variable */
int noclsf = 0;    /* -nocls   */
int nocolf = 0;    /* -nocolor V1.01 */

int totalent = 0;  /* num of entries */

/*
 *   get one word
 *     IN  pt  : strings
 *     OUT word: word
 */
char *get1word(char *pt, char *word)
{
    int i = 0;
    while (*pt != ' ' && *pt != ':' && *pt != '\0' && *pt != '\n') {
	word[i++] = *pt;
	++pt;
    }
    word[i] = '\0';
}

/*
 *   add one ent to conlist
 *     IN  conlist : connect data list
 *     OUT newdat  : add data
 */
int add1ent(con_t *conlist, con_t *newdat)
{
    int i = 0;
    while (conlist[i].size && i < MAXCONENT) {
	if (!strcmp(conlist[i].src, newdat->src)
            && !strcmp(conlist[i].dest, newdat->dest)) {
	    /* add size to current entry */
	    conlist[i].size += newdat->size;
	    conlist[i].delta += newdat->size;   /* V1.01-A */
	    return;
	}
	++i;
    }
    if (i >= MAXCONENT) {
	printf("entry over flow.\n");
	return 1;
    } else {
	/* add new entry */
	strcpy(conlist[i].src,  newdat->src);
	strcpy(conlist[i].dest, newdat->dest);
	conlist[i].size = newdat->size;
	conlist[i].delta = newdat->size;   /* V1.01-A */
	++totalent;
    }
    return 0;
}

/*
 *  compare by delta, size  (for qsort)
 *
 */
int compare(const void *arg1, const void *arg2)
{
    if ((*(con_t **)arg1)->delta != (*(con_t **)arg2)->delta) {
	/* 1st compare by delta */
        if ((*(con_t **)arg1)->delta > (*(con_t **)arg2)->delta) return -1;
        if ((*(con_t **)arg1)->delta < (*(con_t **)arg2)->delta) return 1;
    } else {
	/* 2nd compare by size */
        if ((*(con_t **)arg1)->size > (*(con_t **)arg2)->size) return -1;
        if ((*(con_t **)arg1)->size < (*(con_t **)arg2)->size) return 1;
    }
    return 0;
}

/*
 *  sort and dump
 *
 */
void dump_ent(con_t *conlist)
{
    int i = 0;
    int nent = 0;
    con_t *ent2[MAXCONENT];
    char startcol[16], endcol[16];

    memset(ent2, 0, sizeof(ent2));

    /* copy struct entry pointer */
    nent = 0;
    /* while (conlist[nent].size && nent < MAXCONENT && nent < lines) { */
    while (conlist[nent].size && nent < MAXCONENT) {
	ent2[nent] = &conlist[nent];
	++nent;
    }

    /* sort */
    qsort(ent2, nent, sizeof(con_t *), compare);

    if (noclsf) {
        printf("-------------------------------------------------------------------------------\n");
    } else {
        printf("[0;0H");  /* home */
        printf("Source.Port              %5d => Destination.Port           win Delta/  Total\n", nent);
        printf("-------------------------------------------------------------------------------\n");
    }
    i = 0;
    while (ent2[i] && i < MAXCONENT) {
      if (i < lines || noclsf) {
	/* 1‰æ–Ê•ª•\Ž¦         */
	/* ‘SŒ•\Ž¦ (-noclsŽž) */
	if (ent2[i]->delta && !nocolf) {  /* V1.01-A */
	    strcpy(startcol, "[36m");
	    strcpy(endcol,   "[0m");
	} else {
	    strcpy(startcol, "");
	    strcpy(endcol,   "");
	}
	printf("%s%-30s => %-28s %6dK/%6d%s %s\n",
            startcol,
            ent2[i]->src, 
	    ent2[i]->dest,
	    ent2[i]->delta,
	    (ent2[i]->size >= 10240)? ent2[i]->size/1024: ent2[i]->size,
	    (ent2[i]->size >= 10240)? "M":"K",
	    endcol
	    );
      }
      ent2[i]->delta = 0;                /* V1.01-A */
      ++i;
    }

#if 0
    printf("------------------\n");

    i = 0;
    while (conlist[i].size && i < MAXCONENT) {
	printf("%-30s => %-30s  (%d)\n",conlist[i].src, conlist[i].dest, conlist[i].size);
	++i;
    }
#endif

    fflush(stdout);
}

int main(int a, char *b[])
{
    int i;
    int logf   = 0;  /* -log mode */
    int update = 5;  /* 5sec */
    unsigned int lastsec = 0;

    char buf[4096];
    char winsize[256];
    char *pt;
    con_t con_dat[MAXCONENT];
    con_t con_wk;

    struct timeval  tv;
    struct timezone tz;

    memset(con_dat, 0, sizeof(con_dat));

    for (i = 1; i<a; i++) {
	if (!strcmp(b[i], "-h")) {
	    printf("usage: tcpdmpan [-update <sec(5)>] [-nocls] [-nocolor] [-log]\n");
	    printf("       -update : update sec\n");
	    printf("       -nocls  : no clear screen mode (print all data)\n");
	    printf("       -nocolor: no color\n");
	    printf("       -log    : tcpdump log analyze mode. eg. cat xx |tcpdmpan -log -nocls\n");
	    exit(1);
	}
	if (i+1 < a && !strcmp(b[i], "-update")) {
	    update = atoi(b[++i]);
	    dprintf("update=%d\n",update);
	    continue;
	}
	if (!strcmp(b[i], "-nocls")) {
	    noclsf = 1;
	    continue;
	}
	if (!strcmp(b[i], "-nocolor")) {
	    nocolf = 1;
	    continue;
	}
	if (!strcmp(b[i], "-log")) {
	    logf = 1;
	    continue;
	}
    }

    if (getenv("LINES")) {
	lines = atoi(getenv("LINES"))-3;
    }

    if (!noclsf) {
        printf("[2J");  /* cls */
    }

    while (fgets(buf, sizeof(buf), stdin)) {
	if (!strstr(buf, " win ")) {
	    /* no win data */
	    continue;
	}

	/* skip to source host */
	pt = (char *)strchr(buf, ' ');
	if (!pt) continue;
	++pt;
	get1word(pt, con_wk.src);          /* get src host.port    */

	/* skip to dest host */
	pt = (char *)strchr(pt, ' ');      /* skip src host.port   */
	if (!pt) continue;
	++pt;
	pt = (char *)strchr(pt, ' ');      /* skip '>'             */
	if (!pt) continue;
	++pt;
	get1word(pt, con_wk.dest);         /* get dest host.port   */

        /* search win data */
	pt = (char *)strstr(pt, "win ");  
	if (pt) {
	    pt = (char *)strchr(pt, ' ');  /* skip "win"           */
	    ++pt;
	    get1word(pt, winsize);
	    con_wk.size = atoi(winsize)/1024;
	    add1ent(con_dat, &con_wk);
	    dprintf("%-30s => %-30s  (%d)\n",con_wk.src, con_wk.dest, atoi(winsize));
	} else {
	    /* no "win" => no count */
	}
	if (!logf) {
	    gettimeofday(&tv, &tz);
	    if (tv.tv_sec > lastsec + update) {
                dump_ent(con_dat);
	        lastsec = tv.tv_sec;
	    }
	}
    }

    if (logf) {
        dump_ent(con_dat);
    }
}

