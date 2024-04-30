/*
 *  Excel CSV�t�@�C���̃Z�������s���폜����
 *
 *    2003/05/28 V1.00 by oga.
 *    2007/03/28 V1.01 support -c, -p option
 *
 */
#include <stdio.h>
#include <stdlib.h>
int main(int a, char *b[])
{
    char *fname = NULL;
    FILE *fp;
    int  i;
    int  c;
    int  vf = 0;
    int  pf = 0;
    int  dbl_f;
    char sepchar = 0;

    /* arg check */
    for (i = 1; i<a; i++) {
	if (!strncmp(b[i],"-v",2)) {
	    vf = 1;
	    continue;
	}
	if (!strncmp(b[i],"-p",2)) {
	    pf = 1;
	    continue;
	}
	if (i+1 < a && !strcmp(b[i],"-c")) {
            i++;
            if (isdigit(b[i][0])) {
                sepchar = atoi(b[i]); /* �����������ꍇ�̓R�[�h�Ƃ݂Ȃ� */
            } else {
                sepchar = b[i][0];    /* �����ȊO�������ꍇ�͂��̕������� */
            }
            if (vf) printf("sepchar: 0x%02x\n", sepchar);
	    continue;
	}
	if (!strncmp(b[i],"-h",2)) {
	    printf("usage: csv2nocr [-c <chr_code>] [-p] [filename]\n");
	    printf("       -c : change CR to specific code\n");
	    printf("       -p : change , to .\n");
	    exit(1);
	}
	fname = b[i];
    }

    /* open file */
    if (fname == NULL) {
	fp = stdin;
    } else {
	if ((fp = fopen(fname,"r")) == 0) {
	    perror(b[0]);
	    exit(1);
	}
    }

    dbl_f = 0;
    while ((c = getc(fp)) != EOF) {
        if (dbl_f && (c == 0x0d || c == 0x0a)) {
	    /* �_�u���N�H�[�g���̉��s�͍폜���� */
	    /* V1.01-A start */
	    if (sepchar) {
	        putchar(sepchar);  /* ���s��-c�Ŏw�肵���R�[�h�ɕϊ� */
	    }
	    /* V1.01-A end   */
	    continue;
	}
        /* V1.01-A start */
        if (dbl_f && pf && (c == ',')) {
	    /* -p�w��Ȃ�΁A�_�u���N�H�[�g����,��.�ɕϊ����� */
	    putchar('.');
	    continue;
	}
        /* V1.01-A start */

	if (c == '"') {
	    /* �t���O�̃X�C�b�` */
	    dbl_f = 1 - dbl_f;
	}
	putchar(c);
    }

    if (fp != stdin) fclose(fp);
}

/* vim:ts=8:sw=4:
 */
