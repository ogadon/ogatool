/*
 *   read/write perf
 *			V1.00  1995/07/03 for X68000 by hyper halx.oga
 *			V1.01  1995/07/05 3050RX/AIX support
 *			V1.02  1995/07/08 2nd Read perf.
 *			V1.03  1995/08/24 -r (read only) support. for CD-ROM
 *			V1.04  1995/08/29 add cm flush
 *			V1.05  1995/08/30 display hostname/fstype
 *			V1.06  1995/09/22 -net support.
 *			V1.07  1996/01/08 write() err check by size.
 *			V1.08  1996/02/26 disp value on perf bar
 *			V1.09  1996/03/03 open binarymode / port LINUX
 *			V1.10  1996/03/04 check fstype HP-DFS
 *			V1.11  1996/03/05 check time err
 *			V1.12  1996/03/25 bugfix negative time err
 *			V1.13  1996/03/26 PERF_DEBUG
 *			V1.14  1996/03/29 memcopy virtual to real
 *
 *			V1.15  1996/04/03 add process lock option (by satoy)
 *			V1.16  1996/04/03 add not mem pre-load option (by satoy)
 *			V1.17  1996/04/03 add change file option (by satoy)
 *			V1.18  1996/04/17 1/1000 sec
 *			V1.19  1996/04/18 ngcnt & pre sync(-flush) support 
 *			V1.20  1996/04/20 -v support
 *			V1.21  1996/06/05 bugfix and DOS support
 *			V1.22  1997/05/04 port V1.21 for x68k
 *			V1.23  2003/09/28 fix time 0
 *			V1.24  2010/06/24 memcpy size to 4096/fix 0 sec error
 *			
 *   Compile Flags			
 *			
 *   X68000      : default
 *   HI-UX/HP-UX : -DX3050RX
 *   AIX         : -DX3050RX -DAIX
 *   LINUX       : -DLINUX
 *   DOS         : -DDOS
 */

#define VER "1.24"
#define WRITE_CHECK 1
/*
#define PERF_DEBUG 1
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef AIX
#include <sys/param.h>
#include <sys/times.h>
#include <sys/vfs.h>
#endif
/* AIX & HP DFS */
#define	MNT_DFS	7

#ifdef X3050RX
#include <sys/mount.h>
#include <sys/lock.h>
#include <time.h>
#define clock clock_3050
#endif /* X3050RX */

#ifdef LINUX
#include <sys/time.h>
#include <unistd.h>
#define clock clock_3050
#endif

#ifdef DOS
#define clock() clock()/10
#endif

#if defined(X3050RX) || defined(AIX) || defined(LINUX)
#define MEM_SIZE 4096
#define PERSEC 1000
#else
#ifdef DOS
#define MEM_SIZE 4096
#else  /* !DOS */
#define MEM_SIZE 100
#endif /* DOS */
#define PERSEC 100
#endif

/* default value KB */
#define SIZE 1*1024
#define BLOCK 8
#define SLEEPTIME 5

int block = BLOCK;
int size  = SIZE;
int ngcnt;	/* for write over 1sec check */
int ngcnt_sec;	
int result[10];

