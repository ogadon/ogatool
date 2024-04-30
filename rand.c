/*
 *   generate random value for shell script
 *
 *      06/03/05 V0.10 by oga.
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

void usage()
{
    printf("usage: rand [-s] [-p] [<max_rand_val(10)>]\n");
    printf("   -s           : silent\n");
    printf("   max_rand_val : if 10, return val is 0 - 9\n");
    printf("   -p           : if 10, return val is 1 - 10\n");
    exit(1);
}

int main(int a, char *b[])
{
    struct timeval tv;
    int    max_val = 10;
    int    val     = 0;
    int    sf      = 0;   /* -s */
    int    pf      = 0;   /* -p */
    int    i;


    for (i = 1; i<a; i++) {
        if (!strcmp(b[i],"-s")) {
            sf = 1;
            continue;
        }
        if (!strcmp(b[i],"-p")) {
            pf = 1;
            continue;
        }
        if (!strcmp(b[i],"-h")) {
            usage();
            continue;
        }
        max_val = atoi(b[i]);
    }

    if (max_val == 0) {
        max_val = 10;
    }

    gettimeofday(&tv, 0);
    srand(tv.tv_usec);

    val = (int)(((float)rand()*max_val)/RAND_MAX);

    if (pf) ++val;  /* change val 0- to 1- */

    if (sf == 0) printf("%d\n", val);

    return val;
}


