/*
 *     cal       ÉJÉåÉìÉ_Å[èoóÕÉvÉçÉOÉâÉÄ
 *
 *                                     V1.0  Initial Version. by hyper halx.f
 *                         1994.01.03  V1.1  get current date. 
 *                         1994.11.02  V1.2  holiday support. 
 *                         1994.11.11  V1.3  3050 Series support. 
 *                         1994.11.17  V1.4  HOLIDAY_PATH support. 
 *                         1994.11.22  V1.5  code clean up, and NOCOLOR support.
 *                         1996.04.25  V1.6  workingday on sat or sun support.
 *                         1996.05.02  V1.7  DOS support
 *                         1996.05.18  V1.8  auto shunbun
 *                         1996.09.22  V1.81 bug fix V1.6
 *                         1996.12.12  V1.82 color for 3050RX
 *                         1996.12.20  V1.83 append uminohi (7/20)
 *                         1997.01.24  V1.84 port to FreeBSD
 *                         2000.12.22  V1.85 fix holiday cancel bug
 *                         2014.02.11  V1.86 support NT cmmand prompt
 *                         2014.11.03  V1.90 support variable holiday
 *                         2014.11.03  V1.91 fix NT usage color
 *                         2015.04.13  V1.92 fix continuation substitute holiday, add -holidump
 *                         2019.06.09  V1.93 append new birthday of tennou (2/23)
 *                         2020.05.03  V1.94 apply olympic year holiday(2020)
 *                         2021.08.10  V1.95 apply olympic year holiday(2021)
 *                         2021.08.16  V1.95b fix compile warning
 *
 *  X68000 : default
 *  HI-UX  : -DX_H3050R -D_HIUX_SOURCE
 *  FreeBSD: -DX_H3050R -DFREEBSD
 *  Linux  : -DLINUX
 *  DOS    : -DDOS
 *
 *
 *     usage: cal [[month] year]
 */

#ifdef _WIN32
#define DOS	1
#endif /* _WIN32 */

#ifdef DOS
#include <windows.h>
#endif /* DOS */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FREEBSD
#include <sys/param.h>
#include <sys/mount.h>
#endif /* FREEBSD */

#include <sys/stat.h>

#if defined(X_H3050R) || defined(DOS) || defined(LINUX)
#include <time.h>
#endif

#define VER 	"1.95b"
#define MAX_ENT 2000

#ifdef DOS
#define BLUE   (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY)  /* skyblue */
#define YELLOW (FOREGROUND_RED  | FOREGROUND_GREEN | FOREGROUND_INTENSITY)  /* yellow  */
#define WHITE  (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY)  /* red */
#define NORMAL (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED)        /* normal  */
#endif /* DOS */

void cal_month();
void cal_year();
void set_data();
void get_holiday();
void read_holiday();
void print_usage();
void printf_nt();
int  get_day();               /* V1.90-A */
void add_holiday();           /* V1.90-A */
void adjust_holiday();        /* V1.90-A */
void backup_color();          /* V1.91-A */
void restore_color();         /* V1.91-A */
void dump_holiday();          /* V1.92-A */
int  bcd2bin();               /* V1.95b-A */
int  chk_holiday();           /* V1.95b-A */

#if defined(X_H3050R) || defined(DOS) || defined(LINUX)
int _iocs_bindateget();
#endif

/* V1.90-A start */
typedef struct _holidat {
	int mm;       /* month                  */
	int dd;       /* day                    */
	int st_yy;    /* start year             */
	int en_yy;    /* end year               */
	int nth;      /* <n>th day of week      */
	int wday;     /* day of week(0-6 1:mon) */
} holidat_t;
/* V1.90-A end   */

int            nocolor = 0; 
int            vf      = 0; 
unsigned short colorbk = 0xffff; 

char  msg[512];
int   m1[13] = {0,31,59,90,120,151,181,212,243,273,304,334,365};  /*     ïΩîNóp */
int   m2[13] = {0,31,60,91,121,152,182,213,244,274,305,335,366};  /* Ç§ÇÈÇ§îNóp */

