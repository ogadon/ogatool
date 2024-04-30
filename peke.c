/*
 *   peke.c : ���~�Q�[��  for WWW
 *
 *			96/06/23 V1.00 by oga.
 *			96/06/27 V1.10 VS computer support.
 *			96/07/05 V1.11 add ALT= for lynx
 *			96/07/05 V1.12 �����������̂�2��Ԃ��s�ǂ��C��
 *
 *        ret : -1...���s����
 *        ret :  0...���s����
 *               1...���̏���
 *               2...�~�̏���
 *               3...��������
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int	check_ban();
void	disp_boad();
int	check_win();
void	com_think();
int	ck_num();
int	check_mate();
void	put_rnd();

int vf=0;

main(a,b)
int a;
char *b[];
{
	FILE *fp;
	struct stat st;
	char buf[1024];
	int koma;
	int i, x, y;
	int endf=0;
	int sf = 0;

	if (a < 3) {
		printf("usage : peke <filename> <locate> [-s] [-v]\n");
		printf("        locate : [0-2][0-2]>\n");
		printf("                 90 is reset.\n");
		printf("                 91 is display current status.\n");
		printf("            -s : single player mode\n");
		printf("            -v : cancel URL output\n");
		exit(-1);
	}
	if (a > 3) {
		for (i = 3; i< a; i++) {
			if (!strcmp(b[i],"-v")) {
				vf = 1;
			}
			if (!strcmp(b[i],"-s")) {
				sf = 1;
			}
		}
	}
	if (stat(b[1],&st) || !strcmp(b[2],"90")) {
		if (!(fp = fopen(b[1],"w"))) {
			perror("fopen1");
			exit(-1);
		}
		if (sf) {
			/* for single player */
			fprintf(fp,"000010000");
		} else {
			fprintf(fp,"000000000");
		}
		fclose(fp);
		if (!strcmp(b[2],"90")) {
			exit(0);
		}
	}

	if ((fp = fopen(b[1],"r")) == 0) {
		perror("fopen2");
		exit(-1);
	}
	fgets(buf,sizeof(buf),fp);
	fclose(fp);

	if (!strcmp(b[2],"91")) {
		/* display current board */
		disp_boad(buf,endf);
		exit(0);
	}

	if (strlen(buf) != 9) {
		printf("Invalid file type for peke!\n");
		fclose(fp);
		exit(-1);
	}
	x = b[2][0] - '0';
	y = b[2][1] - '0';
	if (buf[x+y*3] != '0') {
		if (vf) {
		    printf("�����ɂ͒u���܂���!!\n");
		} else {
		    printf("�����ɂ͒u���܂���!!\n");
		}
		fclose(fp);
		disp_boad(buf,endf);
		exit(-1);
	}

	/* �ǂ���̔Ԃ��`�F�b�N���A�R�}��u�� */ 
	buf[x+y*3] = check_ban(buf);

	if (endf = check_win(buf)) {
		/* game is over */
		unlink(b[1]);
	}

	/* �R���s���[�^�ΐ탂�[�h�̏ꍇ */
	if (!endf && sf && strcmp(b[2],"91")) {
		/* �R���s���[�^�̔� (status mode(91) �łȂ��ꍇ) */
		com_think(buf);

		if (endf = check_win(buf)) {
			/* game is over */
			unlink(b[1]);
		}
	}
	disp_boad(buf,endf);

	if (!(fp = fopen(b[1],"w"))) {
		perror("fopen3");
		exit(-1);
	}
	fprintf(fp,"%s",buf);
	fclose(fp);

	if (endf) {
	    if (endf < 0) endf = 3;  /* �������� */
	    /* game over */
	    if (endf == '1') endf = 1;
	    if (endf == '2') endf = 2;
	    return endf;
        }
	return 0;	/* ���s���� */
}

/*   check_ban() : �ǂ���̔Ԃ��`�F�b�N����
 *
 *   ret : '1' ���̔�
 *         '2' �~�̔�
 */
int check_ban(buf)
char *buf;
{
	int i,maru=0,peke=0;

	for (i = 0; i<9; i++) {
		if (buf[i] == '1') {
			maru++;
		}
		if (buf[i] == '2') {
			peke++;
		}
	}
	if (maru == peke) {
		return '1';	/* ���̔� */
	} else {
		return '2';	/* �~�̔� */
	}
}

/*   disp_boad() : �̕\��
 *
 *   endf : 0�ȊO�̏ꍇ�A�󔒒n�т̃����N���s�Ȃ�Ȃ�
 */
