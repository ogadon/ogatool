/*
 *  kita2.c
 *    2ch kitaaaaaa...
 *
 *    2002/03/03 by oga.
 *    2005/02/11 for http://pc5.2ch.net/test/read.cgi/unix/1019380983/ #138 by oga.
 */
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

    printf("\n");
    printf("　　　　　 *　　※ ☆ 　 ※　※ 　 ☆ ※　　*\n");
    printf("　　　　 *　 ※ ☆　 ※ 　 ※ 　 ※ 　☆ ※　 *\n");
    printf("　　　 *　※ ☆　※ 　 ※ ☆ ※　　※　☆ ※　*\n");
    printf("　　 *　※ ☆ ※　 ※ ☆　　.☆ ※　 ※ ☆ ※　*\n");
    printf("　　*　※ ☆ ※　※☆　　　　　☆※　※ ☆ ※　*\n");
    printf("\n\n\n\n\n\n");

    while (1) {
        for (i = 0; i < 7; i++) {
            printf("%cM%cM%cM%cM%cM%cM", 27, 27, 27, 27, 27, 27);
            printf("　　*　※ｷﾀ━━━━━%s━━━━━ !!!※　*\n", kita[i]);
            printf("　　*　※ ☆ ※　※☆　　　　　☆※　※ ☆ ※　*\n");
            printf("　　 *　※ ☆ ※　 ※ ☆　　.☆ ※　 ※ ☆ ※　*\n");
            printf("　　　 *　※ ☆　※ 　 ※ ☆ ※　　※　☆ ※　*\n");
            printf("　　　　 *　 ※ ☆　 ※ 　 ※ 　 ※ 　☆ ※　 *\n");
            printf("　　　　　 *　　※ ☆ 　 ※　※ 　 ☆ ※　　*\n");
	    usleep(200000);
	}
    }
}
