/*
 *  base64 encode/decode routines
 *
 *   2002/04/11 V1.00 by oga.
 *   2007/02/23 V1.01 support file (-f) & check "=" on decode
 *   2007/05/11 V1.02 support J-SH53 backup mail
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HELLO_WORLD "Hello World"
#define ENC_PER_LINE  (72)      /* エンコード後の一行あたりの文字数(4の倍数とすること) */

#define JSH_STARTKEY "Content-Transfer-Encoding: base64"
#define JSH_ENDKEY   "END:VBODY"

#define dprintf     if (vf) printf
#define dfprintf    if (vf) fprintf

/* translate table for encode */
unsigned char *tr_64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* translate table for decode */
unsigned char tr[256]={ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,62, 0, 0, 0,63,
                       52,53,54,55,56,57,58,59,60,61, 0, 0, 0, 0, 0, 0,
                        0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
                       15,16,17,18,19,20,21,22,23,24,25, 0, 0, 0, 0, 0,
                        0,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
                       41,42,43,44,45,46,47,48,49,50,51, 0, 0, 0, 0, 0,
                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

int vf = 0;

/*
 *  base64_enc()
 *
 *   IN  ibuf : binary data
 *   IN  ilen : binary data length
 *   OUT obuf : base64 encoded data (null terminate)
 *
 *   obuf_size = ibuf_size/3*4
 *
 *   2002/04/10 V1.00 by oga.
 */
unsigned char *base64_enc(unsigned char *ibuf, int ilen, 
                          unsigned char *obuf)
{
    unsigned char idat[3+1];
    unsigned char odat[4];
    int i;        /* input  buf pointer */
    int opt = 0;  /* output buf pointer */

    for (i = 0; i<ilen; i+=3) {
        memset(idat, 0, sizeof(idat));
        if (i + 3 > ilen) {
            /* padding 0 */
            memcpy(idat, &ibuf[i], ilen % 3);
        } else {
            memcpy(idat, &ibuf[i], 3);
        }
        // printf("idat=[%s]\n", idat);

        /*   idat[i+0] idat[i+1] idat[i+2]    */
        /*  +---------+---------+---------+   */
        /*  |      :  |    :    |  :      |   */
        /*  +---------+---------+---------+   */
        /*   123456 12 3456 1234 56 123456    */
        /*   ====== ======= ======= ======    */
        odat[0] = tr_64[(idat[0] >> 2)];
        odat[1] = tr_64[(((idat[0]   & 0x03) << 4) | (idat[1] >> 4))];
        if (i + 1 >= ilen) {
            odat[2] = '=';     /* padding */
        } else {
            odat[2] = tr_64[(((idat[1] & 0x0f) << 2) | (idat[2] >> 6))];
        }
        if (i + 2 >= ilen) {
            odat[3] = '=';     /* padding */
        } else {
            odat[3] = tr_64[idat[2] & 0x3f];
        }

        memcpy(&obuf[opt], odat, sizeof(odat));
        opt += 4;
    }
    obuf[opt] = '\0';  /* null terminate */
    return obuf;
}

/* check endian type */
int IsBigEndian()
{
    int n = 1;
    char *pn = (char *)&n;

    if (pn[3] == 1) return 1; /* big endian                */
    return 0;                 /* little endian (eg. INTEL) */
}

/*
 *   Base64のデコード
 *
 *   IN  istr : base64 string
 *   OUT ostr : decoded string
 *       ret  : decoded string
 */
unsigned char *base64_dec(unsigned char *istr, unsigned char *ostr, int *osize)
{
    int wk4;
    int i;
    int eof = 0;
    int size = 0; /* 出力バイト             V1.01-A */
    int bytes;    /* 最後のバイト数調整用   V1.01-A */
    unsigned char *wk4s = (unsigned char *)&wk4;
    unsigned char *ostr_org = ostr;

    while (*istr != '\0' && *istr != '\n' && *istr != '\r') {
	wk4 = 0;
        /* V1.01-A start */
        /* check last "=" */
        bytes = 3;
        if (strlen(istr) > 3) {
            if (istr[3] == '=') {
                bytes = 2;         /* AAA= */
            }
            if (istr[2] == '=') {
                bytes = 1;         /* AA== */
            }
        }
        /* V1.01-A end   */

	for (i = 0; i<4; i++) {
	    wk4 <<= 6;
	    if (*istr == '\0' || *istr == '\n' && *istr != '\r') {
	        eof = 1;
	    }
	    wk4 |= tr[*istr];
	    if (!eof) {
	        ++istr;
	    }
	}

        if (IsBigEndian()) {
	    memcpy(ostr, &wk4s[1], 3);
        } else {
	    ostr[0] = wk4s[2];
	    ostr[1] = wk4s[1];
	    ostr[2] = wk4s[0];
        }

	ostr += 3;
        size += bytes;
	//printf("[%c]",*istr);
    }
    *ostr = '\0';
    if (osize) *osize = size;

    return ostr_org;
}


void usage()
{
    printf("usage: base64 [-d] [<string>] [-f {<filename>|-}]\n");
    printf("usage: base64 -d -jsh53 -f {<filename>|-}\n"); /* V1.02-A */
    exit(1);
}

int main(int a, char *b[])
{
    unsigned char *buf = HELLO_WORLD;      /* 文字直指定用   */
    unsigned char obuf[2048];              /* 出力結果       */
    unsigned char buf2[2048];              /* ファイル指定用 */
    int decf = 0;                          /* -d decode flag */
    int jshf = 0;                          /* -jsh53 dec flag    V1.02-A */
    int decoding = 0;                      /* V1.02-A */
    int i;
    int size  = 0;
    int total = 0;
    char *fname = NULL;
    FILE *fp;

    for (i = 1; i < a; i++) {
        if (!strcmp(b[i], "-h")) {
	    usage();
	}
        if (!strcmp(b[i], "-d")) {
	    decf = 1;                      /* -d : decode */
	    continue;
	}
        /* V1.01-A start */
        if (i+1 < a && !strcmp(b[i], "-f")) {
	    fname = b[++i];                /* -f : file   */
            dfprintf(stderr, "fname: %s\n", fname);
	    continue;
	}
        /* V1.01-A end   */
        /* V1.02-A start */
        if (!strcmp(b[i], "-jsh53")) {
	    jshf = 1;
	    continue;
	}
        /* V1.02-A end   */
        if (!strcmp(b[i], "-v")) {
	    vf = 1;
	    continue;
	}
        buf = b[i];   /* 直接指定文字 */
    }

    /* V1.02-A start */
    if (jshf && (decf == 0 || fname == NULL)) {
        usage();
    }
    /* V1.02-A end   */

    /* ファイル指定 */
    if (fname != NULL) {
        if (!strcmp(fname, "-")) {
	    fp = stdin;
        } else {
	    if ((fp = fopen(fname,"rb")) == 0) {
	        perror(fname);
	        exit(1);
	    }
        }

        if (decf) {
	    /* decode */
            while (fgets(buf2, sizeof(buf2), fp)) {
                size = 0;
                /* decode */

		/* V1.02-A start */
#if 0
		if (buf2[strlen(buf2)-1] == 0x0a) {
		    buf2[strlen(buf2)-1] = '\0';
		}
		if (buf2[strlen(buf2)-1] == 0x0d) {
		    buf2[strlen(buf2)-1] = '\0';
		}
#endif
		if (jshf) {
		    /* J-SH53 base64 encoded file */
		    if (!strncmp(buf2, JSH_ENDKEY, strlen(JSH_ENDKEY))) {
		        dprintf("#### end\n");
		        decoding = 0; /* end */
		    }

		    if (decoding) {
			/* Decode範囲ならDecodeして出力 */
                        base64_dec(buf2, obuf, &size);
			if (size) {
                            fwrite(obuf, 1, size, stdout);   /* write to stdout */
                            /* write(1, obuf, size);  *//* write to stdout */
			} else {
#ifdef _WIN32
			    printf("\n");
#else
			    printf("\r\n");
#endif
			}
		    } else {
			/* そのまま出力 */
		        printf("%s", buf2);
		    }

		    if (!strncmp(buf2, JSH_STARTKEY, strlen(JSH_STARTKEY))) {
		        dprintf("#### start\n");
		        decoding = 1; /* start */
		    }
		} else {   /* V1.02-A end */
		    /* normal base64 encoded file */
                    dfprintf(stderr, "IN:%s", buf2);
                    base64_dec(buf2, obuf, &size);
                    dfprintf(stderr, "size:%d\n", size);
                    /* fwrite(obuf, 1, size, stdout);  */ /* write to stdout */
                    write(1, obuf, size);  /* write to stdout */
		}          /* V1.02-A */
                total += size;
            }
            fprintf(stderr, "output size = %d\n", total);
        } else {
            /* encode */
            while (size = fread(buf2, 1, ENC_PER_LINE/4*3, fp)) {
                dfprintf(stderr, "size:%d\n", size);
                printf("%s\n", base64_enc(buf2, size, obuf));
            }
        }
        if (fp != stdin) fclose(fp);

        return 0;
    }

    /* 直接文字指定 */
    if (decf) {
        /* base64 decode */
	if (vf) {
            printf("[%s]=>[%s]\n", buf, base64_dec(buf, obuf, NULL));
	} else {
            printf("%s\n", base64_dec(buf, obuf, NULL));
	}
    } else {
        /* base64 encode */
	if (vf) {
            printf("[%s]=>[%s]\n", buf, base64_enc(buf, strlen(buf), obuf));
	} else {
            printf("%s\n", base64_enc(buf, strlen(buf), obuf));
	}
    }


    return 0;
}

/* vim:ts=8:sw=8
 */

