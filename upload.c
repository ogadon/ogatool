/*
 *   upload.c
 *
 *     �t�@�C���A�b�v���[�h�̃T���v��CGI
 *
 * <�g�p��>
 * upload.html
 * ------------------
 * <form enctype="multipart/form-data" method="POST" action="http://galaga/cgi-bin/upload">
 *   <input type="file" NAME="file_name">
 *   <input type="submit" value="OK">
 * </form>
 * ------------------
 *
 */
#include <stdio.h>
#include <string.h>

/* �A�b�v���[�h��̃p�X */
#define UPLOAD_PATH "/tmp/oga/upload"
/* #define UPLOAD_PATH "." */

int vf = 0;  /* verbose */

void DelKai(char *buf)
{
    if (buf[strlen(buf)-1] == 0x0a) {
        buf[strlen(buf)-1] = '\0';
    }
    if (buf[strlen(buf)-1] == 0x0d) {
        buf[strlen(buf)-1] = '\0';
    }
}

/*
 *   �p�X�̃t�@�C�������������o��
 *   IN : ibuf  �f�B���N�g�����t���t�@�C���p�X
 *   OUT: obuf  �t�@�C����
 *
 */
void Basename(char *ibuf, char *obuf)
{
    char *pt;

    // ��
    pt = strrchr(ibuf, '/');
    if (pt) {
	strcpy(obuf, ++pt);
    } else {
        pt = strrchr(ibuf, '\\');
        if (pt) {
	    strcpy(obuf, ++pt);
	} else {
	    strcpy(obuf, ibuf);
	}
    }
}

/* 
 *  GetFileBlock
 *
 *    �t�@�C�������o���o�͂���
 *
 *    IN  : filen �o�̓t�@�C����
 *    IN  : sep   �u���b�N�����Z�p���[�^
 *    OUT : ret :  0 success
 *              : -1 error
 */
int GetFileBlock(char *filen, char *sep)
{
    FILE *fp;
    char path[1025];
    char buf[1025];
    int  c, c2, c3;

    if (filen[strlen(filen)-1] == '"') {
        filen[strlen(filen)-1] = '\0';
    }

    if (strlen(UPLOAD_PATH)) {
        sprintf(path, "%s/%s", UPLOAD_PATH, filen);
    }

    if ((fp = fopen(path, "wb")) == NULL) {
        return -1;
    }

    if (vf) printf("output file %s ...\n", filen);

    while (1) {
        c = getchar();
	if (c == EOF) break;
	if (c == 0x0d) {
            c2 = getchar();
	    if (c2 == EOF) break;
	    if (c2 == 0x0a) {
                if (vf) printf("expect 0d0a\n", filen);
                c3 = getchar();
	        if (c3 == EOF) break;
                if (c3 == '-') {
	            fgets(buf, sizeof(buf), stdin);
                    if (vf) printf("expect 0d0a-[%s]\n", buf);
                    if (vf) printf("            [%s]\n", &sep[1]);
		    if (!strncmp(buf, &sep[1], strlen(&sep[1]))) {
		        /* "[0d][0a]----------xxxxxx" �Ȃ�΃u���b�N�I�� */
                        if (vf) printf("expect separator.\n", filen, buf);
		        break;
		    }
	        } else {
	            putc(c, fp);
	            putc(c2, fp);
	            putc(c3, fp);
		    continue;
	        }
	    } else {
	        putc(c, fp);
	        putc(c2, fp);
		continue;
	    }
	}
	putc(c, fp);
    }
    
    fclose(fp);
    return 0;
}

int main(int a, char *b[])
{

    char buf[4096];
    char separator[4096];
    char *pt;
    char filen[1025];
    int  filef = 0;

    printf("Content-type: text/html\n");
    printf("\n");
    printf("<html>\n");
    printf("<head><title>upload</title></head>\n");
    printf("<body>\n");

    printf("<pre>\n");   /* for DEBUG */

    /* get separator */
    fgets(separator, sizeof(separator), stdin);
    DelKai(separator);

    while (1) {
        /* get Content Info 1 */
        if (!fgets(buf, sizeof(buf), stdin)) {
	    break;
	}
        DelKai(buf);
	filef = 0;
	if (!strncasecmp(buf, "Content-Disposition:", strlen("Content-Disposition:"))
	    /* �t�@�C�����w�� */
	    && (pt = strstr(buf, "filename="))) {
	    /* filename */
	    pt += (strlen("filename=") + 1);
	    Basename(pt, filen);  /* �t�@�C�����������擾 */
	    filef = 1;
	}

        /* get Content info Info (other) */
	/* ���s�݂̂̍s������܂ŋ�ǂ� */
	while (1) {
	    if (strlen(buf) == 0) {
	        break;
	    }
            fgets(buf, sizeof(buf), stdin);
            DelKai(buf);
	}

        if (filef && strlen(filen)) {
	    /* �t�@�C�����o�� & �o�� */
	    GetFileBlock(filen, separator);
	} else {
	    /* �e�L�X�g���o�� & �o�� */
	}

    }

    printf("</pre>\n");   /* for DEBUG */

    printf("<h1><font color=\"ff0000\">�A�b�v���[�h����</font></h1>\n");
    printf("<p><a href=\"javascript:history.back()\">�߂�</a>\n");
    printf("</body>\n");
    printf("</html>\n");
}