int holi[MAX_ENT*3];                /* holiday table V1.90-C */

/* V1.90-A start */
/*
 *  èjì˙ÉeÅ[ÉuÉãformat   
 *    +0,+1,        +2,        +3, +4, +5
 *    åé,ì˙,ìKópäJénîN,ìKópèIóπîN,ëÊn,ójì˙(1:åéój)
 *
 *    (íç)çëñØÇÃèjì˙ÇÃêßíËÇÕ1948/7/20é{çs
 *        å≈íËãxì˙ÇÕëÊn, ójì˙ÇÕ0,0
 *        ï°éGÇ»åvéZÇÃètï™ÇÃì˙ÅAèHï™ÇÃì˙ÇÕadjust_holiday()ì‡Ç≈ê›íË 
 */
holidat_t new_holi[] = {
	1,   1, 1949, 9999, 0, 0,   /* å≥ì˙         å≈íË(1949-)               */

	1,  15, 1949, 1999, 0, 0,   /* ê¨êlÇÃì˙     å≈íË(1949-1999)           */
	1,  15, 2000, 9999, 2, 1,   /* ê¨êlÇÃì˙     2000îNÇ©ÇÁHappyMondayìKóp */

	2,  11, 1967, 9999, 0, 0,   /* åöçëãLîOÇÃì˙ å≈íË(1967-)               */
	2,  23, 2020, 9999, 0, 0,   /* ìVçcíaê∂ì˙3  å≈íË(2020-) óﬂòa  V1.93-A */

	/* ìVçcíaê∂ì˙1(1949-1988), óŒÇÃì˙(1989-2006) è∫òaÇÃì˙(2007-)          */
	4,  29, 1949, 9999, 0, 0,   /* è∫òaÇÃì˙     å≈íË(1949-)               */

	5,   3, 1949, 9999, 0, 0,   /* åõñ@ãLîOì˙   å≈íË(1949-)               */

	5,   4, 2007, 9999, 0, 0,   /* Ç›Ç«ÇËÇÃì˙   å≈íË(2007-)               */

	5,   5, 1949, 9999, 0, 0,   /* Ç±Ç«Ç‡ÇÃì˙   å≈íË(1949-)               */

	7,  20, 1996, 2002, 0, 0,   /* äCÇÃì˙       å≈íË(1996-2002)           */
	7,  20, 2003, 2019, 3, 1,   /* äCÇÃì˙       2003îNÇ©ÇÁHappyMondayìKóp V1.94-C */
	7,  23, 2020, 2020, 0, 0,   /* äCÇÃì˙       å≈íË(2020ÇÃÇ›)            V1.94-A */
	7,  22, 2021, 2021, 0, 0,   /* äCÇÃì˙       å≈íË(2021ÇÃÇ›)            V1.95-A */
	7,  20, 2022, 9999, 3, 1,   /* äCÇÃì˙       2003îNÇ©ÇÁHappyMondayìKóp V1.95-C */

	8,  11, 2016, 2019, 0, 0,   /* éRÇÃì˙       å≈íË(2016-)               V1.94-C */
	8,  10, 2020, 2020, 0, 0,   /* éRÇÃì˙       å≈íË(2020ÇÃÇ›)            V1.94-A */
	8,   8, 2021, 2021, 0, 0,   /* éRÇÃì˙       å≈íË(2021ÇÃÇ›)            V1.95-A */
	8,  11, 2022, 9999, 0, 0,   /* éRÇÃì˙       å≈íË(2016-)               V1.95-C */

	9,  15, 1966, 2002, 0, 0,   /* åhòVÇÃì˙     å≈íË(1966-2002)           */
	9,  15, 2003, 9999, 3, 1,   /* åhòVÇÃì˙     2003îNÇ©ÇÁHappyMondayìKóp */

	10, 10, 1966, 1999, 0, 0,   /* ëÃàÁÇÃì˙     å≈íË(1966-1999)           */
	10, 10, 2000, 2019, 2, 1,   /* ëÃàÁÇÃì˙     2000îNÇ©ÇÁHappyMondayìKóp V1.94-C */
	7,  24, 2020, 2020, 0, 0,   /* ÉXÉ|Å[ÉcÇÃì˙ å≈íË(2020ÇÃÇ›)            V1.94-A */
	7,  23, 2021, 2021, 0, 0,   /* ÉXÉ|Å[ÉcÇÃì˙ å≈íË(2021ÇÃÇ›)            V1.95-A */
	10, 10, 2022, 9999, 2, 1,   /* ÉXÉ|Å[ÉcÇÃì˙ 2000îNÇ©ÇÁHappyMondayìKóp V1.95-C */

	11,  3, 1948, 9999, 0, 0,   /* ï∂âªÇÃì˙     å≈íË(1948-)               */

	11, 23, 1948, 9999, 0, 0,   /* ãŒòJä¥é”ÇÃì˙ å≈íË(1948-)               */

	12, 23, 1989, 2018, 0, 0    /* ìVçcíaê∂ì˙2  å≈íË(1989-2018) ïΩê¨ V1.93-C */
};
/* V1.90-A end */

