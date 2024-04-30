/*
 *  expr2
 *    expr‚ÌŠ„‚èZŒ‹‰Ê‚¾‚¯¬”“_‘Î‰”Å
 *
 *    2002/06/02 V0.10 by oga.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void usage()
{
    printf("usage: expr2 <val1> <op> <val2>\n");
    exit(1);
}

int main(int a, char *b[])
{
    int   val1, val2, result;
    float resultf = 0;

    if (a < 4) {
        usage();
    }
    val1 = atoi(b[1]);
    val2 = atoi(b[3]);
    switch (b[2][0]) {
      case '+':
        result = val1 + val2;
	break;
      case '-':
        result = val1 - val2;
	break;
      case '*':
      case 'x':
        result = val1 * val2;
	break;
      case '/':
        result = val1 / val2;
	if (result * val2 != val1) {
	    resultf = (float)val1/(float)val2;
	}
	break;
      default:
        usage();
	break;
    }
    if (resultf) {
        printf("%.5f\n", resultf);
    } else {
        printf("%d\n", result);
    }
    return 0;
}

