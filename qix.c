/*
 *
 *     qix.c               V1.00  93.10.14    by Hyper Halx.f oga.
 *                         V1.01  95.09.13    speed change option.
 *                         V1.10  96.01.09    resize support.
 *                         V1.20  98.11.03    wait by usleep
 *                         V1.21  99.12.27    -y2k,-geometry support
 *
 *     cc qix.c -o qix -I/usr/include/X11R5 -L/usr/lib/X11R5 -lX11 -lm -DX3050R
 *
 */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>


#define VER      "Ver 1.21"
#define DELAY    30
#define STEP     2
#define WAIT     10000

extern unsigned long MyColor();

int X_SIZE = 250;
int Y_SIZE = 200;

/*
union { 
    XEvent	report;
    XKeyEvent	key;
} report;
*/
XEvent	report;

/* V1.21
 *
 *    get y2k delta second
 */
int y2k_delta()
{
     struct tm tm2k;
     time_t cur_time;
     time_t wktime;

     memset(&tm2k, 0, sizeof(tm2k));
     tm2k.tm_mday = 1;         /* day   : 1    */
     tm2k.tm_mon  = 0;         /* month : 1    */
     tm2k.tm_year = 100;       /* year  : 2000 */

     cur_time = time(&wktime);
     /* printf("%d : %d\n", cur_time, mktime(&tm2k)); */

     return (abs(mktime(&tm2k)-cur_time));
}