#ifdef X_H3050R
char *blue   = "[7m";
char *yellow = "[7m";
char *white  = "[0m";
char *normal = "[0m";
#else
#  if defined(DOS) || defined(LINUX)
char *blue   = "[36m";
char *yellow = "[33m";
char *white  = "[37m";
char *normal = "[0m";
#  else  /* X68K */
char *blue   = "[35m";
char *yellow = "[36m";
char *white  = "[37m";
char *normal = "[33m";
#  endif
#endif

#ifdef DOS
typedef struct _coltbl {
    char           *escsec;
    unsigned short color;
} coltbl_t;

coltbl_t col_tbl[] = {
    { "[36m",   BLUE },
    { "[33m", YELLOW },
    { "[37m",  WHITE },
    { "[0m" , NORMAL }
};
#endif /* DOS */


int main(a,b)
int a;
char *b[];
{
	int  year = 0, month = 0, cur_flg = 0;
	int  xdate;
	int  hdf   = 0;   /* holiday dump (secret opt)  V1.92-A */
	int  shift = 0;   /* shift arg                  V1.92-A */
	char *dt=(char *)&xdate;

	backup_color();   /* for dos V1.91-A */

	if (a > 1 && strcmp(b[1],"-h") == 0) {
		print_usage();
		exit(1);
	}

	/* V1.92-A start */
	if (a > 1 && strcmp(b[1],"-holidump") == 0) {
		/* usage: cal -holidump [[month] year] */
		hdf = 1;
	}
	/* V1.92-A end   */

#ifdef X_H3050R
	if (getenv("COLORCAL") != 0) {	/* for 3050RX */
		blue   = "[36m";
		yellow = "[33m";
		white  = "[37m";
		normal = "[0m";
	}
#endif

	if (getenv("NOCOLOR") != 0) {
		blue = yellow = white = normal = "\0";
		nocolor = 1;
	}

    	xdate = _iocs_bindateget();                   /* get date             */

	/* V1.92-A start */
	if (hdf) {
		--a;                                  /* shift argc */
		shift = 1;                            /* shift pos  */
	}
	/* V1.92-A start */

	switch (a) {
		case 1: 
			year    = bcd2bin(dt[1])+1980;        /* day:year:month:date  */
			month   = bcd2bin(dt[2]);
			cur_flg = 1;                          /* today's year & month */
			break;
		case 2: 
			year  = atoi(b[1+shift]);
			break;
		case 3: 
			month = atoi(b[1+shift]);
			year  = atoi(b[2+shift]);
	 		break;
	}
	if (year < 1) {
		printf("Invalid Year value. ( year > 0 )\n");
		print_usage();
		exit(1);
	}
	if (month < 0 || month > 12) {
		printf("Invalid Month value.\n");
		print_usage();
		exit(1);
	}
		
	memset(holi, 0, sizeof(holi));                  /* V1.90-A */

	adjust_holiday(year);         /* adjust holiday    V1.90-A */

	/* V1.92-A start */
	if (hdf) {
		dump_holiday(year);   /* dump holiday              */
		return 0;
	}
	/* V1.92-A end   */

	get_holiday();                /* read holiday file V1.90-M */

	if (month == 0) {
	        cal_year(year);
	} else {
		cal_month(year,month,cur_flg,bcd2bin(dt[3]));
	}

	restore_color();  /* V1.91-C */

	return 0;         /* V1.92-A */
}

