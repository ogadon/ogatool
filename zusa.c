/*
 *  zusa.c
 *    2ch zusaaaa...
 *
 *    2002/04/21 by oga.
 */
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

int main(int a, char *b[])
{

  int i, j;
  char buf[256];
  char work[256];
  char *get[6] = {
    "¡‚¾I%d”ÔƒQƒbƒgƒHƒH««II",
    "PPPPPÉPPP@@@@@@@(LL",
    "@@@ È È@@@@@@ @@@(LÜ(L",
    "@@¼(ß„Dß¼ÜM‚Âßßß(LÜ;;;ßßß",
    "@@@@@@ PP@ (LÜ(LÜ;;",
    "@@@@@@½Þ»Þ°°°°°¯"
  };

  printf("\n\n\n\n\n\n\n");
  for (j = 10; j>=0; j--) {
    printf("%cM%cM%cM%cM%cM%cM", 27,27,27,27,27,27);
    for (i = 0; i<6; i++) {
      memset(buf, 0x20, sizeof(buf));
      strcpy(&buf[j], get[i]);
      strcat(buf, " ");
	  if (i == 0) {
	      sprintf(work, buf, getpid());
          printf("%s\n", work);
	  } else {
          printf("%s\n", buf);
	  }
    }
    usleep(100000);
  }
}
