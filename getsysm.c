#include <windows.h>
#include <stdio.h>

typedef struct _smval_t {
    int  prop;
    char *propname;
    char level;        // 0:default•\Ž¦
} smval_t;

smval_t smvalue[] = {
    SM_CXSCREEN,	"SM_CXSCREEN",	1,
    SM_CYSCREEN,	"SM_CYSCREEN",	1,
    SM_CXVSCROLL,	"SM_CXVSCROLL",	1,
    SM_CYHSCROLL,	"SM_CYHSCROLL",	1,
    SM_CYCAPTION,	"SM_CYCAPTION",	0,
    SM_CXBORDER,	"SM_CXBORDER",	0,
    SM_CYBORDER,	"SM_CYBORDER",	0,
    SM_CXDLGFRAME,	"SM_CXDLGFRAME",	0,
    SM_CYDLGFRAME,	"SM_CYDLGFRAME",	0,
    SM_CYVTHUMB,	"SM_CYVTHUMB",	1,
    SM_CXHTHUMB,	"SM_CXHTHUMB",	1,
    SM_CXICON,		"SM_CXICON",	1,
    SM_CYICON,		"SM_CYICON",	1,
    SM_CXCURSOR,	"SM_CXCURSOR",	1,
    SM_CYCURSOR,	"SM_CYCURSOR",	1,
    SM_CYMENU,		"SM_CYMENU",	0,
    SM_CXFULLSCREEN,	"SM_CXFULLSCREEN",	1,
    SM_CYFULLSCREEN,	"SM_CYFULLSCREEN",	1,
    SM_CYKANJIWINDOW,	"SM_CYKANJIWINDOW",	1,
    SM_MOUSEPRESENT,	"SM_MOUSEPRESENT",	1,
    SM_CYVSCROLL,	"SM_CYVSCROLL",	1,
    SM_CXHSCROLL,	"SM_CXHSCROLL",	1,
    SM_DEBUG,		"SM_DEBUG",	1,
    SM_SWAPBUTTON,	"SM_SWAPBUTTON",	1,
    SM_RESERVED1,	"SM_RESERVED1",	1,
    SM_RESERVED2,	"SM_RESERVED2",	1,
    SM_RESERVED3,	"SM_RESERVED3",	1,
    SM_RESERVED4,	"SM_RESERVED4",	1,
    SM_CXMIN,	"SM_CXMIN",	1,
    SM_CYMIN,	"SM_CYMIN",	1,
    SM_CXSIZE,	"SM_CXSIZE",	1,
    SM_CYSIZE,	"SM_CYSIZE",	1,
    SM_CXFRAME,	"SM_CXFRAME",	1,
    SM_CYFRAME,	"SM_CYFRAME",	1,
    SM_CXMINTRACK,	"SM_CXMINTRACK",	1,
    SM_CYMINTRACK,	"SM_CYMINTRACK",	1,
    SM_CXDOUBLECLK,	"SM_CXDOUBLECLK",	1,
    SM_CYDOUBLECLK,	"SM_CYDOUBLECLK",	1,
    SM_CXICONSPACING,	"SM_CXICONSPACING",	1,
    SM_CYICONSPACING,	"SM_CYICONSPACING",	1,
    SM_MENUDROPALIGNMENT,	"SM_MENUDROPALIGNMENT",	1,
    SM_PENWINDOWS,	"SM_PENWINDOWS",	1,
    SM_DBCSENABLED,	"SM_DBCSENABLED",	1,
    SM_CMOUSEBUTTONS,	"SM_CMOUSEBUTTONS",	1,
    SM_SECURE,	"SM_SECURE",	1,
    SM_CXEDGE,	"SM_CXEDGE",	1,
    SM_CYEDGE,	"SM_CYEDGE",	1,
    SM_CXMINSPACING,	"SM_CXMINSPACING",	1,
    SM_CYMINSPACING,	"SM_CYMINSPACING",	1,
    SM_CXSMICON,	"SM_CXSMICON",	1,
    SM_CYSMICON,	"SM_CYSMICON",	1,
    SM_CYSMCAPTION,	"SM_CYSMCAPTION",	1,
    SM_CXSMSIZE,	"SM_CXSMSIZE",	1,
    SM_CYSMSIZE,	"SM_CYSMSIZE",	1,
    SM_CXMENUSIZE,	"SM_CXMENUSIZE",	1,
    SM_CYMENUSIZE,	"SM_CYMENUSIZE",	1,
    SM_ARRANGE,	"SM_ARRANGE",	1,
    SM_CXMINIMIZED,	"SM_CXMINIMIZED",	1,
    SM_CYMINIMIZED,	"SM_CYMINIMIZED",	1,
    SM_CXMAXTRACK,	"SM_CXMAXTRACK",	1,
    SM_CYMAXTRACK,	"SM_CYMAXTRACK",	1,
    SM_CXMAXIMIZED,	"SM_CXMAXIMIZED",	1,
    SM_CYMAXIMIZED,	"SM_CYMAXIMIZED",	1,
    SM_NETWORK,	"SM_NETWORK",	1,
    SM_CLEANBOOT,	"SM_CLEANBOOT",	1,
    SM_CXDRAG,	"SM_CXDRAG",	1,
    SM_CYDRAG,	"SM_CYDRAG",	1,
    SM_SHOWSOUNDS,	"SM_SHOWSOUNDS",	1,
    SM_CXMENUCHECK,	"SM_CXMENUCHECK",	1,
    SM_CYMENUCHECK,	"SM_CYMENUCHECK",	1,
    SM_SLOWMACHINE,	"SM_SLOWMACHINE",	1,
    SM_MIDEASTENABLED,	"SM_MIDEASTENABLED",	1,
//    SM_MOUSEWHEELPRESENT,	"SM_MOUSEWHEELPRESENT",	1,
//    SM_XVIRTUALSCREEN,	"SM_XVIRTUALSCREEN",	1,
//    SM_YVIRTUALSCREEN,	"SM_YVIRTUALSCREEN",	1,
//    SM_CXVIRTUALSCREEN,	"SM_CXVIRTUALSCREEN",	1,
//    SM_CYVIRTUALSCREEN,	"SM_CYVIRTUALSCREEN",	1,
//    SM_CMONITORS,	"SM_CMONITORS",	1,
//    SM_SAMEDISPLAYFORMAT,	"SM_SAMEDISPLAYFORMAT",	1,
//    SM_CMETRICS,	"SM_CMETRICS",	1,
//    SM_CMETRICS,	"SM_CMETRICS",	1,
};