/* V1.92-A start */
/* 
 *  dump holiday table 
 *    note: call after adjust_holiday()
 * 
 */
void dump_holiday(yy)
int yy;
{
	int i = 0;

	printf("Year %d's holiday\n", yy);

	/* search holiday */
	while (holi[i*3+2]) {
		printf("%4d/%2d/%2d\n", yy, holi[i*3+1], holi[i*3+2]);
		++i;
	}

}
/* V1.92-A end   */


/* V1.90-A start */
/* 
 *  add to holiday table 
 *
 */
void add_holiday(mm, dd)
int mm, dd;
{
	int i = 0;

	/* search empty entry */
	while (holi[i*3+2]) {
		++i;
		if (i >= MAX_ENT) {
			printf("add_holiday() : entry overflow!\n");
			return;
		}
	}

	/* set holiday */
	holi[i*3+1] = mm;
	holi[i*3+2] = dd;
}

/* 
 * éwíËîNåéÇÃëÊ<nth> <wday>ójì˙ÇÃì˙ÇãÅÇﬂÇÈ
 *
 *    IN  year : target year
 *    IN  month: target month
 *    IN  nth  : <n>th wday
 *    IN  wday : target day of the week
 *
 */
int get_day(year, month, nth, wday)
int year, month, nth, wday;
{
	int  r;       /* éwíËåéèâÇÃêºóÔ1îN1åé1ì˙Ç©ÇÁÇÃëäëŒì˙ */
	int  a;       /* åéÇÃç≈èâÇÃì˙ÇÃójì˙ */
	int  i;
	int  cnt = 0;
	int  dd  = 1; /* åãâ  */

	if (year % 4 == 0 && 
	   (year % 100 != 0 || year % 400 == 0) ) {
		r = 365*(year-1) + m2[month-1] ;  /* Ç§ÇÈÇ§îN  */
	} else {
		r = 365*(year-1) + m1[month-1];
	}

	/* 1:åéójÇ©ÇÁénÇÈ */
	r = r + (year-1)/4 - (year-1)/100 + (year-1)/400 + 1; 
	a = r % 7;                            /* åéÇÃç≈èâÇÃì˙ÇÃójì˙ */

	for (i = 0; i < 32; i++) {
		if (((r+i) % 7) == wday) {
			++cnt;
			if (cnt == nth) {
				dd = (i + 1);
				break;
			}
		}
	}

	if (vf) printf("get_day(): %d/%d %d %d = %d\n", year, month, nth, wday, dd);

	return dd;
}

