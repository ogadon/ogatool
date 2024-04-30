/*
 *  ˜a—ï‚ğ¼—ï‚É•ÏŠ· 
 *  –{“ú“_‚Å‚Ì”N—î•\¦
 *    2023/05/03 V1.01 support reiwa
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#include <time.h>
#else
#include <time.h>
#endif


char *gengou[] = { "¼—ï" , "–¾¡", "‘å³", "º˜a", "•½¬", "—ß˜a" };
int   gannen[] = {     1,    1868,   1912,   1926,   1989 ,  2019  };


void usage()
{
    printf("usage: age {Myy|Tyy|Syy|Hyy|Ryy|yyyy}[/mm/dd]\n");
    printf("  /mm/dd : birth day (If specified, display age.)\n");
    exit(1);
}

int main(int a, char *b[])
{
    char *ge      = NULL;
    char *pt;
    int  wareki   = 0;
    int  year     = 0;
    int  org_year = 0;
    int  geyy     = 0;
    int  mm       = 0;
    int  dd       = 0;
    int  age      = 0;
    time_t     tt;
    struct tm *tm;

    if (a < 2) {
        usage();
    }
    ge = b[1];
    switch (ge[0]) {
      case 'M':
        org_year = gannen[1];  /* –¾¡Œ³”N */
	wareki = 1;
	break;
      case 'T':
        org_year = gannen[2];  /* ‘å³Œ³”N */
	wareki = 1;
	break;
      case 'S':
        org_year = gannen[3];  /* º˜aŒ³”N */
	wareki = 1;
	break;
      case 'H':
        org_year = gannen[4];  /* •½¬Œ³”N */
	wareki = 1;
	break;
      case 'R':
        org_year = gannen[5];  /* —ß˜aŒ³”N */
	wareki = 1;
	break;
      default:
        if (strlen(ge) >= 4 &&
	    ge[0] >= '0' && ge[0] <= '9' &&
            ge[1] >= '0' && ge[1] <= '9' &&
            ge[2] >= '0' && ge[2] <= '9' &&
            ge[3] >= '0' && ge[3] <= '9') {
	} else {
            usage();
	}
	break;
    }
    if (wareki) {
        geyy = atoi(&ge[1]);
        if (geyy < 1) {
            usage();
        }
	year = org_year + geyy - 1;  /* ˜a—ï‚ğ¼—ï‚É•ÏŠ· */
    } else {
        year = atoi(ge);
    }

    /* Œ“ú•tw’è‚ª‚ ‚é‚©? */
    pt = strchr(ge, '/');
    if (pt) {
        /* Œ“úw’è‚ ‚è */
	if (strlen(pt) != 6 || pt[3] != '/') {
	    usage();
	}
	++pt;

	/* w’è mm,ddæ“¾ */
	mm = atoi(pt);  /* ’a¶“ú(Œ) */
	pt += 3;
	dd = atoi(pt);  /* ’a¶“ú(“ú) */

	/* Œ»İ”NŒ“úæ“¾ */
	tt = time(0);
	tm = localtime(&tt);

	/* ”N—îŒvZ */
	age = tm->tm_year+1900 - year;
	if (tm->tm_mon+1 < mm || 
	    (tm->tm_mon+1 == mm && tm->tm_mday < dd)) {
	    /* ’a¶“ú‚ª‚«‚Ä‚¢‚È‚¢ê‡ */
	    /* ’a¶(Œ)‚ª¡Œ‚æ‚è‚àŒã‚©A“¯Œ‚Å’a¶(“ú)‚ª‚«‚Ä‚¢‚È‚¢ê‡ */
	    --age;
	}

        printf("%3s => %4d/%02d/%02d age:%d\n", ge, year, mm, dd, age);
    } else {
        /* Œ“úw’è‚È‚µ */
	if (wareki) {
            printf("%3s”N => %d”N\n", ge, year);
	} else {
	    int i;
	    int gen_no = 0;
	    for (i = 0; i < sizeof(gannen)/sizeof(int); i++) {
	        if (year >= gannen[i]) {
		    geyy   = year - gannen[i] + 1;
		    gen_no = i;
		}
	    }
            printf("%d”N => %s%d”N\n", year, gengou[gen_no], geyy);
	}
    }
    return 0;
}
