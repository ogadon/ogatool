#include <windows.h>
#include <stdio.h>

#if 0
#define FOREGROUND_BLUE      0x0001 // text color contains blue.
#define FOREGROUND_GREEN     0x0002 // text color contains green.
#define FOREGROUND_RED       0x0004 // text color contains red.
#define FOREGROUND_INTENSITY 0x0008 // text color is intensified.
#define BACKGROUND_BLUE      0x0010 // background color contains blue.
#define BACKGROUND_GREEN     0x0020 // background color contains green.
#define BACKGROUND_RED       0x0040 // background color contains red.
#define BACKGROUND_INTENSITY 0x0080 // background color is intensified.
#define COMMON_LVB_LEADING_BYTE    0x0100 // Leading Byte of DBCS
#define COMMON_LVB_TRAILING_BYTE   0x0200 // Trailing Byte of DBCS
#define COMMON_LVB_GRID_HORIZONTAL 0x0400 // DBCS: Grid attribute: top horizontal.
#define COMMON_LVB_GRID_LVERTICAL  0x0800 // DBCS: Grid attribute: left vertical.
#define COMMON_LVB_GRID_RVERTICAL  0x1000 // DBCS: Grid attribute: right vertical.
#define COMMON_LVB_REVERSE_VIDEO   0x4000 // DBCS: Reverse fore/back ground attribute.
#define COMMON_LVB_UNDERSCORE      0x8000 // DBCS: Underscore.

#define COMMON_LVB_SBCSDBCS        0x0300 // SBCS or DBCS flag.
#endif


int main(int a, char *b[])
{
	int i;
	WORD color = 7;  /* white */

	for (i = 1; i < a; i++) {
		if (!strcmp(b[i], "-h")) {
			printf("usage: color <color_code>\n");
			printf("  FG_BLUE : 0x0001\n");
			printf("  FG_GREEN: 0x0002\n");
			printf("  FG_RED  : 0x0004\n");
			printf("  FG_INT  : 0x0008 blightness\n");
			printf("  BG_BLUE : 0x0010\n");
			printf("  BG_GREEN: 0x0020\n");
			printf("  BG_RED  : 0x0040\n");
			printf("  BG_INT  : 0x0080 blightness\n");
			return 1;
		}
		color = atoi(b[i]);
	}

	//SetConsoleTextAttribute(
	//	GetStdHandle(STD_OUTPUT_HANDLE), 
	//	FOREGROUND_INTENSITY | FOREGROUND_RED | BACKGROUND_GREEN);
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
  
	//printf("Hello, World!\n");

	return 0;
}

