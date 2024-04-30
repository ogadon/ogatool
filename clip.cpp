/*
 *   clip.cpp  �w�肳�ꂽ�������N���b�v�{�[�h�ɃR�s�[����B
 *             SendTo�ɓ����ƕ֗�
 *
 *     1999/06/28 V1.00 by oga.
 *     2003/06/02 V1.01 ��������o�O�΍�
 *     2007/01/07 V1.02 support -cyg & -unc & bug fix
 */
#include <windows.h>
#include <stdio.h>

#define ISKANJI(c)  (((unsigned char)(c) >= 0x80 && (unsigned char)(c) < 0xa0) || ((unsigned char)(c) >= 0xe0 && (unsigned char)(c) < 0xff))


int  vf   = 0;	   /* verbose           */
int  cygf = 0;	   /* -cyg      V1.02-A */
int  uncf = 0;	   /* -unc      V1.02-A */

/*
 *   get_onepath()
 * 
 *   IN  pt : ������    ��  abcd\def\hij
 *       buf: 1�̃p�X�v�f���擾����o�b�t�@
 *   OUT buf: pt�̐擪����1�̃p�X�v�f��Ԃ�
 *       ret: �擾�����p�X�v�f�̎��̃p�X�v�f�̐擪
 */
char *get_onepath(char *pt, char *buf)
{
    int i      = 0;
    int kanjif = 0;

    if (*pt == '\0') {
        return NULL;
    }
    while (*pt != '\0' && !(!kanjif && *pt == '\\')) {
        buf[i] = *pt;
	if (kanjif == 0 && ISKANJI(*pt)) {
	    kanjif = 1;
	} else {
	    kanjif = 0;
	}
	++pt;
	++i;
        if (vf >= 2) printf("kf:%d 0x%02x\n", kanjif, *pt);
    }
    buf[i] = '\0';
    if (*pt == '\\') {
        ++pt;		/* \�̎��Ɉړ� */
    }
    return pt;
}

/*
 *   UNC�p�X�ɕϊ�
 *
 *     IN : drv     �h���C�u����
 *     IN : size    uncpath�o�b�t�@�T�C�Y
 *     OUT: uncpath �ϊ����ꂽUNC�p�X
 *     OUT: ret     0:����  -1:���s(uncpath�ɂ̓G���[�v�����i�[)
 */
int GetUNCPath(char *drv, char *uncpath, DWORD *psize)
{
    int  st;

    /* UNC�p�X�擾 */
    st = WNetGetConnection(drv, uncpath, psize);
    if (st != NO_ERROR) {
        /* �G���[�̏ꍇ��uncpath�ɃG���[�v�������� */
        switch(st) {
          case ERROR_BAD_DEVICE:
	    strcpy(uncpath, "BAD_DEVICE");
	    break;
          case ERROR_NOT_CONNECTED:
	    strcpy(uncpath, "NOT_CONNECTED");
	    break;
          case ERROR_CONNECTION_UNAVAIL:
	    strcpy(uncpath, "CONNECTION_UNAVAIL");
	    break;
          case ERROR_NO_NETWORK:
	    strcpy(uncpath, "NO_NETWORK");
	    break;
          case ERROR_NO_NET_OR_BAD_PATH:
	    strcpy(uncpath, "NO_NET_OR_BAD_PATH");
	    break;
          default:
	    sprintf(uncpath, "Error %d", st);
	    break;
        }
	return -1; /* Fail */
    }
    return 0; /* Success */
}

void usage()
{
    printf("usage: clip [-v] [-cyg] [-unc] <str>\n");
    printf("   -cyg: cygwin format path\n");
    printf("   -unc: UNC format path\n");
    exit(1);
}