main(a,b)
int	a;
char	*b[];
{
	int fd, i;
	int st_time,en_time, st_wk;
	int readonly  = 0;
	int fstype = 0;
	int netf   = 0;		/* 1:open,closeの時間を入れない    */
	int flushf = 0;		/* 1:sync befor r/w (flush buffer) */
	int bf     = 0;
	int code    = 0;
	int verbose = 0;
	int unlinkf = 1;
#ifdef X3050RX
	int plock_result;       /* plock の戻り値 */
	int plock_flag = 0;     /* plock 成功時にnot 0 */
#endif /* X3050RX */
	int mem_preload_flag = 1; /* 転送用メモリの読み込み動作を行なう */
	char *buf;
	char *mem1,*mem2;
	char *filename = "rwtest";
	struct  stat stbuf;

	for (i = 1; i < a; i++) {
	    if (!strcmp(b[i],"-h")) {
		printf("usage : rw [<block_size(k)> [<total_size(k/m)>]] [-f <filename>]] [-r]\n");
		printf("           [-net] [-nompl]");
#ifdef X3050RX
		printf(" [-plock {p|t|d}]");
#endif /* X3050RX */
		printf("\n");
		printf("        -f     : change test file.\n");
		printf("        -r     : Readonly test (ex. CD-ROM)\n");
		printf("        -net   : Time without open/close\n");
		printf("        -flush : flush ufs buffer befor r/w\n");
		printf("        -nompl : not Memory pre-load\n");
#ifdef X3050RX
		printf("        -plock : Process locking\n");
		printf("                 'p' is process (text and data) lock\n");
		printf("                 't' is text area lock\n");
		printf("                 'd' is data area lock\n");
#endif /* X3050RX */
		printf("        -v     : verbose\n");
		printf("rw version is %s\n", VER);
		exit(1);
	    }
	    if (!strcmp(b[i],"-net")) {
		netf = 1;
		continue;
	    }
	    if (!strcmp(b[i],"-flush")) {
		flushf = 1;
		continue;
	    }
	    if (!strcmp(b[i],"-v")) {		/* V1.20 */
		verbose = 1;
		continue;
	    }
	    if (!strcmp(b[i],"-nompl")) {
		mem_preload_flag = 0;
		continue;
	    }
#ifdef X3050RX
	    if (!strcmp(b[i],"-plock")) {
		if (++i >= a) {
			printf("-plock : Process locking code not specified.\n");
			exit(1);
		}
		if(strlen(b[i]) == 1){
		    switch ((b[i])[0]) {
		        case 'p' :
		        case 'P' :
			    plock_result = plock(PROCLOCK);
			    break;
		        case 't' :
		        case 'T' :
			    plock_result = plock(TXTLOCK);
			    break;
		        case 'd' :
		        case 'D' :
			    plock_result = plock(DATLOCK);
			    break;
		        default :
			    printf("-plock : Bad Process locking code specified.\n");
			    printf("         Code = %s\n",b[i]);
			    exit(1);
		    } 
		} else {
		    printf("-plock : Bad Process locking code specified.\n");
		    printf("         Code = %s\n",b[i]);
		    exit(1);
		}
		if(plock_result<0){
		    perror("plock(2)");
		    exit(1);
		}
		plock_flag = 1;
		continue;
	    }
#endif /* X3050RX */
	    if (!strcmp(b[i],"-f")) {
		if (++i >= a) {
			printf("-f : filename not specified.\n");
			exit(1);
		}
		filename = b[i];
		unlinkf = 0;
		continue;
	    }
	    if (!strcmp(b[i],"-r")) {
		readonly = 1;
		unlinkf = 0;
		continue;
	    }
	    if (!bf) {
		block = atoi(b[i]);
		bf++;
	    } else {
		size = atoi(b[i]);
		switch ((b[i])[strlen(b[i])-1]) {
		    case 'k' :
		    case 'K' :
			size *= 1;
			break;
		    case 'm' :
		    case 'M' :
			size *= 1024;
			break;
		    default :
			printf("error : unit = %c\n",(b[i])[strlen(b[i])-1]);
			break;
		} 
	    }
	}

	printf("■■ rw Version %s  by oga. ■■\n",VER);

	if (netf) 
		printf("★ Time without open/close\n");
	else
		printf("★ Time include open/close\n");

	buf = (char *)malloc(block*1024);

	/* 
	 * Memory copy perf 
	 */
#ifndef XDOS
	mem1= (char *)malloc(MEM_SIZE*1024);
	mem2= (char *)malloc(MEM_SIZE*1024);

	if(mem_preload_flag){
	    memcpy(mem1,mem2,MEM_SIZE*1024);
        }
	st_time = clock();
	memcpy(mem1,mem2,MEM_SIZE*1024);
	en_time = clock();

	free(mem1);
	free(mem2);
	printf("%dKB memcpy (%dKB block) %.2f KB/sec (%.3f sec)\n",
	        MEM_SIZE,MEM_SIZE,(float)MEM_SIZE*PERSEC/(en_time-st_time),(float)(en_time-st_time)/PERSEC);
#else
	st_time = clock();
	en_time = st_time+100;
#endif
        if ((en_time-st_time) == 0) {   /* V1.23-C */
	    result[0] = 0;
	} else {
	    result[0] = MEM_SIZE*PERSEC/(en_time-st_time);
	}

	/* 
	 *  WRITE perf 
	 */
	if (flushf) {
		int st,en;

		printf("flush ufs buffer!!\n");
#if defined(X3050RX) || defined(LINUX)
		sync();
		if (verbose) {
			printf("start sleep(%d) = %d\n",SLEEPTIME,st=clock());
		}
		sleep(SLEEPTIME);
		if (verbose) {
			en = clock();
			printf("  end sleep(%d) = %d (%d 1/%dsec)\n",SLEEPTIME,en,en-st,PERSEC);
		}
#endif
	}
	if (!readonly) {
	    if (!netf) st_time = clock();
#if defined(X3050RX) || defined(AIX) || defined(LINUX)
	    if ((fd = open(filename ,O_RDWR+O_CREAT,0666)) == -1) {
#else
	    if ((fd = open(filename ,O_RDWR+O_CREAT+O_BINARY,0666)) == -1) {
#endif
		perror("open(write)");
		exit(1);
	    } /* } */
	    ngcnt = ngcnt_sec = 0;
	    if (netf) st_time = clock();
	    for (i = 0;i < size; i += block) {
#ifdef WRITE_CHECK
		st_wk=clock();
#endif
		if ((code=write(fd,buf,block*1024)) != block*1024) {
			printf("write %d bytes, code = %d\n",block*1024,code);
			perror("write");
			exit(1);
		}
#ifdef WRITE_CHECK
		if (clock()-st_wk > PERSEC/2) {
			ngcnt++;
			ngcnt_sec += clock()-st_wk;
			if (verbose) {
				printf("1 block write .5 sec over!!(%d [1/%dsec]/%dth block)\n",PERSEC,clock()-st_wk,i/block);
			}
		}
#endif
	    }
	    if (netf) en_time = clock();
	    if (close(fd) == -1) {
		perror("close");
		exit(1);
	    }
	    if (!netf) en_time = clock();
	    if (en_time - st_time < 0) printf("Time error:start=%d, end=%d\n",st_time,en_time);
	    printf("%dKB write (%dKB block) %.2f KB/sec (%.3f sec)\n",
	        size,block,(float)size*PERSEC/(en_time-st_time),(float)(en_time-st_time)/PERSEC);
	    if (en_time-st_time == 0) {  /* V1.24-C */
	        result[1] = 0;
	        result[9] = 0;
	    } else {
	        result[1] = size*PERSEC/(en_time-st_time);
	        result[9] = size*PERSEC/(en_time-st_time-ngcnt_sec);
	    }
	} else {
	    result[1] = 0;
	}

#ifdef X3050RX
	/* cache clear */
	stat(filename,&stbuf);
#  ifdef AIX
#    ifdef DEBUG
	printf("AIX st_vfs = %d\n",stbuf.st_vfstype); 
#    endif /* DEBUG */
	if ((fstype = stbuf.st_vfstype) == MNT_DFS) {
#  else  /* !AIX */
	if ((fstype = stbuf.st_fstype) == MOUNT_DFS || fstype == MNT_DFS) {
#  endif /* AIX */
		char exbuf[256];
		sprintf(exbuf,"cm flush %s",filename);
		printf("%s\n",exbuf);
		system(exbuf);
	}
#endif /* X3050RX */

	/* 
	 *  1st READ perf 
	 */
	if (flushf) {
		printf("flush ufs buffer!!\n");
#if defined(X3050RX) || defined(LINUX)
		sync();
		sleep(SLEEPTIME);
#endif
	}
	if (!netf) st_time = clock();
#if defined(X3050RX) || defined(AIX) || defined(LINUX)
	if ((fd = open(filename,O_RDONLY)) == -1) {
#else
	if ((fd = open(filename,O_RDONLY+O_BINARY)) == -1) {
#endif
		perror("open(read1)");
		exit(1);
	}
	if (netf) st_time = clock();
	for (i = 0;i < size; i += block) {
		if (read(fd,buf,block*1024) == -1) {
			perror("read");
			exit(1);
		}
	}
	if (netf) en_time = clock();
	if (close(fd) == -1) {
		perror("close");
		exit(1);
	}
	if (!netf) en_time = clock();
	if (en_time - st_time < 0) printf("Time error:start=%d, end=%d\n",st_time,en_time);
	printf("%dKB read  (%dKB block) %.2f KB/sec (%.3f sec)\n",
	        size,block,(float)size*PERSEC/(en_time-st_time),(float)(en_time-st_time)/PERSEC);
	if (en_time-st_time == 0) {  /* V1.24-C */
	    result[2] = 0;
	} else {
	    result[2] = size*PERSEC/(en_time-st_time);
	}

	/* 
	 * 2nd READ perf 
	 */
	if (!netf) st_time = clock();
#if defined(X3050RX) || defined(AIX) || defined(LINUX)
	if ((fd = open(filename,O_RDONLY)) == -1) {
#else
	if ((fd = open(filename,O_RDONLY+O_BINARY)) == -1) {
#endif
		perror("open(read2)");
		exit(1);
	}
	if (netf) st_time = clock();
	for (i = 0;i < size; i += block) {
		if (read(fd,buf,block*1024) == -1) {
			perror("read");
			exit(1);
		}
	}
	if (netf) en_time = clock();
	if (close(fd) == -1) {
		perror("close");
		exit(1);
	}
	if (!netf) en_time = clock();
	if (en_time - st_time < 0) printf("Time error:start=%d, end=%d\n",st_time,en_time);
	printf("%dKB read  (%dKB block) %.2f KB/sec (%.3f sec)\n",
	        size,block,(float)size*PERSEC/(en_time-st_time),(float)(en_time-st_time)/PERSEC);
	if (en_time-st_time == 0) {  /* V1.23-C */
	    result[3] = 0;
        } else {
	    result[3] = size*PERSEC/(en_time-st_time);
	}

	result[4] = 0;

	disp_result(result,fstype);

	if (unlinkf) unlink(filename);

#ifdef X3050RX
    if(plock_flag)
	if(plock(UNLOCK)<0){
	    perror("plock(2)");
	    exit(1);
	}
#endif /* X3050RX */

	return 0;
}
disp_result(result,fstype)
int *result,fstype;
{
	char hostname[100];
	char *fsname = "unknown";

	printf("\n");
#ifdef X3050RX
	if (gethostname(hostname,100) < 0) {
		perror("gethostname");
		printf("[ ");
	} else {
		printf("[ %s / ",hostname);
	}
	switch (fstype) {
#ifndef AIX
	    case MOUNT_UFS :
		fsname = "UFS";
		break;
	    case MOUNT_NFS :
		fsname = "NFS";
		break;
	    case MOUNT_CDFS :
		fsname = "CDFS";
		break;
#ifdef MOUNT_VAFS
	    case MOUNT_VAFS :
		fsname = "VAFS";
		break;
	    case MOUNT_DFS :
		fsname = "DFS";
		break;
	    case MOUNT_EFS :
		fsname = "EFS";
		break;
	    case MOUNT_AGFS :
		fsname = "AGFS";
		break;
	    case MNT_DFS :
		fsname = "HP-DFS";
		break;
#endif
#else  /* AIX */
	    case MNT_AIX :
		fsname = "AIX";
		break;
	    case MNT_NFS :
		fsname = "NFS";
		break;
	    case MNT_JFS :
		fsname = "JFS";
		break;
	    case MNT_DFS :
		fsname = "DFS";
		break;
	    case MNT_CDROM :
		fsname = "CDROM";
		break;
#endif /* AIX */
	    default : 
		printf("FSTYPE = %d ]",fstype);
		break;
	}
	if (strcmp(fsname,"unknown")) {
		printf("%s ]",fsname);
	}
	printf("   block %dKB / size %dKB",block,size);
	if (ngcnt) {
		printf("  write over .5sec : %d(%.1fsec)\n",ngcnt,(float)ngcnt_sec/PERSEC);
	} else {
		printf("\n");
	}
	

#endif /* X3050RX */

	printf("---------+---+----+---+----+---+----+---+----+---+----+---+----+--------------\n");
	printf("  TEST   1K  5K  10K 50K 100K 500K 1M  5M  10M  50M 100M 500M 1G  [bytes/sec]\n");
	printf("---------+---+----+---+----+---+----+---+----+---+----+---+----+--------------\n");
#ifndef XDOS
	printf("MEM COPY:");
	disp_bar(result[0],0);
#endif
	printf("   WRITE:");
	disp_bar(result[1],ngcnt);
	printf("1st READ:");
	disp_bar(result[2],0);
	printf("2nd READ:");
	disp_bar(result[3],0);
	printf("---------+---+----+---+----+---+----+---+----+---+----+--------------\n");
	return 0;
}
disp_bar(val,f)
int val, f;
{
	int i, j=0;
	int val2=val;

	while (val > 10) {
		j += 9;
		val /= 10;
	}
	j += val;
	for (i = 1; i <= j; i++)
		if ((i % 9) == 1) {
			printf("|");
		} else {
			printf("*");
		}
	if (f) {
		printf(" %d (%d)\n",val2,result[9]);
	} else {
		printf(" %d\n",val2);
	}
	return 0;
}

#if defined(X3050RX) || defined(LINUX)
/*
 *   clock() for 3050RX 
 *
 *       1/PERSEC sec 単位の値をリターンする
 */
int clock_3050()
{
	int code = 0;
#ifdef AIX
    struct tms buf;

    code = times(&buf)*PERSEC/HZ;
#else
	struct timeval tv;
	struct timezone tz;
	gettimeofday(&tv,&tz);

	code = tv.tv_sec*PERSEC+(tv.tv_usec)/(1000000/PERSEC);
#endif /* AIX */
	return code;
}
#endif

#ifdef DOSX
/*
 *   clock() for DOS
 *
 *       1/100 sec 単位の値をリターンする
 */
int clock_dos()
{
	int tt;
	tt = clock();
	printf("clock() = %d\n",tt);
	return tt;
}
#endif