void adjust_holiday(year)
int year;
{
	int i;

	/* add new_holi table data */
	for (i = 0; i < sizeof(new_holi)/sizeof(holidat_t); i++) {
		if (year >= new_holi[i].st_yy && year <= new_holi[i].en_yy) {
			if (new_holi[i].nth == 0) {
				add_holiday(new_holi[i].mm, new_holi[i].dd);
			} else {
				add_holiday(new_holi[i].mm, 
				            get_day(year, 
				                    new_holi[i].mm,
				                    new_holi[i].nth,
				                    new_holi[i].wday));
			}
		}
	}

	if (1900 <= year && year <= 2099) {
		/* ètï™ÇÃì˙Çí«â¡ */
		switch (year % 4) {
		  case 0:
			  if (1900 <= year && year <= 1956) {
				  add_holiday(3, 21);
			  } else if (year <= 2088) {
				  add_holiday(3, 20);
			  } else {
				  add_holiday(3, 19);
			  }
			  break;
		  case 1:
			  if (1900 <= year && year <= 1989) {
				  add_holiday(3, 21);
			  } else {
				  add_holiday(3, 20);
			  }
			  break;
		  case 2:
			  if (1900 <= year && year <= 2022) {
				  add_holiday(3, 21);
			  } else {
				  add_holiday(3, 20);
			  }
			  break;
		  case 3:
			  if (1900 <= year && year <= 1923) {
				  add_holiday(3, 22);
			  } else if (year <= 2055) {
				  add_holiday(3, 21);
			  } else {
				  add_holiday(3, 20);
			  }
			  break;
		}

		/* èHï™ÇÃì˙Çí«â¡ */
		switch (year % 4) {
		  case 0:
			  if (1900 <= year && year <= 2008) {
				  add_holiday(9, 23);
			  } else {
				  add_holiday(9, 22);
			  }
			  break;
		  case 1:
			  if (1900 <= year && year <= 1917) {
				  add_holiday(9, 24);
			  } else if (year <= 2041) {
				  add_holiday(9, 23);
			  } else {
				  add_holiday(9, 22);
			  }
			  break;
		  case 2:
			  if (1900 <= year && year <= 1946) {
				  add_holiday(9, 24);
			  } else if (year <= 2074) {
				  add_holiday(9, 23);
			  } else {
				  add_holiday(9, 22);
			  }
			  break;
		  case 3:
			  if (1900 <= year && year <= 1979) {
				  add_holiday(9, 24);
			  } else {
				  add_holiday(9, 23);
			  }
			  break;
		}
	}
}
/* V1.90-A end   */

void cal_year(year)
int year;
{
	char d[21][24];            /* [x = 7 X 3][y = 6 X 4] */
	int  j,i,k,l,wk;
	int  you, month, furikae=0;

	set_data(d,year);

	sprintf(msg, "\n%s                       =====  %d îN  =====\n",white,year);
	printf_nt(msg);

	for (i=0; i<4; i++) {  /* i : 3ÉñåéíPà  */
	    sprintf(msg, "\n       %2d åé                   %2d åé                   %2d åé\n",i*3+1,i*3+2,i*3+3);
	    printf_nt(msg);
	    sprintf(msg, "\n%sì˙%s åé âŒ êÖ ñÿ ã‡ %sìy%s    ",yellow,white,blue,white);
	    printf_nt(msg);
	    sprintf(msg, "%sì˙%s åé âŒ êÖ ñÿ ã‡ %sìy%s    ",yellow,white,blue,white);
	    printf_nt(msg);
	    sprintf(msg, "%sì˙%s åé âŒ êÖ ñÿ ã‡ %sìy%s\n",yellow,white,blue,white);
	    printf_nt(msg);
	    for (k=0; k<6; k++) {				/* k : èc/åé */
		for(l=0; l<3; l++) {				/* l : åé    */
		    for(j=0; j<7; j++) {			/* j : ójì˙  */
			month = i*3+l+1;
			wk = d[l*7+j][i*6+k];
			you = j;
			if (chk_holiday(year,month,wk)>0 || furikae > 0) {
			    /* if (furikae > 0)  V1.92-D */
			    if (!chk_holiday(year,month,wk) && furikae > 0) { /* V1.92-C */
				furikae--;
			    }
			    you = 0;				/* ãxì˙   */
			    if (j == 0) {
				furikae = 1;
			    } 
			}
			if (chk_holiday(year,month,wk) < 0) {
				you = 1;			/* èoãŒì˙ */
			}
			switch (you) {
			    case 0:				/* ì˙ój   */
				sprintf(msg, "%s",yellow);	/* â©êF   */
				printf_nt(msg);
				break;
			    case 1:				/* åéój   */
			    case 2:				/* âŒój   */
			    case 3:				/* êÖój   */
			    case 4:				/* ñÿój   */
			    case 5:				/* ã‡ój   */
				sprintf(msg, "%s",white);	/* îíêF   */
				printf_nt(msg);
				break;
			    case 6:				/* ìyój   */
				sprintf(msg, "%s",blue);	/* êÖêF   */
				printf_nt(msg);
				break;
			}
			if (wk) {
			    sprintf(msg, "%2d%s ",wk,white);
			    printf_nt(msg);
			} else {
			    sprintf(msg, "%s   ",white);
			    printf_nt(msg);
			}
		    }
		    sprintf(msg, "%s   ",white);
		    printf_nt(msg);
		}
		printf("\n");
	    }
	    sprintf(msg, "%s",white);
	    printf_nt(msg);
	}
	sprintf(msg, "%s",normal);
	printf_nt(msg);
}

