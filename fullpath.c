/*
 *  �w�肵���p�X�̃t���p�X�����߂�
 *
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

main(int a, char *b[])
{
    char *pathp = NULL;  /* ���ʂ̃t�@�C���������ւ̃|�C���^���i�[����� */
    char buf[1024];	 /* ���S�p�X�i�[�̈� */
    int  ret;

    if (a < 2) {
        printf("usage: fullpath <filename>\n");
	exit(1);
    }

    ret = GetFullPathName(b[1], sizeof(buf), buf, &pathp);
    if (ret == 0) {
        printf("GetFullPathName() failed. code=%d\n", GetLastError());
    }
    printf("fullpath=[%s], basename=[%s]\n",buf, pathp);
    printf("buftop=0x%08x  filetop=0x%08x %s\n",buf, pathp, b[1]);
}
