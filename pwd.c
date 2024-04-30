/*
 *  pwd.c
 *
 *       96/??/?? V1.00 for DOS
 *       97/05/04 V1.01 for X68000
 *
 *
 *  CFLAGS : default   PC/AT
 *           X68K      X68000
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef X68K
#include <unistd.h>
#include <sys/param.h>
#else
#include <direct.h>
#endif

#ifdef X68K
#define _MAX_PATH	MAXPATHLEN
#endif

main()
{
	char buf[_MAX_PATH];
	char buf2[_MAX_PATH];
	int  i;

	memset(buf2, 0, _MAX_PATH);
	getcwd(buf,_MAX_PATH);
	for (i=0; i < strlen(buf); i++) {
		buf2[i] = tolower(buf[i]);
		if (buf2[i] == '\\') {
			buf2[i] = '/';
		}
	}
        printf("%s\n",buf2);
}

#if 0
bzero(buf,len)
char *buf;
int len;
{
	int i;
	for (i=0; i<len; i++)
		buf[i] = 0;
	return len;
}
#endif

