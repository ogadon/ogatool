#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#if 0
/* ===========================================================================
 * Test read/write of .gz files
 */
void test_gzio(out, in, uncompr, uncomprLen)
    const char *out; /* output file */
    const char *in;  /* input file */
    Byte *uncompr;
    int  uncomprLen;
{
    int err;
    int len = strlen(hello)+1;
    gzFile file;

    file = gzopen(out, "wb");
    if (file == NULL) {
        fprintf(stderr, "gzopen error\n");
        exit(1);
    }

    if (gzwrite(file, (const voidp)hello, (unsigned)len) != len) {
        fprintf(stderr, "gzwrite err: %s\n", gzerror(file, &err));
    }
    gzclose(file);

    file = gzopen(in, "rb");
    if (file == NULL) {
        fprintf(stderr, "gzopen error\n");
    }
    strcpy((char*)uncompr, "garbage");

    uncomprLen = gzread(file, uncompr, (unsigned)uncomprLen);
    if (uncomprLen != len) {
        fprintf(stderr, "gzread err: %s\n", gzerror(file, &err));
    }
    gzclose(file);

    if (strcmp((char*)uncompr, hello)) {
        fprintf(stderr, "bad gzread\n");
    } else {
        printf("gzread(): %s\n", uncompr);
    }
}
#endif /* 0 */

int gzip(char *inf, char *outf)
{
    FILE   *infp;
    gzFile ozfp;
    char   buf[4096];
    int    sz;

    if ((infp = fopen(inf,"rb")) == NULL) {
	printf("fopen %s error\n",inf);
	exit(1);
    }
    if ((ozfp = gzopen(outf,"wb")) == NULL) {
	printf("gzopen %s error\n",outf);
	exit(1);
    }
    while (sz = fread(buf,1,4096,infp)) {
	gzwrite(ozfp, buf, sz);
    }

    gzclose(ozfp);
    fclose(infp);
}

int main(int a, char *b[])
{

    if (a < 3) {
        printf("usage: gz <infile> <file.gz>\n");
        exit(1);
    }

    gzip(b[1],b[2]);
    
}
