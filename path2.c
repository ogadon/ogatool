/*
 *   path2
 *     PATH環境変数の値を1行ずつ表示する。
 *
 *   09/09/20 V0.10 by oga.
 *   13/12/12 V0.11 support X68K
 *
 */
#include <stdio.h>

#if defined _WIN32 || defined X68K
#define   KEY_DELM   ';'
#else
#define   KEY_DELM   ':'
#endif

int main(int a, char *b[])
{
	char *pt;
	char buf[4096];
	int  cnt = 0;

	pt = (char *)getenv("PATH");
	if (pt == NULL) pt = (char *)getenv("path");  /* V0.11-A */
	/* printf("PATH=[%s]\n", pt); */
	while (pt != NULL && *pt != '\0') {
		cnt = 0;
		while (*pt != KEY_DELM && *pt != '\0') {
			buf[cnt] = *pt;
			++cnt;
			++pt;
		}
		buf[cnt] = '\0';
		printf("%s\n", buf);
		if (*pt == KEY_DELM) ++pt;
	}
	return 0;
}

