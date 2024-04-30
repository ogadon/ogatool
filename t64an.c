/*
 *  VIC-1001 t64 file analyzer
 *
 *    11/05/05 V0.10 by oga.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define T64_HEADER  "C64S tape image file"

typedef unsigned int	uint;
typedef unsigned short	ushort;
typedef struct _fname_t {
	char fname[20];
} fname_t;

/* basic operation code (0x80-0xff) */
char *op[128] = {
  /* 80 */ "end", "for", "next", "data", "input#", "input", "dim", "read",
  /* 88 */ "let", "goto", "run", "if", "restore", "gosub", "return", "rem",
  /* 90 */ "stop", "on", "wait", "load", "save", "verify", "def ", "poke",
  /* 98 */ "print#", "print", "cont", "list", "clr", "cmd ", "sys ", "open",
  /* a0 */ "close", "get ", "new", "tab(", "to", "fn", "spc(", " then",
  /* a8 */ "not", "step", "+", "-", "*", "/", "^", "and",
  /* b0 */ "or", ">", "=", "<", "sgn", "int", "abs", "usr",
  /* b8 */ "fre", "pos", "sqr", "rnd", "log", "exp", "cos", "sin",
  /* c0 */ "tan", "atn", "peek", "len", "str$", "val", "asc", "chr$",
           /* 0xcc-0xfe : Ver2.0 Super Expander */
  /* c8 */ "left$", "right$", "mid$", "go", "key", "graphic", "xxx", "xxx",
  /* d0 */ "draw", "region", "color", "point", "sound", "char", "paint", "rpot",
  /* d8 */ "rpen", "rsnd", "rcolr", "rgr", "rjoy", "rdot", "", "",
  /* e0 */ "", "", "", "", "", "", "", "",
  /* e8 */ "", "", "", "", "", "", "", "",
  /* f0 */ "", "", "", "", "", "", "", "",
  /* f8 */ "", "", "", "", "", "", "", "pi",
};


int  vf  = 0;		/* -v : verbose     */
int  lff = 0;		/* -lf: line format */

void usage()
{
	printf("usage: t64an [-lf] <xxx.t64>\n");
	printf("       -lf: line format\n");
	exit(1);
}

/* dump hex code */
void dump(FILE *fp, int size)
{
	uint c;
	int  i;
	for (i = 0; i < size; i++) {
		if (i != 0 && (i % 4) == 0) {
			printf(" ");
		}
		c = getc(fp);
		if (c == EOF) break;
		printf("%02x", c);
	}
	printf("\n");
}

/* BASIC 行番号 表示
 * nnnnmmmm  nnnn:source addr  mmmm:line number 
 *   nnnn:source addr
 *   mmmm:line number
 *
 *  IN : fp
 *  OUT: ret : line no  (0: EOS)
 */
int disp_lineno(FILE *fp)
{
	ushort val;
	int    size;

	fread(&val, 1, 2, fp);
	if (vf) printf("0x%4x ", val);

	if (val == 0) {
		/* End of 1 Program Source */
		/* source addr値(nnnn)が(0000)の場合はEOSのため行番号表示しない */
		printf("--------------------\n");
		return 0;
	}

	size = fread(&val, 1, 2, fp);
	if (size == 2) {
		if (lff) {
			printf("%5d ", val);
		} else {
			printf("%d ", val);
		}
		return val;
	}
	return 0;
}


/*
 *   BASIC 中間コードを文字列に変換する
 *
 *   IN     : c    変換対象コード 
 *   OUT    : buf  変換結果 
 *   IN/OUT : strf 文字列中かどうかのフラグ 1:文字列中  
 */

void cbm_bas_trans(uint c, char *obuf, int *strfp)
{
	if (*strfp == 0 && (c >= 0x80 && c <= 0xff) && strlen(op[c-0x80])) {
		sprintf(obuf, "%s", op[c-0x80]);
	} else {
		sprintf(obuf, "%c", c);
		if (c == '"') {
			*strfp = 1 - *strfp;
		}
	}
}

/*
 *   analyze t64
 *
 *     <format>
 *     +00-13(0x14) "C64S tape image file"
 *     +14-27(0x14) ???1 (00000000 00000000 00000000 01000100 01000000)
 *                                                       ==   == num of entry
 *     +28-3f(0x18) tape name (space filled)
 *     -----------------------------------------+
 *     +40-4f(0x10) file header                 | 1 entry ～ n entry
 *     +50-5f(0x10) file name (space filled)    |
 *     -----------------------------------------+
 *     +60-63(0x04) <src_addr(2byte)><basic_line(2byte)>  (little endian)
 *     +64-         Source (null terminate)
 *     +nn          "0x000000"  End Of Source
 *
 */
void t64an(FILE *fp)
{
	char buf[4096];
	char wk[4096];
	int  strf = 0;     /* 1:文字列中 */
	int  nent = 0;
	int  size;
	int  i;
	int  ret;
	uint c;
	fname_t *fnames = NULL;

	/* header */
	size = fread(buf, 1, 20, fp);
	buf[20] = '\0';
	if (strcmp(buf, T64_HEADER))  {
		printf("Error: This is not t64 file!!\n");
		exit(1);
	}

	/* TapeHeader */
	size = fread(buf, 1, 20, fp);
	nent = buf[14];      /* num of tape entry */

	/* file name */
	size = fread(buf, 1, 24, fp);
	buf[24] = '\0';
	//printf("Tape Name: [%s]\n", buf);
	printf("Tape Name: %s\n", buf);
	printf("Num Entry: %d\n", nent);
	fnames = (fname_t *)malloc(nent*sizeof(fname_t));

	for (i = 0; i < nent; i++) {
		/* File Header */
		size = fread(buf, 1, 16, fp);

		/* File Name   */
		size = fread(buf, 1, 16, fp);
		buf[16] = '\0';
		strcpy(fnames[i].fname, buf);
		//printf("File %03d : [%s]\n", i, buf);
		printf("File %03d : %s\n", i+1, fnames[i].fname);
	}


	for (i = 0; i < nent; i++) {
		printf("\n");
		printf("###### File %03d : %s\n", i+1, fnames[i].fname);
		printf("--------------------\n");

		/* source area */
		c = 0;
		while (c != EOF) {
			/* dump(fp, 4); */      /* line data */
			ret = disp_lineno(fp);  /* disp line */
			if (ret == 0) break;    /* EOS       */
			strf = 0;
			while (1) {
				c = getc(fp);
				if (c == 0 || c == EOF) {
					printf("\n");
					break;
				}
				cbm_bas_trans(c, wk, &strf);
				printf("%s", wk);
			}
		}

	}
}

int main(int a, char *b[])
{
	int  i;
	char *filename = NULL;
	FILE *fp;

	for (i = 1; i<a; i++) {
		if (!strcmp(b[i],"-h")) {
			usage();
		}
		if (!strcmp(b[i],"-lf")) {
			lff = 1;   /* line format */
			continue;
		}
		if (!strcmp(b[i],"-v")) {
			vf = 1;
			continue;
		}
		filename = b[i];
	}

	if (filename) {
		if (!(fp = fopen(filename,"r"))) {
			perror("fopen");
			exit(1);
		}
	} else {
		fp = stdin;
	}

	t64an(fp);

	fclose(fp);
}

/* vim:ts=4:sw=4:
 */
        


