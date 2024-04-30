/* 
 *	galaxian [speed<40/2500>] [-n]    .....  GalaxianÇ‡Ç«Ç´
 *
 *						by hyper halx.f oga.
 *
 *			1994.11.27	Ver 1.01
 *			1994.11.28	Ver 1.02   porting to H3050R
 *			1995.08.21	Ver 1.03   color support
 *			1995.08.21	Ver 1.04   high score support
 *			1995.08.23	Ver 1.05   star support
 *			2003.05.23	Ver 1.06   use usleep
 *			2013.12.31	Ver 1.07   fix noenv, rnd() prob
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifdef X3050RX
#include <curses.h>
#include <sys/ioctl.h>
#else  /* X68000 */
#include "cur.h"
#include <unistd.h>
#endif /* X68000 */

#define NUM_AL 		10		/* ÉGÉCÉäÉAÉìÇÃêî         */
#ifdef X3050RX
#define NUM_STAR 	20		/* êØÇÃêî                 */
#else  /* X68000 */
#define NUM_STAR 	10		/* êØÇÃêî                 */
#endif /* X68000 */
#define STAR 		"."		/* êØ                     */
#define DIE    		-1
#define TYP    		(typ/5)%2	/* ÉGÉCÉäÉAÉìÇÃå`  0 or 1 */
#define RSPACE 		8		/* âEÇÃãÛÇ´(SHIPÉTÉCÉYï™) */
#define A_MAX  		14		/* ÉGÉCÉäÉAÉìÇÃêUïù(MAX)  */

#ifdef X3050RX
#define NORMAL  0
#define BLUE	31
#define RED 	32
#define MAGENTA	33
#define GREEN	34
#define CYAN	35
#define YELLOW	36
#define WHITE	37
#else  /* X68000 */
#define NORMAL  0
#define BLUE	31
#define RED 	32
#define MAGENTA	33
#define GREEN	31
#define CYAN	35 
#define YELLOW	36
#define WHITE	37
#endif /* X68000 */

struct mis_loc {
	short x;
	short y;
};
struct al_loc {
	short  x;	/* à íu X             */
	short  y;	/* à íu Y             */
	short dx;	/* à⁄ìÆÉxÉNÉgÉã Xê¨ï™ */
	short dy;	/* à⁄ìÆÉxÉNÉgÉã Yê¨ï™ */
	short delay;	/* ë“ã@ÉJÉEÉìÉ^       */
	short a;	/* êUïù 	      */
	short a_cnt;	/* êUïùÉJÉEÉìÉ^       */
};
struct st_loc {
	short  x;	/* à íu X             */
	short  y;	/* à íu Y             */
};

#ifdef X3050RX
/* int wait = 2500; */			/* game speed                */
int wait = 10;				/* game speed  0.2 sec       */
#else  /* X68000 */
int wait = 40;				/* game speed                */
#endif /* X68000 */

int xmin = 2, xmax = 0, ymax = 0;       /* V1.07-C                   */
int sc = 0, stage = 0, typ = 0;		/* score, stage, alien_type  */
int hisc = 0;				/* high score                */
char hinm[100];				/* high name                 */
int x, y, ships = 3, mf = 0;		/* ship's locate , rest , missile flag */
int extend = 5000;			/* extend ship over 5000pts. */
int starf = 1;				/* star flag */

char  misle[]=" I ";
#ifdef X3050RX
char  ship[]="[32m <H> [1B [35mH H H [1B HHHHH [1B H H H \n";
#else  /* X68000 */
char  ship[]="[37m <H> [1B [35mH H H [1B HHHHH [1B H H H \n";
#endif /* X68000 */
char  space[]="    [1B    ";
char  *alien[2];

