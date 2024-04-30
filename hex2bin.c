/*
 *   hex2bin
 *      hex文字列をascii文字列に変換
 *      "48656c6c6f" => "Hello"
 *
 *   2002/04/16 V0.10 by oga.
 *   2010/09/26 V0.20 support wire shark hex dump
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>


/* Wireshark packet dump format */
/* -----+----1----+----2----+----3----+----4----+----5----+----6----+----7--- */
/* 0060  4f 4e 54 52 4f 4c 3a 20  6d 61 78 2d 61 67 65 3d   ONTROL:  max-age= */
int wsf = 0;    /* Wireshark hex dump format switch */

/*
 *   hex文字列をascii文字列に変換
 *
 *   "48656c6c6f" => "Hello"
 *
 *   ret : length
 */
int hex2str(char *in, char *out)
{
    int  i = 0;
    int  j = 0;
    char wk[16];
    char cc;

    while (isdigit(in[i]) || (tolower(in[i]) >= 'a' && tolower(in[i]) <= 'f') || isspace(in[i])) {    /* V0.20-C */
	/* V0.20-A start */
	if (wsf && (i < 6 || i > 53)) {
	    ++i;
	    continue;              /* skip addr and ascii char */
	}
	if (isspace(in[i])) {
	    ++i;
	    continue;              /* skip space */
    	}
	/* V0.20-A end   */
        strcpy(wk, "0x");
        memcpy(&wk[2], &in[i], 2);
	wk[4] = '\0';
	out[j] = (char)strtoul(wk, (char **)NULL, 0);
	i += 2;
	++j;
    }
    out[j] = '\0';
    return j;       // 変換結果がバイナリの場合のために長さを返す
}

int main(int a, char *b[])
{
    FILE *fp;
    char *fname = NULL;
    char buf[4096];
    char work[4096];
    int  i;
    int  len;

    for (i = 1; i<a; i++) {
        if (!strcmp(b[i], "-h")) {
	    printf("usage: hex2bin [-ws] [<file>]\n");
	    exit(1);
	}
        if (!strcmp(b[i], "-ws")) {
	    wsf = 1;
	    continue;
	}
        fname = b[i];
    }

#ifdef _WIN32
    // 改行とかが勝手に変換されないようにstdoutをバイナリモードにする  
    setmode(fileno(stdout),O_BINARY);
#endif

    if (fname == NULL) {
        fp = stdin;
    } else
    if ((fp = fopen(fname, "r")) == NULL) {
        perror(fname);
	exit(1);
    }

    while (fgets(buf, sizeof(buf), fp)) {
        if (buf[strlen(buf)-1] == 0x0a) {
	    buf[strlen(buf)-1] = '\0';
	}
        if (buf[strlen(buf)-1] == 0x0d) {
	    buf[strlen(buf)-1] = '\0';
	}
        len = hex2str(buf, work);
	fwrite(work, 1, len, stdout);
    }

    if (fp != stdin) {
        fclose(fp);
    }
    return 0;
}