void disp_boad(buf,endf)
char *buf;
int endf;
{
	int i;

	printf("<html>\n");
	for (i=0; i<9; i++) {
		switch(buf[i]) {
		    case '0':
		    	if (vf) {
		    	    printf("��");
		    	} else {
		    	    if (endf) {
		   	        printf("<img src=%c/gif/waku.gif%c",'"','"');
		   	        printf(" alt=%c��%c>",'"','"');
		    	    } else {
		   	        printf("<a href=%c/cgi-bin/%d%d.cgi%c>",'"',i % 3, i/3,'"');
		   	        printf("<img src=%c/gif/waku.gif%c",'"','"');
		   	        printf(" border=%c0%c",'"','"');
		   	        printf(" alt=%c��%c>",'"','"');
		   	        printf("</a>");
		    	    }
		        }
		    	break;
		    case '1':
		    	if (vf)
		    	    printf("��");
		    	else
		   	    printf("<img src=%c/gif/maru.gif%c",'"','"');
		   	    printf(" alt=%c��%c>",'"','"');
		    	break;
		    case '2':
		    	if (vf)
		    	    printf("�~");
		    	else
		   	    printf("<img src=%c/gif/batsu.gif%c",'"','"');
		   	    printf(" alt=%c�~%c>",'"','"');
		    	break;
		}
		if ((i % 3) == 2) {
		    	if (vf)
			    printf("\n");
		    	else
		   	    printf("<br>\n");
		}
	}
	printf("</html>\n");
	
}

/*   check_win() : ���������ǂ����̃`�F�b�N
 *
 *   ret  '1' or '2' : �� or �~ �̏���
 *               -1  : ������
 *                0  : ���s����
 *
 */
int check_win(buf)
char *buf;
{
	int x,y;
	/* �c�`�F�b�N */
	for (x=0;x<3;x++) {
		if (buf[x] != '0' && buf[x] == buf[x+3] && buf[x] == buf[x+6]) {
		    if (vf)
			printf("%s �̏����ł��B\n",(buf[x]=='1')? "��" : "�~");
		    else
			printf("<h1>%s �̏����ł��B</h1>",(buf[x]=='1')? "��" : "�~");
		    return buf[x];
		}
	}
	/* ���`�F�b�N */
	for (y=0;y<3;y++) {
		if (buf[y*3] != '0' && buf[y*3] == buf[y*3+1] && buf[y*3] == buf[y*3+2]) {
		    if (vf)
			printf("%s �̏����ł��B\n",(buf[y*3]=='1')? "��" : "�~");
		    else
			printf("<h1>%s �̏����ł��B</h1>\n",(buf[y*3]=='1')? "��" : "�~");
		    return buf[y*3];	/* V1.12 */
		}
	}
	/* �΂߃`�F�b�N */
	if (buf[0] != '0' && buf[0] == buf[4] && buf[0] == buf[8]) {
	    if (vf)
	        printf("%s �̏����ł��B\n",(buf[0]=='1')? "��" : "�~");
	    else
	        printf("<h1>%s �̏����ł��B</h1>\n",(buf[0]=='1')? "��" : "�~");
	    return buf[0];
	}
	if (buf[2] != '0' && buf[2] == buf[4] && buf[2] == buf[6]) {
	    if (vf)
		printf("%s �̏����ł��B\n",(buf[2]=='1')? "��" : "�~");
	    else
		printf("<h1>%s �̏����ł��B</h1>\n",(buf[2]=='1')? "��" : "�~");
	    return buf[2];
	}
	if (!strchr(buf,'0')) {
	    if (vf)
		printf("�������ł��B\n");
	    else
		printf("<h1>�������ł��B</h1>\n");
	    return -1;
	}
	return 0;
}

/* 
 *  com_think() : �R���s���[�^(��)���v�l���[�`��	(C) oga.
 *
 */
void com_think(buf)
char *buf;
{
	int i,num;

	/* �������Ă�ꍇ�A�����ɒu�� */
	if (check_mate(buf,'1')) {
		/* ������! */
		return;
	}

	/* �~�����������ȏꍇ�A�j�~���� */
	if (check_mate(buf,'2')) {
		/* �j�~����! */
		return;
	}

	num = ck_num(buf);
	switch (num) {
	    case 2:
	        if (buf[1]=='2') {
		    buf[3] = '1';
		} else if (buf[3]=='2') {
		    buf[7] = '1';
		} else if (buf[5]=='2') {
		    buf[1] = '1';
		} else if (buf[7]=='2') {
		    buf[5] = '1';
		} else {
		    put_rnd(buf);
		}
	        break;
	    case 4:
	        if (buf[1]=='1' && buf[2]=='0') {
		    buf[2] = '1';
		} else if (buf[3]=='1' && buf[0]=='0') {
		    buf[0] = '1';
		} else if (buf[5]=='1' && buf[8]=='0') {
		    buf[8] = '1';
		} else if (buf[7]=='1' && buf[6]=='0') {
		    buf[6] = '1';
		} else {
		    put_rnd(buf);
		}
	        break;
	    default :
		put_rnd(buf);
	}
}