void cal_month(year,month,cur_flg,day)
int year,month,cur_flg,day;
{
	char	d[21][24];            /* [x = 7 X 3][y = 6 X 4] */
	int		j,k,wk;
	int		you, furikae=0;

#if 0
	printf("enter cal_month(y=%d,m=%d,f=%d,d=%d)\n",year,month,cur_flg,day);
#endif 

	set_data(d,year);

	sprintf(msg, "\n%s =====  %d îN  =====\n",white,year);
	printf_nt(msg);
	sprintf(msg, "\n         %2d åé  \n",month);
	printf_nt(msg);
	sprintf(msg, "\n %sì˙%s åé âŒ êÖ ñÿ ã‡ %sìy%s\n ",yellow,white,blue,white);
	printf_nt(msg);
	for (k=0; k<6; k++) {
		for(j=0; j<7; j++) {
			wk = d[((month-1)%3)*7+j][((month-1)/3)*6+k];
			you = j;
			if (chk_holiday(year,month,wk) || furikae > 0) {
				/* if (furikae > 0)  V1.92-D */
				if (!chk_holiday(year,month,wk) && furikae > 0) { /* V1.92-C */
					furikae--;
				}
				you = 0;			/* ãxì˙(â©êF) */
				if (j == 0) {
					furikae = 1;
				} 
			}
			if (chk_holiday(year,month,wk) < 0) {
				you = 1;			/* èoãŒì˙ */
			}
			switch (you) {
				case 0:				/* ì˙ój   */
					sprintf(msg, "%s",yellow); /* â©êF*/
					printf_nt(msg);
					break;
				case 1:				/* åéój   */
				case 2:				/* âŒój   */
				case 3:				/* êÖój   */
				case 4:				/* ñÿój   */
				case 5:				/* ã‡ój   */
					sprintf(msg, "%s",white); /* îíêF */
					printf_nt(msg);
					break;
				case 6:				/* ìyój   */
					sprintf(msg, "%s",blue); /*  êÖêF */
					printf_nt(msg);
					break;
			}
			if (wk) {
				if (cur_flg && wk == day) {
					printf("[%2d]",wk);
				} else {
					sprintf(msg, "%2d%s ",wk,white);
					printf_nt(msg);
				}
			} else {
				sprintf(msg, "%s   ",white);
				printf_nt(msg);
			}
		}
		printf("\n ");
	}
	sprintf(msg, "%s\r",normal);
	printf_nt(msg);
}

