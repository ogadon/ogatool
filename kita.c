/*
 *  kita.c
 *    2ch kitaaaaaa...
 *
 *    2002/03/03 V0.10 by oga.
 *    2009/10/30 V0.11 port to Win
 */
#ifdef _WIN32
#include <windows.h>
#endif
#include <stdio.h>

int main(int a, char *b[])
{
    int i;
    char *kita[7] = {
        "(ﾟ∀ﾟ)",
	"( ﾟ∀)",
	"( 　ﾟ)",
	"(　　)",
	"(　　)",
	"(ﾟ 　)",
	"(∀ﾟ )"
    };

    while (1) {
        for (i = 0; i < 7; i++) {
#ifdef _WIN32
            printf("  ｷﾀ━━━%s━━━!!!!!\r", kita[i]);
	    Sleep(150);
#else
            printf("  ｷﾀ━━━%s━━━!!!!!\n%cM", kita[i], 27);
	    usleep(200000);
#endif
	}
    }
}

/* vim:ts=8:sw=8:
 */