int ck_num(buf)
char *buf;
{
	int i,num=0;
	for (i = 0; i<9; i++) {
		if (buf[i] != '0') num++;
	}
	return num;
}

/* 
 *   check_mate() : ���Ă�ꍇ�A�����ɃR�}�������ă��^�[��
 *                  ����̏�����j�~����ꍇ�ɂ��g��
 * 
 *   koma : �����Ɏw�肵���R�}�����Ă�ꍇ�A�����Ɂ��R�}�������ă��^�[��
 *   ret  : 0 no put  
 *          1 put (win!)
 */
int check_mate(buf,koma)
char *buf;
char koma;
{
	int code = 0;
	int i,num=0;
	char rev;

	if ( koma == '1' ) {
		rev = '2';
	} else {
		rev = '1';
	}

	for (i = 0; i<3; i++) {
	    /* �~���Ȃ��ċ󔒂�1��? */
	    /* ���`�F�b�N */
	    if ((buf[i*3+0] != rev && buf[i*3+1] != rev && buf[i*3+2] != rev) &&
		          (buf[i*3+0]+buf[i*3+1]+buf[i*3+2] == koma* 2 + '0')) {
		if (buf[i*3+0] == '0') {
		    buf[i*3+0] = '1';		/* put chip for win! */
		    return 1;
		}
		if (buf[i*3+1] == '0') {
		    buf[i*3+1] = '1';		/* put chip for win! */
		    return 1;
		}
		if (buf[i*3+2] == '0') {
		    buf[i*3+2] = '1';		/* put chip for win! */
		    return 1;
		}
	    }
	    /* �c�`�F�b�N */
	    if ((buf[i+0] != rev && buf[i+3] != rev && buf[i+6] != rev) &&
		          (buf[i+0]+buf[i+3]+buf[i+6] == koma * 2 + '0')) {
		if (buf[i+0] == '0') {
		    buf[i+0] = '1';		/* put chip for win! */
		    return 1;
		}
		if (buf[i+3] == '0') {
		    buf[i+3] = '1';		/* put chip for win! */
		    return 1;
		}
		if (buf[i+6] == '0') {
		    buf[i+6] = '1';		/* put chip for win! */
		    return 1;
		}
	    }
	}
        /* �΂߃`�F�b�N1 */
	if ((buf[0] != rev && buf[4] != rev && buf[8] != rev) &&
		          (buf[0]+buf[4]+buf[8] == koma * 2 + '0')) {
	    if (buf[0] == '0') {
		buf[0] = '1';		/* put chip for win! */
		return 1;
	    }
	    if (buf[4] == '0') {
		buf[4] = '1';		/* put chip for win! */
		return 1;
	    }
	    if (buf[8] == '0') {
		buf[8] = '1';		/* put chip for win! */
		return 1;
	    }
	}
        /* �΂߃`�F�b�N2 */
	if ((buf[2] != rev && buf[4] != rev && buf[6] != rev) &&
		          (buf[2]+buf[4]+buf[6] == koma * 2 + '0')) {
	    if (buf[2] == '0') {
		buf[2] = '1';		/* put chip for win! */
		return 1;
	    }
	    if (buf[4] == '0') {
		buf[4] = '1';		/* put chip for win! */
		return 1;
	    }
	    if (buf[6] == '0') {
		buf[6] = '1';		/* put chip for win! */
		return 1;
	    }
	}

        /* no put */
	return 0;
}

/* 
 *   put_rnd() : �����_����"��"�R�}��u��
 * 
 */
void put_rnd(buf)
char *buf;
{
	int rnd;
	int i;

	srand(time(0));         /* �����n�񏉊��� */
	rnd = rand();
	rnd = (int)((float)rnd/RAND_MAX*10)+1;
	while(1) {
	    for (i = 0; i<9; i++) {
		if (buf[i] == '0') {
		    if (--rnd < 1) {
			buf[i] = '1';
			return;
		    }
		}
	    }
	}
}
