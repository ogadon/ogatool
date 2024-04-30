/*
 *  enews.c
 *
 *    電光掲示板風に横にスクロール表示する
 *
 *    2003/12/05 V1.00 by oga.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/* globals */
unsigned char news_buf[10000] = "";
int  cur    = 0;    /* news_buf current pointer */
int  wtime  = 50;   /* msec                     */
int  ncol   = 80;   /* 画面の横サイズ           */
int  vf     = 0;    /* -v (verbose)             */

/* defines */
#define dprintf if (vf) printf
#define ISKANJI(c) ((c >= 0x80 && c < 0xa0) || (c >= 0xe0 && c<0xff))

/*
 *  AddToBuf
 *    news_bufにデータを追加
 *
 */
void AddToBuf(char *buf)
{
    unsigned char work[4096];

    dprintf("### AddToBuf start\n");

    /* 初めての場合、スペースを埋める */
    if (strlen(news_buf) == 0) {
        memset(news_buf, ' ', ncol-1);
    }

    if (strlen(buf) == 0) {
        /* 空行 */
        strcat(news_buf, "           ");  /* 空行は長めのSpaceとする */
        return;
    }

    if (cur) {
        if (strlen(&news_buf[cur]) == 0) {
	    strcpy(news_buf, "");
	} else {
            strcpy(work, &news_buf[cur]);
            strcpy(news_buf, work);
	}
    }
    cur = 0;
    strcat(news_buf, buf);
    strcat(news_buf, "  ");

    dprintf("### AddToBuf end [%s]\n", news_buf);
    //sleep(3);
}

/*
 *  DelIncompKanji
 *    bufの最後がSJIS 1バイト目の文字で終わっている場合、
 *    スペースに置き換える
 *
 *    ついでにタブをスペースに変換する
 *
 *  IN  buf チェックする文字列
 */
void DelIncompKanji(unsigned char *buf)
{
    int kanjif = 0; /* SJIS check flag          */
    int i = 0;

    while (buf[i] != '\0') {
	if (kanjif == 0 && ISKANJI(buf[i])) {
	    kanjif = 1;  /* SJIS first byte */
	} else {
	    kanjif = 0;
	}
	if (buf[i] == '\t') {
	    buf[i] = ' ';
	}
        ++i;
    }
    if (kanjif && i > 0) {
        buf[i-1] = ' ';
    }
}

/*
 *  DispNews
 *    news_bufの内容を横スクロールして出力
 *
 *    IN  mode : 0 ... 1行分のデータがなくなったところで終了
 *               1 ... news_bufが空になるまで出力
 */
void DispNews(int mode)
{
    unsigned char work[1024];

    dprintf("### DispNews start mode:%d\n", mode);

    if (getenv("COLUMNS")) {
        ncol = atoi(getenv("COLUMNS"));
    }

    if (ncol) --ncol;

    while ( (mode == 0 && strlen(&news_buf[cur]) > ncol) ||
            (mode == 1 && strlen(&news_buf[cur]) > 0   ) ) {
        memset(work, 0, sizeof(work));
	strncpy(work, &news_buf[cur], ncol);
	DelIncompKanji(work);
        printf("%cM%s\n", 27, work);
	fflush(stdout);
	usleep(wtime*1000);

	if (ISKANJI(news_buf[cur])) {
	    /* SJIS先頭1バイトの場合、2バイト分進める */
	    usleep(wtime*1000);
	    ++cur;
	}
	++cur;
    }

    dprintf("### DispNews end\n");
}

int main(int a, char *b[])
{
    char *fname = NULL;
    FILE *fp;
    int  i;
    char buf[8192];

    /* arg check */
    for (i = 1; i<a; i++) {
	if (!strncmp(b[i],"-v",2)) {
	    vf = 1;
	    continue;
	}
	if (i+1 < a && !strncmp(b[i],"-t",2)) {
	    wtime = atoi(b[++i]);
	    continue;
	}
	if (!strncmp(b[i],"-h",2)) {
	    printf("usage: enews [filename] [-t <wait_time(50ms)>\n");
	    exit(1);
	}
	fname = b[i];
    }

    /* open file */
    if (fname == NULL) {
	fp = stdin;
    } else {
	if ((fp = fopen(fname,"r")) == 0) {
	    perror(fname);
	    exit(1);
	}
    }

    printf("\n");

    /* 処理 */
    while (fgets(buf, sizeof(buf), fp)) {
        if (strlen(buf) > 0 && buf[strlen(buf)-1] == 0x0a) {
            buf[strlen(buf)-1] = '\0';
	}
        if (strlen(buf) > 0 && buf[strlen(buf)-1] == 0x0d) {
            buf[strlen(buf)-1] = '\0';
	}
        AddToBuf(buf);
	DispNews(0);
    }
    DispNews(1);  /* 残りを出力 */

    if (fp != stdin) fclose(fp);

}

/* vim:ts=8:sw=8:
 */
