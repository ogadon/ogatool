/*
 * loto6 simurator
 *
 *   2001/01/15  V0.10 by oga
 *   2001/01/15  V0.11 by oga
 */

#include <stdlib.h>

/* 0`(x-1)ŠÔ‚Å‚Ì—” */
#define RAND(x)     	((rand() & 0xffff)*(x)/(RAND_MAX & 0xffff))

#include <stdio.h>
#include <string.h>

int first = 1;

void DispNumber(char *comment, char *hit, char *mynum, unsigned int buy)
{
    int i;

    if (first) {
        printf("‚ ‚½‚è”Ô†        (B#) / ƒ}[ƒN‚µ‚½”Ô†     : “™          ”ƒ‚Á‚½”\n");
        printf("--------------------------------------------------------------------\n");
        first = 0;
    }

    for (i=0; i<6; i++) {
        printf("%2d ", hit[i]);
    }
    printf("(%2d) / ", hit[6]);  /* bonus number */
    for (i=0; i<6; i++) {
        printf("%2d ", mynum[i]);
    }
    printf(" : %s  (buy=%u)\n", comment, buy);
}

void DispHitCount(int *hitcnt, unsigned int buy)
{
    int i, total;

    for (i=0; i<5; i++) {
        printf("# %d. %10d  (%2.6f%%) ", 
                i+1, 
                hitcnt[i], 
                ((float)hitcnt[i])/buy*100);
	switch (i) {
	  case 0:
	    printf("%9d–œ‰~\n", hitcnt[i]*10000);
	    break;
	  case 1:
	    printf("%9d–œ‰~\n", hitcnt[i]* 1500);
	    break;
	  case 2:
	    printf("%9d–œ‰~\n", hitcnt[i]*   50);
	    break;
	  case 3:
	    if (hitcnt[i] < 100) {
	        printf("%9dç‰~\n", (hitcnt[i]*95)/10);
	    } else {
	        printf("%9d–œ‰~\n", (hitcnt[i]*95)/100);
	    }
	    break;
	  case 4:
	    if (hitcnt[i] < 100) {
	        printf("%9dç‰~\n", hitcnt[i]);
	    } else {
	        printf("%9d–œ‰~\n", hitcnt[i]/10);
	    }
	    break;
	  default:
	    printf("\n");
	    break;
	}
    }
    /* unit:man */
    total = hitcnt[0]*10000 +
            hitcnt[1]* 1500 +
            hitcnt[2]*   50 +
            (hitcnt[3]*95)/100 +
            hitcnt[4]/10;

    if (total < 100) {
        /* unit:sen */
        total = hitcnt[0]*100000 +
                hitcnt[1]* 15000 +
                hitcnt[2]*   500 +
                (hitcnt[3]*95)/10 +
                hitcnt[4];
        printf("# buy=%u (%dç‰~)   total income %dç‰~\n", buy, buy/5, total);
    } else {
        printf("# buy=%u (%d–œ‰~)   total income %d–œ‰~\n", buy, buy/50, total);
    }
}

int main(int a, char *b[])
{

    char hit[7];
    char mynum[6];
    int  hitcnt[5];
    int  i, j;
    unsigned int buy = 0;
    unsigned int maxbuy = 0xffffffff;
    int  hazure = 0;
    int  match  = 0;
    int  retry;
    int  vf     = 0;   /* -vf verbose count */

    memset(hitcnt, 0, sizeof(hitcnt));

    for (i = 1; i<a; i++) {
        if (!strcmp(b[i], "-h")) {
            printf("usage: loto6 [<num_of_buy>] [-v [-v ...]]\n");
            exit(1);
        }
        if (!strcmp(b[i], "-v")) {
            ++vf;
            continue;
        }
        maxbuy = atoi(b[i]);
        vf = 99;   /* specify num of card */
    }

    srand(time(0));
    /* make hit number */
    for (i=0; i<7; i++) {
        retry = 1;
        while(retry) {
            retry = 0;
            hit[i] = RAND(43)+1;
            for (j=0; j<i; j++) {
                if (hit[i] == hit[j]) {
                    retry = 1;
                    break;
                }
            }
        }
        /* printf("num%d = %d\n",i, hit[i]); */
    }

    while(buy < maxbuy) {
        /* buy 1unit */
        for (i=0; i<6; i++) {
            retry = 1;
            while(retry) {
                retry = 0;
                mynum[i] = RAND(43)+1;
                for (j=0; j<i; j++) {
                    if (mynum[i] == mynum[j]) {
                        retry = 1;
                        break;
                    }
                }
            }
        }

        ++buy;

        /* check hit */
        match = 0;
        for (i=0; i<6; i++) {
            for (j=0; j<6; j++) {
                if (mynum[i] == hit[j]) {
                    ++match;
                    break;
                } else {
                    hazure = mynum[i];  /* for 2nd prize */
                }
            }
        }

        if (match == 6) {
            printf("š1st prize!\n");
            ++hitcnt[0];
            DispNumber("1st prize!", hit, mynum, buy);
            DispHitCount(hitcnt, buy);
        } else if (match == 5) {
            if (hazure == hit[6]) {
                printf("š2nd prize!\n");
                ++hitcnt[1];
                DispNumber("2nd prize!", hit, mynum, buy);
                DispHitCount(hitcnt, buy);
            } else {
                ++hitcnt[2];
                if (vf >= 1) {
                    DispNumber("3rd prize!", hit, mynum, buy);
	        }
            }
        } else if (match == 4) {
            ++hitcnt[3];
            if (vf >= 2) {
                DispNumber("4th prize!", hit, mynum, buy);
            }
        } else if (match == 3) {
            ++hitcnt[4];
            if (vf >= 3) {
                DispNumber("5th prize!", hit, mynum, buy);
            }
        }
    }
    if (vf == 99) {
        if (hitcnt[0]+hitcnt[1]+hitcnt[2]+hitcnt[3]+hitcnt[4] == 0) {
            printf("\n‘S•”ƒnƒYƒŒ‚Ü‚µ‚½(^^;\n");
        }
        printf("\n# ======== RESULT =========\n");
        DispHitCount(hitcnt, buy);
    }


    return 0;
}
