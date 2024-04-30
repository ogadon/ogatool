/*
 *     df  UNIXy Disk Free command
 *
 *           97/02/08 V1.00  by oga.
 *           97/04/09 V1.01  for MO
 *           97/10/25 V1.02  support html for used as CGI
 *           98/09/07 V1.03  support -v for FAT32 debug
 *           00/06/14 V1.04  fix html bug
 *           03/03/31 V1.05  fix big disk bug
 *           05/09/14 V1.06  check hibun drive
 *           06/02/25 V1.07  support Å`4TB disk, and -gb
 *           10/07/18 V1.08  support over 4TB disk
 */
#include <stdio.h>
#include <windows.h>
#include <sys/types.h>  /* V1.06-A */
#include <sys/stat.h>   /* V1.06-A */

#define VER	"1.08"

/* globals */
int   web   = 0;  	/* -w output Web format*/
typedef unsigned __int64 uint64;

void DispBar(per)
int per;
{
	int i;

	for (i = 0; i<20; i++) {
	    if (per > 100*i/19) {
		if (web) {
	            printf("<font color=\"#ff0000\">Å°</font>");
		} else {
	            printf("Å°");
		}
	    } else {
		if (web) {
	            printf("<font color=\"#00ff00\">Å°</font>");
		} else {
	            printf("Å†");
		}
	    }
	}
	if (per < 0) {
	    printf("Arg Error: percent is %d%%\n", per);
	}
}

/*
 *   ret : val64/1024/1024
 *
 */
unsigned int dev1M(ULARGE_INTEGER *val64)
{
    unsigned int high, low;

    high = val64->HighPart;
    low  = val64->LowPart;

    low  = low >> 20;
    high = high << 12;  

    return (high | low);
}

/*
 *   Hyper2ui64()
 *      ULARGE_INTEGER Ç uint64(unsigned __int64)Ç…ïœä∑Ç∑ÇÈ  
 *
 */
uint64 Hyper2ui64(ULARGE_INTEGER uli)
{
    uint64 ans;

    ans =  uli.HighPart;
    ans *= 0x10000;                     /* 16bit shift */
    ans *= 0x10000;                     /* 16bit shift */
    ans += uli.LowPart;

    return ans;
}

/* 
 *  uint64Çï∂éöóÒÇ…Ç∑ÇÈ  (sprintfÇ™%I64uÇ…ëŒâûÇµÇƒÇ¢Ç»Ç¢ÇΩÇﬂ)
*/
char *ui64toStr(uint64 val, char *ostr)
{
    int  i;
    char buf[1024];

    for (i = 0; ; i++) {
	buf[i] = (char)(val - (val/10)*10) + '0';  // â∫1åÖÇéÊÇËèoÇµÇƒï∂éöÇ…Ç∑ÇÈ  
	val /= 10;
	if (val == 0) break;
    }
    buf[++i] = '\0';

    // ãtÇ…ï¿Ç—ë÷Ç¶ÇÈ
    for (i = 0; i<(int)strlen(buf); i++) {
	ostr[i] = buf[strlen(buf)-i-1];
    }
    ostr[i] = '\0';

    return ostr;
}

