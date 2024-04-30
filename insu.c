#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#define RAND(x) (((rand() & 0xffff) * x)/0x10000)
#define MAXQ  5

// insu bunkai
//   input <val1> <val2>

int main(int a, char *b[])
{
	int a1, a2;
	int aa1, aa2;
	int i;
	int start, end;

	srand(time(0));

	start = time(0);

	for (i = 0; i < MAXQ; i++) {
		a1 = RAND(18) - 9;
		if (a1 >= 0) ++a1;
		a2 = RAND(18) - 9;
		if (a2 >= 0) ++a2;
		//printf("a1 = %2d  a2 = %2d\n", a1, a2);
		printf("x^2 %s %dx %s %d\n", 
				(a1+a2)>0?"+":"-", abs(a1+a2),
				(a1*a2)>0?"+":"-", abs(a1*a2));
		//getchar();
		while (1) {
			printf("input <a> <b>: ");
			scanf("%d %d", &aa1, &aa2);
			//printf("  %d %d\n", aa1, aa2);
			if ((a1 == aa1 && a2 == aa2) || (a1 == aa2 && a2 == aa1)) {
				printf("Bingo!!\n");
				break;
			} else {
				printf("Booo\n");
			}
		}
		printf("  Ans. (x %s %d)(x %s %d)\n\n",
				(a1)>0?"+":"-", abs(a1),
				(a2)>0?"+":"-", abs(a2));
		getchar();
	}
	end = time(0);
	printf("time = %d sec.\n", end - start);
}