int allf = 0;      /* -all */


usage()
{
    printf("usage: getsysm [-a]\n");
}

int main(int a, char *b[])
{
    int i;

    for (i = 1; i<a; i++) {
	if (!strcmp(b[i], "-h")) {
	    usage();
	}
	if (!strcmp(b[i], "-a")) {
	    allf = 1;
	}
    }

    for (i = 0; i<sizeof(smvalue)/sizeof(smval_t); i++) {
	if (allf || smvalue[i].level == 0) {
	    printf("%-20s : %d\n", smvalue[i].propname, GetSystemMetrics(smvalue[i].prop));
	}
    }
}


/*
#define SM_CXSCREEN             0
#define SM_CYSCREEN             1
#define SM_CXVSCROLL            2
#define SM_CYHSCROLL            3
#define SM_CYCAPTION            4
#define SM_CXBORDER             5
#define SM_CYBORDER             6
#define SM_CXDLGFRAME           7
#define SM_CYDLGFRAME           8
#define SM_CYVTHUMB             9
#define SM_CXHTHUMB             10
#define SM_CXICON               11
#define SM_CYICON               12
#define SM_CXCURSOR             13
#define SM_CYCURSOR             14
#define SM_CYMENU               15
#define SM_CXFULLSCREEN         16
#define SM_CYFULLSCREEN         17
#define SM_CYKANJIWINDOW        18
#define SM_MOUSEPRESENT         19
#define SM_CYVSCROLL            20
#define SM_CXHSCROLL            21
#define SM_DEBUG                22
#define SM_SWAPBUTTON           23
#define SM_RESERVED1            24
#define SM_RESERVED2            25
#define SM_RESERVED3            26
#define SM_RESERVED4            27
#define SM_CXMIN                28
#define SM_CYMIN                29
#define SM_CXSIZE               30
#define SM_CYSIZE               31
#define SM_CXFRAME              32
#define SM_CYFRAME              33
#define SM_CXMINTRACK           34
#define SM_CYMINTRACK           35
#define SM_CXDOUBLECLK          36
#define SM_CYDOUBLECLK          37
#define SM_CXICONSPACING        38
#define SM_CYICONSPACING        39
#define SM_MENUDROPALIGNMENT    40
#define SM_PENWINDOWS           41
#define SM_DBCSENABLED          42
#define SM_CMOUSEBUTTONS        43
#define SM_SECURE               44
#define SM_CXEDGE               45
#define SM_CYEDGE               46
#define SM_CXMINSPACING         47
#define SM_CYMINSPACING         48
#define SM_CXSMICON             49
#define SM_CYSMICON             50
#define SM_CYSMCAPTION          51
#define SM_CXSMSIZE             52
#define SM_CYSMSIZE             53
#define SM_CXMENUSIZE           54
#define SM_CYMENUSIZE           55
#define SM_ARRANGE              56
#define SM_CXMINIMIZED          57
#define SM_CYMINIMIZED          58
#define SM_CXMAXTRACK           59
#define SM_CYMAXTRACK           60
#define SM_CXMAXIMIZED          61
#define SM_CYMAXIMIZED          62
#define SM_NETWORK              63
#define SM_CLEANBOOT            67
#define SM_CXDRAG               68
#define SM_CYDRAG               69
#define SM_SHOWSOUNDS           70
#define SM_CXMENUCHECK          71
#define SM_CYMENUCHECK          72
#define SM_SLOWMACHINE          73
#define SM_MIDEASTENABLED       74
#define SM_MOUSEWHEELPRESENT    75
#define SM_XVIRTUALSCREEN       76
#define SM_YVIRTUALSCREEN       77
#define SM_CXVIRTUALSCREEN      78
#define SM_CYVIRTUALSCREEN      79
#define SM_CMONITORS            80
#define SM_SAMEDISPLAYFORMAT    81
#define SM_CMETRICS             83
*/