int main(a,b)
int  a;
char *b[];
{
	DWORD drv;
	char  *drvn = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	char  *type;
	char  path[256];
	char  hibun_path[256];  /* V1.06-A */
	int   i;
	unsigned int sect;		/* Sectors/Cluster       */
	unsigned int bytes;		/* Btyes/Sector          */
	unsigned int free;		/* Num of Free Cluster   */
	unsigned int total;		/* Total Clusters        */
	unsigned int bpc;		/* bytes/Cluster         */
	uint64       totalb,freeb;	/* Total,Free (KB) V1.08-C */
	unsigned int debug_total;	/* Total for DEBUG V1.07 */
	uint64       extotal;	/* FreeSpaceEx() Total  V1.07 */
	uint64       exfree;	/* FreeSpaceEx() Free   V1.07 */
	int   all   = 0;  	/* -a display fd              */
	int   mega  = 1;  	/* 1024: display mega         */
	int   graph = 0;  	/* -g graphic display         */
	int   vf    = 0;        /* -v verbose                 */
	int   tf    = 0;        /* -total (debug total) V1.07 */
	char  *unit="KB";
        int   per;
        int   nodisk = 0;       /* no disk flag         V1.08 */
        struct stat stbuf;      /* V1.06-A */
        ULARGE_INTEGER availx, totalx, freex;

	for (i = 1; i<a; i++) {
	    if (!strncmp(b[i],"-h",2)) {
		printf("DF Ver%s  by Moritaka Ogasawara.\n",VER);
		printf("usage : df [-a|-g|-mb|-gb|-w|-h]\n");
		printf("        -a      : output removable disk info.\n");
		printf("        -g      : graphical\n");
		printf("        -h      : help\n");
		printf("        -mb, -m : MB\n");
		printf("        -gb     : GB\n");
		printf("        -w      : html output for CGI\n");
		exit(1);
	    }
	    if (!strncmp(b[i],"-a",2)) {
	    	all = 1;
	    	continue;
	    }
	    if (!strcmp(b[i],"-m")
	        || !strcmp(b[i],"-mb")) {
	    	mega = 1024;
		unit = "MB";
	    	continue;
	    }
	    if (!strncmp(b[i],"-gb",3)) {
	    	mega = 1024*1024;
		unit = "GB";
	    	continue;
	    }
	    if (!strcmp(b[i],"-g")) {
	    	graph = 1;
	    	continue;
	    }
	    if (!strncmp(b[i],"-w",2)) {
	    	web = 1;
	    	continue;
	    }
	    if (!strncmp(b[i],"-v",2)) {
	    	++vf;
	    	continue;
	    }
	    if (!strcmp(b[i],"-total") && a > i+1) {
	    	tf = 1;
		debug_total = strtoul(b[++i], (char **)NULL, 0);
	    	continue;
	    }
	}

	if (web) {
	    printf("ContentType:text/html\n\n");
	    printf("<html><head><title>Disk Free</title></head>\n");
	    printf("<body bgcolor=\"#ffccff\">\n");
	    printf("<h1><center><font color=\"#aa88ff\">DiskégópèÛãµ</font></center></h1>\n");
	    printf("<pre>\n");
	}

	if (!graph) {
	    printf("Drive Type    Total(%s)    Used(%s)   Avail(%s)  Capa  Cluster(B)\n",
							unit,unit,unit);
	} else {
            printf("Drive Type   |0%% .   .   .   .   |50%%.   .   .   .   |100%%   Free/ Total\n");
	}
	drv = GetLogicalDrives();
#ifdef DEBUG
	printf("drv = 0x%x\n",drv);
#endif

	for (i = 0; i<26; i++) {
	    if (drv & (1 << i)) {
	        sprintf(path,"%c:\\",drvn[i]);
	        sprintf(hibun_path,"%c:\\_hibun_.sys",drvn[i]); /* V1.06-A */
		switch (GetDriveType(path)) {
		  case 0:
		      type = "unknown";
		      break;
		  case 1:
		      type = "Disable";
		      break;
		  case DRIVE_REMOVABLE:
		      if (i < 3) {
		        type = "FD";
		      } else {
		        type = "REMOV.";
		      }
		      break;
		  case DRIVE_FIXED:
		      type = "HDD";
		      break;
		  case DRIVE_REMOTE:
		      type = "REMOTE";
		      break;
		  case DRIVE_CDROM:
		      type = "CDROM";
		      break;
		  case DRIVE_RAMDISK:
		      type = "RAMDISK";
		      break;
		  default :
		      type = "ERR";
		      break;
	        }
		if (all || strcmp(type,"FD")) {
		    nodisk = 0;
	            if (GetDiskFreeSpaceEx(
			        path,           /* drive */
				&availx,	/* free  */
				&totalx,	/* zero  */
				&freex)) {	/* total */
		        if (vf > 1) {
	                    printf("ExDEBUG %s availx.high=%u availx.low=%u\n",
        				 path, availx.HighPart, availx.LowPart);
	                    printf("ExDEBUG %s totalx.high=%u totalx.low=%u\n",
        				 path, totalx.HighPart, totalx.LowPart);
	                    printf("ExDEBUG %s  freex.high=%u  freex.low=%u\n",
        				 path, freex.HighPart, freex.LowPart);
	                    printf("ExDEBUG %s avail=%u /total=%u /free=%u\n",
        				 path, availx,totalx,freex);
			}
		        if (vf >= 1) {
			    printf("ExDEBUG2 %s avail:%u MB  total:%u MB  free=%u MB\n", path, dev1M(&availx), dev1M(&totalx), dev1M(&freex));
		        }
			//totalx.HighPart = 256*8;            /* debug 8TB */
			extotal = Hyper2ui64(totalx);       /* V1.08-C */
			exfree  = Hyper2ui64(freex);        /* V1.08-C */
		        if (vf >= 1) {
			    printf("ExDEBUG3 %s avail:%I64u MB  total:%I64u MB  free=%I64u MB\n", path, Hyper2ui64(availx)/1024/1024, extotal/1024/1024, exfree/1024/1024);
		        }
		    } else {
		        //printf("DEBUG %s NO DISK!\n", path);
			nodisk = 1;
		    }

	            printf("   %c: %-7s",drvn[i],type);
		    /* V1.08-C start */
	            if (nodisk) {
			if (!graph) {
			    printf("  ");
			}
			if (!strcmp(type, "FD") || !strcmp(type, "RAMDISK")) {
		            printf("NO DISK!\n");
			} else {
		            printf("NO DISC!\n");
			}
			continue;
		    } else {
			// get sector & bytes
			if (GetDiskFreeSpace(path,&sect,&bytes,&free,&total) != TRUE) {
			    // error
			}
		    }
		    /* V1.08-C end   */

		    if (tf) {
		        //total =  46970043;  /* 4KB 0.18TB */
		        //total = 469700000;  /* 4KB 1.8TB */
			total   = debug_total;
			extotal = debug_total;
		    }

		    bpc = sect * bytes;		/* calc bytes/Cluster */

		    //totalb = total*((bpc/512)/2); /* KB */
		    //freeb  = free*((bpc/512)/2);  /* KB */

		    totalb = extotal/1024;      /* KB         V1.08-C */
		    freeb  = exfree /1024;      /* KB         V1.08-C */

		    /* V1.05-A start */
		    if (totalb >= 1024*1024) {
		        /* 1GBà»è„ÇÃèÍçá (1TBà»è„ÇÃèÍçá) */
		    	per = (int)(100*(totalb/1024-freeb/1024)/(totalb/1024));
		    } else {
		        per = (int)(100*(totalb-freeb)/totalb);
		    }
		    /* V1.05-A end */

		    if (graph) {
		    	DispBar(per);                           /* V1.05-C */
                        /* V1.06-A start */
			if (!stat(hibun_path, &stbuf)) {
			    printf("îÈ");
			} else {
			    printf("  ");
			}
                        /* V1.06-A end   */

		    	printf(" (%8I64u/%8I64u)%s\n", freeb/(uint64)mega,       /* V1.08-C */
		    			totalb/(uint64)mega,
		    			unit);
		    } else {
			    printf("%10I64u  %10I64u  %10I64u  %3d%%  %6d\n",    /* V1.08-C */
					totalb/(uint64)mega,        /* total        V1.08-C */
					(totalb-freeb)/(uint64)mega,/* used         V1.08-C */
					freeb/(uint64)mega,         /* available    V1.08-C */
					per,                        /* percent      V1.05-C */
					bpc);                       /* cluster size         */
		    }
                    if (vf) {
	                printf("sect=%u /byte=%u /free=%u /total=%u\n",
	        	    sect, bytes, free, total);
	                printf("clust=%uB /free=%uKB /total=%uKB\n",
			    bpc, free*((bpc/512)/2), total*((bpc/512)/2));
                    }
		}

	    } 
	}
	if (web) {
	    printf("<p><center><address>\n");
	    printf("<hr>\n");
	    printf("Presented by oga.</address></center>\n");
	    printf("</body></html>\n");
	}
	return 0;
}

/*
 * vim:ts=8:sw=4:
 */ 


