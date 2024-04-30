/*
 *  mkfile2 : make free size file			by oga.
 *
 *    96/05/09 V1.00 by oga.
 *    05/09/06 V1.01 perf up.
 *    14/07/04 V1.10 support -f, -r option
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define VER      "1.10"
#define RAND(x)  ((rand() & 0xffff)*(x)/(RAND_MAX & 0xffff))

char wbuf[4096];  /* V1.01-A              V1.10-M */
int  num  = 1;    /* num of file          V1.10-M */
int  fcnt = 0;    /* file num counter     V1.10-A */
int  vf = 0;      /* -v: verbose                  */
int  rf = 0;      /* -r: random data      V1.10-A */
int  ff = 0;      /* -f: specify file     V1.10-A */
int  spos = 1;    /* -f file size pos     V1.10-A */
int  fpos = 0;    /* -f file filename pos V1.10-A */

/*
 *  get_item()
 *
 *   IN  buf   : target string
 *   IN  sep   : item separator
 *   IN  pos   : target location
 *   OUT outbuf: target item
 *
 */
char *get_item(char *buf, char *sep, int pos, char *outbuf)
{
	int  i;
	char *pt;
	char *p;
	char wk[4096];
	char msglog[4096];


	strcpy(wk,buf);      /* strtok()はbufを破壊するためコピーして利用する */

	for (i = 0; i<pos; i++) {
		if (i == 0) {
			pt = (char *)strtok(wk,sep);
		} else {
			pt = (char *)strtok(NULL,sep);
		}
		if (pt == NULL) break;
	}
	if (pt == NULL) {
		sprintf(msglog, "Out of item(%s) pos(%d).\n", buf, pos);
		printf(msglog);
		/* 結果領域クリア */
		strcpy(outbuf,"");
		return (char *)0;
	}

	strcpy(outbuf, pt);

	/* cut tail space */
	p = &outbuf[strlen(outbuf)-1];	/* last char */
	while (*p == ' ' || *p == 0x0a || *p == 0x0d) {
		*p = '\0';
		if (p == &outbuf[0]) break;
		--p;
	}

	return outbuf;
} /* get_item */


/*
 *  del_comma()
 *    IN  str : num string (with comma)    ex. "1,234,567"
 *    OUT str : num string (without comma) ex. "1234567"
 *
 */
void del_comma(char *str)
{
	char buf[4096];
	char *pt;
	int  pos = 0;

	strcpy(buf, str);
	pt = &buf[0];
	while (*pt) {
		if (*pt != ',') {
			str[pos++] = *pt;
		}
		++pt;
	}
	str[pos] = '\0';
}


/*
 *  mkfile()
 *    IN  afname : fielname  ("10M", "xxx.txt", ..)
 *    IN  size   : file size
 *    IN  ropt   : 1: random data
 *
 */
int mkfile(char *afname, unsigned int size, int ropt)
{
	int  i;
	FILE *fp;
	char fname[4096];
	char buf[4096];
	char buf2[50];

	unsigned int  wsize;        /* V1.01-A */
	unsigned int  total;        /* V1.01-A */
	unsigned int  result;       /* V1.01-A */

	if (ropt) {
		for (i = 0; i<sizeof(wbuf); i++) {
			wbuf[i] = 0x20+RAND(0x5e);
		}
	}

	if (!afname || strlen(afname) == 0) {
		strcpy(buf, "dummy");
	} else {
		strcpy(buf, afname);
	}

	if (ff) {
		if (!afname || strlen(afname) == 0) {
			/* If there is no filename, assume "dummy_fileNNNNN" */
			sprintf(buf2, "_file%05d", fcnt++);
			strcat(buf,buf2);
		}
	} else {
		if (num == 1) {
			strcat(buf,"_file");
		} else {
			sprintf(buf2, "_file%05d", fcnt++);
			strcat(buf,buf2);
		}
	}

	printf("make %d Bytes file.  filename is %s\n",size,buf);
	if ((fp = fopen(buf,"wb")) == 0) {
		perror("fopen");
		return -1;
	}

	/* make file ! */
	/* V1.01-C start */
	total  = 0;
	while (total < size) {
		if ((size - total) > sizeof(wbuf)) {
			wsize = sizeof(wbuf);
		} else {
			wsize = (size - total);
		}
		result = fwrite(wbuf, 1, wsize, fp);
		total += result;
		if (result <= 0 && size != total) {
			printf("write incompleted!  size=%d write=%d\n", size, total);
			break;
		}
		if (ropt) {
			for (i = 0; i<sizeof(wbuf); i++) {
				wbuf[i] = 0x20+RAND(0x5e);
			}
		}
	}
	/* V1.01-C end   */

	if (fclose(fp)) {
		perror("fclose");
	}

	return 0;
}

