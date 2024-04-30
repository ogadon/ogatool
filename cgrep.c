/*
 *   cgrep (color grep)
 *
 *   00/04/18 V0.10 by oga
 *   13/12/14 V0.11 support -i (ignore case)
 *   14/02/01 V0.12 support MS VC6
 *   14/12/06 V0.13 support -nc (no color), NT color
 *
 */
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>              /* V0.13-A */

#if !defined(strncasecmp)
#define strncasecmp strnicmp
#endif

/* 0.13-A start */
#define BLACK   (                                     FOREGROUND_INTENSITY)  /* black   */
#define BLUE    (FOREGROUND_BLUE                    | FOREGROUND_INTENSITY)  /* blue    */
#define GREEN   (FOREGROUND_GREEN                   | FOREGROUND_INTENSITY)  /* green   */
#define CYAN    (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY)  /* cyan    */
#define RED     (FOREGROUND_RED                     | FOREGROUND_INTENSITY)  /* red     */
#define MAGENTA (FOREGROUND_BLUE | FOREGROUND_RED   | FOREGROUND_INTENSITY)  /* magenta */
#define YELLOW  (FOREGROUND_RED  | FOREGROUND_GREEN | FOREGROUND_INTENSITY)  /* yellow  */
#define WHITE   (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY)  /* white */
#define NORMAL  (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED)        /* normal  */

typedef struct _coltbl {
    char           *escsec;
    unsigned short color;
} coltbl_t;

coltbl_t col_tbl[] = {
    { "[30m", BLACK  },
    { "[31m", RED    },
    { "[32m", GREEN  },
    { "[33m", YELLOW },
    { "[34m", BLUE   },
    { "[35m", MAGENTA},
    { "[36m", CYAN   },
    { "[37m", WHITE  },
    { "[0m" , NORMAL }
};

unsigned short colorbk = 0xffff;
/* 0.13-A end   */

#endif

enum {COL_ESC, COL_BS};
int  color = 6;	  /* -c : yellow?              */
int  vf    = 0;   /* -v : verbose              */
int  igcf  = 0;	  /* -i : ignore case  V0.11-A */
int  ncf   = 0;	  /* -nc: no color     V0.13-A */
char msg[4096];   /*                   V0.13-A */

/* V0.13-A start */
/* NT color */
void backup_color()
{
#ifdef _WIN32
	/* backup current color */
	CONSOLE_SCREEN_BUFFER_INFO scrinfo;

	if (GetConsoleScreenBufferInfo(
		GetStdHandle(STD_OUTPUT_HANDLE), 
		&scrinfo)) {
		colorbk = scrinfo.wAttributes;
		if (vf) printf("[backup color: 0x%04x]\n", colorbk);
	}
#endif /* DOS */
	return;
}

void restore_color()
{
#ifdef _WIN32
	/* restore color */
	if (colorbk != 0xffff) {
		if (vf) printf("[restore color. to 0x%04x]\n", colorbk);
		SetConsoleTextAttribute(
			GetStdHandle(STD_OUTPUT_HANDLE), 
			colorbk);
	}
#endif /* DOS */
	return;
}

void printf_nt(char *str)
{
	int i;
	int j;

#ifdef _WIN32
	for (i = 0; i < strlen(str); i++) {
		if (str[i] == 27) {  /* ESC */
			for (j = 0; j < sizeof(col_tbl)/sizeof(coltbl_t); j++) {
				if (!strncmp(&str[i], col_tbl[j].escsec, strlen(col_tbl[j].escsec))) {
				    	//if (col_tbl[j].color == NORMAL) {
					//	restore_color();
					//} else {
					//	backup_color();
					//}
					//if (vf) printf("[set color: 0x%04x]\n", colorbk);
					SetConsoleTextAttribute(
						GetStdHandle(STD_OUTPUT_HANDLE), 
						col_tbl[j].color);
					i += strlen(col_tbl[j].escsec);
					break;
				}
			}
		}
		if (str[i]) putchar((unsigned char)str[i]);

	}
#else
	printf("%s", str);
#endif
}
/* V0.13-A end   */

/*
 *  add color ESC sequence to "key" in "buf"
 */