int main(int a, char *b[])
{
    int  i;
    char *path = NULL;

    for (i = 1; i < a; i++) {
        if (!strcmp(b[i], "-h")) {
	    usage();
	}
        if (!strcmp(b[i], "-cyg")) {  /* V1.02-A */
	    cygf = 1;
	    continue;
	}
        if (!strcmp(b[i], "-unc")) {  /* V1.02-A */
	    uncf = 1;
	    continue;
	}
        if (!strcmp(b[i], "-v")) {
	    ++vf;
	    continue;
	}
	path = b[i];
    }

    if (path == NULL) {
        usage();
    }

    if (OpenClipboard(NULL) == FALSE) {
        printf("OpenClipboard Error.(%d)\n",GetLastError());
	return 1;
    }

    if (EmptyClipboard() == FALSE) {
        printf("EmptyClipboard Error.(%d)\n",GetLastError());
	return 1;
    }

    HGLOBAL hgp;

    /* �O���[�o���̈�擾 */
    if ((hgp = GlobalAlloc(GMEM_MOVEABLE|
                           GMEM_DDESHARE|
			   GMEM_ZEROINIT, 
			   1024)) == NULL) {
        printf("GlobalAlloc Error.(%d)\n",GetLastError());
	return 1;
    }

    char *str;                  /* ClipBoard�o�͗p�p�X */
    char buf[1024];
    char shstr[1024];		/* short path */
    char wkshstr[1024] = "";	/* FindFirstFile�p work */

    /* str�ɃO���[�o���ȗ̈��ݒ� */
    if ((str = (char *)GlobalLock(hgp)) == NULL) {
        printf("GlobalLock Error.(%d)\n",GetLastError());
	return 1;
    }

    strcpy(shstr, path);   /* short path */

    /* Change to Long Path */
    WIN32_FIND_DATA d;
    HANDLE h;
    char *pt = shstr;
    char *newpt;
    char *delm = "\\"; /* V1.02-A */

    /* V1.02-A start */
    if (cygf) {
        delm = "/";
    }
    /* V1.02-A end   */

    while ((newpt = get_onepath(pt, buf)) != NULL) {
        if (strlen(wkshstr)) strcat(wkshstr, "\\"); /* FindFirst�p�Ȃ̂�"\" */
        strcat(wkshstr, buf);
	if (vf) printf("DEBUG1: wkshstr:[%s]\n", wkshstr);

        /* V1.02-A start */
	/* ���o�����p�X�v�f���h���C�u�����̏ꍇ */
        if (strlen(buf) == 2 && buf[1] == ':') {
            if (strlen(str)) strcat(str, delm);

	    if (vf) printf("DEBUG0: drive:[%s]\n", buf);

            /* buf(�h���C�u����)��UNC�ɕϊ� */
            if (uncf) {
	        char uncpath[1024];
		DWORD size = sizeof(uncpath);
		if (GetUNCPath(buf, uncpath, &size) == 0) {
                    /* �h���C�u������UNC�p�X�ɒu�������� */
                    strcpy(buf, uncpath);
		} else {
		    /* �ϊ��G���[ */
		    if (vf) printf("GetUNCPath Error: %s\n", uncpath);
		}
            }

	    if (cygf) {
	        if (uncf) {
                    strcat(str, buf);  /* UNC���R�s�[ */
		} else {
		    /* �h���C�u�����R�s�[ */
	            sprintf(str, "/cygdrive/%s", buf);
		    /* cygwin path�̏ꍇ ":"������ */
		    str[strlen(str)-1] = '\0';
		    /* �h���C�u������������ */
		    str[strlen(str)-1] = tolower(str[strlen(str)-1]);
		}
	    } else {
	        /* �h���C�u������FindFirst����ƌ��݃p�X���Ԃ��Ă��邽�� */
	        /* �h���C�u�����𒼐ڃR�s�[����                          */
                strcat(str, buf);
	    }
	    pt = newpt;
	    continue;
	}
        /* V1.02-A end   */

        /* Explorer�����Short�p�X�œn����Ă��邽��Long�p�X�ϊ����� */
        h = FindFirstFile(wkshstr, &d);
        if (h != INVALID_HANDLE_VALUE) {
            if (strlen(str)) strcat(str, delm);
            strcat(str, d.cFileName);
	    if (vf) printf("DEBUG2: Long:[%s]\n", d.cFileName);
            FindClose(h);
        } else {
	    if (vf) printf("DEBUG3: short:[%s]\n", buf);
            if (strlen(str)) strcat(str, delm);
            strcat(str, buf);
        }
	pt = newpt;
    }

    if (vf) printf("str=[%s]\n", str);

    GlobalUnlock(hgp);

    /* �N���b�v�{�[�h�ɃR�s�[ */
    if (SetClipboardData(CF_TEXT, hgp) == NULL) {
        printf("SetClipboardData Error.(%d)\n",GetLastError());
	return 1;
    }
    if (CloseClipboard() == FALSE) {
        printf("CloseClipboard Error.(%d)\n",GetLastError());
	return 1;
    }
    return 0;
}