void usage()
{
	printf("mkfile2 Ver%s\n", VER);
	printf("usage : mkfile2 { size | sizeK | sizeM } [<num>]\n");
	printf("usage : mkfile2 -f <size_file> [-sp <size_pos(1)>] [-fp <filename_pos>] [-r]\n");
	printf("        size_file: ex. \"<size> [<filename>]\"\n");
	printf("        -sp      : size position (default:1)\n");
	printf("        -fp      : filename position\n");
	printf("        -r       : random data\n");
	exit(1);
}

int main(a,b)
int a;
char *b[];
{
	FILE *fpin = NULL;
	char buf[4096];
	char sizestr[1024];
	char sizearg[256];
	int  i, j;
	int  c='A', size=-1;

	char *filename = NULL;
	char fname[4096];

	for (i = 1; i<a; i++) {
		if (!strcmp(b[i],"-h")) {
			usage();
		}
		if (!strcmp(b[i],"-r")) {
			rf = 1;
			continue;
		}
		if (!strcmp(b[i],"-v")) {
			vf = 1;
			continue;
		}
		if (i+1 < a && !strcmp(b[i],"-f")) {
			ff = 1;
			filename = b[++i];
			continue;
		}

		/* -f filename : size position */
		if (i+1 < a && !strcmp(b[i],"-sp")) {
			spos = atoi(b[++i]);
			continue;
		}

		/* -f filename : filename position */
		if (i+1 < a && !strcmp(b[i],"-fp")) {
			fpos = atoi(b[++i]);
			continue;
		}

		if (size == -1) {
			strcpy(sizearg, b[i]);
			size = atoi(sizearg);
			if (sizearg[strlen(sizearg)-1]== 'K' || sizearg[strlen(sizearg)-1]== 'k') {
				size = atoi(sizearg) * 1024;
			}
			if (sizearg[strlen(sizearg)-1]== 'M' || sizearg[strlen(sizearg)-1]== 'm') {
				size = atoi(sizearg) * 1024 * 1024;
			}
			continue;
		} else {   /* num */
			num = atoi(b[i]);
			continue;
		}
	}

	/* V1.10-A start */
	if (!filename && size < 0) {
		usage();
	}
	if (ff && strlen(sizearg)) {
		usage();
	}
	/* V1.10-A end   */

    /* V1.01-A start */
	for (i = 0; i<sizeof(wbuf); i++) {
	    wbuf[i] = 'A';
	}
    /* V1.01-A end   */

	fcnt = 0;
	if (filename) {
		if (!(fpin = fopen(filename,"r"))) {
			perror("fopen");
			exit(1);
		}

		while (fgets(buf,sizeof(buf), fpin)) {
			if (vf) printf("#%s", buf);
			if (buf[strlen(buf)-1] == 0x0a) {
				buf[strlen(buf)-1] = '\0';
			} 
			if (buf[strlen(buf)-1] == 0x0d) {
				buf[strlen(buf)-1] = '\0';
			} 

			/* comment line */
			if (buf[0] == ' ' || buf[0] == '\t' || buf[0] == '#' || strlen(buf) == 0) continue;

			/* filesize */
			get_item(buf, " ", spos, sizestr);
			del_comma(sizestr);
			size = atoi(sizestr);
			if (size == 0) continue;

			/* filename */
			if (fpos == 0) {
				strcpy(fname, "");
			} else {
				get_item(buf, " ", fpos, fname);
			}
			mkfile(fname, size, rf);
		}

		if (fpin) fclose(fpin);
	} else {
		/* no -f option (<sizearg> <num>) */
		for (j = 0; j<num; j++) {
			mkfile(sizearg, size, rf);
		}
	}

	return 0;
}
