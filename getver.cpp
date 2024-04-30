//
//  getver : get version info
//
//  V1.01 2001/10/02 by oga.
//
//
#include <stdio.h>
#include <windows.h>

int  csvf = 0;

//
//   IN  block : block pointer from GetFileVersionInfo()
//   IN  bhead : Block Header Name (ex.) "FileDescription"
//   OUT str   : resource Value
//   OUT <ret> : 0:succuess 1:failed
//
int GetRcValue(char *block, char *bhead, char **str)
{
    char wk[2048];
    unsigned int size;		// dummy

    sprintf(wk,"\\StringFileInfo\\041104b0\\%s",bhead);
    if (VerQueryValue(block, 
	    	      TEXT(wk),
	    	      (LPVOID *)str, &size) == FALSE) {
         if (csvf) {
             printf(",????", bhead);
	 } else {
             printf("Block Header(%s) not found.\n", bhead);
	 }
	 return 1;
    }
    return 0;
}

//
//  expand_wild()
//
//   IN  int  argc
//   IN  char *argv[] 
//   IN  int  filename start point of argv
//   OUT char *filelist[]
//
void expand_wild(int argc, char **argv, int pos, char **filelist)
{
    int i;
    int j = 0;
    int firstf = 1;
    for (i = pos; i<argc; i++) {
        //printf("file=%s  i=%d/argc=%d\n", argv[i], i, argc);
	if (strchr(argv[i], '*') || strchr(argv[i], '?')) {
	    // include wild char
	    HANDLE hdir;
	    WIN32_FIND_DATA wfd;
	    char *path = argv[i];
	    int  status;
	    while (1) {
                if (firstf) {
                    hdir = FindFirstFile(path, &wfd);
                    if (hdir == INVALID_HANDLE_VALUE) {
		        // no matching file
                        //printf("expand_wild: FindFirstFile() error\n");
                        break;
                    }
                    firstf = 0;
                } else {
                    status = FindNextFile(hdir, &wfd);
                    if (status == FALSE) {
                        break;
                    }
                }
                filelist[j] = (char *) malloc(strlen(wfd.cFileName)+1);
	        strcpy(filelist[j], wfd.cFileName);
	        j++;
	    }
	    FindClose(hdir);
	} else {
	    // not wild
            filelist[j] = (char *) malloc(strlen(argv[i])+1);
	    strcpy(filelist[j], argv[i]);
	    j++;
	}
    }
    //printf("return expand_wild()\n");
}

int main(int a, char *b[])
{
    char buf[65536];	// resource data
    char *str;
    char *filename;
    unsigned int  size = 0;
    int  i = 0;
    int  k = 0;
    char *filelist[2048];         // max 2048 files/dir
    VS_FIXEDFILEINFO *vfinfo = NULL;
    char *header[] = {
        "CompanyName",
	"FileDescription",
	"FileVersion",
	"InternalName",
	"LegalCopyright",
	"OriginalFilename",
	"ProductName",
	"ProductVersion",
	NULL
    };

    memset(filelist, 0, sizeof(filelist));

    if (a < 2 || !strcmp(b[1], "-h")) {
        printf("usage: getver [-csv] <file.exe> [<file.exe> ...]\n");
        exit(1);
    }
    i = 1;
    if (!strcmp(b[1], "-csv")) {
        csvf = 1;
	++i;
    }

    // expand wildcard filename
    expand_wild(a, b, i, filelist);

    if (csvf) {
        // write csv header
	printf("FileName,FileVersion,ProductVersion");
        i = 0;
        while (header[i] != NULL) {
	    printf(",%s", header[i]);
	    i++;
	}
	printf("\n");
    }

    k = 0;
    while (filelist[k] != NULL) {
      filename = filelist[k++];
      if (csvf) {
          printf("%s",filename);
      } else {
          printf("===== %s =====\n",filename);
      }
      if (GetFileVersionInfo(filename, 0, sizeof(buf), buf)) {
        // printf("GetFileVersionInfo() success.\n");
	if (VerQueryValue(buf, 
              		  TEXT("\\"), 
	      		  (LPVOID *)&vfinfo,  &size)) {
            // printf("VerQueryValue() success (size=%d).\n", size);

            if (csvf) {
	      // FileVersion
	      printf(",\"%d,%d,%d,%d\"",
	    	vfinfo->dwFileVersionMS >> 16,
	    	vfinfo->dwFileVersionMS &  0xffff,
	    	vfinfo->dwFileVersionLS >> 16,
	    	vfinfo->dwFileVersionLS &  0xffff );
	      // ProductVersion
	      printf(",\"%d,%d,%d,%d\"",
	    	vfinfo->dwProductVersionMS >> 16,
	    	vfinfo->dwProductVersionMS &  0xffff,
	    	vfinfo->dwProductVersionLS >> 16,
	    	vfinfo->dwProductVersionLS &  0xffff );
	    } else {
	      printf("FileVersion      : %d,%d,%d,%d\n",
	    	vfinfo->dwFileVersionMS >> 16,
	    	vfinfo->dwFileVersionMS &  0xffff,
	    	vfinfo->dwFileVersionLS >> 16,
	    	vfinfo->dwFileVersionLS &  0xffff );
	      printf("ProductVersion   : %d,%d,%d,%d\n",
	    	vfinfo->dwProductVersionMS >> 16,
	    	vfinfo->dwProductVersionMS &  0xffff,
	    	vfinfo->dwProductVersionLS >> 16,
	    	vfinfo->dwProductVersionLS &  0xffff );
	    }
        } else {
            printf("GetFileVersionInfo() error.\n");
	}
	i = 0;
	while (header[i] != NULL) {
	     if (!GetRcValue(buf, header[i], &str)) {
	         if (csvf) {
                     printf(",\"%s\"", str);
		 } else {
                     printf("%-16s : [%s]\n", header[i], str);
		 }
	     }
	     ++i;
	}
	printf("\n");
      } else {
        if (csvf) {
            printf(",no version info.\n");
	} else {
            printf("no version info.\n");
	}
      }
    }
    return 0;
}

