/*
 *   テキスト文書中のURLをリンク形式に変換する
 *
 *                                       Copyright (C)1997 M.Oga
 *
 *      usage : htlink [filename]
 *              filename指定がない場合、標準入力から読み取る
 *
 *      97/04/29 Ver1.00 by oga.
 *      99/12/03 Ver1.01 support "ftp://"
 *      11/08/20 Ver1.02 support DOS path "C:\xxx"
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int vf = 0;

main(a,b)
int a;
char *b[];
{
	FILE *fp;
	char buf[4096],outbuf[4096];
	char wk[4096], wk2[4096];
	char url[4096];
	unsigned char *pt,*pt2,*pto,*pte, *pt3;
	unsigned char *pt4;
	int len;


	if (a < 2) {
	    fp = stdin;
	} else {
	    if (!strncmp(b[1],"-h",2)) {
		printf("usage : htlink [filename]\n");
		exit(1);
	    }
	    if ((fp = fopen(b[1],"r")) == 0) {
		perror(b[0]);
		exit(1);
	    }
	}

	while (fgets(buf,sizeof(buf),fp)) {
	    pt  = buf;				    /* pt : inbuf           */
	    pto = outbuf;			    /* pto: outbuf          */
	    outbuf[0] = '\0';
	    while ((pt2 = (char *)strstr(pt,"http://"))||  /*pt2: "http:" ptr */
		   (pt3 = (char *)strstr(pt,"ftp://")) ||  /*pt3: "ftp:" ptr  */
		   (pt4 = (char *)strstr(pt,":\\"))        /*pt4: "X:\" ptr   */
		   ) {  /*pt2: "ftp:"  ptr */
		/* http: or ftp:の前までコピー */
		if (pt2) {
		    len = (unsigned int)pt2 - (unsigned int)pt;
		} else if(pt3) {
		    len = (unsigned int)pt3 - (unsigned int)pt;
		} else if (pt4) {
		    --pt4;
		    len = (unsigned int)pt4 - (unsigned int)pt;
		}
		/* "http:" の手前までコピー */
		strncpy(pto,pt,len);		    /* pto: outbuf          */
		pto[len] = '\0';

		/* pt を "http:"の先頭へ          */
		pt  += len;
		/* pto をコピーした文字列の末尾へ */
		pto += len;

		/* "http://.../"の終わりをサーチ */
		pte = pt;		/* pte: "http://.../ " last pointer */
		/* while (!isspace(*pte) */
		while (*pte != ' ' && *pte != '\r' && *pte != '\n' 
		       && *pte != '\0' /* && *pte < 128 */
		       && *pte != '"' && *pte != '(' && *pte != ')'
		       && *pte != '\t' && *pte != ',' 
		       && !(*pte >= ';' && *pte <= '>' && *pte != '=')
		       && !(*pte >= '[' && *pte <= '^' && *pte != '\\')
		       && *pte != '}' && *pte != '{'  ) {
		     ++pte;
		}
		if (vf) printf("end char = [%c]\n", *pte);
		len = (unsigned int)pte - (unsigned int)pt;
		strncpy(wk,pt,len);	/* copy "http://.../" */
		wk[len] = '\0';

		if (vf) printf("wk: [%s]\n", wk);

		if (pt4) {
		    /* C:\xxx */
		    sprintf(url, "file://%s", wk);
		} else {
		    /* http://xxx, ftp://xxx */
		    strcpy(url, wk);
		}
		sprintf(wk2,"<a href=\"%s\">%s</a>",url,wk);
		strcat(pto,wk2);
		pt += len;
		pto += strlen(wk2);
		if (*pt == '\0') {
		    break;
		}
	    }
	    strcat(pto,pt);

	    if (outbuf[strlen(outbuf)-1] != 0x0a) {
		strcat(outbuf,"\n");
	    }
	    printf("%s",outbuf);
	}
}

/* vim:ts=8:sw=8:
 */