void set_data(d,year)
char d[21][24];
int year;
{
	int   cy,cx,month,m0,a,s,j,i,k,l,wk;
	int   r;
#ifdef DEBUG
	printf("enter cal_year()\n");
#endif 
	for (cy=0; cy<24; cy++) {
		for (cx=0; cx<21; cx++) {
			d[cx][cy]=0;
		}
	}
	
	for (cy=0; cy<=3; cy++) {
	    for (cx=0; cx<=2; cx++) {
		month=cy*3+cx+1;

		if (year % 4 == 0 && 
		   (year % 100 != 0 || year % 400 == 0) ) {
			r = 365*(year-1) + m2[month-1] ;  /* Ç§ÇÈÇ§îN  */
			m0=m2[month]-m2[month-1];         /* åéÇÃç≈å„ÇÃì˙ */
			/* holi[14] = 20;  V1.92-D */     /* V1.8   ètï™ÇÃì˙:3/20 */
		} else {
			r = 365*(year-1) + m1[month-1];
			m0=m1[month]-m1[month-1];         /* åéÇÃç≈å„ÇÃì˙ */
		}

		/* 1:åéójÇ©ÇÁénÇÈ */
		r = r + (year-1)/4 - (year-1)/100 + (year-1)/400 + 1; 
#ifdef DEBUG
		printf("r = %d\n",r);
#endif 	
#if 0
		if (r<694033 || r>747024) {
			printf("Invalid year value. (1900 - 2100)\n");
			printf("usage : cal [year] [month]\n");
			exit(1);
		}
#endif
		a = r % 7;                            /* åéÇÃç≈èâÇÃì˙ÇÃójì˙ */
		
		s=0;                                  /* ì˙ÉJÉEÉìÉ^         */
		for (j=0; j<=5; j++) {
			if (j == 0) {
				for (i=a; i<=6; i++) d[cx*7+i][cy*6] = ++s;
			} else {
				for (i=0; i<=6; i++) 
					if (s < m0) d[cx*7+i][cy*6+j] = ++s;
			}
		}
	    } /* cx */
	} /* cy */
}	

int bcd2bin(c)
char c;
{
	return ((c/16)*10 + (c & 0xf));
}

/*
 *   OUT 1:holiday  0:non holiday  -1:workingday on sat or sun
 */
int chk_holiday(yy,mm,dd)
int yy, mm, dd;
{

	int i, ret = 0;
	
	for (i=0; i<MAX_ENT*3; i+=3) {
		if (holi[i]) {
		    /* holiday file data */
		    /* printf("yy=%d/mm=%d/dd=%d\n",yy,mm,dd); /* DEBUG */
		    if (holi[i] == yy && holi[i+1] == mm && holi[i+2] == dd) {
			ret = 1;
			break;
		    }
		    if (holi[i] == yy && holi[i+1] == mm && holi[i+2] == -dd) {
			ret = -1;
			break;
		    }
		} else {
		    /* default holiday */
		    if (holi[i+1] == mm && holi[i+2] == dd) {
			ret = 1;
			/* break; V1.85 */  
		    }
		    if (holi[i+1] == mm && holi[i+2] == -dd) {
			ret = -1;
			/* break; V1.85 */
		    }
		}
	}
	return ret;
}

#if defined(X_H3050R) || defined(DOS) || defined(LINUX)
/*
 *		    +0 +1 +2 +3
 * 	return int |00|YY|MM|DD|
 *
 *		YY = year-1980 / MM = month / DD = date
 */
int _iocs_bindateget() 
{
	int 	i  = 0;
	char 	*c = (char *)&i;
	int yy,mm,dd;

	struct tm *tmp;
	time_t tt;

	tt = time(0);
	tmp = localtime(&tt);

	yy = tmp->tm_year - 80;
	mm = tmp->tm_mon   + 1;
	dd = tmp->tm_mday;

	c[1] = (yy/10)*16 + yy % 10;
	c[2] = (mm/10)*16 + mm % 10;
	c[3] = (dd/10)*16 + dd % 10;
#if 0
	printf("tm_mday=%d (c[3]=%d)\n",tmp->tm_mday,c[3]);
#endif

	return i;
}
#endif

