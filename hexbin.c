/*
 *   hexbin : �e�L�X�g��HEX�f�[�^(0x00, 0x11, ...)���o�C�i���t�@�C���ɕϊ�����
 *            �܂����̋t���s��
 *
 *   2003/09/04 V1.00 by oga.
 *   2003/12/22 V1.01 support 2byte data
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define DATA_PER_LINE 10

/*
 *  HexStrlen
 *    0xNNNN, 0xNNNNN, ...  �`����HEX�f�[�^�̒�����Ԃ�
 *
 *  IN  : str : "0x"�Ŏn�܂镶����
 *  OUT : ret : "0x"���܂�HEX������̒���
 *              "0x"�Ŏn�܂�Ȃ��ꍇ��0��Ԃ�
 *
 */
int HexStrlen(char *str)
{
    int len = 0;

    if (strncmp(str, "0x", 2)) {
        return len;  /* return 0 */
    }
    str += 2;
    len += 2;
    while (isdigit(*str) || 
           ('a' <= *str && *str <= 'f') ||
	   ('A' <= *str && *str <= 'F')) {
        ++len;
	++str;
    }
    /* fprintf(stderr, "len = %d\n", len); */
    return len;
}

/*
 *  hex2bin
 *    �e�L�X�g��HEX�f�[�^���o�C�i���t�@�C���ɕϊ�
 *
 *  IN  : fp : �e�L�X�g��HEX�f�[�^�t�@�C���̃t�@�C���|�C���^
 *             0x00,0x11,0x22,....�`��
 *             0x0NNNN, 0xNNNN (2�o�C�g�̒l�܂őΉ� (03/12/22))
 *
 */
void hex2bin(FILE *fp)
{
    char buf[4096*10];
    char work[16];
    char *pt;
    int  c;
    int  len;

    while (fgets(buf, sizeof(buf), fp)) {
        pt = &buf[0];
        while (pt = strstr(pt, "0x")) {
	    len = HexStrlen(pt);
	    strncpy(work, pt, len);
	    work[len] = '\0';
            c = strtoul(work,(char **)NULL,0);
	    if (len > 4) {
	        /* 0xNNNNN... => 2�o�C�g�����Ƃ��Ĉ��� */
		if (c > 0xffff) {
		    fprintf(stderr, "Warning: %s is over 0xffff\n", work);
		} else {
		    /* 2�o�C�g�ȓ� */
		    unsigned short short_val = c;
		    unsigned char *uc_val = (unsigned char *)&short_val;
	            putchar((int)(short_val / 256));
	            putchar((int)(short_val & 0xff));
		}
	    } else {
	        /* 0xNN */
	        putchar(c);
	    }
	    ++pt;
	}
    }
}

/*
 *  bin2hex
 *    �o�C�i���t�@�C�����e�L�X�g��HEX�f�[�^�ɕϊ�
 *
 *  IN  : fp : �o�C�i���t�@�C���̃t�@�C���|�C���^
 *
 */
void bin2hex(FILE *fp)
{
    int c;
    int i = 0;
    int  linefirst = 1;

    while ((c = getc(fp)) != EOF) {
        if (i > 0 && i % DATA_PER_LINE == 0) {
            printf(",\n");
            linefirst = 1;
        }
        if (!linefirst) printf(",");
        printf("0x%02x", c);
        linefirst = 0;
        i++;
    }
    printf("\n");
    if (fp == stdin) {
        printf("/* size:%d */\n", i);
    }
}

int main(int a, char *b[])
{
    char *fname = NULL;
    FILE *fp;
    int  vf = 0;
    int  rf = 0;
    int  i;

    /* arg check */
    for (i = 1; i<a; i++) {
	if (!strncmp(b[i],"-v",2)) {
	    vf = 1;
	    continue;
	}
	if (!strcmp(b[i],"-r")) {
	    rf = 1;
	    continue;
	}
	if (!strncmp(b[i],"-h",2)) {
	    printf("usage: hexbin [-r] [filename]\n");
	    printf("       -r : bin to hex\n");
	    exit(1);
	}
	fname = b[i];
    }

    /* open file */
    if (fname == NULL) {
	fp = stdin;
    } else {
	if ((fp = fopen(fname,"rb")) == 0) {
	    perror(fname);
	    exit(1);
	}
    }

    /* ���� */
    if (rf) {
       /* binary => 0xnn, 0xnn, ... */
       if (fp != stdin) {
           struct stat stbuf;
	   char   work[1024];
	   char   *pt;

	   stat(fname, &stbuf);
	   strcpy(work, fname);
	   if (pt = strrchr(fname, '/')) {
	       strcpy(work, ++pt);
	   } else {
	       strcpy(work, fname);
	   }
           printf("/* file:%s  size:%d */\n", work, stbuf.st_size);
       }
       bin2hex(fp);
    } else {
       /* 0xnn, 0xnn, ... => binary */
       hex2bin(fp);
    }

    if (fp != stdin) fclose(fp);
}

/* vim:ts=8:sw=8:
 */