main(a,b)
int a;
char *b[];
{
	int   	i=0, c, mf = 0, rel=1;
	int	al_rest;
	long	size;
	char 	buf[30];
	FILE	*fp;

	struct	mis_loc mi;
	struct	al_loc al[NUM_AL];
	struct	st_loc star[NUM_STAR];

	/* INITIALIZE */
	for (i = 1; i<a ; i++) {
		if (!strcmp(b[i],"-h")) {
			printf("usage : galaxian [<wait(%d)>] [-n]\n",wait);
			printf("        <wait>    : wait <wait> x 1/100sec\n");
			printf("        -n        : no star\n");
			printf("        move ship : [J]<=>[L]\n");
			exit(1);
		} else if (!strcmp(b[i],"-n")) {
			starf = 0;
		} else {
			wait = atoi(b[1]);
			printf("wait = %d\n",wait);
		}
	}
#ifdef X3050RX
	alien[0]="[33mV  v[1B[36mXooX\n";
	alien[1]="[33mv  V[1B[36mXOOX\n";
#else  /* X68000 */
	alien[0]="[37mV  v[1B[36mXooX\n";
	alien[1]="[37mv  V[1B[36mXOOX\n";
#endif /* X68000 */
#ifdef X3050RX
        if (getenv("COLUMNS")) {    /* V1.07-A */
        	xmax = atoi(getenv("COLUMNS"))-RSPACE;
	}
	if (getenv("LINES")) {      /* V1.07-A */
        	ymax = atoi(getenv("LINES"))-1;
	}
        if (xmax == 0)
                xmax = 80-RSPACE;
        if (ymax == 0)
                ymax = 24-1;
#else  /* X68000 */
	xmax = 95-RSPACE;
	ymax = 31; 
#endif /* X68000 */

	if (fp = fopen("galax.hi","rb")) {
		fread(&hisc,4,1,fp);
		fread(&hinm,11,1,fp);
		hinm[10]='\0';
		fclose(fp);
	}
	/* initialize STAR position */
	for (i=0; i<NUM_STAR; i++) {
		star[i].x     = rnd(xmax);
		star[i].y     = rnd(ymax*2)+6;
		xprint(star[i].x, star[i].y/2,STAR,WHITE);
	}

	/* STAGE LOOP */
	while (1) {
#ifdef X3050RX
                initscr();
                /* nonl(); */
                cbreak();
                noecho();
                clear();
                refresh();
#else  /* X68000 */
		cls();
		CUR_OFF;
#endif /* X68000 */
		/* initialize ALIEN position */
		for (i=0; i<NUM_AL; i++) {
			al[i].x     = ((xmax-(A_MAX+2)*2)/5)*(i%5) + A_MAX+2 ;
			al[i].y     = (i/5)*4 + 3;
			al[i].dx    = (rnd(2)*2-1)*(rnd(2)+1);
			/* printf("dx = %d /",al[i].dx); */
			al[i].dy    = 1;
			al[i].delay = (stage > 13 ? 0 : rnd(500-stage*500/15));
			al[i].a     = al[i].a_cnt = rnd(A_MAX/2)+A_MAX/2;
			xprint(al[i].x, al[i].y, alien[0],YELLOW);
			
		}
		y = ymax - 3;			
		x = (xmin + xmax) / 2;
		al_rest = NUM_AL;

		xprint(x+1,y-1,misle,YELLOW);
		xprint(x,y,ship,CYAN);

		sprintf(buf,"STAGE %02d",++stage);
		xprint(xmax-1,1,buf,MAGENTA);
		sc_disp(sc);

		/* MAIN LOOP */
		while (1) {

			/* if (starf) move_star(star);    V1.05 */
			if (starf) {
				for (i=0; i<NUM_STAR; i++) {
					xprint(star[i].x, star[i].y/2,"  ",WHITE);
					if (++star[i].y >= ymax*2) 
						star[i].y = 4;
					if (i >= NUM_STAR/2 && ++star[i].y >= ymax*2) 
						star[i].y = 4;
					xprint(star[i].x, star[i].y/2,STAR,WHITE);
				}
			}
	#ifdef X3050RX
			/* c = getch(); */
	                ioctl(fileno(stdin),FIONREAD,&size);
       	         	if (size > 0)
       	                 	c = getch();
			else
				c = 0;

	#else  /* X68000 */
			c = inkey(0);
	#endif /* X68000 */

			/* KEY SCAN */
			switch (c) {
				case 'j' :
					rel = 1;
					if (--x < xmin ) x = xmin ;
					break;
				case 'l' :
					rel = 1;
					if (++x > xmax) x = xmax;
					break;
				case 'i' :
				case ' ' :
	#ifdef REPEAT
					if (mf == 0) {
	#else
					if (rel && mf == 0) {    /* } */
	#endif 
						mf = 1;
						mi.x = x+1;
						mi.y = y-1;
					}
					rel = 0;
					break;
				case 12 :	/* ^L  */
				case 27 :	/* ESC */
#ifdef X3050RX
			                clear();
                			refresh();
#else  /* X68000 */
					cls();
#endif /* X68000 */
					xprint(x+1,y-1,misle,YELLOW);
					xprint(x,y,ship,CYAN);
					sprintf(buf,"STAGE %02d",++stage);
					xprint(xmax-1,1,buf,MAGENTA);
					sc_disp(sc);
					break;
				case 'q' :
#ifdef X3050RX
                                        nocbreak();
                                        endwin();
#else
					CUR_ON;
#endif
					exit(0);
				default :
					rel = 1;
					break;

			}

			/* MISSILE CHECK */
			if (mf) {
				switch (i = move_misle(&mi,al)) {
					case 0:		/* nop */
						break;
					case 5:		/* alien hit */
					case 4:		/* alien hit */
					case 3:		/* alien hit */
					case 2:		/* alien hit */
						al_rest -= (i-1);
#if 0
						printf("i=%d/al_rest=%d\n",i,al_rest);
#endif
					case 1:		/* missle go away */
						mf = 0;
						break;
				}
				if (al_rest == 0) 
					break;		/* next stage */
			} else {
				xprint(x+1,y-1,misle,YELLOW);
			}

			/* DISPLAY SHIP  */
			xprint(x,y,ship,CYAN);
			/* xwait(wait); */
			usleep(wait*10000);   /* V1.06-C */

			/* MOVE ALIEN    */
			for (i=0; i<NUM_AL; i++) {
				if (al[i].delay == 0) {
					if (move_al(&al[i])) {
						bomb_ship(al,&mi,star);
					}
				} else {
					if (al[i].delay > 0) { 		
						xprint(al[i].x, al[i].y, alien[TYP],YELLOW);
						--al[i].delay;
					} else {		/* alian die */
						xprint(al[i].x, al[i].y, space,YELLOW);
					}
				}
			}

			if (stage < 10) {

				/* MISSILE CHECK 2 */
				if (mf) {
					switch (i = move_misle(&mi,al)) {
						case 0:		/* nop */
							break;
						case 5:		/* alien hit */
						case 4:		/* alien hit */
						case 3:		/* alien hit */
						case 2:		/* alien hit */
							al_rest -= (i-1);
#if 0
							printf("i=%d/al_rest=%d\n",i,al_rest);
#endif
						case 1:		/* missle go away */
							mf = 0;
							break;
					}
					if (al_rest == 0) 
						break;		/* next stage */
				} else {
					xprint(x+1,y-1,misle,YELLOW);
				}

				/* DISPLAY SHIP 2 */
				xprint(x,y,ship,CYAN);
				/* xwait(wait); */
			        usleep(wait*10000);   /* V1.06-C */
			}
			++typ;
		}
		xprint(xmax/2-15, ymax/2, " <<  S T A G E   C L E A R !  >> \n",CYAN);
#ifdef X3050RX
		sleep(2);
#else  /* X68000 */
		xwait(10000);		
#endif /* X68000 */
        }
}

/* LOCATE PRINT */
/* x:x_location,  y:y_location  s:print_string  c:color */
xprint(x,y,s,c)
int  x, y, c;
char *s;
{
	printf("[%d;%dH[%dm%s",y,x,c,s);
}

/* CHANGE COLOR */
color(c)
int c;
{
	printf("[%dm",c);
}

#if 0
cls()
{
	printf("[2J[0;0H\n");                        /*  Clear Screen & HOME  */
}
#endif

#ifdef NO_USLEEP   /* for 3050RX */
/* Select WAIT */
/*
 *  usleep(usec)
 *
 *  IN 
 *   usec : micro seconds
 *
 */
void usleep(int usec)
{
	struct timeval tv;

	if (!usec) return;
	tv.tv_sec  = 0;  		/* second        */
	tv.tv_usec = usec;		/* micro second  */

	select(0,0,0,0,&tv);		/* wait a little */	
}
#endif

/* LOOP WAIT */
xwait(val)
int val;
{
	int i,x;
	for (i=0;i<val*100;i++)
		x=i*3;                             /*  dummy instruction  */
}

#define ax al[i].x
#define ay al[i].y
#define mx (mip->x)+1
#define my mip->y

/* MOVE MISSILE */
move_misle(mip,al)
struct mis_loc *mip;
struct al_loc  *al;
{
	int i;
	int ret = 0;

	xprint(mip->x,mip->y,"   ",WHITE);
	if (--mip->y <= 1) {
		ret = 1;		
	} else {
		xprint(mip->x,mip->y,misle,YELLOW);

		/* CHECK HIT ALIEN */
		for (i=0; i<NUM_AL; i++) {
			if (al[i].delay == DIE) continue;
			if (ax <= mx && mx <= ax+3 && ay <= my && my <= ay+1) {
				xprint(ax,ay,"ÅôÅô[1BÅôÅô",YELLOW);
				if (al[i].delay == 0)
					sc += 100 * stage; /* moving point */
				else
					sc += 50 * stage;  /*   stay point */
				sc_disp(sc);
				al[i].delay = DIE;
				if (ret) 
					ret += 1;
				else 
					ret = 2;
			}
		}
	}
	return ret;
}

/* MOVE ALIEN */
move_al(alp)
struct al_loc *alp;
{
	xprint(alp->x, alp->y, space,WHITE);
	alp->x += alp->dx;
	alp->y += alp->dy;

	/* âÊñ ÇÃâ∫Ç…çsÇ¡ÇΩÇ©? */
	if (alp->y > ymax-2) {
		if (alp->x < A_MAX+2) alp->x = A_MAX+2;
		if (alp->x > xmax-A_MAX-2) alp->x = xmax-A_MAX-2;
		alp->y = 2;
		alp->dx    = (rnd(2)*2-1)*(rnd(2)+1);
		if (alp->dx < -1 && alp->x < A_MAX*2+2) alp->x = A_MAX*2+2;
		if (alp->dx >  1 && alp->x > xmax-A_MAX-2*2-2) alp->dx = xmax-A_MAX*2-2;
		alp->a     = alp->a_cnt = rnd(A_MAX/2)+A_MAX/2;
#if 0  
		alp->delay = rnd(3);
#endif
	}

	/* êUïùÉJÉEÉìÉ^ update */
	if (--alp->a_cnt == 0) {
		alp->dx *= -1;
		alp->a_cnt = alp->a;
	}
	xprint(alp->x, alp->y, alien[TYP],YELLOW);
	
	/* SHIP HIT CHECK */
	if (alp->y > ymax-5 && alp->x <= x+3 && alp->x >= x) {
		return 1;
	}
	return 0;

}

/* GENERATE RANDOM VALUE */
rnd(n)
int n;
{
	int val;
	int max = RAND_MAX;

	if (max > 0xffff) {
		max = 0xffff;
	}
	/* return(rand()*n/RAND_MAX); */
	return((rand() & max)*n/max);
}

/* DISPLAY SCORE  */
sc_disp(sc)
int sc;
{
	char buf[40];
	char bufhi[50];
	int i;

	if (sc >= extend) {
		printf("");
		++ships;
		extend += 20000;
	}
	xprint(55,0,"SHIP:",YELLOW);
	for (i=1;i<ships ; i++) {
		printf("[35mA");
	}
	sprintf(buf,  "SCORE [37m%08d\n",sc);
	sprintf(bufhi,"HI-SCORE [37m%08d [35m%s\n",hisc,hinm);
	xprint(0,0,buf,RED);
	xprint(18,0,bufhi,RED);
}

bomb_ship(al, mi, star)
struct	al_loc *al;
struct	mis_loc *mi;
struct	star_loc *star;
{
	int	i, size;
	char	buf[80];
	FILE	*fp;

	xprint(x,y," ÅôÅô[1B ÅôÅôÅô[1B ÅôÅôÅô[1B ÅôÅôÅô\n",RED);
#ifdef X3050RX
	sleep(1);
#else  /* X68000 */
	xwait(6000);
#endif /* X68000 */
	if (--ships < 1) {
		/* game is over */
#ifdef X3050RX
                nocbreak();
                endwin();
		xprint((xmin+xmax)/2-5,ymax/2,"  ÇfÇ`ÇlÇdÅ@ÇnÇuÇdÇqÅI  \n",WHITE);
#else
		CUR_ON;
		xprint((xmin+xmax)/2-5,ymax/2,"  ÇfÇ`ÇlÇdÅ@ÇnÇuÇdÇqÅI  \n",RED);
#endif
		if (hisc < sc) {
			xprint((xmin+xmax)/2-7,ymax/2+2,"  High Score! Your Name? ",CYAN);
			scanf("%s",buf);
			if (fp = fopen("galax.hi","wb")) {
				fwrite(&sc,4,1,fp);
				fwrite(buf,11,1,fp);
				fclose(fp);
			}
		} else {
			xprint((xmin+xmax)/2-7,ymax/2+2,"  Hit <RETURN> Key! \n",CYAN);
#ifdef X3050RX
			while(getch() != '\n') ;
#else  /* X68000 */
			while(getch() != 13) ;
#endif /* X68000 */
		}
		color(NORMAL);
		printf("\n");
		exit(0);
	}
#ifdef X3050RX
        clear();
        refresh();
#else  /* X68000 */
	cls();
	CUR_OFF;
#endif /* X68000 */
	if (starf) move_star(star);
	for (i=0; i<NUM_AL; i++) {
		al[i].x     = ((xmax-(A_MAX+2)*2)/5)*(i%5) + A_MAX+2 ;
		al[i].y     = (i/5)*4 + 3;
		al[i].dx    = (rnd(2)*2-1)*(rnd(2)+1);
		al[i].dy    = 1;
		if (al[i].delay != DIE) {
			al[i].delay = (stage > 13 ? 0 : rnd(500-stage*500/15));
			xprint(al[i].x, al[i].y, alien[0],YELLOW);
		}
		al[i].a     = al[i].a_cnt = rnd(A_MAX/2)+A_MAX/2;
	}
	y = ymax - 3;			
	x = (xmin + xmax) / 2;
	mf = 0;
	/* printf("mfÇÕÉNÉäÉAÇµÇ‹ÇµÇΩ\n"); */
	mi->x = x+1;
	mi->y = y-1;
	sc_disp(sc);
	sprintf(buf,"STAGE %02d",stage);
	xprint(xmax-1,1,buf,MAGENTA);
	xprint(x,y-8,"ÇqÇdÇ`ÇcÇxÅI",RED);	/* READY */
	xprint(x+1,y-1,misle,YELLOW);
	xprint(x,y,ship,CYAN);
#ifdef X3050RX
	sleep(2);
	ioctl(fileno(stdin),FIONREAD,&size);
	while (size>0) {
               	getch();
		ioctl(fileno(stdin),FIONREAD,&size);
	}

#else  /* X68000 */
	xwait(13000);
#endif /* X68000 */
	xprint(x,y-8,"            \n",WHITE);
}
move_star(star)
struct st_loc *star;
{
	int i;

	if (starf == 0) return 0;
	for (i=0; i<NUM_STAR; i++) {
		xprint(star[i].x, star[i].y/2,"  ",WHITE);
		if (++star[i].y >= ymax*2) 
			star[i].y = 4;
		if (i >= NUM_STAR/2 && ++star[i].y >= ymax*2) 
			star[i].y = 4;
		xprint(star[i].x, star[i].y/2,STAR,WHITE);
	}
}