void get_holiday()
{
	char buf[100];
	struct stat stbuf;

	if (getenv("HOLIDAY_PATH") != 0) {
		strcpy(buf,(char *)getenv("HOLIDAY_PATH"));
		strcat(buf,"/");
	} else {
		return;
	}

	strcat(buf,"holiday");
	if (stat(buf,&stbuf)== 0) {
		read_holiday(buf,holi);
	} 
}
void read_holiday(fname)
char *fname;
{
	FILE *fp;
	int ret=0, yy,mm,dd,i=0;
	
	while (holi[i+1]+holi[i+2]) {
		i+=3;
		if (i >= MAX_ENT*3) {
			printf("read_holiday() : entry overflow!\n");
			return;
		}
	}
	if ((fp = fopen(fname,"r")) == 0) {
		perror("read_holiday()");
		return;
	}

	while (ret != EOF) {
		yy = mm = dd = 0;
		ret = fscanf(fp,"%d %d %d",&yy,&mm,&dd);
		if (yy+mm+dd == 0) {
			continue;
		}
		holi[i  ] = yy;
		holi[i+1] = mm;
		holi[i+2] = dd;
		i += 3;
		if (i >= MAX_ENT*3) {
			printf("read_holiday() : entry overflow!\n");
			return;
		}
	}
}

void print_usage()
{
	printf("\ncal Ver %s    by oga.\n",VER);
#if defined(X_H3050R) || defined(LINUX)
	printf("usage : cal2 [-h] [[month] year]\n\n");
#else
	/* V1.90-C start */
	sprintf(msg, "\n%s usage : cal [-h] [[month] year] %s\n\n",yellow,normal);
	printf_nt(msg);
	/* V1.90-C end   */
	restore_color();  /* V1.91-A */
#endif
	printf("Environment Variable\n");
#ifdef X_H3050R
	printf("   COLORCAL=1 : color cal\n");
#endif
	printf("   NOCOLOR=1  : plain text\n\n");
	printf("If you want to specify your own holiday, make $HOLIDAY_PATH/holiday file,\n");
	printf("and set environment variable HOLIDAY_PATH.\n");
	printf("File 'holiday' is following format. Describe your holiday!\n\n");
	printf("19YY MM DD     ... holiday\n");
	printf("19YY MM DD\n");
	printf("19YY MM -DD    ... working day on Sat or Sun\n");
	printf("   :\n\n");
}

void printf_nt(char *str)
{
	int i;
	int j;

	if (nocolor) {
		printf("%s", str);
		return ;
	}

#ifdef DOS
	for (i = 0; i < strlen(str); i++) {
		if (str[i] == 27) {  /* ESC */
			for (j = 0; j < sizeof(col_tbl)/sizeof(coltbl_t); j++) {
				if (!strncmp(&str[i], col_tbl[j].escsec, strlen(col_tbl[j].escsec))) {
					SetConsoleTextAttribute(
						GetStdHandle(STD_OUTPUT_HANDLE), 
						col_tbl[j].color);
					i += strlen(col_tbl[j].escsec);
					break;
				}
			}
		}
		if (str[i]) putchar((unsigned char)str[i]);

	}
#else
	printf("%s", str);
#endif
}

/* V1.91-M start */
void backup_color()
{
#ifdef DOS
	/* backup current color */
	CONSOLE_SCREEN_BUFFER_INFO scrinfo;

	if (GetConsoleScreenBufferInfo(
		GetStdHandle(STD_OUTPUT_HANDLE), 
		&scrinfo)) {
		colorbk = scrinfo.wAttributes;
		if (vf) printf("backup color: 0x%04x\n", colorbk);
	}
#endif /* DOS */
	return;
}

void restore_color()
{
#ifdef DOS
	/* restore color */
	if (colorbk != 0xffff) {
		if (vf) printf("restore color.\n");
		SetConsoleTextAttribute(
			GetStdHandle(STD_OUTPUT_HANDLE), 
			colorbk);
	}
#endif /* DOS */
	return;
}
/* V1.91-M end   */

/*  vim:ts=8:sw=8:
 */

