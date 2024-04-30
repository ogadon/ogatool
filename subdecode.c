/*
 *  subdecode : SubjectのISO-2022をデコードする
 *
 *      V1.01 98/11/19  ShiftINがある場合はShiftINをつけない
 *
 *  CFLAG : INTEL ... Intel byte order
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define ISO2022	"=?ISO-2022-JP?B?"
#define iso2022	"=?iso-2022-jp?B?"

char kin[]  ={0x1b,0x24,0x42,0};      /* kanji mode in  */
char kout[] ={0x1b,0x28,0x42,0};      /* kanji mode out */

int vf = 0;

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

/* すでにJISのShift IN ESCシーケンスが存在するか? */
char *IsEscExist(buf)
char *buf;
{
    char SIN[16];
    sprintf(SIN, "%c%c%c",27,'$','B');
    return strstr(buf,SIN);
}

/* 文字列がJISか? */
int IsJIS(buf)
char *buf;
{
    int i;
    int st = 1;

    for (i = 0; i<strlen(buf); i++) {
	if (buf[i] & 0x80) {
	    st = 0;	/* SJIS/EUCを含む */
	    break;
	}
    }
    if (vf) printf("%s:[%s]\n", st?"JIS ":"SJIS", buf);
    return st;
}

void BinDisp(num,bit)
{
	unsigned int mask;
	int i;

	printf("[%3d]:",num);
	mask = (1 << (bit-1));
	for (i = 0; i<bit; i++) {
	    if (num & mask) {
		printf("1");
	    } else {
		printf("0");
	    }
	    mask >>= 1;
	}
	printf("\n");
}

void DecodeBase64(istr,ostr)
unsigned char *istr, *ostr;
{
	int wk4;
	int i;
	unsigned char *wk4s = (unsigned char *)&wk4;
	unsigned char *osv = ostr;

	if (vf) printf("Decode64 IN [%s]\n", istr);

	while (*istr != '\0' && *istr != '\n' ) {
	    wk4 = 0;
	    for (i = 0; i<4; i++) {
	        wk4 <<= 6;
	        if (*istr == '\0' || *istr == '\n') {
	            break;
	        }
#ifdef DEBUG
	        printf("[%c]",*istr);
	        BinDisp(tr[*istr],8);
#endif
	        wk4 |= tr[*istr];
	        ++istr;
	    }
#ifdef DEBUG
	    BinDisp(wk4,24);
#endif
#ifdef INTEL
	    ostr[0] = wk4s[2];
	    ostr[1] = wk4s[1];
	    ostr[2] = wk4s[0];
#else
	    memcpy(ostr,&wk4s[1],3);
#endif
	    ostr += 3;
	    if (*istr == '?' && *(istr+1) == '=') {
	        /* "?=" is terminate */
	        *istr += 2;
	        continue;
	    }
	}
	*ostr = '\0';
	if (vf) printf("Decode64 OUT[%s]\n", osv);

}

/*
 *   ISO2022...が含まれていたら、JISコードに変換する
 *
 *   IN   in : 	"...=?ISO-2022-JP?B?<str>?="
 *   OUT  out: 	<SI>JISコード<SO>
 *        ret: 	char *outを返す
 *
 */
char *chg_iso2022(in, out)
char *in,*out;
{
    char wk[4096];
    int  pt = 0;
    int  i;

    if (vf) printf("IN [%s]\n",in);
    if (strstr(in,ISO2022) || strstr(in, iso2022)) {
	i = 0;
	while (strncasecmp(&in[i],ISO2022,strlen(ISO2022))) {
	    out[pt++] = in[i++];
	}
	i += strlen(ISO2022);
	/* printf("decode start = [%s]\n",&in[i]); */
	DecodeBase64(&in[i],wk);
	if (IsEscExist(wk)) {
	    sprintf(&out[pt],"%s",wk);
	} else {
	    sprintf(&out[pt],"%c%c%c%s%c%c%c",27,'$','B',wk,27,'(','B');
	}
    } else {
        strcpy(out,in);
    }
    if (vf) printf("OUT[%s]\n",out);
    return out;
}

/*
 *  j2s(in,out);
 *
 *  IN  : in  JISコード(2バイト)
 *  OUT : out SJISコード(2バイト)
 *
 */
void j2s(unsigned char *in, unsigned char *out)
{
    if (in[0] % 2) {
        out[0] = in[0]/2 + 0x71;
        out[1] = in[1] + 0x1f;
        if (out[1] > 0x7f) {  /* or >= */
            out[1]++;
        }
    } else {
        out[0] = in[0]/2 + 0x70;
        out[1] = in[1] + 0x7e;
    }
#if 0
    printf("IN 0x%04x / OUT 0x%04x\n",htons(*(unsigned short *)in),
    				      htons(*(unsigned short *)out));
#endif
}


/*
 *   unsigned char *jis2sjis((u_char *)buf, (u_char *)obuf, int* sisof)
 *
 *   kanji IN/OUT付き
 *
 *   IN  : buf   変換元文字列(SJIS)
 *         siso  現在のSI/SOの状態     1:SI 0:SO
 *   OUT : obuf  変換後文字列(JIS)
 *         ret   obufのアドレス
 */

unsigned char *jis2sjis(buf,obuf,siso)
unsigned char *buf, *obuf;
int *siso;
{
    int ip=0, op=0;
    if (vf) printf("IN =[%s]\n",buf);
    for (ip = 0; ip<strlen(buf); ) {
        if (*siso) {
            /* Under Shift In */
            if (!memcmp(&buf[ip],kin,3)) {
                ip += 3;
                continue;
            }
            if (!memcmp(&buf[ip],kout,3)) {
                /* Shift Out */
                if (vf != vf) printf("Shift OUT!!\n");
                *siso = 0;
                ip += 3;
            } else {
                j2s(&buf[ip],&obuf[op]);
                ip += 2;
                op += 2;
            }
        } else {
            /* Under Shift Out */
            if (!memcmp(&buf[ip],kout,3)) {
                ip += 3;
                continue;
            }
            if (!memcmp(&buf[ip],kin,3)) {
                /* Shift In */
                if (vf != vf) printf("Shift IN!!\n");
                *siso = 1;
                ip += 3;
            } else {
                obuf[op] = buf[ip]; 
                ++ip;
                ++op;
            }
        }
    }
    obuf[op] = '\0';
    if (vf) printf("OUT=[%s]\n",obuf);
    return obuf;
}

main(a,b)
int a;
char *b[];
{

	FILE	*fp;
	int	i, siso = 0;
	char	buf[2048], wk[2048];

	if (a == 1) {
	    fp = stdin;
	} else {
	    if (!(fp = fopen(b[1],"r"))) {
		perror("fopen");
		exit(1);
	    }
	}

        if (a > 2 && !strcmp(b[2], "-v")) {
	    vf = 1;
	}

	while (fgets(buf,sizeof(buf),fp)) {
#if 0
	    printf("%s\n",chg_iso2022(buf,wk));
#else
	    chg_iso2022(buf,wk);
	    printf("%s\n",jis2sjis(wk,buf,&siso));
#endif
	}
}
