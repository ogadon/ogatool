/*
 *  和暦を西暦に変換 
 *  本日時点での年齢表示
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


char *gengou[] = { "西暦" , "明治", "大正", "昭和", "平成", "令和" };
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
        org_year = gannen[1];  /* 明治元年 */
	wareki = 1;
	break;
      case 'T':
        org_year = gannen[2];  /* 大正元年 */
	wareki = 1;
	break;
      case 'S':
        org_year = gannen[3];  /* 昭和元年 */
	wareki = 1;
	break;
      case 'H':
        org_year = gannen[4];  /* 平成元年 */
	wareki = 1;
	break;
      case 'R':
        org_year = gannen[5];  /* 令和元年 */
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
	year = org_year + geyy - 1;  /* 和暦を西暦に変換 */
    } else {
        year = atoi(ge);
    }

    /* 月日付指定があるか? */
    pt = strchr(ge, '/');
    if (pt) {
        /* 月日指定あり */
	if (strlen(pt) != 6 || pt[3] != '/') {
	    usage();
	}
	++pt;

	/* 指定 mm,dd取得 */
	mm = atoi(pt);  /* 誕生日(月) */
	pt += 3;
	dd = atoi(pt);  /* 誕生日(日) */

	/* 現在年月日取得 */
	tt = time(0);
	tm = localtime(&tt);

	/* 年齢計算 */
	age = tm->tm_year+1900 - year;
	if (tm->tm_mon+1 < mm || 
	    (tm->tm_mon+1 == mm && tm->tm_mday < dd)) {
	    /* 誕生日がきていない場合 */
	    /* 誕生(月)が今月よりも後か、同月で誕生(日)がきていない場合 */
	    --age;
	}

        printf("%3s => %4d/%02d/%02d age:%d\n", ge, year, mm, dd, age);
    } else {
        /* 月日指定なし */
	if (wareki) {
            printf("%3s年 => %d年\n", ge, year);
	} else {
	    int i;
	    int gen_no = 0;
	    for (i = 0; i < sizeof(gannen)/sizeof(int); i++) {
	        if (year >= gannen[i]) {
		    geyy   = year - gannen[i] + 1;
		    gen_no = i;
		}
	    }
            printf("%d年 => %s%d年\n", year, gengou[gen_no], geyy);
	}
    }
    return 0;
}