main(a,b)
int a;
char *b[];
{
	Display *d;
	Window  w;
	GC      gc, gc2, gwhite, gyellow;
	XSetWindowAttributes att;
	unsigned long red, green, white, black, yellow;
	float i = 0;
	int   loopwait = 0, vf = 0;
	int j, c, rnd_fl = 0;
	int sw = 0;
	int x1, x2, x3, x4;
	int y1, y2, y3, y4;
	int wait = WAIT;
	int y2k  = 0;             /* -y2k       V1.21 */
	int geof = 0;             /* -geometry  V1.21 */
        int delta, deltax = 0;
        int hh;
        int x_home=0, y_home=0;
	char name[256];
        char y2kstr[256];
        char *fontn = "7x14bold";
        Font font;

        /* for XGetGeometry() */
        Window root;
        int x,y;
        unsigned int width,height, border, depth;


	d = XOpenDisplay(NULL);

	green = MyColor(d,"green");

	for (j = 1; j<a; j++) {
	    if (!strncmp(b[j],"-h",2)) {
	        printf("usage : qix [<colorname>] [-w <wait(1)>] [-loopwait] [-y2k] [-geometry <geometry>]\n");
	        printf("        colorname : ex. red blue purple ...\n");
	        printf("        -w        : wait time\n");
	        printf("        -loopwat  : wait with loop\n");
	        printf("        -y2k      : countdown y2k\n");
	        printf("        -geometry : window position (<X>x<Y>+<x>+<y>)\n");
		exit(1);
	    }
	    if (!strncmp(b[j],"-w",2)) {
		wait = atoi(b[++j]) * 10000;      /* 1/100 sec */
		continue;
	    }
	    if (!strncmp(b[j],"-v",2)) {
	        vf = 1;
		continue;
	    }
	    if (!strcmp(b[j],"-loopwait")) {
	        loopwait = 1;
	        continue;
	    }
	    if (!strcmp(b[j],"-y2k")) {           /* V1.21 */
	        y2k = 1;
	        continue;
	    }
	    if (!strcmp(b[j],"-geometry")) {      /* V1.21 */
	        XParseGeometry(b[++j], &x_home, &y_home, &X_SIZE, &Y_SIZE);
	        geof = 1;
	        continue;
	    }
	    green = MyColor(d,b[j]);
#ifdef DEBUG
	    printf("color = %d(0x%x)\n",green,green);
#endif
	    if (green == 0xffffffff) {
		rnd_fl = 1;
	    }
	}
	red   = MyColor(d,"red");
	white = MyColor(d,"white");
	black = MyColor(d,"Black");
	yellow= MyColor(d,"yellow");
	
#ifdef DEBUG
	printf("green = %d, red = %d, white = %d, black = %d\n",green,red,white,black);
#endif
	w = XCreateSimpleWindow(d, RootWindow (d,0),
					x_home,y_home,  /* home x,y         */ 
					X_SIZE,Y_SIZE,  /* window size x,y  */
					5,              /* border_width */
					1,              /* border       */
					0   );          /* background   */

	att.override_redirect = 0;         /* Window Manager ‰î“ü‚ ‚è ... 0 */
                                           /*                    ‚È‚µ ... 1 */

	XChangeWindowAttributes(d,w,CWOverrideRedirect,&att);
	XMapWindow(d,w);

        /* V1.21 start */
        XGetGeometry(d,RootWindow(d,0),&root,&x,&y,&width,&height,&border,&depth);
        if (x_home < 0) {
           x_home = width - X_SIZE - 5;
        } else {
           x_home += 5;
        }
        if (y_home < 0) {
           y_home = height - Y_SIZE - 5 - 25;
        } else {
           y_home += 22;
        }
        if (geof) {
            XMoveWindow(d,w,x_home,y_home); /* V1.21            */
        }
        if (vf) {
            printf("%dx%d %d %d\n", 
            		X_SIZE, Y_SIZE, x_home, y_home);
        }
        /* V1.21 end */

	gc     = XCreateGC(d,w,0,0);        /* create gc(green) */
	gc2    = XCreateGC(d,w,0,0);        /* create gc2(black)*/
	gwhite = XCreateGC(d,w,0,0);        /* create gwhite    */
	gyellow= XCreateGC(d,w,0,0);        /* create gyellow   */

	XSetForeground(d,gc ,green);        /* set color to gc     */
	XSetForeground(d,gc2,black);        /* set color to gc2    */
	XSetForeground(d,gwhite,white);     /* set color to gwhite */
	XSetForeground(d,gyellow,yellow);   /* set color to gyellow*/

	/* load font and set */
	font = XLoadFont(d, fontn);
	XSetFont(d,gc2,font);               /* black  */
	XSetFont(d,gwhite,font);            /* white  */
	XSetFont(d,gyellow,font);           /* yellow */

	/* V1.10 */
	sprintf(name,"QIX %s",VER);
	XStoreName(d,w,name);
	XSelectInput(d,w, ButtonPressMask|ExposureMask);

	sprintf(name, "QIX %s  by Moritaka Ogasawara.",VER);

	while (1){
		x1 = cos(i/50)*X_SIZE/2+X_SIZE/2;
		y1 = sin(i/35)*Y_SIZE/2+Y_SIZE/2;
		x2 = cos(i/20)*X_SIZE/2+X_SIZE/2;
		y2 = sin(i/45)*Y_SIZE/2+Y_SIZE/2;

		x3 = cos((i-DELAY)/50)*X_SIZE/2+X_SIZE/2;
		y3 = sin((i-DELAY)/35)*Y_SIZE/2+Y_SIZE/2;
		x4 = cos((i-DELAY)/20)*X_SIZE/2+X_SIZE/2;
		y4 = sin((i-DELAY)/45)*Y_SIZE/2+Y_SIZE/2;

		if (rnd_fl)
			/* set random? color to gc  */
			XSetForeground(d,gc ,(unsigned long)(c % 4095)+1);   
  		XDrawLine(d,w,gc2,x3,y3,x4,y4); /* erase */
		XDrawLine(d,w,gc,x1,y1,x2,y2);  /* draw  */

		if (y2k) {
		    delta = y2k_delta();
		    hh    = delta/3600;
		    if (delta != deltax) {
		        XDrawString(d,w,gc2,20,28,y2kstr,strlen(y2kstr)); /* erase */
		        sprintf(y2kstr, "%02d:%02d:%02d (%d sec)  ",
		            hh, (delta-hh*3600)/60, delta % 60, delta);
		        deltax = delta;
		    }
		    XDrawString(d,w,gyellow,20,28,y2kstr,strlen(y2kstr));
	        }

		XFlush(d);

		if (loopwait) {
		    for (j=0; j<wait; j++);
		} else {
		    usleep(wait); /* V1.20 */
		}

		i += STEP;
		c++;

		if (sw) {
		    XDrawString(d,w,gwhite,0,16,name,strlen(name));
	        }

		/* V1.10 */
		if (XCheckMaskEvent(d,ButtonPressMask,&report) == True) {
	            sw = 1-sw;
		    if (sw) {
		      XDrawString(d,w,gwhite,0,16,name,strlen(name));
	            } else {
		      XDrawString(d,w,gc2,0,16,name,strlen(name));
	            }
		}

		/* resize event */
		if (XCheckMaskEvent(d,ExposureMask,&report) == True) {

		    XGetGeometry(d,w,&root,&x,&y,&width,&height,&border,&depth);
		    if (X_SIZE != width || Y_SIZE != height) {
			X_SIZE = width;
			Y_SIZE = height;
			XClearWindow(d,w);
		    }
		}
	}
}

unsigned long MyColor(display,color)
Display *display;
char *color;
{
	Colormap cmap;
	XColor c0,c1;
	int code;

	cmap = DefaultColormap(display,0);

	code = XAllocNamedColor(display,cmap,color,&c1,&c0);
	if (code)
		return(c1.pixel);
	else
		return(-1);
}