void ChangeBold(char *buf, char *key, int type)
{
    char wk[4096];
    int  i;
    int  j = 0;
    int  k;
    int  klen;
    int  ret;

    klen = strlen(key);
    memset (wk, 0, sizeof(wk));

    for (i = 0; i < strlen(buf); i++) {
        if (igcf) {                                 /* V0.11-A */
            ret = strncasecmp(&buf[i], key, klen);  /* V0.11-A */
	} else {                                    /* V0.11-A */
            ret = strncmp(&buf[i], key, klen);
	}                                           /* V0.11-A */
        if (ret) {                                  /* V0.11-C */
            /* no match */
            wk[j++] = buf[i];
        } else {
            /* match */
            if (type == COL_ESC) {
                /* ESC color */
                wk[j++] = 27;		/* ESC   */
                wk[j++] = '[';
                wk[j++] = '3';
                wk[j++] = ('0'+color);	/* color 1-7 */
                wk[j++] = 'm';
                /* strcat(wk, key); */
                strncpy(&wk[j], &buf[i], klen);  /* V0.11-C */
                j += klen;                       /* V0.11-C */
                wk[j++] = 27;		/* ESC   */
                wk[j++] = '[';
                wk[j++] = '0';		/* normal color */
                wk[j++] = 'm';
            } else {
                /* COL_BS */
                /* Back Space color (for less?) */
                /* not available                */
		for (k = 0; k<klen; k++) {
		     /* wk[j++] = key[k]; */
		     wk[j++] = buf[i+k];         /* V0.11-C   */ 
		     wk[j++] = 0x08;             /* backspace */
		     /* wk[j++] = key[k]; */
		     wk[j++] = buf[i+k];         /* V0.11-C   */ 
		}
            }
            i += (klen-1);
        }
    }
    strcpy(buf, wk);
}

void usage()
{
    printf("usage: cgrep <search_str> [files ...] [-c <color>] [-b] [-i]\n");
    printf("       -c <color> : specify 1-7. default 6\n");
    printf("       -b         : bold with backspace (for less)\n");
    printf("       -i         : ignore case\n");
    printf("       -nc        : no color\n");       /* V0.13-A */
}

/*
 *  strcasestr()
 *
 *    case ignore strstr()
 *
 *  IN  : buf   search target string
 *        key   search key
 *  OUT : ret   NULL : match
 *              str  : match point
 */
char *strcasestr(char *buf, char *key)
{
    int  i;
    int  buflen, keylen;
    int  ret = 1;          /* no match */
    char *pt;

    buflen = strlen(buf);
    keylen = strlen(key);

    if (buflen < keylen) {
        return NULL;
    }

    /* [0][1][2][3][4] */
    /*       [0][1][2] */
    for (i = 0; i <= buflen-keylen; i++) {
    	if (!strncasecmp(&buf[i], key, keylen)) {
	    return &buf[i];
	}
    }

    return NULL;
}

int main(int a, char *b[])
{
    int  i, x;
    FILE *fp;
    char buf[4096];
    int  ctype = COL_ESC;
    char *filename[1000];
    int  files = 0;
    char *pt;
    char *key = NULL;

    for (i = 1; i<a; i++) {
        if (!strcmp(b[i],"-h")) {
	    usage();
            return 1;
        }
        if (!strcmp(b[i],"-b")) {
            ctype = COL_BS;
            continue;
        }
        if (!strcmp(b[i],"-c")) {
            color = atoi(b[++i]);
            continue;
        }

	/* V0.11-A start */
        if (!strcmp(b[i],"-i")) {
            igcf = 1;
            continue;
        }
	/* V0.11-A end   */

	/* V0.13-A start */
        if (!strcmp(b[i],"-nc")) {
            ncf = 1;
            continue;
        }
        if (!strcmp(b[i],"-v")) {
            ++vf;
            continue;
        }
	/* V0.13-A end   */

        if (key == NULL) {
            key = b[i];
            continue;
        }
        filename[files++] = b[i];
    }

    if (key == NULL) {
	usage();
	return 1;
    }

    backup_color();   /* V0.13-A */
#ifdef _WIN32
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), NORMAL);
#endif /* _WIN32 */

    x = 0;
    do {
        if (files) {
	    if (!(fp = fopen(filename[x],"r"))) {
		perror("fopen");
		continue;
	    }
        } else {
            fp = stdin;
        }

        while (fgets(buf,sizeof(buf),fp)) {
	    if (igcf) {
	        /* ignore case */
                pt = strcasestr(buf, key);
	    } else {
                pt = strstr(buf, key);
	    }
            if (pt) {
                if (!ncf) ChangeBold(buf, key, ctype);       /* V0.13-C */
                if (files > 1) {
                    sprintf(msg, "%s:%s", filename[x], buf); /* V0.13-C */
                    printf_nt(msg);                          /* V0.13-A */
                } else {
                    sprintf(msg, "%s", buf);                 /* V0.13-C */
                    printf_nt(msg);                          /* V0.13-A */
                }
            }
        }

        if (files) fclose(fp);
    } while ( ++x < files);

    restore_color();  /* V0.13-A */

    return 0;
}

/* 
 * vim:ts=8:sw=4:
 */

