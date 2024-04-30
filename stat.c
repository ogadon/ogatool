/*
 *   stat() and statfs() test program
 *
 *           97/05/16  V1.01 by oga
 *           97/11/06  V1.02 add errro exit
 *           00/08/18  V1.03 time format disp.
 *           04/10/11  V1.04 use lstat
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <sys/vfs.h>
#endif

void tfm(tt)
time_t *tt;
{
	struct tm *tm;
	tm = localtime(tt);
	printf("%04d/%02d/%02d %02d:%02d:%02d\n",
			tm->tm_year+1900,
			tm->tm_mon+1,
			tm->tm_mday,
			tm->tm_hour,
			tm->tm_min,
			tm->tm_sec);
}

main(a,b)
int a;
char *b[]; 
{
	struct  stat buf;
#ifndef _WIN32
	struct  statfs bufs;
#endif

	printf("***  stat(%s) ***\n",b[1]);
#ifdef _WIN32
	if (stat(b[1],&buf) < 0)
#else
	if (lstat(b[1],&buf) < 0)
#endif
	{
	  perror("stat");
	  exit(0);
	}
	printf("st_dev    = 0x%x\n",buf.st_dev);
	printf("st_rdev   = 0x%x\n",buf.st_rdev);
	printf("st_ino    = %d\n",buf.st_ino);
#if !defined(LINUX) && !defined(_WIN32)
	printf("st_fstype = %d\n",buf.st_fstype);
#endif
	printf("st_mode   = 0%o\n",buf.st_mode);	/* 8i” */
	printf("st_uid/gid= %d/%d\n",buf.st_uid,buf.st_gid);
	printf("st_size   = %d\n",buf.st_size);

	printf("st_atime  = %d(%d) ",buf.st_atime,mktime(gmtime(&buf.st_atime)));
	tfm(&buf.st_atime);

	printf("st_mtime  = %d(%d) ",buf.st_mtime,mktime(gmtime(&buf.st_mtime)));
	tfm(&buf.st_mtime);

	printf("st_ctime  = %d(%d) ",buf.st_ctime,mktime(gmtime(&buf.st_ctime)));
	tfm(&buf.st_ctime);

#if 0
	{
	    time_t tt;
	    struct tm *tm, tm2;
	    char buf[1024];

	    tt = time(0);         /* local */
	    tm = gmtime(&tt);
	    strftime(buf,sizeof(buf),"%y/%m/%d %H:%M:%S",tm);
	    printf("GMT  %s\n",buf);

	    tt = mktime(tm);	  /* GMT */
	    tm = gmtime(&tt);
	    strftime(buf,sizeof(buf),"%y/%m/%d %H:%M:%S",tm);
	    printf("GMT2 %s\n",buf);

	    tt = mktime(tm);	  /* GMT */
	    tm = localtime(&tt);
	    strftime(buf,sizeof(buf),"%y/%m/%d %H:%M:%S",tm);
	    printf("UTC => local %s\n",buf);

	}
#endif

#ifndef _WIN32
	printf("***  statfs() ***\n");
	if (statfs(b[1],&bufs) < 0) {
	  perror("statfs");
	  exit(1);
	}
	printf("f_bavail   = 0x%08x (%7d)  free blocks\n",
						bufs.f_bavail,bufs.f_bavail);
	printf("f_bfree    = 0x%08x (%7d)  free blocks for super user\n",
						bufs.f_bfree,bufs.f_bfree);
	printf("f_blocks   = 0x%08x (%7d)  total blocks\n",
						bufs.f_blocks,bufs.f_blocks);
	printf("f_bsize    = 0x%08x (%7d)  block size\n",bufs.f_bsize,bufs.f_bsize);
	printf("f_ffree    = 0x%08x (%7d)\n",bufs.f_ffree,bufs.f_ffree);
	printf("f_files    = 0x%08x (%7d)\n",bufs.f_files,bufs.f_files);
	printf("f_type     = 0x%08x (%7d)\n",bufs.f_type,bufs.f_type);
#ifdef LINUX
	printf("f_fsid     = 0x%x\n",bufs.f_fsid);
#else
	printf("f_fsid[0]  = 0x%x\n",bufs.f_fsid[0]);
	printf("f_fsid[1]  = 0x%x  filesystem type\n",
						bufs.f_fsid[1]);
#endif /* LINUX  */
#endif /* _WIN32 */

}

