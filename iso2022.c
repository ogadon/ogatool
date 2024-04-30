#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ISO2022	"=?ISO-2022-JP?B?"
#define iso2022	"=?iso-2022-jp?B?"

char kin[]  ={0x1b,0x24,0x42,0};      /* kanji mode in   ^[(B */
char kout[] ={0x1b,0x28,0x42,0};      /* kanji mode out  ^{)B */
char kout2[] ={0x1b,0x28,0x4a,0};     /* kanji mode out2 ^[)J */

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


int vf = 0;

int IsBigEndian()
{
    int n = 1;
    char *pn = (char *)&n;

    if (pn[3] == 1) return 1; /* big endian                */
    return 0;                 /* little endian (eg. INTEL) */
}

/*
 *  delcrlf(str)
 *
 *  Delete CR/LF
 *
 *  IN  str : string with CR/LF
 *  OUT str : string with no CR/LF
 */
void delcrlf(char *str)
{
    int i;

    for (i = 0; i<strlen(str); i++) {
        if (str[i] == 0x0a || str[i] == 0x0d) {
            str[i] = ' ';
        }
    }
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
    for (ip = 0; ip<strlen(buf); ) {
        if (*siso) {
            /* Under Shift In */
            if (!memcmp(&buf[ip],kin,3)) {
                ip += 3;
                continue;
            }
            if (!memcmp(&buf[ip],kout,3) || !memcmp(&buf[ip],kout2,3)) {
                /* Shift Out */
                if (vf == vf) printf("Shift OUT!!\n");
                *siso = 0;
                ip += 3;
            } else {
                j2s(&buf[ip],&obuf[op]);
                ip += 2;
                op += 2;
            }
        } else {
            /* Under Shift Out */
            if (!memcmp(&buf[ip],kout,3 || !memcmp(&buf[ip],kout2,3))) {
                ip += 3;
                continue;
            }
            if (!memcmp(&buf[ip],kin,3)) {
                /* Shift In */
                if (vf == vf) printf("Shift IN!!\n");
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
    return obuf;
}

/*
 *   Base64のデコード
 *
 *   IN  istr : base64 string
 *   OUT ostr : decoded string
 *       ret  : decoded string
 */

unsigned char *DecodeBase64(istr,ostr)
unsigned char *istr, *ostr;
{
	int wk4;
	int i;
	unsigned char *wk4s = (unsigned char *)&wk4;

	while (*istr != '\0' && *istr != '\n' ) {
	    wk4 = 0;
	    for (i = 0; i<4; i++) {
	        wk4 <<= 6;
	        if (*istr == '\0' || *istr == '\n') {
	            break;
	        }
	        wk4 |= tr[*istr];
	        ++istr;
	    }

            if (IsBigEndian()) {
	        memcpy(ostr,&wk4s[1],3);
            } else {
	        ostr[0] = wk4s[2];
	        ostr[1] = wk4s[1];
	        ostr[2] = wk4s[0];
            }

	    ostr += 3;
	    printf("[%c]",*istr);
	    if (*istr == '?' && *(istr+1) == '=') {
	        /* "?=" is terminate */
	        istr += 2;	/* skip "?=" */
	        break;
	    }
	}
	*ostr = '\0';

	return istr;
}

/* 
 *  すでにJISのShift IN ESCシーケンスが存在するか? 
 */
int IsEscExist(buf)
char *buf;
{
    char SIN[16];
    sprintf(SIN, "%c%c%c",27,'$','B');
    return strstr(buf,SIN)?1:0 ;
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
    char *pp;
    int  pt = 0;
    int  i;

    if (vf != vf) printf("BEFORE:[%s]\n",in);

    if (strstr(in,ISO2022) || strstr(in,iso2022)) {
	i = 0;
	while (strncasecmp(&in[i],ISO2022,strlen(ISO2022))) {
	    out[pt++] = in[i++];
	}
	i += strlen(ISO2022);
	/* printf("decode start = [%s]\n",&in[i]); */
	pp = DecodeBase64(&in[i],wk);
	if (IsEscExist(wk)) {
	    sprintf(&out[pt],"%s\n",wk);
	} else {
	    sprintf(&out[pt],"%c%c%c%s%c%c%c\n",27,'$','B',wk,27,'(','B');
	}
	if (pp) strcat(out, pp);  /* 残りをコピー */
    } else {
        strcpy(out,in);
    }
    if (vf != vf) printf("AFTER :[%s]\n",out);
    return out;
}

int main(int a, char *b[])
{

    char buf[4096];
    char rbuf[4096];
    char wk[4096];
    int  siso = 0;
    int  i;
    int  ff = 0;

    for (i = 1; i<a; i++) {
        if (!strcmp(b[i],"-f")) {
            ff = 1;
            continue;
        }
    }

    if (ff) {
	FILE *fp;

	if (!(fp = fopen(b[2],"r"))) {
		perror("fopen");
		exit(1);
	}
	while (fgets(rbuf,sizeof(rbuf),fp)) {
            printf("%s\n=>%s\n",rbuf,chg_iso2022(rbuf, buf));
	}
	fclose(fp);
    } else {
        printf("%s\n=>%s\n",b[1],chg_iso2022(b[1], buf));
        printf("[%s]\n", jis2sjis(buf,wk,&siso));
        for (i = 0; i<strlen(wk); i++) {
            printf("%02x ",(unsigned char)wk[i]);
        }
        printf("\n");
    }
    return 0;
}
