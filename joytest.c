/*
 *   joytest.c
 *
 *   2003/12/07 V0.10 by oga.
 *
 */

#define USE_JOYSTICK

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#ifdef USE_JOYSTICK
#include <linux/joystick.h>
#endif /* USE_JOYSTICK */

int joyf     = 0;		/* 1: Use Joystick       V1.02 */
int joyinit  = 0;		/* get_joystick init flg V1.02 */
int joy_x_max= 0;		/* joy maxvalue x        V1.02 */
int joy_y_max= 0;		/* joy maxvalue y        V1.02 */
char joydev[256] = "/dev/js0";  /* joystick device name  V1.02 */

/*
 *  charcter vector define / keycode
*/
#define JOY_NOMOVE  0x00
#define JOY_UP      0x01
#define JOY_DOWN    0x02
#define JOY_LEFT    0x04
#define JOY_RIGHT   0x08

#define JOY_TRIG1   0x10
#define JOY_TRIG2   0x20
#define JOY_TRIG3   0x40
#define JOY_TRIG4   0x80

/*
 *   JOYSTICK取り込み  V1.02
 *
 *   IN  : なし
 *   IN  : global : joydev : joystickデバイス名
 *   OUT : ret 
 *           JOY_NOMOVE : 入力なし
 *           UP,DOWN,LEFT,RIGHT,TRIG1,TRIG2 : 各キーが押された。
 *
 */
int get_joystick()
{
    int status = JOY_NOMOVE;
    int st;
#ifdef USE_JOYSTICK
    struct JS_DATA_TYPE js;
    int fd;

    /* open device file */
    if ((fd = open(joydev, O_RDONLY)) < 0) {
	printf("%s : open error(%d)\n", joydev, errno);
	return JOY_NOMOVE;
    }

    st = read(fd, &js, JS_RETURN);
    if (st != JS_RETURN) {
	printf("%s : open error(%d)\n", joydev, errno);
	return JOY_NOMOVE;
    }

    if (joyinit == 0) {
        joy_x_max = js.x*2;
	joy_y_max = js.y*2;
	joyinit = 1;
    }

    if (joy_x_max < js.x) joy_x_max = js.x;
    if (joy_y_max < js.y) joy_y_max = js.y;

    if (js.x < joy_x_max/3)   status |= JOY_LEFT;	/* move left    */
    if (js.x > joy_x_max*2/3) status |= JOY_RIGHT;	/* move right   */
    if (js.y < joy_y_max/3)   status |= JOY_UP;		/* move up      */
    if (js.y > joy_y_max*2/3) status |= JOY_DOWN;	/* move down    */

    /* ボタンの方が優先 */
    if (js.buttons & 1) status |= JOY_TRIG1;		/* left button  */
    if (js.buttons & 2) status |= JOY_TRIG2;		/* right button */
    if (js.buttons & 4) status |= JOY_TRIG3;		/* right button */
    if (js.buttons & 8) status |= JOY_TRIG4;		/* right button */

    close(fd);
#endif /* USE_JOYSTICK */

    return status;
}

void print_joystat(int val)
{
    /*            0  3    8    13     20 23 26 29 */
    char *str  = "UP DOWN LEFT RIGHT  T1 T2 T3 T4";
    char sts[] = "□ □   □   □     □ □ □ □";

    if (val & JOY_UP)    memcpy(&sts[ 0], "■", 2);
    if (val & JOY_DOWN)  memcpy(&sts[ 3], "■", 2);
    if (val & JOY_LEFT)  memcpy(&sts[ 8], "■", 2);
    if (val & JOY_RIGHT) memcpy(&sts[13], "■", 2);
    if (val & JOY_TRIG1) memcpy(&sts[20], "■", 2);
    if (val & JOY_TRIG2) memcpy(&sts[23], "■", 2);
    if (val & JOY_TRIG3) memcpy(&sts[26], "■", 2);
    if (val & JOY_TRIG4) memcpy(&sts[29], "■", 2);

    printf("%s  (0x%02x)\n", str, val);
    printf("%s\n", sts);
    fflush(stdout);
}

int main(int a, char *b[])
{
        int val;
	int prev_val = 999;
	int i;

	/* get args */
	for (i = 1; i < a; i++) {
	    if (!strncmp(b[i],"-h",2)) {
		printf("usage : joytest [-joy {<0>|1}]\n");
		exit(1);
	    }
	    if (!strncmp(b[i],"-joy",4)) {
	        /* use joystick */
	        sprintf(joydev, "/dev/js%s", b[++i]);
	        joyf = 1;
	        continue;
	    }
	}
	printf("joy device = %s\n", joydev);
	printf("\n\n");
	while (1) {
	    val = get_joystick();
	    if (prev_val != val) {
	        //printf("joy = %d\n", val);
		printf("%cM%cM", 27, 27);
		print_joystat(val);
		prev_val = val;
	    }
	    usleep(100000);
	}
	return 0;
}


