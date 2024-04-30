/*
 *	filer[.x] : ƒtƒ@ƒCƒ‹‚ğ‘I‘ğ‚µAŠÂ‹«•Ï” FILE_NAME ‚ÉƒZƒbƒg‚·‚éB(X68000)
 *                                    •W€ƒGƒ‰[o—Í‚Éo—Í‚·‚é         (3050RX)
 *
 *   X680x0  usage : filer [ <X> <Y> off_y [-aster] ]
 *
 *		IN  -aster             : Š¿š‚ğ*‚É•ÏŠ·‚·‚éB
 *		OUT (ŠÂ‹«•Ï”)
 *	             FILE_NAME         : ‘I‘ğ‚µ‚½ƒtƒ@ƒCƒ‹–¼
 *	             LOCATE_X,LOCATE_Y : ƒJ[ƒ\ƒ‹X,YˆÊ’u
 *	             OFF_Y             : ƒtƒ@ƒCƒ‹‘I‘ğ‚Ì•\¦ƒy[ƒW
 *	             DEL_F             : íœ—v‹ƒtƒ‰ƒO 
 *	             VI_F              : ƒGƒfƒBƒ^‹N“®—v‹ƒtƒ‰ƒO 
 *
 *
 *   H3050R  usage : filer [ <X> <Y> off_y ]
 *
 *		IN  off_y      	      : •\¦ƒy[ƒW(ƒtƒ@ƒCƒ‹‚ª‘½‚¢ê‡)
 *		OUT (•W€ƒGƒ‰[o—Í)      1    2 3  4    5     6     7
 *	   	    		      FILENAME X Y CWD OFF_Y DEL_F VI_F
 *		    CWD   : ƒJƒŒƒ“ƒgƒfƒBƒŒƒNƒgƒŠ
 *		    OFF_Y : ƒtƒ@ƒCƒ‹‘I‘ğ‚Ì•\¦ƒy[ƒW
 *		    DEL_F : íœ—v‹ƒtƒ‰ƒO
 *	            VI_F  : ƒGƒfƒBƒ^‹N“®—v‹ƒtƒ‰ƒO 
 *
 *
 *		related files : cur.h, vvi[.bat]
 *
 *   CAUTION : driveƒRƒ}ƒ“ƒh‚Åƒhƒ‰ƒCƒu–¼‚ğ“ü‚ê‘Ö‚¦‚Ä‚¢‚é‚Æ³í‚É“®ì‚µ‚Ü‚¹‚ñB
 *
 *			1994.04.20  V0.01	Initial Version. by ‚¨‚ªš
 *			1994.04.26  V1.00	add readdir.
 *			1994.05.07  V1.01	X,Y save support for X68000.
 *			1994.05.07  V1.02	chdir support.
 *			1994.05.08  V1.03	-aster support for X68000's vi.
 *			1994.05.08  V1.04	ESC support.
 *			1994.10.13  V1.10	TERM size and dir ind. support.
 *			1994.10.21  V1.11	'D'(delete file) support.
 *			1995.01.29  V1.12	X68K COLUMNS/LINES support.
 *			1995.03.17  V1.13	cursor shift support.
 *			1995.03.18  V1.14	add nocbreak() for eof="^@"
 *			1995.04.10  V1.15	add endwin()   for stty -echo 
 *			1995.05.14  V1.16	'v' force invoke editor suport.
 *			1996.03.24  V1.17	port to Linux
 *			2006.07.03  V1.18	check getenv() NULL
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include "cur.h"
#include <sys/stat.h>

#ifdef X68000
#include <sys/dos.h>
#include <sys/iocs.h>
#include <unistd.h>			
#else /* H3050RX */
#include <curses.h>
#include <sys/param.h>
#endif

#define FILE_LEN 20
#define MAX_ENT  2000
#define CALXY  	 x*y_max+y+off_y*x_max*y_max

int x_max = 0, y_max = 0, off_y = 0;

char	dirf[MAX_ENT];

