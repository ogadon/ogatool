/*
 *   peke.c : ○×ゲーム  for WWW
 *
 *					96/06/23 V1.00 by oga.
 *					96/06/26 V1.10 VS computer support.
 *
 *        ret : 2...game is over
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
		exit(1);
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
			exit(1);
		}
		if (sf) {
			/* for single player */
			fprintf(fp,"000010000");
		} else {
			fprintf(fp,"000000000");
		}
		fclose(fp);
		if (!strcmp(b[2],"90")) {
			exit(1);
		}
	}

	if ((fp = fopen(b[1],"r")) == 0) {
		perror("fopen2");
		exit(1);
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
		exit(1);
	}
	x = b[2][0] - '0';
	y = b[2][1] - '0';
	if (buf[x+y*3] != '0') {
		if (vf) {
		    printf("そこには置けません!!\n");
		} else {
		    printf("そこには置けません!!\n");
		}
		fclose(fp);
		disp_boad(buf,endf);
		exit(1);
	}

	/* どちらの番かチェックし、コマを置く */ 
	buf[x+y*3] = check_ban(buf);

	if (check_win(buf)) {
		/* game is over */
		unlink(b[1]);
		endf = 1;
	}

	if (!endf && sf && strcmp(b[2],"91")) {
		/* コンピュータの番 (status mode(91) でない場合) */
		com_think(buf);

		if (check_win(buf)) {
			/* game is over */
			unlink(b[1]);
			endf = 1;
		}
	}
	disp_boad(buf,endf);

	if (!(fp = fopen(b[1],"w"))) {
		perror("fopen3");
		exit(1);
	}
	fprintf(fp,"%s",buf);
	fclose(fp);

	if (endf) {
	    /* game over */
	    return 2;
        }
	return 0;
}

/*   check_ban() : どちらの番かチェックする
 *
 *   ret : '1' ○の番
 *         '2' ×の番
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
		return '1';	/* ○の番 */
	} else {
		return '2';	/* ×の番 */
	}
}

/*   disp_boad() : 板の表示
 *
 *   endf : 空白地帯のリンクを行なわない
 */
void disp_boad(buf,endf)
char *buf;
int endf;
{
	int i;
	for (i=0; i<9; i++) {
		switch(buf[i]) {
		    case '0':
		    	if (vf) {
		    	    printf("□");
		    	} else {
		    	    if (endf) {
		   	        printf("<img src=%c/gif/waku.gif%c>",'"','"');
		    	    } else {
		   	        printf("<a href=%c/cgi-bin/%d%d.cgi%c>",'"',i % 3, i/3,'"');
		   	        printf("<img src=%c/gif/waku.gif%c",'"','"');
		   	        printf(" border=%c0%c>",'"','"');
		   	        printf("</a>");
		    	    }
		        }
		    	break;
		    case '1':
		    	if (vf)
		    	    printf("○");
		    	else
		   	    printf("<img src=%c/gif/maru.gif%c>",'"','"');
		    	break;
		    case '2':
		    	if (vf)
		    	    printf("×");
		    	else
		   	    printf("<img src=%c/gif/batsu.gif%c>",'"','"');
		    	break;
		}
		if ((i % 3) == 2) {
		    	if (vf)
			    printf("\n");
		    	else
		   	    printf("<br>\n");
		}
	}
	
}

/*   check_win() : 勝ったかどうかのチェック
 *
 *   ret  '1' or '2' : ○ or × の勝ち
 *               -1  : 引分け
 *                0  : 勝敗未決
 *
 */
int check_win(buf)
char *buf;
{
	int x,y;
	for (x=0;x<3;x++) {
		if (buf[x] != '0' && buf[x] == buf[x+3] && buf[x] == buf[x+6]) {
		    if (vf)
			printf("%s の勝ちです。\n",(buf[x]=='1')? "○" : "×");
		    else
			printf("<h1>%s の勝ちです。</h1>",(buf[x]=='1')? "○" : "×");
		    return buf[x];
		}
	}
	for (y=0;y<3;y++) {
		if (buf[y*3] != '0' && buf[y*3] == buf[y*3+1] && buf[y*3] == buf[y*3+2]) {
		    if (vf)
			printf("%s の勝ちです。\n",(buf[y*3]=='1')? "○" : "×");
		    else
			printf("<h1>%s の勝ちです。</h1>\n",(buf[y*3]=='1')? "○" : "×");
		    return buf[y];
		}
	}
	if (buf[0] != '0' && buf[0] == buf[4] && buf[0] == buf[8]) {
	    if (vf)
	        printf("%s の勝ちです。\n",(buf[0]=='1')? "○" : "×");
	    else
	        printf("<h1>%s の勝ちです。</h1>\n",(buf[0]=='1')? "○" : "×");
	    return buf[0];
	}
	if (buf[2] != '0' && buf[2] == buf[4] && buf[2] == buf[6]) {
	    if (vf)
		printf("%s の勝ちです。\n",(buf[2]=='1')? "○" : "×");
	    else
		printf("<h1>%s の勝ちです。</h1>\n",(buf[2]=='1')? "○" : "×");
	    return buf[2];
	}
	if (!strchr(buf,'0')) {
	    if (vf)
		printf("引分けです。\n");
	    else
		printf("<h1>引分けです。</h1>\n");
	    return -1;
	}
	return 0;
}

void com_think(buf)
char *buf;
{
	int i,num;

	if (check_mate(buf)) {
		return;
	}
	num = ck_num(buf);
	switch (num) {
	    case 2:
	        if (buf[1]=='0') {
		    buf[3] = '1';
		} else if (buf[3]=='0') {
		    buf[7] = '1';
		} else if (buf[5]=='0') {
		    buf[1] = '1';
		} else if (buf[7]=='0') {
		    buf[5] = '1';
		} else {
		    put_rnd(buf);
		}
	        break;
	    case 4:
	        if (buf[1]=='1') {
		    buf[2] = '1';
		} else if (buf[3]=='1') {
		    buf[0] = '1';
		} else if (buf[5]=='1') {
		    buf[7] = '1';
		} else if (buf[7]=='1') {
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
 *   check_mate() : 勝てる場合、そこにコマをおいてリターン
 * 
 *   ret : 0 no put  
 *         1 put (win!)
 */
int check_mate(buf)
char *buf;
{
	int code = 0;
	int i,num=0;

	for (i = 0; i<3; i++) {
	    /* ×がなくて空白が1個? */
	    /* 横チェック */
	    if ((buf[i*3+0] != '2' && buf[i*3+1] != '2' && buf[i*3+2] != '2') &&
		          (buf[i*3+0]+buf[i*3+1]+buf[i*3+2] == '1' * 2 + '0')) {
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
	    /* 縦チェック */
	    if ((buf[i+0] != '2' && buf[i+3] != '2' && buf[i+6] != '2') &&
		          (buf[i+0]+buf[i+3]+buf[i+6] == '1' * 2 + '0')) {
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
        /* 斜めチェック1 */
	if ((buf[0] != '2' && buf[4] != '2' && buf[8] != '2') &&
		          (buf[0]+buf[4]+buf[8] == '1' * 2 + '0')) {
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
        /* 斜めチェック2 */
	if ((buf[2] != '2' && buf[4] != '2' && buf[6] != '2') &&
		          (buf[0]+buf[4]+buf[8] == '1' * 2 + '0')) {
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
 *   put_rnd() : ランダムにコマを置く
 * 
 */
void put_rnd(buf)
char *buf;
{
	int rnd;
	int i;

	srand(time(0));         /* 乱数系列初期化 */
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
