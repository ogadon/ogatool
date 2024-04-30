/*
 *  hscp.c : 同一デバイス高速コピー 
 *    
 *    2004/10/20 V0.10 by oga.
 */
#include<stdio.h>
#include<stdlib.h>
#include<sys/stat.h>
#include<string.h>
#ifndef _WIN32
#include<unistd.h>
#endif  /* _WIN32 */


#ifdef _WIN32
#define PDELM	"\\"
#define PDELMC	'\\'
#define S_ISDIR(m)  ((m) & S_IFDIR)
#else
#define PDELM	"/"
#define PDELMC	'/'
#endif

#define MEM_SIZE (20*1024*1024)


int main(int a, char *b[])
{
    FILE   *fp1;          /* for src_file    */
    FILE   *fp2;          /* for dest_file   */
    struct stat stbuf;
    int    size;          /* read  size      */
    int    wsize;         /* write size      */
    int    pcent;         /* copy percent    */
    int    total;         /* total copy size */
    int    all_size = 0;  /* file size       */
    char   *buf;
    char   strbar[100];
    char   *bar = "##################################################                                                  ";
    char   *pt;
    char   src_file[1025];
    char   dest_file[1025];
    char   fname[1025];   /* file basename   */
    int    mem_size = MEM_SIZE;

#ifdef _WIN32
    char   *cur_up = "";
#else  /* UNIX */
    char   *cur_up = "\033M";
#endif /* _WIN32 */

    if (a != 3) {
	//printf("usage : hscp [-b <buff_size(20MB)>] <src_file> <dest_file>\n");
	printf("usage : hscp <src_file> <dest_file>\n");
	exit(1);
    }

    strcpy(src_file,  b[1]);
    strcpy(dest_file, b[2]);

    /* dest_file is directory? */
    if (stat(dest_file, &stbuf) == 0) {
        if (S_ISDIR(stbuf.st_mode)) {
	    if (strchr(src_file, PDELMC)) {
	        /* exist path delm */
	        pt = strrchr(&src_file[strlen(src_file)-1], PDELMC);
		++pt;
		strcpy(fname, pt);
	    } else {
	        /* filename only */
		strcpy(fname, src_file);
	    }
	    strcat(dest_file, PDELM);
	    strcat(dest_file, fname);
	}
    }

    if ( (fp1 = fopen(src_file,"rb")) == 0) {
	perror(src_file);
	exit(1);
    }
    if ( (fp2 = fopen(dest_file,"wb")) == 0) {
	perror(dest_file);
	exit(1);
    }

    if (stat(src_file, &stbuf) == 0) {
        all_size = stbuf.st_size;    /* src_file size */
    }

    if (all_size < mem_size) {
        mem_size = all_size;
    }

    buf = (char *)malloc(mem_size);
    if (buf == NULL) {
        perror("hscp: malloc");
	exit(1);
    }

    total = 0;
    while (size = fread(buf, 1, mem_size, fp1)) { 
        wsize = fwrite(buf, 1, size, fp2);
	if (wsize != size) {
            fprintf(stderr, "Write Incomplete %d/%d\n", wsize, size);
	}

	/* for disp progress bar */
	total += size;
	if (all_size > 1024*1024) {
            pcent = total/(all_size/100);
	} else {
            pcent = (total*100)/all_size;
	}
	strncpy(strbar, &bar[50-pcent/2], 50);
	strbar[0] = strbar[10] = strbar[20] = strbar[30] 
	          = strbar[40] = strbar[50] = '|';
	strbar[51] = '\0';
#if 0
#ifdef _WIN32
        printf("  %3d%% %s %dKB/%dKB\r", pcent, strbar, total/1024, all_size/1024, cur_up);
#else  /* !_WIN32 */
        printf("  %3d%% %s %dKB/%dKB\n%s", pcent, strbar, total/1024, all_size/1024, cur_up);
#endif /* !_WIN32 */
#endif /* 0 */
        fflush(stdout);
    }

#if 0
    printf("\n");
#endif

    if (!feof(fp1)) {
        fprintf(stderr, "Copy Incompleted.\n");
    }
    if (all_size != total) {
        fprintf(stderr, "Copy Incompleted. size=%d write=%d\n", all_size, total);
    }
    fclose(fp1);
    fclose(fp2);
}

/* vim:ts=8:sw=8
 */
