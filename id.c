#include <windows.h>
#include <stdio.h>

//#define GET_USER_NAME_EX 1

void usage()
{
#ifdef GET_USER_NAME_EX
    printf("usage: id [{1|2|...|10}]\n");
#else
    printf("usage: id\n");
#endif
    exit(1);
}

int main(int a, char *b[])
{
    char userid[2048];
    int  size = sizeof(userid);
    int  name_fmt;

    if (a > 1 && !strcmp(b[1], "-h")) {
        usage();
    }

#ifdef GET_USER_NAME_EX
    if (a > 1) {
        name_fmt = atoi(b[1]);
        if (name_fmt < 1 || name_fmt > 10) {
	    usage();
	}
        if (GetUserNameEx(name_fmt, userid, &size) == TRUE) {
            printf("%s\n", userid);
        } else {
            printf("GetUserName: Error code = %d\n", GetLastError());
	    return 1;
        }
    } else {
#endif
        if (GetUserName(userid, &size) == TRUE) {
            printf("%s\n", userid);
        } else {
            printf("GetUserName: Error code = %d\n", GetLastError());
	    return 1;
        }
#ifdef GET_USER_NAME_EX
    }
#endif
    return 0;
}

