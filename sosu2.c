/*
 *   sosu2.c : 1～10000までの素数を求める (改造用)
 *
 *     97/08/27 V1.01  time support
 *     97/09/04 V1.02  usage changed.
 *     98/11/03 V1.03  -flush support
 *     99/01/14 V1.04  display S'Mark99 
 *     99/02/11 V1.05  add new algorithm
 *     02/07/27 V1.06  add save memory (-4)
 *     04/05/19 V1.06b fix usage
 *     07/09/29 V1.07  disp sosu count
 *     13/12/12 V1.07b disp malloc size
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LIM	10000
#define VER	"1.07b"

int vf=0,ff=0;
int sosu_cnt = 0;   /* V1.07-A */

/* 
 *  sosu vup 4
 * 
 *    エラトステネスのふるい型 (bit配列)
 * 
 *    <素数の最大値> / 8byte のメモリを消費します
 * 
 *    メモリ節約型(大量の素数を検索する場合)
 * 
 *
 */
int sosu4(int lim)
{
    int  i,j;
    int  counter = 0;
    char *nsosu;   /* Vup4 */

    fprintf(stderr,"sosu4: preparing... memsize(%dKB)\n", lim*sizeof(char)/8/1024);

    nsosu = (char *)malloc(lim * sizeof(char)/8); /* Vup4 */
    if (!nsosu) {
       fprintf(stderr,"sosu4: malloc() error!\n");
       return 0;
    }

    memset(nsosu, 0xff, lim * sizeof(char)/8);	/* 0xffで埋める     */

    fprintf(stderr,"sosu4: Start!!...\n");

    for (i=2; i<=lim; i++) {
	/* ビットが0なら素数候補でない */
        if (!(nsosu[i/8] & (0x80 >> (i%8)))) continue;

	/* 素数表示            */
	if (vf) printf("%d\n",i);
	++sosu_cnt;   /* V1.07-A */

        for (j=i; j<=lim; j+=i) {
            ++counter;
            nsosu[j/8] &= ~(0x80 >> (j%8)); /* iの倍数を消去していく */
        }
    }

    return counter;
}


/* 
 *  sosu vup 3
 * 
 *    エラトステネスのふるい型 (char配列)
 * 
 *    <素数の最大値>byte のメモリを消費します
 * 
 *    FMV/K6/233
 *        S'Mark  128      => 1000
 *
 *                  sosu0        sosu2    sosu3
 *           10,000  0.78 sec =>  0.01 =>  0.01
 *          100,000 59.34 sec =>  0.19 =>  0.06
 *        1,000,000           =>  3.22 =>  1.94
 *       10,000,000           => 42.41 => 24.74
 *
 */
int sosu3(int lim)
{
    int  i,j;
    int  counter = 0;
    char *nsosu;   /* Vup3 */

    fprintf(stderr,"sosu3: preparing... memsize(%dKB)\n", lim*sizeof(char)/1024);

    nsosu = (char *)malloc(lim * sizeof(char)); /* Vup3 */
    if (!nsosu) {
       fprintf(stderr,"sosu3: malloc() error!\n");
       return 0;
    }

    memset(nsosu, 1, lim * sizeof(char));	/* 1で埋める     */

    fprintf(stderr,"sosu3: Start!!...\n");

    for (i=2; i<=lim; i++) {
        if (!nsosu[i]) continue;	/* 0なら素数候補でない   */

	if (vf) printf("%d\n",i);	/* 素数表示              */
	++sosu_cnt;   /* V1.07-A */

        for (j=i; j<=lim; j+=i) {
            ++counter;
            nsosu[j] = 0;		/* iの倍数を消去していく */
        }
    }

    return counter;
}

/* 
 *  sosu vup 2
 * 
 *    エラトステネスのふるい型 (int配列)
 * 
 *    <素数の最大値> * 4 byte のメモリを消費します
 * 
 *    FMV/K6/233
 *        S'Mark  128      => 10000
 *
 *                  sosu0        sosu2
 *           10,000  0.78 sec =>  0.01
 *          100,000 59.34 sec =>  0.17
 *        1,000,000           =>  2.81
 *       10,000,000           => 43.59
 *
 */