main(a,b)
int a;
char *b[];
{
	int	c, i=0, j;
	int	x=0,y=0,cnt=0, f_aster=0, del_f = 0, vi_f = 0;
	int 	env_x, env_y;
	DIR	*dirp;
	int	move_dir_f = 0;			/* 94.11.02 for move dir */
	struct	dirent *dp;
	struct	stat statbuf;
	char	*file[MAX_ENT];
#ifdef X68000
	char	buf[PATH_MAX+1];
#else /* H3050RX */
	char	buf[MAXPATHLEN+1];
#endif

	if (a >= 3) {
		x = atoi(b[1]);
		y = atoi(b[2]);
	}

	if (a >= 4) {
		off_y = atoi(b[3]);
	}

	if (a == 5) {
		if (strcmp(b[4],"-aster") == 0)
			f_aster = 1;
	}

#ifdef X68000
	if (_iocs_crtmod(-1) == 16) {
		env_x = 100;				/* 96 */
		env_y = 31;
		x_max = env_x / FILE_LEN;
	} else {
		env_x = 64;
		env_y = 31;
		x_max = env_x / FILE_LEN;
		if (x > x_max-1) x=x_max-1;
	}
	/*
	if (i = atoi(getenv("COLUMNS"))) {
		env_x = i;
		x_max = env_x / FILE_LEN;
		if (x > x_max-1) x=x_max-1;
	}
	if (i = atoi(getenv("LINES"))) {
		env_y = i-1;
	}
	*/
#else
	if (getenv("COLUMNS")) {
		env_x = atoi(getenv("COLUMNS"));
	} else {
		env_x = 80;
	}
	if (getenv("LINES")) {
		env_y = atoi(getenv("LINES"));
	} else {
		env_y = 24;
	}
	x_max = env_x / FILE_LEN;
#endif

	do {
#ifdef X68000
		cls();
		CUR_OFF;		/* ƒJ[ƒ\ƒ‹ƒIƒt */
#else
		initscr();
		/* nonl(); */
		cbreak();
		noecho();
		clear();
		refresh();
#endif
	
		for (i=0; i<MAX_ENT; i++)  {
			file[i] = (char *)0;		/* filename ptr  */
			dirf[i] = 0;			/* dir indicator */
		}	
		i=0;
		dirp = opendir(".");

	        while((dp=readdir(dirp)) !=NULL){
			file[i] = (char *)malloc(strlen(dp->d_name)+1);
			strcpy(file[i],dp->d_name);
			i++;
			if (i >= MAX_ENT) {
				printf("filer : ƒGƒ“ƒgƒŠƒI[ƒo[ƒtƒ[  ");
				printf("(MAX is %d)\n",MAX_ENT);
#ifndef X68000
				sleep(1);
#endif
				break;
			}
	    	}
	    	closedir(dirp);
	    	xsort(file,i);

		j=0;
		while (file[j]) {
			stat(file[j],&statbuf);
#if defined(X68000) || defined(LINUX)
			if (statbuf.st_mode & S_IFDIR ) { 
#else
			if (statbuf.st_mode & _S_IFDIR ) { 
#endif
				dirf[j] = '/';
			}
			j++;
		}

		if (move_dir_f)			/* 94.11.02 for move dir */
			off_y = 0;		/* 94.10.25 for move dir */
		dispdir(buf);
	
		/* noecho(); */
		y_max = (i-1) / x_max + 1;

		if (y_max >= env_y - 2) {
			y_max = env_y - 3;
		}

		dispall(file);
		if (y > y_max-1) y = y_max-1;
		while (file[CALXY] == (char *)0)
				--x;
		disp(file,x,y,1);
#ifdef X68000
		while ((c = getch()) != 13) { 	/* } */
#else  /* H3050RX */
		while ((c = getch()) != '\n') {
#endif
			switch(c) {
				case 'j' :
				case 'J' :
					disp(file,x,y,0);
					if (++y >= y_max) {
						y -= y_max;
						if (++x >= x_max) x -= x_max;
					}
					if (file[CALXY] == (char *)0) {
						x = 0;
						y = 0;
					}
					disp(file,x,y,1);
					break;
				case 'k' :
				case 'K' :
					disp(file,x,y,0);
					if (--y < 0) {
						y += y_max;
						if (--x < 0) x += x_max;
					}
					while (file[CALXY] == (char *)0) {
						if (--y < 0) {
							y += y_max;
							if (--x < 0) x += x_max;
						}
					}
					disp(file,x,y,1);
					break;
				case 'h' :
				case 'H' :
					disp(file,x,y,0);
					if (--x < 0) {
						x += x_max;
						if (--y < 0) y += y_max;
					}
					while (file[CALXY] == (char *)0) {
						if (--x < 0) {
							x += x_max;
							if (--y < 0) y += y_max;
						}
					}
					disp(file,x,y,1);
					break;
				case 'l' :
				case 'L' :
					disp(file,x,y,0);
					if (++x >= x_max) {
						x -= x_max;
						if (++y >= y_max) y -= y_max;
					}
					if (file[CALXY] == (char *)0) {
						x = 0;
						if (++y >= y_max) y -= y_max;
						if (file[CALXY] == (char *)0) y = 0;
					}
					disp(file,x,y,1);
					break;
				case 6   :		/* ^F and «(X68000) */
#ifndef X68000
				case 'B' :		/* « H3050R */
#endif
					++off_y;
					if (file[off_y*x_max*y_max] == (char *)0) {
						--off_y;
					} else {
						/* 94.10.25 */
						if (file[CALXY] == (char *)0) {
							x = y = 0;
						}
#ifdef X68000
						cls();
#else
						clear();
						refresh();
#endif
						dispdir(buf);
						dispall(file);
						disp(file,x,y,1);
					}
					break;
				case 2   :		/* ^B  */
#ifdef X68000
				case 1   :		/* ª X68000 */
#else
				case 'A' :		/* ª H3050R */
#endif
					--off_y;
					if (off_y < 0) {
						off_y = 0;
					} else {
						/* 94.10.25 */
						if (file[CALXY] == (char *)0) {
							x = y = 0;
						}
#ifdef X68000
						cls();
#else
						clear();
						refresh();
#endif
						dispdir(buf);
						dispall(file);
						disp(file,x,y,1);
					}
					break;
				case 'D' :		/* D(delete)        V1.11 */
					del_f = 1;
					break;
				case 'v' :		/* v(invode editor) V1.16 */
				case 'V' :		
					vi_f = 1;
					break;
				case 27 :		/* ESC */
					disp(file,x,y,0);
					if (y_max == 1) {
						x = 1;
						y = 0;
					} else {
						x = 0;
						y = 1;
					}
					if (off_y) {
						off_y = 0;
#ifdef X68000
						cls();
#else
						clear();
						refresh();
#endif
						dispdir(buf);
						dispall(file);
					}
					disp(file,x,y,1);
					break;
				case 'q' :
				case 'Q' :
					freex(file);
#ifdef X68000
					cls();
					CUR_ON;		/* ƒJ[ƒ\ƒ‹ƒIƒ“ */
#else
					clear();
					nocbreak();
					refresh();
					endwin();
#endif
					exit(1);	/* end vvi (no EXEC vi) */
					break;
			}
			if (del_f || vi_f)
				break;
		}

		move_dir_f = 1;			/* 94.11.02 for move dir */

	} while (chdir(file[CALXY]) == 0);	/* ƒfƒBƒŒƒNƒgƒŠ‚È‚ç‚ÎˆÚ“®‚·‚éB */

#ifdef X68000
	if (f_aster) {				/* Š¿š‚ğƒAƒXƒ^ƒŠƒXƒNˆêŒÂ‚É•ÏŠ·‚·‚é */
		for (i=0; i<strlen(file[CALXY]); i++) {
			if ((unsigned char) *(file[CALXY]+ i) > 127) {	/* ‚½‚Ô‚ñŠ¿š */
				*(file[CALXY]+ i) = '*';
				*(file[CALXY]+ i+1) = '\0';
				break;
			}
		}
	}
	_dos_setenv("FILE_NAME",0,file[CALXY]); /* e‚ÌŠÂ‹«•Ï”‚Éƒtƒ@ƒCƒ‹–¼‚ğƒZƒbƒg */
	sprintf(buf,"%d",x);			/* XˆÊ’u‚ÌƒZ[ƒu	*/
	_dos_setenv("LOCATE_X",0,buf);
	sprintf(buf,"%d",y);			/* YˆÊ’u‚ÌƒZ[ƒu	*/
	_dos_setenv("LOCATE_Y",0,buf);
	sprintf(buf,"%d",off_y);		/* ƒJƒŒƒ“ƒgƒy[ƒW	*/
	_dos_setenv("OFF_Y",0,buf);
	sprintf(buf,"%d",del_f);		/* íœƒtƒ‰ƒO    	*/
	_dos_setenv("DEL_F",0,buf);
	sprintf(buf,"%d",vi_f);			/* ƒGƒfƒBƒ^‹N“®ƒtƒ‰ƒO 	*/
	_dos_setenv("VI_F",0,buf);
	CUR_ON;					/* ƒJ[ƒ\ƒ‹ƒIƒ“ */
#else
   	/* stderr‚Éƒtƒ@ƒCƒ‹–¼‚ÆXˆÊ’u,YˆÊ’u,pwd, off_y, del_f ‚ğo—Í  */
	fprintf(stderr,"%s %d %d %s %d %d %d",file[CALXY],x,y,getcwd(buf,MAXPATHLEN+1),off_y, del_f,vi_f);	

	sprintf(buf,"FILE_NAME=%s",file[CALXY]); /* <= ‚ ‚Ü‚èˆÓ–¡‚Í‚È‚¢  */
	putenv(buf);
	sprintf(buf,"LOCATE_X=%d",x);		/* XˆÊ’u‚ÌƒZ[ƒu	*/
	putenv(buf);
	sprintf(buf,"LOCATE_Y=%d",y);		/* YˆÊ’u‚ÌƒZ[ƒu	*/
	putenv(buf);
	nocbreak();
	endwin();
#endif
	freex(file);
	exit(0);				/* EXEC "vi"        */
}

disp(file,x,y,sw)
char **file;
int x,y,sw;
{
	locate(x*FILE_LEN,y+3);
#ifdef X68000
	if (sw) 
		printf("[43m%s%c[33m\n",file[CALXY],dirf[CALXY]); /*  ƒŠƒo[ƒX•\¦  */
	else 
		printf("[33m%s%c\n",file[CALXY],dirf[CALXY]);	/*  ƒm[ƒ}ƒ‹•\¦  */
#else  /* for H3050RX mjpterm */
	if (sw) 
		printf("[7m%s%c[0m\n",file[CALXY],dirf[CALXY]); /*  ƒŠƒo[ƒX•\¦  */

	else 
		printf("%s%c\n",file[CALXY],dirf[CALXY]);	/*  ƒm[ƒ}ƒ‹•\¦  */
#endif
	/* locate(x*FILE_LEN,y+3-1); */
	locate(0,0); 
	printf("\n");
#ifdef LINUX
	refresh();
#endif

}

dispall(file)
char *file[];
{
	int i,j;

	for (j=0; j<x_max ; j++) {
		for (i=0; i<y_max ; i++) {
			if (file[j*y_max+i])
				disp(file,j,i,0);
			else
				break;
		}
	}
}
freex(file)
char *file[];
{
	int i=0;
	while (file[i])
		free(file[i++]);

}

xsort(file,n)
char *file[];
int n;
{
	char *wk;
	int i,j;

	for (i=1;i<n;i++) {
		for (j=0;j<n-1;j++) {
			if (strcmp(file[j],file[j+1]) > 0 ) {
				wk 	  = file[j];
				file[j]   = file[j+1];
				file[j+1] = wk;
			}
		}
	}
}

dispdir(buf)
char *buf;
{
#ifdef X68000
	printf("  Current Directory : %s (No.%d)\n",getcwd(buf,PATH_MAX+1),off_y+1); 
#else  /* H3050RX */
	printf("  Current Directory : %s (No.%d)\n",getcwd(buf,MAXPATHLEN+1),off_y+1); 
#endif
}