int sosu2(int lim)
{
    int  i,j;
    int  counter = 0;
    int  *nsosu;

    fprintf(stderr,"sosu2: preparing... memsize(%dKB)\n", lim*sizeof(int)/1024);

    nsosu = (int *)malloc(lim * sizeof(int));
    if (!nsosu) {
       fprintf(stderr,"sosu2: malloc() error!\n");
       return 0;
    }

    memset(nsosu, 1, lim * sizeof(int));	/* 1で埋める     */

    fprintf(stderr,"sosu2: Start!!...\n");

    for (i=2; i<=lim; i++) {
        if (!nsosu[i]) continue;	/* 0なら素数候補でない   */

	if (vf) printf("%d\n",i);	/* 素数表示              */
	++sosu_cnt;   /* V1.07-A */

        for (j=i; j<=lim; j+=i) {
            ++counter;
            nsosu[j] = 0;		/* iの倍数を消去していく */
        }
    }

    return counter;
}

/* 
 *  sosu vup 1
 * 
 *    sosu0から無駄なループを減らしてみました
 * 
 *    FMV/K6/233  0.78 sec => 0.54 sec
 *        S'Mark  128      => 185
 */
int sosu1(int lim)
{
	int i,x;
	int counter = 0;

        if (vf) printf("%d\n",2);	/* sosu1 */
	for (x=2; x<=lim; x++) {	/* sosu1 */
	    for (i=3; i<x; i+=2) {	/* sosu1 */
		counter++;
		if((x % i) == 0) {
	            /* 割り切れたら素数ではない */
		    break;
		}
	    }
	    /* iがxになるまで割り切れなかった */
	    if (x == i) {
	        /* 素数発見 */
	        if (vf) printf("%d\n",x);
	        ++sosu_cnt;   /* V1.07-A */
	    }
	}
	return counter;
}

/* 
 *  original sosu 
 */
int sosu0(int lim)
{
	int i,x;
	int counter = 0;

	for (x=2; x<=lim; x++) {
	    for (i=2; i<x; i++) {
		counter++;
		if((x % i) == 0) {
		    break;
		}
	    }
	    if (x == i) {
	        if (vf) printf("%d\n",x);
	        ++sosu_cnt;   /* V1.07-A */
	    }
	}
	return counter;
}

main(a,b)
int a;
char *b[];
{
	int	i;
	int	st_tm, en_tm;
	int	lim = LIM;
	int	type = 0;
	unsigned long counter=0;
	int 	(*fsosu[10])();

	fprintf(stderr,"sosu V%s by oga.\n",VER);
	for (i=1; i<a; i++) {
	    if (!strncmp(b[i],"-h",2)) {
		printf("usage: sosu [-v] [num<%d>] [-<type>]\n",LIM);
		printf("       type : 0 Normal\n");
		printf("              1 TwinCam\n");
		printf("              2 Turbo\n");
		printf("              3 TwinCam Turbo\n");
		printf("              4 TwinCam Turbo Eco (Save Memory)\n");
		exit(1);
            }
	    if (!strncmp(b[i],"-v",2)) {
		vf = 1;
		continue;
            }
	    if (!strcmp(b[i],"-flush")) {
		ff = 1;
		continue;
            }
	    if (!strncmp(b[i],"-",1)) {
		type = atoi(&b[i][1]);
		continue;
            }
	    lim = atoi(b[i]);
	}

	fsosu[0] = (int (*)())sosu0;	/* normal type */
	fsosu[1] = (int (*)())sosu1;	/* powerup 1   */
	fsosu[2] = (int (*)())sosu2;	/* powerup 2   */
	fsosu[3] = (int (*)())sosu3;	/* powerup 3   */
	fsosu[4] = (int (*)())sosu4;    /* many nums   */

	/* 
	 *  main  start
	 */
	fprintf(stderr,"Sosu Start(%d) type(%d)...\n",lim, type);

	st_tm = clock();
	counter = (*(fsosu[type]))(lim);	/* Do Sosu */
	en_tm = clock();

	if (counter == 0) {
	    printf("no sosu.\n");
	    return 0;                   /* error or nsosu=0 */
	}

	fprintf(stderr,"\nMAX %d: CALC=%lu NSOSU=%d TIME=%3.3f sec\n",
				lim,
				counter,
				sosu_cnt,   /* V1.07-A */
				(float)(en_tm-st_tm)/CLOCKS_PER_SEC);

	if (lim == 10000) {
	    fprintf(stderr,"\nS'Mark99 => %d\n\n", (en_tm-st_tm)?
	        (100*CLOCKS_PER_SEC)/(en_tm-st_tm): -1);
	}

	return 0;
}

/*
 *  性能測定結果 
 *
 *    FLORA210/MMXPen233/64MB  (Linux)
 * 
 *        S'Mark   65      => 10000
 *
 *                                   -2       -3       -4
 *                                  int     char      bit
 *                   sosu0        sosu2    sosu3    sosu4
 *           10,000   1.53 sec =>  0.01 =>  0.00 =>  0.00
 *          100,000 120.26 sec =>  0.05 =>  0.03 =>  0.03
 *        1,000,000            =>  0.55 =>  0.35 =>  0.51
 *       10,000,000            =>  7.19 =>  4.37 => 10.92
 *      100,000,000            =>       =>       =>143.01
 *
 *
 *    IBM-PC 330-P75/K6-200/175MHz/128MB  (Linux)
 * 
 *        S'Mark  103      => 10000
 *
 *                   sosu0        sosu2    sosu3    sosu4
 *           10,000   0.97 sec =>  0.01 =>  0.00 =>  0.00
 *          100,000  76.39 sec =>  0.19 =>  0.12 =>  0.03
 *        1,000,000            =>  2.35 =>  1.56 =>  0.98
 *       10,000,000            => 26.12 => 18.01 => 12.53
 *      100,000,000            =>       =>206.53 =>149.37
 *
 *
 *    ILIOS /K6-2/366MHz/256MB  (Linux)
 * 
 *        S'Mark  217      => 10000
 *
 *                   sosu0        sosu2    sosu3    sosu4
 *           10,000   0.46 sec =>  0.00 =>  0.00 =>  0.00
 *          100,000  36.61 sec =>  0.11 =>  0.06 =>  0.01
 *        1,000,000            =>  1.48 =>  0.92 =>  0.44
 *       10,000,000            => 17.07 => 11.69 =>  6.15
 *      100,000,000            =>       =>136.67 => 79.03
 *    1,000,000,000            =>       =>       =>969.21
 *
 *
 *    Dell /PentiumIII/1GHz/512MB  (Win2000)
 * 
 *        S'Mark  454      => 10000
 *
 *                   sosu0        sosu2    sosu3    sosu4
 *           10,000   0.220sec =>  0.000=>  0.000=>  0.000
 *          100,000  18.317sec =>  0.020=>  0.010=>  0.000
 *        1,000,000            =>  0.380=>  0.220=>  0.040
 *       10,000,000            =>  4.386=>  3.014=>  1.922
 *      100,000,000            =>       => 34.499=> 23.884
 *    1,000,000,000            =>       =>       =>284.829
 *
 *
 *    ASUS A7S333/AthlonXP2000+/1.67GHz/512MB  (Win2000)
 * 
 *        S'Mark  641      => 10000
 *
 *                   sosu0        sosu2    sosu3    sosu4
 *           10,000   0.156sec =>  0.000=>  0.000=>  0.000
 *          100,000  12.656sec =>  0.015=>  0.000=>  0.000
 *        1,000,000            =>  0.218=>  0.125=>  0.031
 *       10,000,000            =>  2.563=>  1.796=>  1.140
 *      100,000,000            =>       => 21.843=> 14.390
 *    1,000,000,000            =>       =>       =>182.421
 *
 */

/* vim:ts=8:sw=8
 */

