/*
 *  hdinfo
 *
 *    display pertition info and fat info
 *
 *    07/02/25 V0.10 by oga.
 *    07/03/03 V0.11 bigfile support
 *    07/03/04 V0.12 -o -s: recovery option support
 *    07/03/07 V0.13 -ds support & change union description
 *    07/03/08 V0.14 -l support (disp date)
 *    07/03/25 V0.15 support WIN32
 *
 */

/*
 *  /dev/hda
 *  +-------------+
 *  |MBR          |
 *  +-------------+
 *
 *  /dev/hda1
 *  +-------------+ <- sector 0
 *  |BPB16/32     |
 *  |boot img     |
 *  |         55aa|
 *  +-------------+ <- sector 1 (ex.) / 0x0200
 *  |FSINFO       |
 *  |RRaA........ |
 *  |....rrAaxxxx |
 *  |         55aa|
 *  +-------------+ <- sector 2 (ex.) / 0x0400
 *  |???          |
 *  |         55aa|
 *  +-------------+
 *  |      :      |
 *  +-------------+ <- sector 6 (ex.) / 0x0c00
 *  |BPB16/32 copy|
 *  |boot img copy|
 *  |         55aa|
 *  +-------------+ <- sector 7 (ex.) / 0x0e00
 *  |FSINFO   copy|
 *  |RRaA........ |
 *  |....rrAaxxxx |
 *  |         55aa|
 *  +-------------+ <- sector 8 (ex.) / 0x1000
 *  |      :      |
 *  +-------------+ <- fat_start_sect / fat_start_pos(bytes)
 *  |FAT1         |
 *  +-------------+ <- fat_start_sect + fat_sectors /
 *  |FAT2         |
 *  +-------------+
 *  |      :      |
 *  +-------------+ <-                / root_dir_pos
 *  |ROOTdir      |
 *  +-------------+
 *
 *  DirEnt
 *  [ShortName]                          [LongName]
 *  +00 Filename[8]                      +00 SequenceByte[1]
 *  +08 Suffix[3]                        +01 UnicodeFileName1[10]
 *  +0b Attr[1]                          +0b Attr[1] 0x0f fix
 *  +0c Reserved[1]                      +0c LongEntryType[1] 0x00 fix
 *  +0d Creation Time 10ms[1] (VFAT)     +0d ShortNameCheckSum[1]
 *  +0e Creation Time[2]      (VFAT)     +0e UnicodeFileName2[12]
 *  +10 Creation Date[2]      (VFAT)
 *  +12 Access Date[2]        (VFAT)
 *  +14 File Clustor High[2]  (FAT32)
 *  +16 Modification Time[2]
 *  +18 Modification Date[2]
 *  +1a File Clustor Low[2]              +1a Reserved[2] 0x0000 fix
 *  +1c File Size[2]                     +1c UnicodeFileName3[4]
 *
 *
 *
 */

#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

/* globals */
int vf     = 0;   /* -v:    verbose       */
int mapf   = 0;   /* -m:    disp map      */
int dirf   = 0;   /* -d:    disp dirent   */
int dumpf  = 0;   /* -dump: dump dirent   */
int dsf    = 0;   /* -ds:   search dirent V0.13-A */
int clustf = 0;   /* -c:    clust dump    */
int lf     = 0;   /* -lf:   disp date     V0.14-A */

/* defines */
#define VERSION           "0.15"
#define dprintf           if (vf) printf
#define CLUST2POS(bpbp, clustf) \
	((((uint64)(bpbp)->fat_start_sect) \
         + (bpbp)->num_of_fat*(bpbp)->fat_sectors \
         + ((clustf)-2) * (bpbp)->sects_per_clust) \
           * (bpbp)->sector_size)

#define PAGE_SIZE 4096   /* FAT cache page size */
#ifdef _WIN32
#define lseek64	_lseeki64
#define open64	open
#endif


/* typedefs */
typedef unsigned char          uchar;
#ifdef _WIN32
typedef unsigned short         ushort;
typedef unsigned int           uint;
typedef unsigned __int64       uint64;
typedef __int64                off64_t;
#else
typedef unsigned long long int uint64;
#endif

/* BPB FAT12/16 for read */
typedef struct _bpb16 {
  char    jump_inst[3];       /* +00 Jump Instruction                  */
  char    oem_label[8];       /* +03 OEM Label                         */
  char  s_sector_size[2];     /* +0b Logical Sector Size               */
  uchar   sects_per_clust;    /* +0d Sectors per Cluster               */
  char  s_reserve_sect[2];    /* +0e Reserved Sector(FAT Start Sector) */
  uchar   num_of_fat;         /* +10 number of FAT (2)                 */
  char  s_max_root_dirent[2]; /* +11 Max Root Dir Entry(forFAT12/16)   */
  char  s_total_sectors[2];   /* +13 Total Sectors     (forFAT12/16)   */
  uchar   media_desc;         /* +15 Media Descriptor  (HDD:0xf8)      */
  char  s_fat_sectors[2];     /* +16 FAT Sectors       (forFAT12/16)   */
  char  s_sect_per_track[2];  /* +18 Sectors per Track                 */
  char  s_num_of_head[2];     /* +1a Num of Head                       */
  char  i_secret_sects[4];    /* +1c Secret Sectors                    */
  char  i_total_sectors[4];   /* +20 Total Sectors(Large) (forFAT32)   */

  uchar   pdrive_num;         /* +24 Physical driver Num               */
  char    reserved2[1];       /* +25 #Reserved2                        */
  uchar   boot_sign;          /* +26 Boot Signature                    */
  char  i_vol_serial[4];      /* +27 Volume Serial Number              */
  char    vol_label[11];      /* +2b Volume Label                      */
  char    filesys_type[8];    /* +36 Filesytem Type                    */
                              /* +3a End                               */
} bpb16_t;

/* BPB FAT32 for read */
typedef struct _bpb32 {
  char    jump_inst[3];       /* +00 Jump Instruction                  */
  char    oem_label[8];       /* +03 OEM Label                         */
  char  s_sector_size[2];     /* +0b Logical Sector Size               */
  uchar   sects_per_clust;    /* +0d Sector Size per Cluster           */
  char  s_reserve_sect[2];    /* +0e Reserved Sector(FAT Start Sector) */
  uchar   num_of_fat;         /* +10 number of FAT (2)                 */
  char  s_max_root_dirent[2]; /* +11 Max Root Dir Entry(forFAT12/16)   */
  char  s_total_sectors[2];   /* +13 Total Sectors     (forFAT12/16)   */
  uchar   media_desc;         /* +15 Media Descriptor  (HDD:0xf8)      */
  char  s_fat_sectors[2];     /* +16 FAT Sectors       (forFAT12/16)   */
  char  s_sect_per_track[2];  /* +18 Sectors per Track                 */
  char  s_num_of_head[2];     /* +1a Num of Head                       */
  char  i_secret_sects[4];    /* +1c Secret Sectors                    */
  char  i_total_sectors[4];   /* +20 Total Sectors(Large) (forFAT32)   */

  char  i_sect_per_fat[4];    /* +24 Sectors per FAT      (forFAT32)   */
  char  s_media_desc[2];      /* +28 Media Description flg(forFAT32)   */
  char  s_filesys_ver[2];     /* +2a Filesystem Version   (forFAT32)   */
  char  i_root_clust[4];      /* +2c Root Dir Start Clust (forFAT32)   */
  char  s_fsinfo_sect[2];     /* +30 FSINFO Sect          (forFAT32)   */
  char  s_boot_sect_copy[2];  /* +32 Boot Copy Sector     (forFAT32)   */
  char    reserved1[12];      /* +34 #Reserved1           (forFAT32)   */

  uchar   pdrive_num;         /* +40 Physical driver Num               */
  char    reserved2[1];       /* +41 Reserved2                         */
  uchar   boot_sign;          /* +42 Boot Signature                    */
  char  i_vol_serial[4];      /* +43 Volume Serial Number              */
  char    vol_label[11];      /* +47 Volume Label                      */
  char    filesys_type[8];    /* +52 Filesytem Type                    */
                              /* +5a End                               */
} bpb32_t;

/* BPB common (BIOS Parameter Block) */
typedef struct _bpb {
  uchar  fat_type;            /* FAT Type 0:NTFS/12/16/32              */
  uint   secret_size;         /* Secret Area Size (bytes)              */
  uint   clustor_size;        /* Clustor Size     (bytes)              */
  uint   root_dir_pos;        /* Root Dir Pos                          */
  uint   fat_start_pos;       /* FAT1 Start Pos                        */

  ushort sector_size;         /* +0b Logical Sector Size               */
  uchar  sects_per_clust;     /* +0d Sector Size per Cluster           */
  ushort fat_start_sect;      /* +0e Reserved Sector(FAT Start Sector) */
  uchar  num_of_fat;          /* +10 number of FAT (2)                 */
  uint   total_sectors;       /* +13/+20 Total Sectors                 */
  uint   secret_sects;        /* +1c Secret Sectors                    */
  ushort fat_sectors;         /* +16 FAT Sectors     (forFAT12/16)(32) */

  /* FAT16 */
  ushort max_root_dirent;     /* +11 Max Root Dir Entry(forFAT12/16)   */

  /* FAT32 */
  ushort filesys_ver;         /* +2a Filesystem Version   (forFAT32)   */
  //uint   sect_per_fat;        /* +24 Sectors per FAT      (forFAT32)   */
  uint   root_clust;          /* +2c Root Dir Start Clust (forFAT32)   */
  ushort fsinfo_sect;         /* +30 FSINFO Sect          (forFAT32)   */
  ushort boot_sect_copy;      /* +32 Boot Copy Sector     (forFAT32)   */
} bpb_t;

/* for FAT32 */
typedef struct _fsinfo {
  char    filesys_sig1[4];    /* +000 Filesystem Signature1 ("RRaA")   */
  char    reserved[0x1e0];    /* +004 Reserved                         */
  char    filesys_sig2[4];    /* +1e4 Filesystem Signature2 ("rrAa")   */
  char  i_free_clusts[4];     /* +1e8 Free Clusters                    */
  char  i_last_mod_clust[4];  /* +1ec Last Write Cluster               */
  char    reserved1[6];       /* +1f0 Reserved                         */
} FSINFO;

/* dirent atter */
#define ENT_ATTR_READONLY   0x01
#define ENT_ATTR_HIDDEN     0x02
#define ENT_ATTR_SYSFILE    0x04
#define ENT_ATTR_VOLLABEL   0x08
#define ENT_ATTR_DIR        0x10
#define ENT_ATTR_FILE       0x20  /* Archive? */

/*
 *   dir entry format  (maybe)
 *   [file1 long  entry1]
 *   [file1 long  entry2]
 *   [file1 long  entry:]
 *   [file1 long  entryn]
 *   [file1 short entry1]
 */

/* short 8.3 filename entry */
typedef struct _dirent_short {
    char   filename[8];    /*  +00 Filename[8]                  */
    char   suffix[3];      /*  +08 Suffix[3]                    */
    uchar  attr;           /*  +0b Attr[1]                      */
    char   reserved[1];    /*  +0c Reserved[1]                  */
    uchar  ctime10ms;      /*  +0d Creation Time 10ms[1] (VFAT) */
    char   ctime[2];       /*  +0e Creation Time[2]      (VFAT) */
    char   cdate[2];       /*  +10 Creation Date[2]      (VFAT) */
    char   adate[2];       /*  +12 Access Date[2]        (VFAT) */
    char   fcl_high[2];    /*  +14 File Clustor High[2]  (FAT32)*/
    char   mtime[2];       /*  +16 Modification Time[2]         */
    char   mdate[2];       /*  +18 Modification Date[2]         */
    char   fcl_low[2];     /*  +1a File Clustor Low[2]          */
    char   file_size[4];   /*  +1c File Size[4]                 */
} dirent_short_t;

/* long filename entry */
typedef struct _dirent_long {
    uchar  seq_byte;       /* +00 SequenceByte[1]               */
    char   uni_fname1[10]; /* +01 UnicodeFileName1[10]          */
    uchar  attr;           /* +0b Attr[1] 0x0f fix (long mark)  */
    uchar  longent_type;   /* +0c LongEntryType[1] 0x00 fix     */
    uchar  sortname_sum;   /* +0d ShortNameCheckSum[1]          */
    char   uni_fname2[12]; /* +0e UnicodeFileName2[12]          */
    char   reserved[2];    /* +1a Reserved[2] 0x0000 fix        */
    char   uni_fname3[4];  /* +1c UnicodeFileName3[4]           */
} dirent_long_t;

typedef struct _fat_dirent {
    union {
        dirent_short_t dshort;
        dirent_long_t  dlong;
    } u;
} fat_dirent_t;

/* date/time */
typedef struct _datetime_t {
    short year;  /* year yyyy */
    short month; /* month     */
    short day;   /* date      */
    short hour;  /* hour      */
    short min;   /* year yyyy */
    short sec;   /* year yyyy */
} datetime_t;

/* globals */
int  cur_fatpage = -1;     /* FAT cache page */
char fatdata[PAGE_SIZE];   /* FAT cache data */

/* cp932 to Unicode table Ver2.0.1 */
unsigned short sjis_uni_tbl[] =
{
//  SJIS, UNI
    0x01, 0x0001, //#START OF HEADING
    0x02, 0x0002, //#START OF TEXT
    0x03, 0x0003, //#END OF TEXT
    0x04, 0x0004, //#END OF TRANSMISSION
    0x05, 0x0005, //#ENQUIRY
    0x06, 0x0006, //#ACKNOWLEDGE
    0x07, 0x0007, //#BELL
    0x08, 0x0008, //#BACKSPACE
    0x09, 0x0009, //#HORIZONTAL TABULATION
    0x0A, 0x000A, //#LINE FEED
    0x0B, 0x000B, //#VERTICAL TABULATION
    0x0C, 0x000C, //#FORM FEED
    0x0D, 0x000D, //#CARRIAGE RETURN
    0x0E, 0x000E, //#SHIFT OUT
    0x0F, 0x000F, //#SHIFT IN
    0x10, 0x0010, //#DATA LINK ESCAPE
    0x11, 0x0011, //#DEVICE CONTROL ONE
    0x12, 0x0012, //#DEVICE CONTROL TWO
    0x13, 0x0013, //#DEVICE CONTROL THREE
    0x14, 0x0014, //#DEVICE CONTROL FOUR
    0x15, 0x0015, //#NEGATIVE ACKNOWLEDGE
    0x16, 0x0016, //#SYNCHRONOUS IDLE
    0x17, 0x0017, //#END OF TRANSMISSION BLOCK
    0x18, 0x0018, //#CANCEL
    0x19, 0x0019, //#END OF MEDIUM
    0x1A, 0x001A, //#SUBSTITUTE
    0x1B, 0x001B, //#ESCAPE
    0x1C, 0x001C, //#FILE SEPARATOR
    0x1D, 0x001D, //#GROUP SEPARATOR
    0x1E, 0x001E, //#RECORD SEPARATOR
    0x1F, 0x001F, //#UNIT SEPARATOR
    0x20, 0x0020, //#SPACE
    0x21, 0x0021, //#EXCLAMATION MARK
    0x22, 0x0022, //#QUOTATION MARK
    0x23, 0x0023, //#NUMBER SIGN
    0x24, 0x0024, //#DOLLAR SIGN
    0x25, 0x0025, //#PERCENT SIGN
    0x26, 0x0026, //#AMPERSAND
    0x27, 0x0027, //#APOSTROPHE
    0x28, 0x0028, //#LEFT PARENTHESIS
    0x29, 0x0029, //#RIGHT PARENTHESIS
    0x2A, 0x002A, //#ASTERISK
    0x2B, 0x002B, //#PLUS SIGN
    0x2C, 0x002C, //#COMMA
    0x2D, 0x002D, //#HYPHEN-MINUS
    0x2E, 0x002E, //#FULL STOP
    0x2F, 0x002F, //#SOLIDUS
    0x30, 0x0030, //#DIGIT ZERO
    0x31, 0x0031, //#DIGIT ONE
    0x32, 0x0032, //#DIGIT TWO
    0x33, 0x0033, //#DIGIT THREE
    0x34, 0x0034, //#DIGIT FOUR
    0x35, 0x0035, //#DIGIT FIVE
    0x36, 0x0036, //#DIGIT SIX
    0x37, 0x0037, //#DIGIT SEVEN
    0x38, 0x0038, //#DIGIT EIGHT
    0x39, 0x0039, //#DIGIT NINE
    0x3A, 0x003A, //#COLON
    0x3B, 0x003B, //#SEMICOLON
    0x3C, 0x003C, //#LESS-THAN SIGN
    0x3D, 0x003D, //#EQUALS SIGN
    0x3E, 0x003E, //#GREATER-THAN SIGN
    0x3F, 0x003F, //#QUESTION MARK
    0x40, 0x0040, //#COMMERCIAL AT
    0x41, 0x0041, //#LATIN CAPITAL LETTER A
    0x42, 0x0042, //#LATIN CAPITAL LETTER B
    0x43, 0x0043, //#LATIN CAPITAL LETTER C
    0x44, 0x0044, //#LATIN CAPITAL LETTER D
    0x45, 0x0045, //#LATIN CAPITAL LETTER E
    0x46, 0x0046, //#LATIN CAPITAL LETTER F
    0x47, 0x0047, //#LATIN CAPITAL LETTER G
    0x48, 0x0048, //#LATIN CAPITAL LETTER H
    0x49, 0x0049, //#LATIN CAPITAL LETTER I
    0x4A, 0x004A, //#LATIN CAPITAL LETTER J
    0x4B, 0x004B, //#LATIN CAPITAL LETTER K
    0x4C, 0x004C, //#LATIN CAPITAL LETTER L
    0x4D, 0x004D, //#LATIN CAPITAL LETTER M
    0x4E, 0x004E, //#LATIN CAPITAL LETTER N
    0x4F, 0x004F, //#LATIN CAPITAL LETTER O
    0x50, 0x0050, //#LATIN CAPITAL LETTER P
    0x51, 0x0051, //#LATIN CAPITAL LETTER Q
    0x52, 0x0052, //#LATIN CAPITAL LETTER R
    0x53, 0x0053, //#LATIN CAPITAL LETTER S
    0x54, 0x0054, //#LATIN CAPITAL LETTER T
    0x55, 0x0055, //#LATIN CAPITAL LETTER U
    0x56, 0x0056, //#LATIN CAPITAL LETTER V
    0x57, 0x0057, //#LATIN CAPITAL LETTER W
    0x58, 0x0058, //#LATIN CAPITAL LETTER X
    0x59, 0x0059, //#LATIN CAPITAL LETTER Y
    0x5A, 0x005A, //#LATIN CAPITAL LETTER Z
    0x5B, 0x005B, //#LEFT SQUARE BRACKET
    0x5C, 0x005C, //#REVERSE SOLIDUS
    0x5D, 0x005D, //#RIGHT SQUARE BRACKET
    0x5E, 0x005E, //#CIRCUMFLEX ACCENT
    0x5F, 0x005F, //#LOW LINE
    0x60, 0x0060, //#GRAVE ACCENT
    0x61, 0x0061, //#LATIN SMALL LETTER A
    0x62, 0x0062, //#LATIN SMALL LETTER B
    0x63, 0x0063, //#LATIN SMALL LETTER C
    0x64, 0x0064, //#LATIN SMALL LETTER D
    0x65, 0x0065, //#LATIN SMALL LETTER E
    0x66, 0x0066, //#LATIN SMALL LETTER F
    0x67, 0x0067, //#LATIN SMALL LETTER G
    0x68, 0x0068, //#LATIN SMALL LETTER H
    0x69, 0x0069, //#LATIN SMALL LETTER I
    0x6A, 0x006A, //#LATIN SMALL LETTER J
    0x6B, 0x006B, //#LATIN SMALL LETTER K
    0x6C, 0x006C, //#LATIN SMALL LETTER L
    0x6D, 0x006D, //#LATIN SMALL LETTER M
    0x6E, 0x006E, //#LATIN SMALL LETTER N
    0x6F, 0x006F, //#LATIN SMALL LETTER O
    0x70, 0x0070, //#LATIN SMALL LETTER P
    0x71, 0x0071, //#LATIN SMALL LETTER Q
    0x72, 0x0072, //#LATIN SMALL LETTER R
    0x73, 0x0073, //#LATIN SMALL LETTER S
    0x74, 0x0074, //#LATIN SMALL LETTER T
    0x75, 0x0075, //#LATIN SMALL LETTER U
    0x76, 0x0076, //#LATIN SMALL LETTER V
    0x77, 0x0077, //#LATIN SMALL LETTER W
    0x78, 0x0078, //#LATIN SMALL LETTER X
    0x79, 0x0079, //#LATIN SMALL LETTER Y
    0x7A, 0x007A, //#LATIN SMALL LETTER Z
    0x7B, 0x007B, //#LEFT CURLY BRACKET
    0x7C, 0x007C, //#VERTICAL LINE
    0x7D, 0x007D, //#RIGHT CURLY BRACKET
    0x7E, 0x007E, //#TILDE
    0x7F, 0x007F, //#DELETE
#if 0
    0x80,       , //#UNDEFINED
    0x81,       , //#DBCS LEAD BYTE
    0x82,       , //#DBCS LEAD BYTE
    0x83,       , //#DBCS LEAD BYTE
    0x84,       , //#DBCS LEAD BYTE
    0x85,       , //#DBCS LEAD BYTE
    0x86,       , //#DBCS LEAD BYTE
    0x87,       , //#DBCS LEAD BYTE
    0x88,       , //#DBCS LEAD BYTE
    0x89,       , //#DBCS LEAD BYTE
    0x8A,       , //#DBCS LEAD BYTE
    0x8B,       , //#DBCS LEAD BYTE
    0x8C,       , //#DBCS LEAD BYTE
    0x8D,       , //#DBCS LEAD BYTE
    0x8E,       , //#DBCS LEAD BYTE
    0x8F,       , //#DBCS LEAD BYTE
    0x90,       , //#DBCS LEAD BYTE
    0x91,       , //#DBCS LEAD BYTE
    0x92,       , //#DBCS LEAD BYTE
    0x93,       , //#DBCS LEAD BYTE
    0x94,       , //#DBCS LEAD BYTE
    0x95,       , //#DBCS LEAD BYTE
    0x96,       , //#DBCS LEAD BYTE
    0x97,       , //#DBCS LEAD BYTE
    0x98,       , //#DBCS LEAD BYTE
    0x99,       , //#DBCS LEAD BYTE
    0x9A,       , //#DBCS LEAD BYTE
    0x9B,       , //#DBCS LEAD BYTE
    0x9C,       , //#DBCS LEAD BYTE
    0x9D,       , //#DBCS LEAD BYTE
    0x9E,       , //#DBCS LEAD BYTE
    0x9F,       , //#DBCS LEAD BYTE
    0xA0,       , //#UNDEFINED
#endif
    0xA1, 0xFF61, //#HALFWIDTH IDEOGRAPHIC FULL STOP
    0xA2, 0xFF62, //#HALFWIDTH LEFT CORNER BRACKET
    0xA3, 0xFF63, //#HALFWIDTH RIGHT CORNER BRACKET
    0xA4, 0xFF64, //#HALFWIDTH IDEOGRAPHIC COMMA
    0xA5, 0xFF65, //#HALFWIDTH KATAKANA MIDDLE DOT
    0xA6, 0xFF66, //#HALFWIDTH KATAKANA LETTER WO
    0xA7, 0xFF67, //#HALFWIDTH KATAKANA LETTER SMALL A
    0xA8, 0xFF68, //#HALFWIDTH KATAKANA LETTER SMALL I
    0xA9, 0xFF69, //#HALFWIDTH KATAKANA LETTER SMALL U
    0xAA, 0xFF6A, //#HALFWIDTH KATAKANA LETTER SMALL E
    0xAB, 0xFF6B, //#HALFWIDTH KATAKANA LETTER SMALL O
    0xAC, 0xFF6C, //#HALFWIDTH KATAKANA LETTER SMALL YA
    0xAD, 0xFF6D, //#HALFWIDTH KATAKANA LETTER SMALL YU
    0xAE, 0xFF6E, //#HALFWIDTH KATAKANA LETTER SMALL YO
    0xAF, 0xFF6F, //#HALFWIDTH KATAKANA LETTER SMALL TU
    0xB0, 0xFF70, //#HALFWIDTH KATAKANA-HIRAGANA PROLONGED SOUND MARK
    0xB1, 0xFF71, //#HALFWIDTH KATAKANA LETTER A
    0xB2, 0xFF72, //#HALFWIDTH KATAKANA LETTER I
    0xB3, 0xFF73, //#HALFWIDTH KATAKANA LETTER U
    0xB4, 0xFF74, //#HALFWIDTH KATAKANA LETTER E
    0xB5, 0xFF75, //#HALFWIDTH KATAKANA LETTER O
    0xB6, 0xFF76, //#HALFWIDTH KATAKANA LETTER KA
    0xB7, 0xFF77, //#HALFWIDTH KATAKANA LETTER KI
    0xB8, 0xFF78, //#HALFWIDTH KATAKANA LETTER KU
    0xB9, 0xFF79, //#HALFWIDTH KATAKANA LETTER KE
    0xBA, 0xFF7A, //#HALFWIDTH KATAKANA LETTER KO
    0xBB, 0xFF7B, //#HALFWIDTH KATAKANA LETTER SA
    0xBC, 0xFF7C, //#HALFWIDTH KATAKANA LETTER SI
    0xBD, 0xFF7D, //#HALFWIDTH KATAKANA LETTER SU
    0xBE, 0xFF7E, //#HALFWIDTH KATAKANA LETTER SE
    0xBF, 0xFF7F, //#HALFWIDTH KATAKANA LETTER SO
    0xC0, 0xFF80, //#HALFWIDTH KATAKANA LETTER TA
    0xC1, 0xFF81, //#HALFWIDTH KATAKANA LETTER TI
    0xC2, 0xFF82, //#HALFWIDTH KATAKANA LETTER TU
    0xC3, 0xFF83, //#HALFWIDTH KATAKANA LETTER TE
    0xC4, 0xFF84, //#HALFWIDTH KATAKANA LETTER TO
    0xC5, 0xFF85, //#HALFWIDTH KATAKANA LETTER NA
    0xC6, 0xFF86, //#HALFWIDTH KATAKANA LETTER NI
    0xC7, 0xFF87, //#HALFWIDTH KATAKANA LETTER NU
    0xC8, 0xFF88, //#HALFWIDTH KATAKANA LETTER NE
    0xC9, 0xFF89, //#HALFWIDTH KATAKANA LETTER NO
    0xCA, 0xFF8A, //#HALFWIDTH KATAKANA LETTER HA
    0xCB, 0xFF8B, //#HALFWIDTH KATAKANA LETTER HI
    0xCC, 0xFF8C, //#HALFWIDTH KATAKANA LETTER HU
    0xCD, 0xFF8D, //#HALFWIDTH KATAKANA LETTER HE
    0xCE, 0xFF8E, //#HALFWIDTH KATAKANA LETTER HO
    0xCF, 0xFF8F, //#HALFWIDTH KATAKANA LETTER MA
    0xD0, 0xFF90, //#HALFWIDTH KATAKANA LETTER MI
    0xD1, 0xFF91, //#HALFWIDTH KATAKANA LETTER MU
    0xD2, 0xFF92, //#HALFWIDTH KATAKANA LETTER ME
    0xD3, 0xFF93, //#HALFWIDTH KATAKANA LETTER MO
    0xD4, 0xFF94, //#HALFWIDTH KATAKANA LETTER YA
    0xD5, 0xFF95, //#HALFWIDTH KATAKANA LETTER YU
    0xD6, 0xFF96, //#HALFWIDTH KATAKANA LETTER YO
    0xD7, 0xFF97, //#HALFWIDTH KATAKANA LETTER RA
    0xD8, 0xFF98, //#HALFWIDTH KATAKANA LETTER RI
    0xD9, 0xFF99, //#HALFWIDTH KATAKANA LETTER RU
    0xDA, 0xFF9A, //#HALFWIDTH KATAKANA LETTER RE
    0xDB, 0xFF9B, //#HALFWIDTH KATAKANA LETTER RO
    0xDC, 0xFF9C, //#HALFWIDTH KATAKANA LETTER WA
    0xDD, 0xFF9D, //#HALFWIDTH KATAKANA LETTER N
    0xDE, 0xFF9E, //#HALFWIDTH KATAKANA VOICED SOUND MARK
    0xDF, 0xFF9F, //#HALFWIDTH KATAKANA SEMI-VOICED SOUND MARK
#if 0
    0xE0,       , //#DBCS LEAD BYTE
    0xE1,       , //#DBCS LEAD BYTE
    0xE2,       , //#DBCS LEAD BYTE
    0xE3,       , //#DBCS LEAD BYTE
    0xE4,       , //#DBCS LEAD BYTE
    0xE5,       , //#DBCS LEAD BYTE
    0xE6,       , //#DBCS LEAD BYTE
    0xE7,       , //#DBCS LEAD BYTE
    0xE8,       , //#DBCS LEAD BYTE
    0xE9,       , //#DBCS LEAD BYTE
    0xEA,       , //#DBCS LEAD BYTE
    0xEB,       , //#DBCS LEAD BYTE
    0xEC,       , //#DBCS LEAD BYTE
    0xED,       , //#DBCS LEAD BYTE
    0xEE,       , //#DBCS LEAD BYTE
    0xEF,       , //#DBCS LEAD BYTE
    0xF0,       , //#DBCS LEAD BYTE
    0xF1,       , //#DBCS LEAD BYTE
    0xF2,       , //#DBCS LEAD BYTE
    0xF3,       , //#DBCS LEAD BYTE
    0xF4,       , //#DBCS LEAD BYTE
    0xF5,       , //#DBCS LEAD BYTE
    0xF6,       , //#DBCS LEAD BYTE
    0xF7,       , //#DBCS LEAD BYTE
    0xF8,       , //#DBCS LEAD BYTE
    0xF9,       , //#DBCS LEAD BYTE
    0xFA,       , //#DBCS LEAD BYTE
    0xFB,       , //#DBCS LEAD BYTE
    0xFC,       , //#DBCS LEAD BYTE
    0xFD,       , //#UNDEFINED
    0xFE,       , //#UNDEFINED
    0xFF,       , //#UNDEFINED
#endif
    0x8140, 0x3000, //#IDEOGRAPHIC SPACE
    0x8141, 0x3001, //#IDEOGRAPHIC COMMA
    0x8142, 0x3002, //#IDEOGRAPHIC FULL STOP
    0x8143, 0xFF0C, //#FULLWIDTH COMMA
    0x8144, 0xFF0E, //#FULLWIDTH FULL STOP
    0x8145, 0x30FB, //#KATAKANA MIDDLE DOT
    0x8146, 0xFF1A, //#FULLWIDTH COLON
    0x8147, 0xFF1B, //#FULLWIDTH SEMICOLON
    0x8148, 0xFF1F, //#FULLWIDTH QUESTION MARK
    0x8149, 0xFF01, //#FULLWIDTH EXCLAMATION MARK
    0x814A, 0x309B, //#KATAKANA-HIRAGANA VOICED SOUND MARK
    0x814B, 0x309C, //#KATAKANA-HIRAGANA SEMI-VOICED SOUND MARK
    0x814C, 0x00B4, //#ACUTE ACCENT
    0x814D, 0xFF40, //#FULLWIDTH GRAVE ACCENT
    0x814E, 0x00A8, //#DIAERESIS
    0x814F, 0xFF3E, //#FULLWIDTH CIRCUMFLEX ACCENT
    0x8150, 0xFFE3, //#FULLWIDTH MACRON
    0x8151, 0xFF3F, //#FULLWIDTH LOW LINE
    0x8152, 0x30FD, //#KATAKANA ITERATION MARK
    0x8153, 0x30FE, //#KATAKANA VOICED ITERATION MARK
    0x8154, 0x309D, //#HIRAGANA ITERATION MARK
    0x8155, 0x309E, //#HIRAGANA VOICED ITERATION MARK
    0x8156, 0x3003, //#DITTO MARK
    0x8157, 0x4EDD, //#CJK UNIFIED IDEOGRAPH
    0x8158, 0x3005, //#IDEOGRAPHIC ITERATION MARK
    0x8159, 0x3006, //#IDEOGRAPHIC CLOSING MARK
    0x815A, 0x3007, //#IDEOGRAPHIC NUMBER ZERO
    0x815B, 0x30FC, //#KATAKANA-HIRAGANA PROLONGED SOUND MARK
    0x815C, 0x2015, //#HORIZONTAL BAR
    0x815D, 0x2010, //#HYPHEN
    0x815E, 0xFF0F, //#FULLWIDTH SOLIDUS
    0x815F, 0xFF3C, //#FULLWIDTH REVERSE SOLIDUS
    0x8160, 0xFF5E, //#FULLWIDTH TILDE
    0x8161, 0x2225, //#PARALLEL TO
    0x8162, 0xFF5C, //#FULLWIDTH VERTICAL LINE
    0x8163, 0x2026, //#HORIZONTAL ELLIPSIS
    0x8164, 0x2025, //#TWO DOT LEADER
    0x8165, 0x2018, //#LEFT SINGLE QUOTATION MARK
    0x8166, 0x2019, //#RIGHT SINGLE QUOTATION MARK
    0x8167, 0x201C, //#LEFT DOUBLE QUOTATION MARK
    0x8168, 0x201D, //#RIGHT DOUBLE QUOTATION MARK
    0x8169, 0xFF08, //#FULLWIDTH LEFT PARENTHESIS
    0x816A, 0xFF09, //#FULLWIDTH RIGHT PARENTHESIS
    0x816B, 0x3014, //#LEFT TORTOISE SHELL BRACKET
    0x816C, 0x3015, //#RIGHT TORTOISE SHELL BRACKET
    0x816D, 0xFF3B, //#FULLWIDTH LEFT SQUARE BRACKET
    0x816E, 0xFF3D, //#FULLWIDTH RIGHT SQUARE BRACKET
    0x816F, 0xFF5B, //#FULLWIDTH LEFT CURLY BRACKET
    0x8170, 0xFF5D, //#FULLWIDTH RIGHT CURLY BRACKET
    0x8171, 0x3008, //#LEFT ANGLE BRACKET
    0x8172, 0x3009, //#RIGHT ANGLE BRACKET
    0x8173, 0x300A, //#LEFT DOUBLE ANGLE BRACKET
    0x8174, 0x300B, //#RIGHT DOUBLE ANGLE BRACKET
    0x8175, 0x300C, //#LEFT CORNER BRACKET
    0x8176, 0x300D, //#RIGHT CORNER BRACKET
    0x8177, 0x300E, //#LEFT WHITE CORNER BRACKET
    0x8178, 0x300F, //#RIGHT WHITE CORNER BRACKET
    0x8179, 0x3010, //#LEFT BLACK LENTICULAR BRACKET
    0x817A, 0x3011, //#RIGHT BLACK LENTICULAR BRACKET
    0x817B, 0xFF0B, //#FULLWIDTH PLUS SIGN
    0x817C, 0xFF0D, //#FULLWIDTH HYPHEN-MINUS
    0x817D, 0x00B1, //#PLUS-MINUS SIGN
    0x817E, 0x00D7, //#MULTIPLICATION SIGN
    0x8180, 0x00F7, //#DIVISION SIGN
    0x8181, 0xFF1D, //#FULLWIDTH EQUALS SIGN
    0x8182, 0x2260, //#NOT EQUAL TO
    0x8183, 0xFF1C, //#FULLWIDTH LESS-THAN SIGN
    0x8184, 0xFF1E, //#FULLWIDTH GREATER-THAN SIGN
    0x8185, 0x2266, //#LESS-THAN OVER EQUAL TO
    0x8186, 0x2267, //#GREATER-THAN OVER EQUAL TO
    0x8187, 0x221E, //#INFINITY
    0x8188, 0x2234, //#THEREFORE
    0x8189, 0x2642, //#MALE SIGN
    0x818A, 0x2640, //#FEMALE SIGN
    0x818B, 0x00B0, //#DEGREE SIGN
    0x818C, 0x2032, //#PRIME
    0x818D, 0x2033, //#DOUBLE PRIME
    0x818E, 0x2103, //#DEGREE CELSIUS
    0x818F, 0xFFE5, //#FULLWIDTH YEN SIGN
    0x8190, 0xFF04, //#FULLWIDTH DOLLAR SIGN
    0x8191, 0xFFE0, //#FULLWIDTH CENT SIGN
    0x8192, 0xFFE1, //#FULLWIDTH POUND SIGN
    0x8193, 0xFF05, //#FULLWIDTH PERCENT SIGN
    0x8194, 0xFF03, //#FULLWIDTH NUMBER SIGN
    0x8195, 0xFF06, //#FULLWIDTH AMPERSAND
    0x8196, 0xFF0A, //#FULLWIDTH ASTERISK
    0x8197, 0xFF20, //#FULLWIDTH COMMERCIAL AT
    0x8198, 0x00A7, //#SECTION SIGN
    0x8199, 0x2606, //#WHITE STAR
    0x819A, 0x2605, //#BLACK STAR
    0x819B, 0x25CB, //#WHITE CIRCLE
    0x819C, 0x25CF, //#BLACK CIRCLE
    0x819D, 0x25CE, //#BULLSEYE
    0x819E, 0x25C7, //#WHITE DIAMOND
    0x819F, 0x25C6, //#BLACK DIAMOND
    0x81A0, 0x25A1, //#WHITE SQUARE
    0x81A1, 0x25A0, //#BLACK SQUARE
    0x81A2, 0x25B3, //#WHITE UP-POINTING TRIANGLE
    0x81A3, 0x25B2, //#BLACK UP-POINTING TRIANGLE
    0x81A4, 0x25BD, //#WHITE DOWN-POINTING TRIANGLE
    0x81A5, 0x25BC, //#BLACK DOWN-POINTING TRIANGLE
    0x81A6, 0x203B, //#REFERENCE MARK
    0x81A7, 0x3012, //#POSTAL MARK
    0x81A8, 0x2192, //#RIGHTWARDS ARROW
    0x81A9, 0x2190, //#LEFTWARDS ARROW
    0x81AA, 0x2191, //#UPWARDS ARROW
    0x81AB, 0x2193, //#DOWNWARDS ARROW
    0x81AC, 0x3013, //#GETA MARK
    0x81B8, 0x2208, //#ELEMENT OF
    0x81B9, 0x220B, //#CONTAINS AS MEMBER
    0x81BA, 0x2286, //#SUBSET OF OR EQUAL TO
    0x81BB, 0x2287, //#SUPERSET OF OR EQUAL TO
    0x81BC, 0x2282, //#SUBSET OF
    0x81BD, 0x2283, //#SUPERSET OF
    0x81BE, 0x222A, //#UNION
    0x81BF, 0x2229, //#INTERSECTION
    0x81C8, 0x2227, //#LOGICAL AND
    0x81C9, 0x2228, //#LOGICAL OR
    0x81CA, 0xFFE2, //#FULLWIDTH NOT SIGN
    0x81CB, 0x21D2, //#RIGHTWARDS DOUBLE ARROW
    0x81CC, 0x21D4, //#LEFT RIGHT DOUBLE ARROW
    0x81CD, 0x2200, //#FOR ALL
    0x81CE, 0x2203, //#THERE EXISTS
    0x81DA, 0x2220, //#ANGLE
    0x81DB, 0x22A5, //#UP TACK
    0x81DC, 0x2312, //#ARC
    0x81DD, 0x2202, //#PARTIAL DIFFERENTIAL
    0x81DE, 0x2207, //#NABLA
    0x81DF, 0x2261, //#IDENTICAL TO
    0x81E0, 0x2252, //#APPROXIMATELY EQUAL TO OR THE IMAGE OF
    0x81E1, 0x226A, //#MUCH LESS-THAN
    0x81E2, 0x226B, //#MUCH GREATER-THAN
    0x81E3, 0x221A, //#SQUARE ROOT
    0x81E4, 0x223D, //#REVERSED TILDE
    0x81E5, 0x221D, //#PROPORTIONAL TO
    0x81E6, 0x2235, //#BECAUSE
    0x81E7, 0x222B, //#INTEGRAL
    0x81E8, 0x222C, //#DOUBLE INTEGRAL
    0x81F0, 0x212B, //#ANGSTROM SIGN
    0x81F1, 0x2030, //#PER MILLE SIGN
    0x81F2, 0x266F, //#MUSIC SHARP SIGN
    0x81F3, 0x266D, //#MUSIC FLAT SIGN
    0x81F4, 0x266A, //#EIGHTH NOTE
    0x81F5, 0x2020, //#DAGGER
    0x81F6, 0x2021, //#DOUBLE DAGGER
    0x81F7, 0x00B6, //#PILCROW SIGN
    0x81FC, 0x25EF, //#LARGE CIRCLE
    0x824F, 0xFF10, //#FULLWIDTH DIGIT ZERO
    0x8250, 0xFF11, //#FULLWIDTH DIGIT ONE
    0x8251, 0xFF12, //#FULLWIDTH DIGIT TWO
    0x8252, 0xFF13, //#FULLWIDTH DIGIT THREE
    0x8253, 0xFF14, //#FULLWIDTH DIGIT FOUR
    0x8254, 0xFF15, //#FULLWIDTH DIGIT FIVE
    0x8255, 0xFF16, //#FULLWIDTH DIGIT SIX
    0x8256, 0xFF17, //#FULLWIDTH DIGIT SEVEN
    0x8257, 0xFF18, //#FULLWIDTH DIGIT EIGHT
    0x8258, 0xFF19, //#FULLWIDTH DIGIT NINE
    0x8260, 0xFF21, //#FULLWIDTH LATIN CAPITAL LETTER A
    0x8261, 0xFF22, //#FULLWIDTH LATIN CAPITAL LETTER B
    0x8262, 0xFF23, //#FULLWIDTH LATIN CAPITAL LETTER C
    0x8263, 0xFF24, //#FULLWIDTH LATIN CAPITAL LETTER D
    0x8264, 0xFF25, //#FULLWIDTH LATIN CAPITAL LETTER E
    0x8265, 0xFF26, //#FULLWIDTH LATIN CAPITAL LETTER F
    0x8266, 0xFF27, //#FULLWIDTH LATIN CAPITAL LETTER G
    0x8267, 0xFF28, //#FULLWIDTH LATIN CAPITAL LETTER H
    0x8268, 0xFF29, //#FULLWIDTH LATIN CAPITAL LETTER I
    0x8269, 0xFF2A, //#FULLWIDTH LATIN CAPITAL LETTER J
    0x826A, 0xFF2B, //#FULLWIDTH LATIN CAPITAL LETTER K
    0x826B, 0xFF2C, //#FULLWIDTH LATIN CAPITAL LETTER L
    0x826C, 0xFF2D, //#FULLWIDTH LATIN CAPITAL LETTER M
    0x826D, 0xFF2E, //#FULLWIDTH LATIN CAPITAL LETTER N
    0x826E, 0xFF2F, //#FULLWIDTH LATIN CAPITAL LETTER O
    0x826F, 0xFF30, //#FULLWIDTH LATIN CAPITAL LETTER P
    0x8270, 0xFF31, //#FULLWIDTH LATIN CAPITAL LETTER Q
    0x8271, 0xFF32, //#FULLWIDTH LATIN CAPITAL LETTER R
    0x8272, 0xFF33, //#FULLWIDTH LATIN CAPITAL LETTER S
    0x8273, 0xFF34, //#FULLWIDTH LATIN CAPITAL LETTER T
    0x8274, 0xFF35, //#FULLWIDTH LATIN CAPITAL LETTER U
    0x8275, 0xFF36, //#FULLWIDTH LATIN CAPITAL LETTER V
    0x8276, 0xFF37, //#FULLWIDTH LATIN CAPITAL LETTER W
    0x8277, 0xFF38, //#FULLWIDTH LATIN CAPITAL LETTER X
    0x8278, 0xFF39, //#FULLWIDTH LATIN CAPITAL LETTER Y
    0x8279, 0xFF3A, //#FULLWIDTH LATIN CAPITAL LETTER Z
    0x8281, 0xFF41, //#FULLWIDTH LATIN SMALL LETTER A
    0x8282, 0xFF42, //#FULLWIDTH LATIN SMALL LETTER B
    0x8283, 0xFF43, //#FULLWIDTH LATIN SMALL LETTER C
    0x8284, 0xFF44, //#FULLWIDTH LATIN SMALL LETTER D
    0x8285, 0xFF45, //#FULLWIDTH LATIN SMALL LETTER E
    0x8286, 0xFF46, //#FULLWIDTH LATIN SMALL LETTER F
    0x8287, 0xFF47, //#FULLWIDTH LATIN SMALL LETTER G
    0x8288, 0xFF48, //#FULLWIDTH LATIN SMALL LETTER H
    0x8289, 0xFF49, //#FULLWIDTH LATIN SMALL LETTER I
    0x828A, 0xFF4A, //#FULLWIDTH LATIN SMALL LETTER J
    0x828B, 0xFF4B, //#FULLWIDTH LATIN SMALL LETTER K
    0x828C, 0xFF4C, //#FULLWIDTH LATIN SMALL LETTER L
    0x828D, 0xFF4D, //#FULLWIDTH LATIN SMALL LETTER M
    0x828E, 0xFF4E, //#FULLWIDTH LATIN SMALL LETTER N
    0x828F, 0xFF4F, //#FULLWIDTH LATIN SMALL LETTER O
    0x8290, 0xFF50, //#FULLWIDTH LATIN SMALL LETTER P
    0x8291, 0xFF51, //#FULLWIDTH LATIN SMALL LETTER Q
    0x8292, 0xFF52, //#FULLWIDTH LATIN SMALL LETTER R
    0x8293, 0xFF53, //#FULLWIDTH LATIN SMALL LETTER S
    0x8294, 0xFF54, //#FULLWIDTH LATIN SMALL LETTER T
    0x8295, 0xFF55, //#FULLWIDTH LATIN SMALL LETTER U
    0x8296, 0xFF56, //#FULLWIDTH LATIN SMALL LETTER V
    0x8297, 0xFF57, //#FULLWIDTH LATIN SMALL LETTER W
    0x8298, 0xFF58, //#FULLWIDTH LATIN SMALL LETTER X
    0x8299, 0xFF59, //#FULLWIDTH LATIN SMALL LETTER Y
    0x829A, 0xFF5A, //#FULLWIDTH LATIN SMALL LETTER Z
    0x829F, 0x3041, //#HIRAGANA LETTER SMALL A
    0x82A0, 0x3042, //#HIRAGANA LETTER A
    0x82A1, 0x3043, //#HIRAGANA LETTER SMALL I
    0x82A2, 0x3044, //#HIRAGANA LETTER I
    0x82A3, 0x3045, //#HIRAGANA LETTER SMALL U
    0x82A4, 0x3046, //#HIRAGANA LETTER U
    0x82A5, 0x3047, //#HIRAGANA LETTER SMALL E
    0x82A6, 0x3048, //#HIRAGANA LETTER E
    0x82A7, 0x3049, //#HIRAGANA LETTER SMALL O
    0x82A8, 0x304A, //#HIRAGANA LETTER O
    0x82A9, 0x304B, //#HIRAGANA LETTER KA
    0x82AA, 0x304C, //#HIRAGANA LETTER GA
    0x82AB, 0x304D, //#HIRAGANA LETTER KI
    0x82AC, 0x304E, //#HIRAGANA LETTER GI
    0x82AD, 0x304F, //#HIRAGANA LETTER KU
    0x82AE, 0x3050, //#HIRAGANA LETTER GU
    0x82AF, 0x3051, //#HIRAGANA LETTER KE
    0x82B0, 0x3052, //#HIRAGANA LETTER GE
    0x82B1, 0x3053, //#HIRAGANA LETTER KO
    0x82B2, 0x3054, //#HIRAGANA LETTER GO
    0x82B3, 0x3055, //#HIRAGANA LETTER SA
    0x82B4, 0x3056, //#HIRAGANA LETTER ZA
    0x82B5, 0x3057, //#HIRAGANA LETTER SI
    0x82B6, 0x3058, //#HIRAGANA LETTER ZI
    0x82B7, 0x3059, //#HIRAGANA LETTER SU
    0x82B8, 0x305A, //#HIRAGANA LETTER ZU
    0x82B9, 0x305B, //#HIRAGANA LETTER SE
    0x82BA, 0x305C, //#HIRAGANA LETTER ZE
    0x82BB, 0x305D, //#HIRAGANA LETTER SO
    0x82BC, 0x305E, //#HIRAGANA LETTER ZO
    0x82BD, 0x305F, //#HIRAGANA LETTER TA
    0x82BE, 0x3060, //#HIRAGANA LETTER DA
    0x82BF, 0x3061, //#HIRAGANA LETTER TI
    0x82C0, 0x3062, //#HIRAGANA LETTER DI
    0x82C1, 0x3063, //#HIRAGANA LETTER SMALL TU
    0x82C2, 0x3064, //#HIRAGANA LETTER TU
    0x82C3, 0x3065, //#HIRAGANA LETTER DU
    0x82C4, 0x3066, //#HIRAGANA LETTER TE
    0x82C5, 0x3067, //#HIRAGANA LETTER DE
    0x82C6, 0x3068, //#HIRAGANA LETTER TO
    0x82C7, 0x3069, //#HIRAGANA LETTER DO
    0x82C8, 0x306A, //#HIRAGANA LETTER NA
    0x82C9, 0x306B, //#HIRAGANA LETTER NI
    0x82CA, 0x306C, //#HIRAGANA LETTER NU
    0x82CB, 0x306D, //#HIRAGANA LETTER NE
    0x82CC, 0x306E, //#HIRAGANA LETTER NO
    0x82CD, 0x306F, //#HIRAGANA LETTER HA
    0x82CE, 0x3070, //#HIRAGANA LETTER BA
    0x82CF, 0x3071, //#HIRAGANA LETTER PA
    0x82D0, 0x3072, //#HIRAGANA LETTER HI
    0x82D1, 0x3073, //#HIRAGANA LETTER BI
    0x82D2, 0x3074, //#HIRAGANA LETTER PI
    0x82D3, 0x3075, //#HIRAGANA LETTER HU
    0x82D4, 0x3076, //#HIRAGANA LETTER BU
    0x82D5, 0x3077, //#HIRAGANA LETTER PU
    0x82D6, 0x3078, //#HIRAGANA LETTER HE
    0x82D7, 0x3079, //#HIRAGANA LETTER BE
    0x82D8, 0x307A, //#HIRAGANA LETTER PE
    0x82D9, 0x307B, //#HIRAGANA LETTER HO
    0x82DA, 0x307C, //#HIRAGANA LETTER BO
    0x82DB, 0x307D, //#HIRAGANA LETTER PO
    0x82DC, 0x307E, //#HIRAGANA LETTER MA
    0x82DD, 0x307F, //#HIRAGANA LETTER MI
    0x82DE, 0x3080, //#HIRAGANA LETTER MU
    0x82DF, 0x3081, //#HIRAGANA LETTER ME
    0x82E0, 0x3082, //#HIRAGANA LETTER MO
    0x82E1, 0x3083, //#HIRAGANA LETTER SMALL YA
    0x82E2, 0x3084, //#HIRAGANA LETTER YA
    0x82E3, 0x3085, //#HIRAGANA LETTER SMALL YU
    0x82E4, 0x3086, //#HIRAGANA LETTER YU
    0x82E5, 0x3087, //#HIRAGANA LETTER SMALL YO
    0x82E6, 0x3088, //#HIRAGANA LETTER YO
    0x82E7, 0x3089, //#HIRAGANA LETTER RA
    0x82E8, 0x308A, //#HIRAGANA LETTER RI
    0x82E9, 0x308B, //#HIRAGANA LETTER RU
    0x82EA, 0x308C, //#HIRAGANA LETTER RE
    0x82EB, 0x308D, //#HIRAGANA LETTER RO
    0x82EC, 0x308E, //#HIRAGANA LETTER SMALL WA
    0x82ED, 0x308F, //#HIRAGANA LETTER WA
    0x82EE, 0x3090, //#HIRAGANA LETTER WI
    0x82EF, 0x3091, //#HIRAGANA LETTER WE
    0x82F0, 0x3092, //#HIRAGANA LETTER WO
    0x82F1, 0x3093, //#HIRAGANA LETTER N
    0x8340, 0x30A1, //#KATAKANA LETTER SMALL A
    0x8341, 0x30A2, //#KATAKANA LETTER A
    0x8342, 0x30A3, //#KATAKANA LETTER SMALL I
    0x8343, 0x30A4, //#KATAKANA LETTER I
    0x8344, 0x30A5, //#KATAKANA LETTER SMALL U
    0x8345, 0x30A6, //#KATAKANA LETTER U
    0x8346, 0x30A7, //#KATAKANA LETTER SMALL E
    0x8347, 0x30A8, //#KATAKANA LETTER E
    0x8348, 0x30A9, //#KATAKANA LETTER SMALL O
    0x8349, 0x30AA, //#KATAKANA LETTER O
    0x834A, 0x30AB, //#KATAKANA LETTER KA
    0x834B, 0x30AC, //#KATAKANA LETTER GA
    0x834C, 0x30AD, //#KATAKANA LETTER KI
    0x834D, 0x30AE, //#KATAKANA LETTER GI
    0x834E, 0x30AF, //#KATAKANA LETTER KU
    0x834F, 0x30B0, //#KATAKANA LETTER GU
    0x8350, 0x30B1, //#KATAKANA LETTER KE
    0x8351, 0x30B2, //#KATAKANA LETTER GE
    0x8352, 0x30B3, //#KATAKANA LETTER KO
    0x8353, 0x30B4, //#KATAKANA LETTER GO
    0x8354, 0x30B5, //#KATAKANA LETTER SA
    0x8355, 0x30B6, //#KATAKANA LETTER ZA
    0x8356, 0x30B7, //#KATAKANA LETTER SI
    0x8357, 0x30B8, //#KATAKANA LETTER ZI
    0x8358, 0x30B9, //#KATAKANA LETTER SU
    0x8359, 0x30BA, //#KATAKANA LETTER ZU
    0x835A, 0x30BB, //#KATAKANA LETTER SE
    0x835B, 0x30BC, //#KATAKANA LETTER ZE
    0x835C, 0x30BD, //#KATAKANA LETTER SO
    0x835D, 0x30BE, //#KATAKANA LETTER ZO
    0x835E, 0x30BF, //#KATAKANA LETTER TA
    0x835F, 0x30C0, //#KATAKANA LETTER DA
    0x8360, 0x30C1, //#KATAKANA LETTER TI
    0x8361, 0x30C2, //#KATAKANA LETTER DI
    0x8362, 0x30C3, //#KATAKANA LETTER SMALL TU
    0x8363, 0x30C4, //#KATAKANA LETTER TU
    0x8364, 0x30C5, //#KATAKANA LETTER DU
    0x8365, 0x30C6, //#KATAKANA LETTER TE
    0x8366, 0x30C7, //#KATAKANA LETTER DE
    0x8367, 0x30C8, //#KATAKANA LETTER TO
    0x8368, 0x30C9, //#KATAKANA LETTER DO
    0x8369, 0x30CA, //#KATAKANA LETTER NA
    0x836A, 0x30CB, //#KATAKANA LETTER NI
    0x836B, 0x30CC, //#KATAKANA LETTER NU
    0x836C, 0x30CD, //#KATAKANA LETTER NE
    0x836D, 0x30CE, //#KATAKANA LETTER NO
    0x836E, 0x30CF, //#KATAKANA LETTER HA
    0x836F, 0x30D0, //#KATAKANA LETTER BA
    0x8370, 0x30D1, //#KATAKANA LETTER PA
    0x8371, 0x30D2, //#KATAKANA LETTER HI
    0x8372, 0x30D3, //#KATAKANA LETTER BI
    0x8373, 0x30D4, //#KATAKANA LETTER PI
    0x8374, 0x30D5, //#KATAKANA LETTER HU
    0x8375, 0x30D6, //#KATAKANA LETTER BU
    0x8376, 0x30D7, //#KATAKANA LETTER PU
    0x8377, 0x30D8, //#KATAKANA LETTER HE
    0x8378, 0x30D9, //#KATAKANA LETTER BE
    0x8379, 0x30DA, //#KATAKANA LETTER PE
    0x837A, 0x30DB, //#KATAKANA LETTER HO
    0x837B, 0x30DC, //#KATAKANA LETTER BO
    0x837C, 0x30DD, //#KATAKANA LETTER PO
    0x837D, 0x30DE, //#KATAKANA LETTER MA
    0x837E, 0x30DF, //#KATAKANA LETTER MI
    0x8380, 0x30E0, //#KATAKANA LETTER MU
    0x8381, 0x30E1, //#KATAKANA LETTER ME
    0x8382, 0x30E2, //#KATAKANA LETTER MO
    0x8383, 0x30E3, //#KATAKANA LETTER SMALL YA
    0x8384, 0x30E4, //#KATAKANA LETTER YA
    0x8385, 0x30E5, //#KATAKANA LETTER SMALL YU
    0x8386, 0x30E6, //#KATAKANA LETTER YU
    0x8387, 0x30E7, //#KATAKANA LETTER SMALL YO
    0x8388, 0x30E8, //#KATAKANA LETTER YO
    0x8389, 0x30E9, //#KATAKANA LETTER RA
    0x838A, 0x30EA, //#KATAKANA LETTER RI
    0x838B, 0x30EB, //#KATAKANA LETTER RU
    0x838C, 0x30EC, //#KATAKANA LETTER RE
    0x838D, 0x30ED, //#KATAKANA LETTER RO
    0x838E, 0x30EE, //#KATAKANA LETTER SMALL WA
    0x838F, 0x30EF, //#KATAKANA LETTER WA
    0x8390, 0x30F0, //#KATAKANA LETTER WI
    0x8391, 0x30F1, //#KATAKANA LETTER WE
    0x8392, 0x30F2, //#KATAKANA LETTER WO
    0x8393, 0x30F3, //#KATAKANA LETTER N
    0x8394, 0x30F4, //#KATAKANA LETTER VU
    0x8395, 0x30F5, //#KATAKANA LETTER SMALL KA
    0x8396, 0x30F6, //#KATAKANA LETTER SMALL KE
    0x839F, 0x0391, //#GREEK CAPITAL LETTER ALPHA
    0x83A0, 0x0392, //#GREEK CAPITAL LETTER BETA
    0x83A1, 0x0393, //#GREEK CAPITAL LETTER GAMMA
    0x83A2, 0x0394, //#GREEK CAPITAL LETTER DELTA
    0x83A3, 0x0395, //#GREEK CAPITAL LETTER EPSILON
    0x83A4, 0x0396, //#GREEK CAPITAL LETTER ZETA
    0x83A5, 0x0397, //#GREEK CAPITAL LETTER ETA
    0x83A6, 0x0398, //#GREEK CAPITAL LETTER THETA
    0x83A7, 0x0399, //#GREEK CAPITAL LETTER IOTA
    0x83A8, 0x039A, //#GREEK CAPITAL LETTER KAPPA
    0x83A9, 0x039B, //#GREEK CAPITAL LETTER LAMDA
    0x83AA, 0x039C, //#GREEK CAPITAL LETTER MU
    0x83AB, 0x039D, //#GREEK CAPITAL LETTER NU
    0x83AC, 0x039E, //#GREEK CAPITAL LETTER XI
    0x83AD, 0x039F, //#GREEK CAPITAL LETTER OMICRON
    0x83AE, 0x03A0, //#GREEK CAPITAL LETTER PI
    0x83AF, 0x03A1, //#GREEK CAPITAL LETTER RHO
    0x83B0, 0x03A3, //#GREEK CAPITAL LETTER SIGMA
    0x83B1, 0x03A4, //#GREEK CAPITAL LETTER TAU
    0x83B2, 0x03A5, //#GREEK CAPITAL LETTER UPSILON
    0x83B3, 0x03A6, //#GREEK CAPITAL LETTER PHI
    0x83B4, 0x03A7, //#GREEK CAPITAL LETTER CHI
    0x83B5, 0x03A8, //#GREEK CAPITAL LETTER PSI
    0x83B6, 0x03A9, //#GREEK CAPITAL LETTER OMEGA
    0x83BF, 0x03B1, //#GREEK SMALL LETTER ALPHA
    0x83C0, 0x03B2, //#GREEK SMALL LETTER BETA
    0x83C1, 0x03B3, //#GREEK SMALL LETTER GAMMA
    0x83C2, 0x03B4, //#GREEK SMALL LETTER DELTA
    0x83C3, 0x03B5, //#GREEK SMALL LETTER EPSILON
    0x83C4, 0x03B6, //#GREEK SMALL LETTER ZETA
    0x83C5, 0x03B7, //#GREEK SMALL LETTER ETA
    0x83C6, 0x03B8, //#GREEK SMALL LETTER THETA
    0x83C7, 0x03B9, //#GREEK SMALL LETTER IOTA
    0x83C8, 0x03BA, //#GREEK SMALL LETTER KAPPA
    0x83C9, 0x03BB, //#GREEK SMALL LETTER LAMDA
    0x83CA, 0x03BC, //#GREEK SMALL LETTER MU
    0x83CB, 0x03BD, //#GREEK SMALL LETTER NU
    0x83CC, 0x03BE, //#GREEK SMALL LETTER XI
    0x83CD, 0x03BF, //#GREEK SMALL LETTER OMICRON
    0x83CE, 0x03C0, //#GREEK SMALL LETTER PI
    0x83CF, 0x03C1, //#GREEK SMALL LETTER RHO
    0x83D0, 0x03C3, //#GREEK SMALL LETTER SIGMA
    0x83D1, 0x03C4, //#GREEK SMALL LETTER TAU
    0x83D2, 0x03C5, //#GREEK SMALL LETTER UPSILON
    0x83D3, 0x03C6, //#GREEK SMALL LETTER PHI
    0x83D4, 0x03C7, //#GREEK SMALL LETTER CHI
    0x83D5, 0x03C8, //#GREEK SMALL LETTER PSI
    0x83D6, 0x03C9, //#GREEK SMALL LETTER OMEGA
    0x8440, 0x0410, //#CYRILLIC CAPITAL LETTER A
    0x8441, 0x0411, //#CYRILLIC CAPITAL LETTER BE
    0x8442, 0x0412, //#CYRILLIC CAPITAL LETTER VE
    0x8443, 0x0413, //#CYRILLIC CAPITAL LETTER GHE
    0x8444, 0x0414, //#CYRILLIC CAPITAL LETTER DE
    0x8445, 0x0415, //#CYRILLIC CAPITAL LETTER IE
    0x8446, 0x0401, //#CYRILLIC CAPITAL LETTER IO
    0x8447, 0x0416, //#CYRILLIC CAPITAL LETTER ZHE
    0x8448, 0x0417, //#CYRILLIC CAPITAL LETTER ZE
    0x8449, 0x0418, //#CYRILLIC CAPITAL LETTER I
    0x844A, 0x0419, //#CYRILLIC CAPITAL LETTER SHORT I
    0x844B, 0x041A, //#CYRILLIC CAPITAL LETTER KA
    0x844C, 0x041B, //#CYRILLIC CAPITAL LETTER EL
    0x844D, 0x041C, //#CYRILLIC CAPITAL LETTER EM
    0x844E, 0x041D, //#CYRILLIC CAPITAL LETTER EN
    0x844F, 0x041E, //#CYRILLIC CAPITAL LETTER O
    0x8450, 0x041F, //#CYRILLIC CAPITAL LETTER PE
    0x8451, 0x0420, //#CYRILLIC CAPITAL LETTER ER
    0x8452, 0x0421, //#CYRILLIC CAPITAL LETTER ES
    0x8453, 0x0422, //#CYRILLIC CAPITAL LETTER TE
    0x8454, 0x0423, //#CYRILLIC CAPITAL LETTER U
    0x8455, 0x0424, //#CYRILLIC CAPITAL LETTER EF
    0x8456, 0x0425, //#CYRILLIC CAPITAL LETTER HA
    0x8457, 0x0426, //#CYRILLIC CAPITAL LETTER TSE
    0x8458, 0x0427, //#CYRILLIC CAPITAL LETTER CHE
    0x8459, 0x0428, //#CYRILLIC CAPITAL LETTER SHA
    0x845A, 0x0429, //#CYRILLIC CAPITAL LETTER SHCHA
    0x845B, 0x042A, //#CYRILLIC CAPITAL LETTER HARD SIGN
    0x845C, 0x042B, //#CYRILLIC CAPITAL LETTER YERU
    0x845D, 0x042C, //#CYRILLIC CAPITAL LETTER SOFT SIGN
    0x845E, 0x042D, //#CYRILLIC CAPITAL LETTER E
    0x845F, 0x042E, //#CYRILLIC CAPITAL LETTER YU
    0x8460, 0x042F, //#CYRILLIC CAPITAL LETTER YA
    0x8470, 0x0430, //#CYRILLIC SMALL LETTER A
    0x8471, 0x0431, //#CYRILLIC SMALL LETTER BE
    0x8472, 0x0432, //#CYRILLIC SMALL LETTER VE
    0x8473, 0x0433, //#CYRILLIC SMALL LETTER GHE
    0x8474, 0x0434, //#CYRILLIC SMALL LETTER DE
    0x8475, 0x0435, //#CYRILLIC SMALL LETTER IE
    0x8476, 0x0451, //#CYRILLIC SMALL LETTER IO
    0x8477, 0x0436, //#CYRILLIC SMALL LETTER ZHE
    0x8478, 0x0437, //#CYRILLIC SMALL LETTER ZE
    0x8479, 0x0438, //#CYRILLIC SMALL LETTER I
    0x847A, 0x0439, //#CYRILLIC SMALL LETTER SHORT I
    0x847B, 0x043A, //#CYRILLIC SMALL LETTER KA
    0x847C, 0x043B, //#CYRILLIC SMALL LETTER EL
    0x847D, 0x043C, //#CYRILLIC SMALL LETTER EM
    0x847E, 0x043D, //#CYRILLIC SMALL LETTER EN
    0x8480, 0x043E, //#CYRILLIC SMALL LETTER O
    0x8481, 0x043F, //#CYRILLIC SMALL LETTER PE
    0x8482, 0x0440, //#CYRILLIC SMALL LETTER ER
    0x8483, 0x0441, //#CYRILLIC SMALL LETTER ES
    0x8484, 0x0442, //#CYRILLIC SMALL LETTER TE
    0x8485, 0x0443, //#CYRILLIC SMALL LETTER U
    0x8486, 0x0444, //#CYRILLIC SMALL LETTER EF
    0x8487, 0x0445, //#CYRILLIC SMALL LETTER HA
    0x8488, 0x0446, //#CYRILLIC SMALL LETTER TSE
    0x8489, 0x0447, //#CYRILLIC SMALL LETTER CHE
    0x848A, 0x0448, //#CYRILLIC SMALL LETTER SHA
    0x848B, 0x0449, //#CYRILLIC SMALL LETTER SHCHA
    0x848C, 0x044A, //#CYRILLIC SMALL LETTER HARD SIGN
    0x848D, 0x044B, //#CYRILLIC SMALL LETTER YERU
    0x848E, 0x044C, //#CYRILLIC SMALL LETTER SOFT SIGN
    0x848F, 0x044D, //#CYRILLIC SMALL LETTER E
    0x8490, 0x044E, //#CYRILLIC SMALL LETTER YU
    0x8491, 0x044F, //#CYRILLIC SMALL LETTER YA
    0x849F, 0x2500, //#BOX DRAWINGS LIGHT HORIZONTAL
    0x84A0, 0x2502, //#BOX DRAWINGS LIGHT VERTICAL
    0x84A1, 0x250C, //#BOX DRAWINGS LIGHT DOWN AND RIGHT
    0x84A2, 0x2510, //#BOX DRAWINGS LIGHT DOWN AND LEFT
    0x84A3, 0x2518, //#BOX DRAWINGS LIGHT UP AND LEFT
    0x84A4, 0x2514, //#BOX DRAWINGS LIGHT UP AND RIGHT
    0x84A5, 0x251C, //#BOX DRAWINGS LIGHT VERTICAL AND RIGHT
    0x84A6, 0x252C, //#BOX DRAWINGS LIGHT DOWN AND HORIZONTAL
    0x84A7, 0x2524, //#BOX DRAWINGS LIGHT VERTICAL AND LEFT
    0x84A8, 0x2534, //#BOX DRAWINGS LIGHT UP AND HORIZONTAL
    0x84A9, 0x253C, //#BOX DRAWINGS LIGHT VERTICAL AND HORIZONTAL
    0x84AA, 0x2501, //#BOX DRAWINGS HEAVY HORIZONTAL
    0x84AB, 0x2503, //#BOX DRAWINGS HEAVY VERTICAL
    0x84AC, 0x250F, //#BOX DRAWINGS HEAVY DOWN AND RIGHT
    0x84AD, 0x2513, //#BOX DRAWINGS HEAVY DOWN AND LEFT
    0x84AE, 0x251B, //#BOX DRAWINGS HEAVY UP AND LEFT
    0x84AF, 0x2517, //#BOX DRAWINGS HEAVY UP AND RIGHT
    0x84B0, 0x2523, //#BOX DRAWINGS HEAVY VERTICAL AND RIGHT
    0x84B1, 0x2533, //#BOX DRAWINGS HEAVY DOWN AND HORIZONTAL
    0x84B2, 0x252B, //#BOX DRAWINGS HEAVY VERTICAL AND LEFT
    0x84B3, 0x253B, //#BOX DRAWINGS HEAVY UP AND HORIZONTAL
    0x84B4, 0x254B, //#BOX DRAWINGS HEAVY VERTICAL AND HORIZONTAL
    0x84B5, 0x2520, //#BOX DRAWINGS VERTICAL HEAVY AND RIGHT LIGHT
    0x84B6, 0x252F, //#BOX DRAWINGS DOWN LIGHT AND HORIZONTAL HEAVY
    0x84B7, 0x2528, //#BOX DRAWINGS VERTICAL HEAVY AND LEFT LIGHT
    0x84B8, 0x2537, //#BOX DRAWINGS UP LIGHT AND HORIZONTAL HEAVY
    0x84B9, 0x253F, //#BOX DRAWINGS VERTICAL LIGHT AND HORIZONTAL HEAVY
    0x84BA, 0x251D, //#BOX DRAWINGS VERTICAL LIGHT AND RIGHT HEAVY
    0x84BB, 0x2530, //#BOX DRAWINGS DOWN HEAVY AND HORIZONTAL LIGHT
    0x84BC, 0x2525, //#BOX DRAWINGS VERTICAL LIGHT AND LEFT HEAVY
    0x84BD, 0x2538, //#BOX DRAWINGS UP HEAVY AND HORIZONTAL LIGHT
    0x84BE, 0x2542, //#BOX DRAWINGS VERTICAL HEAVY AND HORIZONTAL LIGHT
    0x8740, 0x2460, //#CIRCLED DIGIT ONE
    0x8741, 0x2461, //#CIRCLED DIGIT TWO
    0x8742, 0x2462, //#CIRCLED DIGIT THREE
    0x8743, 0x2463, //#CIRCLED DIGIT FOUR
    0x8744, 0x2464, //#CIRCLED DIGIT FIVE
    0x8745, 0x2465, //#CIRCLED DIGIT SIX
    0x8746, 0x2466, //#CIRCLED DIGIT SEVEN
    0x8747, 0x2467, //#CIRCLED DIGIT EIGHT
    0x8748, 0x2468, //#CIRCLED DIGIT NINE
    0x8749, 0x2469, //#CIRCLED NUMBER TEN
    0x874A, 0x246A, //#CIRCLED NUMBER ELEVEN
    0x874B, 0x246B, //#CIRCLED NUMBER TWELVE
    0x874C, 0x246C, //#CIRCLED NUMBER THIRTEEN
    0x874D, 0x246D, //#CIRCLED NUMBER FOURTEEN
    0x874E, 0x246E, //#CIRCLED NUMBER FIFTEEN
    0x874F, 0x246F, //#CIRCLED NUMBER SIXTEEN
    0x8750, 0x2470, //#CIRCLED NUMBER SEVENTEEN
    0x8751, 0x2471, //#CIRCLED NUMBER EIGHTEEN
    0x8752, 0x2472, //#CIRCLED NUMBER NINETEEN
    0x8753, 0x2473, //#CIRCLED NUMBER TWENTY
    0x8754, 0x2160, //#ROMAN NUMERAL ONE
    0x8755, 0x2161, //#ROMAN NUMERAL TWO
    0x8756, 0x2162, //#ROMAN NUMERAL THREE
    0x8757, 0x2163, //#ROMAN NUMERAL FOUR
    0x8758, 0x2164, //#ROMAN NUMERAL FIVE
    0x8759, 0x2165, //#ROMAN NUMERAL SIX
    0x875A, 0x2166, //#ROMAN NUMERAL SEVEN
    0x875B, 0x2167, //#ROMAN NUMERAL EIGHT
    0x875C, 0x2168, //#ROMAN NUMERAL NINE
    0x875D, 0x2169, //#ROMAN NUMERAL TEN
    0x875F, 0x3349, //#SQUARE MIRI
    0x8760, 0x3314, //#SQUARE KIRO
    0x8761, 0x3322, //#SQUARE SENTI
    0x8762, 0x334D, //#SQUARE MEETORU
    0x8763, 0x3318, //#SQUARE GURAMU
    0x8764, 0x3327, //#SQUARE TON
    0x8765, 0x3303, //#SQUARE AARU
    0x8766, 0x3336, //#SQUARE HEKUTAARU
    0x8767, 0x3351, //#SQUARE RITTORU
    0x8768, 0x3357, //#SQUARE WATTO
    0x8769, 0x330D, //#SQUARE KARORII
    0x876A, 0x3326, //#SQUARE DORU
    0x876B, 0x3323, //#SQUARE SENTO
    0x876C, 0x332B, //#SQUARE PAASENTO
    0x876D, 0x334A, //#SQUARE MIRIBAARU
    0x876E, 0x333B, //#SQUARE PEEZI
    0x876F, 0x339C, //#SQUARE MM
    0x8770, 0x339D, //#SQUARE CM
    0x8771, 0x339E, //#SQUARE KM
    0x8772, 0x338E, //#SQUARE MG
    0x8773, 0x338F, //#SQUARE KG
    0x8774, 0x33C4, //#SQUARE CC
    0x8775, 0x33A1, //#SQUARE M SQUARED
    0x877E, 0x337B, //#SQUARE ERA NAME HEISEI
    0x8780, 0x301D, //#REVERSED DOUBLE PRIME QUOTATION MARK
    0x8781, 0x301F, //#LOW DOUBLE PRIME QUOTATION MARK
    0x8782, 0x2116, //#NUMERO SIGN
    0x8783, 0x33CD, //#SQUARE KK
    0x8784, 0x2121, //#TELEPHONE SIGN
    0x8785, 0x32A4, //#CIRCLED IDEOGRAPH HIGH
    0x8786, 0x32A5, //#CIRCLED IDEOGRAPH CENTRE
    0x8787, 0x32A6, //#CIRCLED IDEOGRAPH LOW
    0x8788, 0x32A7, //#CIRCLED IDEOGRAPH LEFT
    0x8789, 0x32A8, //#CIRCLED IDEOGRAPH RIGHT
    0x878A, 0x3231, //#PARENTHESIZED IDEOGRAPH STOCK
    0x878B, 0x3232, //#PARENTHESIZED IDEOGRAPH HAVE
    0x878C, 0x3239, //#PARENTHESIZED IDEOGRAPH REPRESENT
    0x878D, 0x337E, //#SQUARE ERA NAME MEIZI
    0x878E, 0x337D, //#SQUARE ERA NAME TAISYOU
    0x878F, 0x337C, //#SQUARE ERA NAME SYOUWA
    0x8790, 0x2252, //#APPROXIMATELY EQUAL TO OR THE IMAGE OF
    0x8791, 0x2261, //#IDENTICAL TO
    0x8792, 0x222B, //#INTEGRAL
    0x8793, 0x222E, //#CONTOUR INTEGRAL
    0x8794, 0x2211, //#N-ARY SUMMATION
    0x8795, 0x221A, //#SQUARE ROOT
    0x8796, 0x22A5, //#UP TACK
    0x8797, 0x2220, //#ANGLE
    0x8798, 0x221F, //#RIGHT ANGLE
    0x8799, 0x22BF, //#RIGHT TRIANGLE
    0x879A, 0x2235, //#BECAUSE
    0x879B, 0x2229, //#INTERSECTION
    0x879C, 0x222A, //#UNION
    0x889F, 0x4E9C, //#CJK UNIFIED IDEOGRAPH
    0x88A0, 0x5516, //#CJK UNIFIED IDEOGRAPH
    0x88A1, 0x5A03, //#CJK UNIFIED IDEOGRAPH
    0x88A2, 0x963F, //#CJK UNIFIED IDEOGRAPH
    0x88A3, 0x54C0, //#CJK UNIFIED IDEOGRAPH
    0x88A4, 0x611B, //#CJK UNIFIED IDEOGRAPH
    0x88A5, 0x6328, //#CJK UNIFIED IDEOGRAPH
    0x88A6, 0x59F6, //#CJK UNIFIED IDEOGRAPH
    0x88A7, 0x9022, //#CJK UNIFIED IDEOGRAPH
    0x88A8, 0x8475, //#CJK UNIFIED IDEOGRAPH
    0x88A9, 0x831C, //#CJK UNIFIED IDEOGRAPH
    0x88AA, 0x7A50, //#CJK UNIFIED IDEOGRAPH
    0x88AB, 0x60AA, //#CJK UNIFIED IDEOGRAPH
    0x88AC, 0x63E1, //#CJK UNIFIED IDEOGRAPH
    0x88AD, 0x6E25, //#CJK UNIFIED IDEOGRAPH
    0x88AE, 0x65ED, //#CJK UNIFIED IDEOGRAPH
    0x88AF, 0x8466, //#CJK UNIFIED IDEOGRAPH
    0x88B0, 0x82A6, //#CJK UNIFIED IDEOGRAPH
    0x88B1, 0x9BF5, //#CJK UNIFIED IDEOGRAPH
    0x88B2, 0x6893, //#CJK UNIFIED IDEOGRAPH
    0x88B3, 0x5727, //#CJK UNIFIED IDEOGRAPH
    0x88B4, 0x65A1, //#CJK UNIFIED IDEOGRAPH
    0x88B5, 0x6271, //#CJK UNIFIED IDEOGRAPH
    0x88B6, 0x5B9B, //#CJK UNIFIED IDEOGRAPH
    0x88B7, 0x59D0, //#CJK UNIFIED IDEOGRAPH
    0x88B8, 0x867B, //#CJK UNIFIED IDEOGRAPH
    0x88B9, 0x98F4, //#CJK UNIFIED IDEOGRAPH
    0x88BA, 0x7D62, //#CJK UNIFIED IDEOGRAPH
    0x88BB, 0x7DBE, //#CJK UNIFIED IDEOGRAPH
    0x88BC, 0x9B8E, //#CJK UNIFIED IDEOGRAPH
    0x88BD, 0x6216, //#CJK UNIFIED IDEOGRAPH
    0x88BE, 0x7C9F, //#CJK UNIFIED IDEOGRAPH
    0x88BF, 0x88B7, //#CJK UNIFIED IDEOGRAPH
    0x88C0, 0x5B89, //#CJK UNIFIED IDEOGRAPH
    0x88C1, 0x5EB5, //#CJK UNIFIED IDEOGRAPH
    0x88C2, 0x6309, //#CJK UNIFIED IDEOGRAPH
    0x88C3, 0x6697, //#CJK UNIFIED IDEOGRAPH
    0x88C4, 0x6848, //#CJK UNIFIED IDEOGRAPH
    0x88C5, 0x95C7, //#CJK UNIFIED IDEOGRAPH
    0x88C6, 0x978D, //#CJK UNIFIED IDEOGRAPH
    0x88C7, 0x674F, //#CJK UNIFIED IDEOGRAPH
    0x88C8, 0x4EE5, //#CJK UNIFIED IDEOGRAPH
    0x88C9, 0x4F0A, //#CJK UNIFIED IDEOGRAPH
    0x88CA, 0x4F4D, //#CJK UNIFIED IDEOGRAPH
    0x88CB, 0x4F9D, //#CJK UNIFIED IDEOGRAPH
    0x88CC, 0x5049, //#CJK UNIFIED IDEOGRAPH
    0x88CD, 0x56F2, //#CJK UNIFIED IDEOGRAPH
    0x88CE, 0x5937, //#CJK UNIFIED IDEOGRAPH
    0x88CF, 0x59D4, //#CJK UNIFIED IDEOGRAPH
    0x88D0, 0x5A01, //#CJK UNIFIED IDEOGRAPH
    0x88D1, 0x5C09, //#CJK UNIFIED IDEOGRAPH
    0x88D2, 0x60DF, //#CJK UNIFIED IDEOGRAPH
    0x88D3, 0x610F, //#CJK UNIFIED IDEOGRAPH
    0x88D4, 0x6170, //#CJK UNIFIED IDEOGRAPH
    0x88D5, 0x6613, //#CJK UNIFIED IDEOGRAPH
    0x88D6, 0x6905, //#CJK UNIFIED IDEOGRAPH
    0x88D7, 0x70BA, //#CJK UNIFIED IDEOGRAPH
    0x88D8, 0x754F, //#CJK UNIFIED IDEOGRAPH
    0x88D9, 0x7570, //#CJK UNIFIED IDEOGRAPH
    0x88DA, 0x79FB, //#CJK UNIFIED IDEOGRAPH
    0x88DB, 0x7DAD, //#CJK UNIFIED IDEOGRAPH
    0x88DC, 0x7DEF, //#CJK UNIFIED IDEOGRAPH
    0x88DD, 0x80C3, //#CJK UNIFIED IDEOGRAPH
    0x88DE, 0x840E, //#CJK UNIFIED IDEOGRAPH
    0x88DF, 0x8863, //#CJK UNIFIED IDEOGRAPH
    0x88E0, 0x8B02, //#CJK UNIFIED IDEOGRAPH
    0x88E1, 0x9055, //#CJK UNIFIED IDEOGRAPH
    0x88E2, 0x907A, //#CJK UNIFIED IDEOGRAPH
    0x88E3, 0x533B, //#CJK UNIFIED IDEOGRAPH
    0x88E4, 0x4E95, //#CJK UNIFIED IDEOGRAPH
    0x88E5, 0x4EA5, //#CJK UNIFIED IDEOGRAPH
    0x88E6, 0x57DF, //#CJK UNIFIED IDEOGRAPH
    0x88E7, 0x80B2, //#CJK UNIFIED IDEOGRAPH
    0x88E8, 0x90C1, //#CJK UNIFIED IDEOGRAPH
    0x88E9, 0x78EF, //#CJK UNIFIED IDEOGRAPH
    0x88EA, 0x4E00, //#CJK UNIFIED IDEOGRAPH
    0x88EB, 0x58F1, //#CJK UNIFIED IDEOGRAPH
    0x88EC, 0x6EA2, //#CJK UNIFIED IDEOGRAPH
    0x88ED, 0x9038, //#CJK UNIFIED IDEOGRAPH
    0x88EE, 0x7A32, //#CJK UNIFIED IDEOGRAPH
    0x88EF, 0x8328, //#CJK UNIFIED IDEOGRAPH
    0x88F0, 0x828B, //#CJK UNIFIED IDEOGRAPH
    0x88F1, 0x9C2F, //#CJK UNIFIED IDEOGRAPH
    0x88F2, 0x5141, //#CJK UNIFIED IDEOGRAPH
    0x88F3, 0x5370, //#CJK UNIFIED IDEOGRAPH
    0x88F4, 0x54BD, //#CJK UNIFIED IDEOGRAPH
    0x88F5, 0x54E1, //#CJK UNIFIED IDEOGRAPH
    0x88F6, 0x56E0, //#CJK UNIFIED IDEOGRAPH
    0x88F7, 0x59FB, //#CJK UNIFIED IDEOGRAPH
    0x88F8, 0x5F15, //#CJK UNIFIED IDEOGRAPH
    0x88F9, 0x98F2, //#CJK UNIFIED IDEOGRAPH
    0x88FA, 0x6DEB, //#CJK UNIFIED IDEOGRAPH
    0x88FB, 0x80E4, //#CJK UNIFIED IDEOGRAPH
    0x88FC, 0x852D, //#CJK UNIFIED IDEOGRAPH
    0x8940, 0x9662, //#CJK UNIFIED IDEOGRAPH
    0x8941, 0x9670, //#CJK UNIFIED IDEOGRAPH
    0x8942, 0x96A0, //#CJK UNIFIED IDEOGRAPH
    0x8943, 0x97FB, //#CJK UNIFIED IDEOGRAPH
    0x8944, 0x540B, //#CJK UNIFIED IDEOGRAPH
    0x8945, 0x53F3, //#CJK UNIFIED IDEOGRAPH
    0x8946, 0x5B87, //#CJK UNIFIED IDEOGRAPH
    0x8947, 0x70CF, //#CJK UNIFIED IDEOGRAPH
    0x8948, 0x7FBD, //#CJK UNIFIED IDEOGRAPH
    0x8949, 0x8FC2, //#CJK UNIFIED IDEOGRAPH
    0x894A, 0x96E8, //#CJK UNIFIED IDEOGRAPH
    0x894B, 0x536F, //#CJK UNIFIED IDEOGRAPH
    0x894C, 0x9D5C, //#CJK UNIFIED IDEOGRAPH
    0x894D, 0x7ABA, //#CJK UNIFIED IDEOGRAPH
    0x894E, 0x4E11, //#CJK UNIFIED IDEOGRAPH
    0x894F, 0x7893, //#CJK UNIFIED IDEOGRAPH
    0x8950, 0x81FC, //#CJK UNIFIED IDEOGRAPH
    0x8951, 0x6E26, //#CJK UNIFIED IDEOGRAPH
    0x8952, 0x5618, //#CJK UNIFIED IDEOGRAPH
    0x8953, 0x5504, //#CJK UNIFIED IDEOGRAPH
    0x8954, 0x6B1D, //#CJK UNIFIED IDEOGRAPH
    0x8955, 0x851A, //#CJK UNIFIED IDEOGRAPH
    0x8956, 0x9C3B, //#CJK UNIFIED IDEOGRAPH
    0x8957, 0x59E5, //#CJK UNIFIED IDEOGRAPH
    0x8958, 0x53A9, //#CJK UNIFIED IDEOGRAPH
    0x8959, 0x6D66, //#CJK UNIFIED IDEOGRAPH
    0x895A, 0x74DC, //#CJK UNIFIED IDEOGRAPH
    0x895B, 0x958F, //#CJK UNIFIED IDEOGRAPH
    0x895C, 0x5642, //#CJK UNIFIED IDEOGRAPH
    0x895D, 0x4E91, //#CJK UNIFIED IDEOGRAPH
    0x895E, 0x904B, //#CJK UNIFIED IDEOGRAPH
    0x895F, 0x96F2, //#CJK UNIFIED IDEOGRAPH
    0x8960, 0x834F, //#CJK UNIFIED IDEOGRAPH
    0x8961, 0x990C, //#CJK UNIFIED IDEOGRAPH
    0x8962, 0x53E1, //#CJK UNIFIED IDEOGRAPH
    0x8963, 0x55B6, //#CJK UNIFIED IDEOGRAPH
    0x8964, 0x5B30, //#CJK UNIFIED IDEOGRAPH
    0x8965, 0x5F71, //#CJK UNIFIED IDEOGRAPH
    0x8966, 0x6620, //#CJK UNIFIED IDEOGRAPH
    0x8967, 0x66F3, //#CJK UNIFIED IDEOGRAPH
    0x8968, 0x6804, //#CJK UNIFIED IDEOGRAPH
    0x8969, 0x6C38, //#CJK UNIFIED IDEOGRAPH
    0x896A, 0x6CF3, //#CJK UNIFIED IDEOGRAPH
    0x896B, 0x6D29, //#CJK UNIFIED IDEOGRAPH
    0x896C, 0x745B, //#CJK UNIFIED IDEOGRAPH
    0x896D, 0x76C8, //#CJK UNIFIED IDEOGRAPH
    0x896E, 0x7A4E, //#CJK UNIFIED IDEOGRAPH
    0x896F, 0x9834, //#CJK UNIFIED IDEOGRAPH
    0x8970, 0x82F1, //#CJK UNIFIED IDEOGRAPH
    0x8971, 0x885B, //#CJK UNIFIED IDEOGRAPH
    0x8972, 0x8A60, //#CJK UNIFIED IDEOGRAPH
    0x8973, 0x92ED, //#CJK UNIFIED IDEOGRAPH
    0x8974, 0x6DB2, //#CJK UNIFIED IDEOGRAPH
    0x8975, 0x75AB, //#CJK UNIFIED IDEOGRAPH
    0x8976, 0x76CA, //#CJK UNIFIED IDEOGRAPH
    0x8977, 0x99C5, //#CJK UNIFIED IDEOGRAPH
    0x8978, 0x60A6, //#CJK UNIFIED IDEOGRAPH
    0x8979, 0x8B01, //#CJK UNIFIED IDEOGRAPH
    0x897A, 0x8D8A, //#CJK UNIFIED IDEOGRAPH
    0x897B, 0x95B2, //#CJK UNIFIED IDEOGRAPH
    0x897C, 0x698E, //#CJK UNIFIED IDEOGRAPH
    0x897D, 0x53AD, //#CJK UNIFIED IDEOGRAPH
    0x897E, 0x5186, //#CJK UNIFIED IDEOGRAPH
    0x8980, 0x5712, //#CJK UNIFIED IDEOGRAPH
    0x8981, 0x5830, //#CJK UNIFIED IDEOGRAPH
    0x8982, 0x5944, //#CJK UNIFIED IDEOGRAPH
    0x8983, 0x5BB4, //#CJK UNIFIED IDEOGRAPH
    0x8984, 0x5EF6, //#CJK UNIFIED IDEOGRAPH
    0x8985, 0x6028, //#CJK UNIFIED IDEOGRAPH
    0x8986, 0x63A9, //#CJK UNIFIED IDEOGRAPH
    0x8987, 0x63F4, //#CJK UNIFIED IDEOGRAPH
    0x8988, 0x6CBF, //#CJK UNIFIED IDEOGRAPH
    0x8989, 0x6F14, //#CJK UNIFIED IDEOGRAPH
    0x898A, 0x708E, //#CJK UNIFIED IDEOGRAPH
    0x898B, 0x7114, //#CJK UNIFIED IDEOGRAPH
    0x898C, 0x7159, //#CJK UNIFIED IDEOGRAPH
    0x898D, 0x71D5, //#CJK UNIFIED IDEOGRAPH
    0x898E, 0x733F, //#CJK UNIFIED IDEOGRAPH
    0x898F, 0x7E01, //#CJK UNIFIED IDEOGRAPH
    0x8990, 0x8276, //#CJK UNIFIED IDEOGRAPH
    0x8991, 0x82D1, //#CJK UNIFIED IDEOGRAPH
    0x8992, 0x8597, //#CJK UNIFIED IDEOGRAPH
    0x8993, 0x9060, //#CJK UNIFIED IDEOGRAPH
    0x8994, 0x925B, //#CJK UNIFIED IDEOGRAPH
    0x8995, 0x9D1B, //#CJK UNIFIED IDEOGRAPH
    0x8996, 0x5869, //#CJK UNIFIED IDEOGRAPH
    0x8997, 0x65BC, //#CJK UNIFIED IDEOGRAPH
    0x8998, 0x6C5A, //#CJK UNIFIED IDEOGRAPH
    0x8999, 0x7525, //#CJK UNIFIED IDEOGRAPH
    0x899A, 0x51F9, //#CJK UNIFIED IDEOGRAPH
    0x899B, 0x592E, //#CJK UNIFIED IDEOGRAPH
    0x899C, 0x5965, //#CJK UNIFIED IDEOGRAPH
    0x899D, 0x5F80, //#CJK UNIFIED IDEOGRAPH
    0x899E, 0x5FDC, //#CJK UNIFIED IDEOGRAPH
    0x899F, 0x62BC, //#CJK UNIFIED IDEOGRAPH
    0x89A0, 0x65FA, //#CJK UNIFIED IDEOGRAPH
    0x89A1, 0x6A2A, //#CJK UNIFIED IDEOGRAPH
    0x89A2, 0x6B27, //#CJK UNIFIED IDEOGRAPH
    0x89A3, 0x6BB4, //#CJK UNIFIED IDEOGRAPH
    0x89A4, 0x738B, //#CJK UNIFIED IDEOGRAPH
    0x89A5, 0x7FC1, //#CJK UNIFIED IDEOGRAPH
    0x89A6, 0x8956, //#CJK UNIFIED IDEOGRAPH
    0x89A7, 0x9D2C, //#CJK UNIFIED IDEOGRAPH
    0x89A8, 0x9D0E, //#CJK UNIFIED IDEOGRAPH
    0x89A9, 0x9EC4, //#CJK UNIFIED IDEOGRAPH
    0x89AA, 0x5CA1, //#CJK UNIFIED IDEOGRAPH
    0x89AB, 0x6C96, //#CJK UNIFIED IDEOGRAPH
    0x89AC, 0x837B, //#CJK UNIFIED IDEOGRAPH
    0x89AD, 0x5104, //#CJK UNIFIED IDEOGRAPH
    0x89AE, 0x5C4B, //#CJK UNIFIED IDEOGRAPH
    0x89AF, 0x61B6, //#CJK UNIFIED IDEOGRAPH
    0x89B0, 0x81C6, //#CJK UNIFIED IDEOGRAPH
    0x89B1, 0x6876, //#CJK UNIFIED IDEOGRAPH
    0x89B2, 0x7261, //#CJK UNIFIED IDEOGRAPH
    0x89B3, 0x4E59, //#CJK UNIFIED IDEOGRAPH
    0x89B4, 0x4FFA, //#CJK UNIFIED IDEOGRAPH
    0x89B5, 0x5378, //#CJK UNIFIED IDEOGRAPH
    0x89B6, 0x6069, //#CJK UNIFIED IDEOGRAPH
    0x89B7, 0x6E29, //#CJK UNIFIED IDEOGRAPH
    0x89B8, 0x7A4F, //#CJK UNIFIED IDEOGRAPH
    0x89B9, 0x97F3, //#CJK UNIFIED IDEOGRAPH
    0x89BA, 0x4E0B, //#CJK UNIFIED IDEOGRAPH
    0x89BB, 0x5316, //#CJK UNIFIED IDEOGRAPH
    0x89BC, 0x4EEE, //#CJK UNIFIED IDEOGRAPH
    0x89BD, 0x4F55, //#CJK UNIFIED IDEOGRAPH
    0x89BE, 0x4F3D, //#CJK UNIFIED IDEOGRAPH
    0x89BF, 0x4FA1, //#CJK UNIFIED IDEOGRAPH
    0x89C0, 0x4F73, //#CJK UNIFIED IDEOGRAPH
    0x89C1, 0x52A0, //#CJK UNIFIED IDEOGRAPH
    0x89C2, 0x53EF, //#CJK UNIFIED IDEOGRAPH
    0x89C3, 0x5609, //#CJK UNIFIED IDEOGRAPH
    0x89C4, 0x590F, //#CJK UNIFIED IDEOGRAPH
    0x89C5, 0x5AC1, //#CJK UNIFIED IDEOGRAPH
    0x89C6, 0x5BB6, //#CJK UNIFIED IDEOGRAPH
    0x89C7, 0x5BE1, //#CJK UNIFIED IDEOGRAPH
    0x89C8, 0x79D1, //#CJK UNIFIED IDEOGRAPH
    0x89C9, 0x6687, //#CJK UNIFIED IDEOGRAPH
    0x89CA, 0x679C, //#CJK UNIFIED IDEOGRAPH
    0x89CB, 0x67B6, //#CJK UNIFIED IDEOGRAPH
    0x89CC, 0x6B4C, //#CJK UNIFIED IDEOGRAPH
    0x89CD, 0x6CB3, //#CJK UNIFIED IDEOGRAPH
    0x89CE, 0x706B, //#CJK UNIFIED IDEOGRAPH
    0x89CF, 0x73C2, //#CJK UNIFIED IDEOGRAPH
    0x89D0, 0x798D, //#CJK UNIFIED IDEOGRAPH
    0x89D1, 0x79BE, //#CJK UNIFIED IDEOGRAPH
    0x89D2, 0x7A3C, //#CJK UNIFIED IDEOGRAPH
    0x89D3, 0x7B87, //#CJK UNIFIED IDEOGRAPH
    0x89D4, 0x82B1, //#CJK UNIFIED IDEOGRAPH
    0x89D5, 0x82DB, //#CJK UNIFIED IDEOGRAPH
    0x89D6, 0x8304, //#CJK UNIFIED IDEOGRAPH
    0x89D7, 0x8377, //#CJK UNIFIED IDEOGRAPH
    0x89D8, 0x83EF, //#CJK UNIFIED IDEOGRAPH
    0x89D9, 0x83D3, //#CJK UNIFIED IDEOGRAPH
    0x89DA, 0x8766, //#CJK UNIFIED IDEOGRAPH
    0x89DB, 0x8AB2, //#CJK UNIFIED IDEOGRAPH
    0x89DC, 0x5629, //#CJK UNIFIED IDEOGRAPH
    0x89DD, 0x8CA8, //#CJK UNIFIED IDEOGRAPH
    0x89DE, 0x8FE6, //#CJK UNIFIED IDEOGRAPH
    0x89DF, 0x904E, //#CJK UNIFIED IDEOGRAPH
    0x89E0, 0x971E, //#CJK UNIFIED IDEOGRAPH
    0x89E1, 0x868A, //#CJK UNIFIED IDEOGRAPH
    0x89E2, 0x4FC4, //#CJK UNIFIED IDEOGRAPH
    0x89E3, 0x5CE8, //#CJK UNIFIED IDEOGRAPH
    0x89E4, 0x6211, //#CJK UNIFIED IDEOGRAPH
    0x89E5, 0x7259, //#CJK UNIFIED IDEOGRAPH
    0x89E6, 0x753B, //#CJK UNIFIED IDEOGRAPH
    0x89E7, 0x81E5, //#CJK UNIFIED IDEOGRAPH
    0x89E8, 0x82BD, //#CJK UNIFIED IDEOGRAPH
    0x89E9, 0x86FE, //#CJK UNIFIED IDEOGRAPH
    0x89EA, 0x8CC0, //#CJK UNIFIED IDEOGRAPH
    0x89EB, 0x96C5, //#CJK UNIFIED IDEOGRAPH
    0x89EC, 0x9913, //#CJK UNIFIED IDEOGRAPH
    0x89ED, 0x99D5, //#CJK UNIFIED IDEOGRAPH
    0x89EE, 0x4ECB, //#CJK UNIFIED IDEOGRAPH
    0x89EF, 0x4F1A, //#CJK UNIFIED IDEOGRAPH
    0x89F0, 0x89E3, //#CJK UNIFIED IDEOGRAPH
    0x89F1, 0x56DE, //#CJK UNIFIED IDEOGRAPH
    0x89F2, 0x584A, //#CJK UNIFIED IDEOGRAPH
    0x89F3, 0x58CA, //#CJK UNIFIED IDEOGRAPH
    0x89F4, 0x5EFB, //#CJK UNIFIED IDEOGRAPH
    0x89F5, 0x5FEB, //#CJK UNIFIED IDEOGRAPH
    0x89F6, 0x602A, //#CJK UNIFIED IDEOGRAPH
    0x89F7, 0x6094, //#CJK UNIFIED IDEOGRAPH
    0x89F8, 0x6062, //#CJK UNIFIED IDEOGRAPH
    0x89F9, 0x61D0, //#CJK UNIFIED IDEOGRAPH
    0x89FA, 0x6212, //#CJK UNIFIED IDEOGRAPH
    0x89FB, 0x62D0, //#CJK UNIFIED IDEOGRAPH
    0x89FC, 0x6539, //#CJK UNIFIED IDEOGRAPH
    0x8A40, 0x9B41, //#CJK UNIFIED IDEOGRAPH
    0x8A41, 0x6666, //#CJK UNIFIED IDEOGRAPH
    0x8A42, 0x68B0, //#CJK UNIFIED IDEOGRAPH
    0x8A43, 0x6D77, //#CJK UNIFIED IDEOGRAPH
    0x8A44, 0x7070, //#CJK UNIFIED IDEOGRAPH
    0x8A45, 0x754C, //#CJK UNIFIED IDEOGRAPH
    0x8A46, 0x7686, //#CJK UNIFIED IDEOGRAPH
    0x8A47, 0x7D75, //#CJK UNIFIED IDEOGRAPH
    0x8A48, 0x82A5, //#CJK UNIFIED IDEOGRAPH
    0x8A49, 0x87F9, //#CJK UNIFIED IDEOGRAPH
    0x8A4A, 0x958B, //#CJK UNIFIED IDEOGRAPH
    0x8A4B, 0x968E, //#CJK UNIFIED IDEOGRAPH
    0x8A4C, 0x8C9D, //#CJK UNIFIED IDEOGRAPH
    0x8A4D, 0x51F1, //#CJK UNIFIED IDEOGRAPH
    0x8A4E, 0x52BE, //#CJK UNIFIED IDEOGRAPH
    0x8A4F, 0x5916, //#CJK UNIFIED IDEOGRAPH
    0x8A50, 0x54B3, //#CJK UNIFIED IDEOGRAPH
    0x8A51, 0x5BB3, //#CJK UNIFIED IDEOGRAPH
    0x8A52, 0x5D16, //#CJK UNIFIED IDEOGRAPH
    0x8A53, 0x6168, //#CJK UNIFIED IDEOGRAPH
    0x8A54, 0x6982, //#CJK UNIFIED IDEOGRAPH
    0x8A55, 0x6DAF, //#CJK UNIFIED IDEOGRAPH
    0x8A56, 0x788D, //#CJK UNIFIED IDEOGRAPH
    0x8A57, 0x84CB, //#CJK UNIFIED IDEOGRAPH
    0x8A58, 0x8857, //#CJK UNIFIED IDEOGRAPH
    0x8A59, 0x8A72, //#CJK UNIFIED IDEOGRAPH
    0x8A5A, 0x93A7, //#CJK UNIFIED IDEOGRAPH
    0x8A5B, 0x9AB8, //#CJK UNIFIED IDEOGRAPH
    0x8A5C, 0x6D6C, //#CJK UNIFIED IDEOGRAPH
    0x8A5D, 0x99A8, //#CJK UNIFIED IDEOGRAPH
    0x8A5E, 0x86D9, //#CJK UNIFIED IDEOGRAPH
    0x8A5F, 0x57A3, //#CJK UNIFIED IDEOGRAPH
    0x8A60, 0x67FF, //#CJK UNIFIED IDEOGRAPH
    0x8A61, 0x86CE, //#CJK UNIFIED IDEOGRAPH
    0x8A62, 0x920E, //#CJK UNIFIED IDEOGRAPH
    0x8A63, 0x5283, //#CJK UNIFIED IDEOGRAPH
    0x8A64, 0x5687, //#CJK UNIFIED IDEOGRAPH
    0x8A65, 0x5404, //#CJK UNIFIED IDEOGRAPH
    0x8A66, 0x5ED3, //#CJK UNIFIED IDEOGRAPH
    0x8A67, 0x62E1, //#CJK UNIFIED IDEOGRAPH
    0x8A68, 0x64B9, //#CJK UNIFIED IDEOGRAPH
    0x8A69, 0x683C, //#CJK UNIFIED IDEOGRAPH
    0x8A6A, 0x6838, //#CJK UNIFIED IDEOGRAPH
    0x8A6B, 0x6BBB, //#CJK UNIFIED IDEOGRAPH
    0x8A6C, 0x7372, //#CJK UNIFIED IDEOGRAPH
    0x8A6D, 0x78BA, //#CJK UNIFIED IDEOGRAPH
    0x8A6E, 0x7A6B, //#CJK UNIFIED IDEOGRAPH
    0x8A6F, 0x899A, //#CJK UNIFIED IDEOGRAPH
    0x8A70, 0x89D2, //#CJK UNIFIED IDEOGRAPH
    0x8A71, 0x8D6B, //#CJK UNIFIED IDEOGRAPH
    0x8A72, 0x8F03, //#CJK UNIFIED IDEOGRAPH
    0x8A73, 0x90ED, //#CJK UNIFIED IDEOGRAPH
    0x8A74, 0x95A3, //#CJK UNIFIED IDEOGRAPH
    0x8A75, 0x9694, //#CJK UNIFIED IDEOGRAPH
    0x8A76, 0x9769, //#CJK UNIFIED IDEOGRAPH
    0x8A77, 0x5B66, //#CJK UNIFIED IDEOGRAPH
    0x8A78, 0x5CB3, //#CJK UNIFIED IDEOGRAPH
    0x8A79, 0x697D, //#CJK UNIFIED IDEOGRAPH
    0x8A7A, 0x984D, //#CJK UNIFIED IDEOGRAPH
    0x8A7B, 0x984E, //#CJK UNIFIED IDEOGRAPH
    0x8A7C, 0x639B, //#CJK UNIFIED IDEOGRAPH
    0x8A7D, 0x7B20, //#CJK UNIFIED IDEOGRAPH
    0x8A7E, 0x6A2B, //#CJK UNIFIED IDEOGRAPH
    0x8A80, 0x6A7F, //#CJK UNIFIED IDEOGRAPH
    0x8A81, 0x68B6, //#CJK UNIFIED IDEOGRAPH
    0x8A82, 0x9C0D, //#CJK UNIFIED IDEOGRAPH
    0x8A83, 0x6F5F, //#CJK UNIFIED IDEOGRAPH
    0x8A84, 0x5272, //#CJK UNIFIED IDEOGRAPH
    0x8A85, 0x559D, //#CJK UNIFIED IDEOGRAPH
    0x8A86, 0x6070, //#CJK UNIFIED IDEOGRAPH
    0x8A87, 0x62EC, //#CJK UNIFIED IDEOGRAPH
    0x8A88, 0x6D3B, //#CJK UNIFIED IDEOGRAPH
    0x8A89, 0x6E07, //#CJK UNIFIED IDEOGRAPH
    0x8A8A, 0x6ED1, //#CJK UNIFIED IDEOGRAPH
    0x8A8B, 0x845B, //#CJK UNIFIED IDEOGRAPH
    0x8A8C, 0x8910, //#CJK UNIFIED IDEOGRAPH
    0x8A8D, 0x8F44, //#CJK UNIFIED IDEOGRAPH
    0x8A8E, 0x4E14, //#CJK UNIFIED IDEOGRAPH
    0x8A8F, 0x9C39, //#CJK UNIFIED IDEOGRAPH
    0x8A90, 0x53F6, //#CJK UNIFIED IDEOGRAPH
    0x8A91, 0x691B, //#CJK UNIFIED IDEOGRAPH
    0x8A92, 0x6A3A, //#CJK UNIFIED IDEOGRAPH
    0x8A93, 0x9784, //#CJK UNIFIED IDEOGRAPH
    0x8A94, 0x682A, //#CJK UNIFIED IDEOGRAPH
    0x8A95, 0x515C, //#CJK UNIFIED IDEOGRAPH
    0x8A96, 0x7AC3, //#CJK UNIFIED IDEOGRAPH
    0x8A97, 0x84B2, //#CJK UNIFIED IDEOGRAPH
    0x8A98, 0x91DC, //#CJK UNIFIED IDEOGRAPH
    0x8A99, 0x938C, //#CJK UNIFIED IDEOGRAPH
    0x8A9A, 0x565B, //#CJK UNIFIED IDEOGRAPH
    0x8A9B, 0x9D28, //#CJK UNIFIED IDEOGRAPH
    0x8A9C, 0x6822, //#CJK UNIFIED IDEOGRAPH
    0x8A9D, 0x8305, //#CJK UNIFIED IDEOGRAPH
    0x8A9E, 0x8431, //#CJK UNIFIED IDEOGRAPH
    0x8A9F, 0x7CA5, //#CJK UNIFIED IDEOGRAPH
    0x8AA0, 0x5208, //#CJK UNIFIED IDEOGRAPH
    0x8AA1, 0x82C5, //#CJK UNIFIED IDEOGRAPH
    0x8AA2, 0x74E6, //#CJK UNIFIED IDEOGRAPH
    0x8AA3, 0x4E7E, //#CJK UNIFIED IDEOGRAPH
    0x8AA4, 0x4F83, //#CJK UNIFIED IDEOGRAPH
    0x8AA5, 0x51A0, //#CJK UNIFIED IDEOGRAPH
    0x8AA6, 0x5BD2, //#CJK UNIFIED IDEOGRAPH
    0x8AA7, 0x520A, //#CJK UNIFIED IDEOGRAPH
    0x8AA8, 0x52D8, //#CJK UNIFIED IDEOGRAPH
    0x8AA9, 0x52E7, //#CJK UNIFIED IDEOGRAPH
    0x8AAA, 0x5DFB, //#CJK UNIFIED IDEOGRAPH
    0x8AAB, 0x559A, //#CJK UNIFIED IDEOGRAPH
    0x8AAC, 0x582A, //#CJK UNIFIED IDEOGRAPH
    0x8AAD, 0x59E6, //#CJK UNIFIED IDEOGRAPH
    0x8AAE, 0x5B8C, //#CJK UNIFIED IDEOGRAPH
    0x8AAF, 0x5B98, //#CJK UNIFIED IDEOGRAPH
    0x8AB0, 0x5BDB, //#CJK UNIFIED IDEOGRAPH
    0x8AB1, 0x5E72, //#CJK UNIFIED IDEOGRAPH
    0x8AB2, 0x5E79, //#CJK UNIFIED IDEOGRAPH
    0x8AB3, 0x60A3, //#CJK UNIFIED IDEOGRAPH
    0x8AB4, 0x611F, //#CJK UNIFIED IDEOGRAPH
    0x8AB5, 0x6163, //#CJK UNIFIED IDEOGRAPH
    0x8AB6, 0x61BE, //#CJK UNIFIED IDEOGRAPH
    0x8AB7, 0x63DB, //#CJK UNIFIED IDEOGRAPH
    0x8AB8, 0x6562, //#CJK UNIFIED IDEOGRAPH
    0x8AB9, 0x67D1, //#CJK UNIFIED IDEOGRAPH
    0x8ABA, 0x6853, //#CJK UNIFIED IDEOGRAPH
    0x8ABB, 0x68FA, //#CJK UNIFIED IDEOGRAPH
    0x8ABC, 0x6B3E, //#CJK UNIFIED IDEOGRAPH
    0x8ABD, 0x6B53, //#CJK UNIFIED IDEOGRAPH
    0x8ABE, 0x6C57, //#CJK UNIFIED IDEOGRAPH
    0x8ABF, 0x6F22, //#CJK UNIFIED IDEOGRAPH
    0x8AC0, 0x6F97, //#CJK UNIFIED IDEOGRAPH
    0x8AC1, 0x6F45, //#CJK UNIFIED IDEOGRAPH
    0x8AC2, 0x74B0, //#CJK UNIFIED IDEOGRAPH
    0x8AC3, 0x7518, //#CJK UNIFIED IDEOGRAPH
    0x8AC4, 0x76E3, //#CJK UNIFIED IDEOGRAPH
    0x8AC5, 0x770B, //#CJK UNIFIED IDEOGRAPH
    0x8AC6, 0x7AFF, //#CJK UNIFIED IDEOGRAPH
    0x8AC7, 0x7BA1, //#CJK UNIFIED IDEOGRAPH
    0x8AC8, 0x7C21, //#CJK UNIFIED IDEOGRAPH
    0x8AC9, 0x7DE9, //#CJK UNIFIED IDEOGRAPH
    0x8ACA, 0x7F36, //#CJK UNIFIED IDEOGRAPH
    0x8ACB, 0x7FF0, //#CJK UNIFIED IDEOGRAPH
    0x8ACC, 0x809D, //#CJK UNIFIED IDEOGRAPH
    0x8ACD, 0x8266, //#CJK UNIFIED IDEOGRAPH
    0x8ACE, 0x839E, //#CJK UNIFIED IDEOGRAPH
    0x8ACF, 0x89B3, //#CJK UNIFIED IDEOGRAPH
    0x8AD0, 0x8ACC, //#CJK UNIFIED IDEOGRAPH
    0x8AD1, 0x8CAB, //#CJK UNIFIED IDEOGRAPH
    0x8AD2, 0x9084, //#CJK UNIFIED IDEOGRAPH
    0x8AD3, 0x9451, //#CJK UNIFIED IDEOGRAPH
    0x8AD4, 0x9593, //#CJK UNIFIED IDEOGRAPH
    0x8AD5, 0x9591, //#CJK UNIFIED IDEOGRAPH
    0x8AD6, 0x95A2, //#CJK UNIFIED IDEOGRAPH
    0x8AD7, 0x9665, //#CJK UNIFIED IDEOGRAPH
    0x8AD8, 0x97D3, //#CJK UNIFIED IDEOGRAPH
    0x8AD9, 0x9928, //#CJK UNIFIED IDEOGRAPH
    0x8ADA, 0x8218, //#CJK UNIFIED IDEOGRAPH
    0x8ADB, 0x4E38, //#CJK UNIFIED IDEOGRAPH
    0x8ADC, 0x542B, //#CJK UNIFIED IDEOGRAPH
    0x8ADD, 0x5CB8, //#CJK UNIFIED IDEOGRAPH
    0x8ADE, 0x5DCC, //#CJK UNIFIED IDEOGRAPH
    0x8ADF, 0x73A9, //#CJK UNIFIED IDEOGRAPH
    0x8AE0, 0x764C, //#CJK UNIFIED IDEOGRAPH
    0x8AE1, 0x773C, //#CJK UNIFIED IDEOGRAPH
    0x8AE2, 0x5CA9, //#CJK UNIFIED IDEOGRAPH
    0x8AE3, 0x7FEB, //#CJK UNIFIED IDEOGRAPH
    0x8AE4, 0x8D0B, //#CJK UNIFIED IDEOGRAPH
    0x8AE5, 0x96C1, //#CJK UNIFIED IDEOGRAPH
    0x8AE6, 0x9811, //#CJK UNIFIED IDEOGRAPH
    0x8AE7, 0x9854, //#CJK UNIFIED IDEOGRAPH
    0x8AE8, 0x9858, //#CJK UNIFIED IDEOGRAPH
    0x8AE9, 0x4F01, //#CJK UNIFIED IDEOGRAPH
    0x8AEA, 0x4F0E, //#CJK UNIFIED IDEOGRAPH
    0x8AEB, 0x5371, //#CJK UNIFIED IDEOGRAPH
    0x8AEC, 0x559C, //#CJK UNIFIED IDEOGRAPH
    0x8AED, 0x5668, //#CJK UNIFIED IDEOGRAPH
    0x8AEE, 0x57FA, //#CJK UNIFIED IDEOGRAPH
    0x8AEF, 0x5947, //#CJK UNIFIED IDEOGRAPH
    0x8AF0, 0x5B09, //#CJK UNIFIED IDEOGRAPH
    0x8AF1, 0x5BC4, //#CJK UNIFIED IDEOGRAPH
    0x8AF2, 0x5C90, //#CJK UNIFIED IDEOGRAPH
    0x8AF3, 0x5E0C, //#CJK UNIFIED IDEOGRAPH
    0x8AF4, 0x5E7E, //#CJK UNIFIED IDEOGRAPH
    0x8AF5, 0x5FCC, //#CJK UNIFIED IDEOGRAPH
    0x8AF6, 0x63EE, //#CJK UNIFIED IDEOGRAPH
    0x8AF7, 0x673A, //#CJK UNIFIED IDEOGRAPH
    0x8AF8, 0x65D7, //#CJK UNIFIED IDEOGRAPH
    0x8AF9, 0x65E2, //#CJK UNIFIED IDEOGRAPH
    0x8AFA, 0x671F, //#CJK UNIFIED IDEOGRAPH
    0x8AFB, 0x68CB, //#CJK UNIFIED IDEOGRAPH
    0x8AFC, 0x68C4, //#CJK UNIFIED IDEOGRAPH
    0x8B40, 0x6A5F, //#CJK UNIFIED IDEOGRAPH
    0x8B41, 0x5E30, //#CJK UNIFIED IDEOGRAPH
    0x8B42, 0x6BC5, //#CJK UNIFIED IDEOGRAPH
    0x8B43, 0x6C17, //#CJK UNIFIED IDEOGRAPH
    0x8B44, 0x6C7D, //#CJK UNIFIED IDEOGRAPH
    0x8B45, 0x757F, //#CJK UNIFIED IDEOGRAPH
    0x8B46, 0x7948, //#CJK UNIFIED IDEOGRAPH
    0x8B47, 0x5B63, //#CJK UNIFIED IDEOGRAPH
    0x8B48, 0x7A00, //#CJK UNIFIED IDEOGRAPH
    0x8B49, 0x7D00, //#CJK UNIFIED IDEOGRAPH
    0x8B4A, 0x5FBD, //#CJK UNIFIED IDEOGRAPH
    0x8B4B, 0x898F, //#CJK UNIFIED IDEOGRAPH
    0x8B4C, 0x8A18, //#CJK UNIFIED IDEOGRAPH
    0x8B4D, 0x8CB4, //#CJK UNIFIED IDEOGRAPH
    0x8B4E, 0x8D77, //#CJK UNIFIED IDEOGRAPH
    0x8B4F, 0x8ECC, //#CJK UNIFIED IDEOGRAPH
    0x8B50, 0x8F1D, //#CJK UNIFIED IDEOGRAPH
    0x8B51, 0x98E2, //#CJK UNIFIED IDEOGRAPH
    0x8B52, 0x9A0E, //#CJK UNIFIED IDEOGRAPH
    0x8B53, 0x9B3C, //#CJK UNIFIED IDEOGRAPH
    0x8B54, 0x4E80, //#CJK UNIFIED IDEOGRAPH
    0x8B55, 0x507D, //#CJK UNIFIED IDEOGRAPH
    0x8B56, 0x5100, //#CJK UNIFIED IDEOGRAPH
    0x8B57, 0x5993, //#CJK UNIFIED IDEOGRAPH
    0x8B58, 0x5B9C, //#CJK UNIFIED IDEOGRAPH
    0x8B59, 0x622F, //#CJK UNIFIED IDEOGRAPH
    0x8B5A, 0x6280, //#CJK UNIFIED IDEOGRAPH
    0x8B5B, 0x64EC, //#CJK UNIFIED IDEOGRAPH
    0x8B5C, 0x6B3A, //#CJK UNIFIED IDEOGRAPH
    0x8B5D, 0x72A0, //#CJK UNIFIED IDEOGRAPH
    0x8B5E, 0x7591, //#CJK UNIFIED IDEOGRAPH
    0x8B5F, 0x7947, //#CJK UNIFIED IDEOGRAPH
    0x8B60, 0x7FA9, //#CJK UNIFIED IDEOGRAPH
    0x8B61, 0x87FB, //#CJK UNIFIED IDEOGRAPH
    0x8B62, 0x8ABC, //#CJK UNIFIED IDEOGRAPH
    0x8B63, 0x8B70, //#CJK UNIFIED IDEOGRAPH
    0x8B64, 0x63AC, //#CJK UNIFIED IDEOGRAPH
    0x8B65, 0x83CA, //#CJK UNIFIED IDEOGRAPH
    0x8B66, 0x97A0, //#CJK UNIFIED IDEOGRAPH
    0x8B67, 0x5409, //#CJK UNIFIED IDEOGRAPH
    0x8B68, 0x5403, //#CJK UNIFIED IDEOGRAPH
    0x8B69, 0x55AB, //#CJK UNIFIED IDEOGRAPH
    0x8B6A, 0x6854, //#CJK UNIFIED IDEOGRAPH
    0x8B6B, 0x6A58, //#CJK UNIFIED IDEOGRAPH
    0x8B6C, 0x8A70, //#CJK UNIFIED IDEOGRAPH
    0x8B6D, 0x7827, //#CJK UNIFIED IDEOGRAPH
    0x8B6E, 0x6775, //#CJK UNIFIED IDEOGRAPH
    0x8B6F, 0x9ECD, //#CJK UNIFIED IDEOGRAPH
    0x8B70, 0x5374, //#CJK UNIFIED IDEOGRAPH
    0x8B71, 0x5BA2, //#CJK UNIFIED IDEOGRAPH
    0x8B72, 0x811A, //#CJK UNIFIED IDEOGRAPH
    0x8B73, 0x8650, //#CJK UNIFIED IDEOGRAPH
    0x8B74, 0x9006, //#CJK UNIFIED IDEOGRAPH
    0x8B75, 0x4E18, //#CJK UNIFIED IDEOGRAPH
    0x8B76, 0x4E45, //#CJK UNIFIED IDEOGRAPH
    0x8B77, 0x4EC7, //#CJK UNIFIED IDEOGRAPH
    0x8B78, 0x4F11, //#CJK UNIFIED IDEOGRAPH
    0x8B79, 0x53CA, //#CJK UNIFIED IDEOGRAPH
    0x8B7A, 0x5438, //#CJK UNIFIED IDEOGRAPH
    0x8B7B, 0x5BAE, //#CJK UNIFIED IDEOGRAPH
    0x8B7C, 0x5F13, //#CJK UNIFIED IDEOGRAPH
    0x8B7D, 0x6025, //#CJK UNIFIED IDEOGRAPH
    0x8B7E, 0x6551, //#CJK UNIFIED IDEOGRAPH
    0x8B80, 0x673D, //#CJK UNIFIED IDEOGRAPH
    0x8B81, 0x6C42, //#CJK UNIFIED IDEOGRAPH
    0x8B82, 0x6C72, //#CJK UNIFIED IDEOGRAPH
    0x8B83, 0x6CE3, //#CJK UNIFIED IDEOGRAPH
    0x8B84, 0x7078, //#CJK UNIFIED IDEOGRAPH
    0x8B85, 0x7403, //#CJK UNIFIED IDEOGRAPH
    0x8B86, 0x7A76, //#CJK UNIFIED IDEOGRAPH
    0x8B87, 0x7AAE, //#CJK UNIFIED IDEOGRAPH
    0x8B88, 0x7B08, //#CJK UNIFIED IDEOGRAPH
    0x8B89, 0x7D1A, //#CJK UNIFIED IDEOGRAPH
    0x8B8A, 0x7CFE, //#CJK UNIFIED IDEOGRAPH
    0x8B8B, 0x7D66, //#CJK UNIFIED IDEOGRAPH
    0x8B8C, 0x65E7, //#CJK UNIFIED IDEOGRAPH
    0x8B8D, 0x725B, //#CJK UNIFIED IDEOGRAPH
    0x8B8E, 0x53BB, //#CJK UNIFIED IDEOGRAPH
    0x8B8F, 0x5C45, //#CJK UNIFIED IDEOGRAPH
    0x8B90, 0x5DE8, //#CJK UNIFIED IDEOGRAPH
    0x8B91, 0x62D2, //#CJK UNIFIED IDEOGRAPH
    0x8B92, 0x62E0, //#CJK UNIFIED IDEOGRAPH
    0x8B93, 0x6319, //#CJK UNIFIED IDEOGRAPH
    0x8B94, 0x6E20, //#CJK UNIFIED IDEOGRAPH
    0x8B95, 0x865A, //#CJK UNIFIED IDEOGRAPH
    0x8B96, 0x8A31, //#CJK UNIFIED IDEOGRAPH
    0x8B97, 0x8DDD, //#CJK UNIFIED IDEOGRAPH
    0x8B98, 0x92F8, //#CJK UNIFIED IDEOGRAPH
    0x8B99, 0x6F01, //#CJK UNIFIED IDEOGRAPH
    0x8B9A, 0x79A6, //#CJK UNIFIED IDEOGRAPH
    0x8B9B, 0x9B5A, //#CJK UNIFIED IDEOGRAPH
    0x8B9C, 0x4EA8, //#CJK UNIFIED IDEOGRAPH
    0x8B9D, 0x4EAB, //#CJK UNIFIED IDEOGRAPH
    0x8B9E, 0x4EAC, //#CJK UNIFIED IDEOGRAPH
    0x8B9F, 0x4F9B, //#CJK UNIFIED IDEOGRAPH
    0x8BA0, 0x4FA0, //#CJK UNIFIED IDEOGRAPH
    0x8BA1, 0x50D1, //#CJK UNIFIED IDEOGRAPH
    0x8BA2, 0x5147, //#CJK UNIFIED IDEOGRAPH
    0x8BA3, 0x7AF6, //#CJK UNIFIED IDEOGRAPH
    0x8BA4, 0x5171, //#CJK UNIFIED IDEOGRAPH
    0x8BA5, 0x51F6, //#CJK UNIFIED IDEOGRAPH
    0x8BA6, 0x5354, //#CJK UNIFIED IDEOGRAPH
    0x8BA7, 0x5321, //#CJK UNIFIED IDEOGRAPH
    0x8BA8, 0x537F, //#CJK UNIFIED IDEOGRAPH
    0x8BA9, 0x53EB, //#CJK UNIFIED IDEOGRAPH
    0x8BAA, 0x55AC, //#CJK UNIFIED IDEOGRAPH
    0x8BAB, 0x5883, //#CJK UNIFIED IDEOGRAPH
    0x8BAC, 0x5CE1, //#CJK UNIFIED IDEOGRAPH
    0x8BAD, 0x5F37, //#CJK UNIFIED IDEOGRAPH
    0x8BAE, 0x5F4A, //#CJK UNIFIED IDEOGRAPH
    0x8BAF, 0x602F, //#CJK UNIFIED IDEOGRAPH
    0x8BB0, 0x6050, //#CJK UNIFIED IDEOGRAPH
    0x8BB1, 0x606D, //#CJK UNIFIED IDEOGRAPH
    0x8BB2, 0x631F, //#CJK UNIFIED IDEOGRAPH
    0x8BB3, 0x6559, //#CJK UNIFIED IDEOGRAPH
    0x8BB4, 0x6A4B, //#CJK UNIFIED IDEOGRAPH
    0x8BB5, 0x6CC1, //#CJK UNIFIED IDEOGRAPH
    0x8BB6, 0x72C2, //#CJK UNIFIED IDEOGRAPH
    0x8BB7, 0x72ED, //#CJK UNIFIED IDEOGRAPH
    0x8BB8, 0x77EF, //#CJK UNIFIED IDEOGRAPH
    0x8BB9, 0x80F8, //#CJK UNIFIED IDEOGRAPH
    0x8BBA, 0x8105, //#CJK UNIFIED IDEOGRAPH
    0x8BBB, 0x8208, //#CJK UNIFIED IDEOGRAPH
    0x8BBC, 0x854E, //#CJK UNIFIED IDEOGRAPH
    0x8BBD, 0x90F7, //#CJK UNIFIED IDEOGRAPH
    0x8BBE, 0x93E1, //#CJK UNIFIED IDEOGRAPH
    0x8BBF, 0x97FF, //#CJK UNIFIED IDEOGRAPH
    0x8BC0, 0x9957, //#CJK UNIFIED IDEOGRAPH
    0x8BC1, 0x9A5A, //#CJK UNIFIED IDEOGRAPH
    0x8BC2, 0x4EF0, //#CJK UNIFIED IDEOGRAPH
    0x8BC3, 0x51DD, //#CJK UNIFIED IDEOGRAPH
    0x8BC4, 0x5C2D, //#CJK UNIFIED IDEOGRAPH
    0x8BC5, 0x6681, //#CJK UNIFIED IDEOGRAPH
    0x8BC6, 0x696D, //#CJK UNIFIED IDEOGRAPH
    0x8BC7, 0x5C40, //#CJK UNIFIED IDEOGRAPH
    0x8BC8, 0x66F2, //#CJK UNIFIED IDEOGRAPH
    0x8BC9, 0x6975, //#CJK UNIFIED IDEOGRAPH
    0x8BCA, 0x7389, //#CJK UNIFIED IDEOGRAPH
    0x8BCB, 0x6850, //#CJK UNIFIED IDEOGRAPH
    0x8BCC, 0x7C81, //#CJK UNIFIED IDEOGRAPH
    0x8BCD, 0x50C5, //#CJK UNIFIED IDEOGRAPH
    0x8BCE, 0x52E4, //#CJK UNIFIED IDEOGRAPH
    0x8BCF, 0x5747, //#CJK UNIFIED IDEOGRAPH
    0x8BD0, 0x5DFE, //#CJK UNIFIED IDEOGRAPH
    0x8BD1, 0x9326, //#CJK UNIFIED IDEOGRAPH
    0x8BD2, 0x65A4, //#CJK UNIFIED IDEOGRAPH
    0x8BD3, 0x6B23, //#CJK UNIFIED IDEOGRAPH
    0x8BD4, 0x6B3D, //#CJK UNIFIED IDEOGRAPH
    0x8BD5, 0x7434, //#CJK UNIFIED IDEOGRAPH
    0x8BD6, 0x7981, //#CJK UNIFIED IDEOGRAPH
    0x8BD7, 0x79BD, //#CJK UNIFIED IDEOGRAPH
    0x8BD8, 0x7B4B, //#CJK UNIFIED IDEOGRAPH
    0x8BD9, 0x7DCA, //#CJK UNIFIED IDEOGRAPH
    0x8BDA, 0x82B9, //#CJK UNIFIED IDEOGRAPH
    0x8BDB, 0x83CC, //#CJK UNIFIED IDEOGRAPH
    0x8BDC, 0x887F, //#CJK UNIFIED IDEOGRAPH
    0x8BDD, 0x895F, //#CJK UNIFIED IDEOGRAPH
    0x8BDE, 0x8B39, //#CJK UNIFIED IDEOGRAPH
    0x8BDF, 0x8FD1, //#CJK UNIFIED IDEOGRAPH
    0x8BE0, 0x91D1, //#CJK UNIFIED IDEOGRAPH
    0x8BE1, 0x541F, //#CJK UNIFIED IDEOGRAPH
    0x8BE2, 0x9280, //#CJK UNIFIED IDEOGRAPH
    0x8BE3, 0x4E5D, //#CJK UNIFIED IDEOGRAPH
    0x8BE4, 0x5036, //#CJK UNIFIED IDEOGRAPH
    0x8BE5, 0x53E5, //#CJK UNIFIED IDEOGRAPH
    0x8BE6, 0x533A, //#CJK UNIFIED IDEOGRAPH
    0x8BE7, 0x72D7, //#CJK UNIFIED IDEOGRAPH
    0x8BE8, 0x7396, //#CJK UNIFIED IDEOGRAPH
    0x8BE9, 0x77E9, //#CJK UNIFIED IDEOGRAPH
    0x8BEA, 0x82E6, //#CJK UNIFIED IDEOGRAPH
    0x8BEB, 0x8EAF, //#CJK UNIFIED IDEOGRAPH
    0x8BEC, 0x99C6, //#CJK UNIFIED IDEOGRAPH
    0x8BED, 0x99C8, //#CJK UNIFIED IDEOGRAPH
    0x8BEE, 0x99D2, //#CJK UNIFIED IDEOGRAPH
    0x8BEF, 0x5177, //#CJK UNIFIED IDEOGRAPH
    0x8BF0, 0x611A, //#CJK UNIFIED IDEOGRAPH
    0x8BF1, 0x865E, //#CJK UNIFIED IDEOGRAPH
    0x8BF2, 0x55B0, //#CJK UNIFIED IDEOGRAPH
    0x8BF3, 0x7A7A, //#CJK UNIFIED IDEOGRAPH
    0x8BF4, 0x5076, //#CJK UNIFIED IDEOGRAPH
    0x8BF5, 0x5BD3, //#CJK UNIFIED IDEOGRAPH
    0x8BF6, 0x9047, //#CJK UNIFIED IDEOGRAPH
    0x8BF7, 0x9685, //#CJK UNIFIED IDEOGRAPH
    0x8BF8, 0x4E32, //#CJK UNIFIED IDEOGRAPH
    0x8BF9, 0x6ADB, //#CJK UNIFIED IDEOGRAPH
    0x8BFA, 0x91E7, //#CJK UNIFIED IDEOGRAPH
    0x8BFB, 0x5C51, //#CJK UNIFIED IDEOGRAPH
    0x8BFC, 0x5C48, //#CJK UNIFIED IDEOGRAPH
    0x8C40, 0x6398, //#CJK UNIFIED IDEOGRAPH
    0x8C41, 0x7A9F, //#CJK UNIFIED IDEOGRAPH
    0x8C42, 0x6C93, //#CJK UNIFIED IDEOGRAPH
    0x8C43, 0x9774, //#CJK UNIFIED IDEOGRAPH
    0x8C44, 0x8F61, //#CJK UNIFIED IDEOGRAPH
    0x8C45, 0x7AAA, //#CJK UNIFIED IDEOGRAPH
    0x8C46, 0x718A, //#CJK UNIFIED IDEOGRAPH
    0x8C47, 0x9688, //#CJK UNIFIED IDEOGRAPH
    0x8C48, 0x7C82, //#CJK UNIFIED IDEOGRAPH
    0x8C49, 0x6817, //#CJK UNIFIED IDEOGRAPH
    0x8C4A, 0x7E70, //#CJK UNIFIED IDEOGRAPH
    0x8C4B, 0x6851, //#CJK UNIFIED IDEOGRAPH
    0x8C4C, 0x936C, //#CJK UNIFIED IDEOGRAPH
    0x8C4D, 0x52F2, //#CJK UNIFIED IDEOGRAPH
    0x8C4E, 0x541B, //#CJK UNIFIED IDEOGRAPH
    0x8C4F, 0x85AB, //#CJK UNIFIED IDEOGRAPH
    0x8C50, 0x8A13, //#CJK UNIFIED IDEOGRAPH
    0x8C51, 0x7FA4, //#CJK UNIFIED IDEOGRAPH
    0x8C52, 0x8ECD, //#CJK UNIFIED IDEOGRAPH
    0x8C53, 0x90E1, //#CJK UNIFIED IDEOGRAPH
    0x8C54, 0x5366, //#CJK UNIFIED IDEOGRAPH
    0x8C55, 0x8888, //#CJK UNIFIED IDEOGRAPH
    0x8C56, 0x7941, //#CJK UNIFIED IDEOGRAPH
    0x8C57, 0x4FC2, //#CJK UNIFIED IDEOGRAPH
    0x8C58, 0x50BE, //#CJK UNIFIED IDEOGRAPH
    0x8C59, 0x5211, //#CJK UNIFIED IDEOGRAPH
    0x8C5A, 0x5144, //#CJK UNIFIED IDEOGRAPH
    0x8C5B, 0x5553, //#CJK UNIFIED IDEOGRAPH
    0x8C5C, 0x572D, //#CJK UNIFIED IDEOGRAPH
    0x8C5D, 0x73EA, //#CJK UNIFIED IDEOGRAPH
    0x8C5E, 0x578B, //#CJK UNIFIED IDEOGRAPH
    0x8C5F, 0x5951, //#CJK UNIFIED IDEOGRAPH
    0x8C60, 0x5F62, //#CJK UNIFIED IDEOGRAPH
    0x8C61, 0x5F84, //#CJK UNIFIED IDEOGRAPH
    0x8C62, 0x6075, //#CJK UNIFIED IDEOGRAPH
    0x8C63, 0x6176, //#CJK UNIFIED IDEOGRAPH
    0x8C64, 0x6167, //#CJK UNIFIED IDEOGRAPH
    0x8C65, 0x61A9, //#CJK UNIFIED IDEOGRAPH
    0x8C66, 0x63B2, //#CJK UNIFIED IDEOGRAPH
    0x8C67, 0x643A, //#CJK UNIFIED IDEOGRAPH
    0x8C68, 0x656C, //#CJK UNIFIED IDEOGRAPH
    0x8C69, 0x666F, //#CJK UNIFIED IDEOGRAPH
    0x8C6A, 0x6842, //#CJK UNIFIED IDEOGRAPH
    0x8C6B, 0x6E13, //#CJK UNIFIED IDEOGRAPH
    0x8C6C, 0x7566, //#CJK UNIFIED IDEOGRAPH
    0x8C6D, 0x7A3D, //#CJK UNIFIED IDEOGRAPH
    0x8C6E, 0x7CFB, //#CJK UNIFIED IDEOGRAPH
    0x8C6F, 0x7D4C, //#CJK UNIFIED IDEOGRAPH
    0x8C70, 0x7D99, //#CJK UNIFIED IDEOGRAPH
    0x8C71, 0x7E4B, //#CJK UNIFIED IDEOGRAPH
    0x8C72, 0x7F6B, //#CJK UNIFIED IDEOGRAPH
    0x8C73, 0x830E, //#CJK UNIFIED IDEOGRAPH
    0x8C74, 0x834A, //#CJK UNIFIED IDEOGRAPH
    0x8C75, 0x86CD, //#CJK UNIFIED IDEOGRAPH
    0x8C76, 0x8A08, //#CJK UNIFIED IDEOGRAPH
    0x8C77, 0x8A63, //#CJK UNIFIED IDEOGRAPH
    0x8C78, 0x8B66, //#CJK UNIFIED IDEOGRAPH
    0x8C79, 0x8EFD, //#CJK UNIFIED IDEOGRAPH
    0x8C7A, 0x981A, //#CJK UNIFIED IDEOGRAPH
    0x8C7B, 0x9D8F, //#CJK UNIFIED IDEOGRAPH
    0x8C7C, 0x82B8, //#CJK UNIFIED IDEOGRAPH
    0x8C7D, 0x8FCE, //#CJK UNIFIED IDEOGRAPH
    0x8C7E, 0x9BE8, //#CJK UNIFIED IDEOGRAPH
    0x8C80, 0x5287, //#CJK UNIFIED IDEOGRAPH
    0x8C81, 0x621F, //#CJK UNIFIED IDEOGRAPH
    0x8C82, 0x6483, //#CJK UNIFIED IDEOGRAPH
    0x8C83, 0x6FC0, //#CJK UNIFIED IDEOGRAPH
    0x8C84, 0x9699, //#CJK UNIFIED IDEOGRAPH
    0x8C85, 0x6841, //#CJK UNIFIED IDEOGRAPH
    0x8C86, 0x5091, //#CJK UNIFIED IDEOGRAPH
    0x8C87, 0x6B20, //#CJK UNIFIED IDEOGRAPH
    0x8C88, 0x6C7A, //#CJK UNIFIED IDEOGRAPH
    0x8C89, 0x6F54, //#CJK UNIFIED IDEOGRAPH
    0x8C8A, 0x7A74, //#CJK UNIFIED IDEOGRAPH
    0x8C8B, 0x7D50, //#CJK UNIFIED IDEOGRAPH
    0x8C8C, 0x8840, //#CJK UNIFIED IDEOGRAPH
    0x8C8D, 0x8A23, //#CJK UNIFIED IDEOGRAPH
    0x8C8E, 0x6708, //#CJK UNIFIED IDEOGRAPH
    0x8C8F, 0x4EF6, //#CJK UNIFIED IDEOGRAPH
    0x8C90, 0x5039, //#CJK UNIFIED IDEOGRAPH
    0x8C91, 0x5026, //#CJK UNIFIED IDEOGRAPH
    0x8C92, 0x5065, //#CJK UNIFIED IDEOGRAPH
    0x8C93, 0x517C, //#CJK UNIFIED IDEOGRAPH
    0x8C94, 0x5238, //#CJK UNIFIED IDEOGRAPH
    0x8C95, 0x5263, //#CJK UNIFIED IDEOGRAPH
    0x8C96, 0x55A7, //#CJK UNIFIED IDEOGRAPH
    0x8C97, 0x570F, //#CJK UNIFIED IDEOGRAPH
    0x8C98, 0x5805, //#CJK UNIFIED IDEOGRAPH
    0x8C99, 0x5ACC, //#CJK UNIFIED IDEOGRAPH
    0x8C9A, 0x5EFA, //#CJK UNIFIED IDEOGRAPH
    0x8C9B, 0x61B2, //#CJK UNIFIED IDEOGRAPH
    0x8C9C, 0x61F8, //#CJK UNIFIED IDEOGRAPH
    0x8C9D, 0x62F3, //#CJK UNIFIED IDEOGRAPH
    0x8C9E, 0x6372, //#CJK UNIFIED IDEOGRAPH
    0x8C9F, 0x691C, //#CJK UNIFIED IDEOGRAPH
    0x8CA0, 0x6A29, //#CJK UNIFIED IDEOGRAPH
    0x8CA1, 0x727D, //#CJK UNIFIED IDEOGRAPH
    0x8CA2, 0x72AC, //#CJK UNIFIED IDEOGRAPH
    0x8CA3, 0x732E, //#CJK UNIFIED IDEOGRAPH
    0x8CA4, 0x7814, //#CJK UNIFIED IDEOGRAPH
    0x8CA5, 0x786F, //#CJK UNIFIED IDEOGRAPH
    0x8CA6, 0x7D79, //#CJK UNIFIED IDEOGRAPH
    0x8CA7, 0x770C, //#CJK UNIFIED IDEOGRAPH
    0x8CA8, 0x80A9, //#CJK UNIFIED IDEOGRAPH
    0x8CA9, 0x898B, //#CJK UNIFIED IDEOGRAPH
    0x8CAA, 0x8B19, //#CJK UNIFIED IDEOGRAPH
    0x8CAB, 0x8CE2, //#CJK UNIFIED IDEOGRAPH
    0x8CAC, 0x8ED2, //#CJK UNIFIED IDEOGRAPH
    0x8CAD, 0x9063, //#CJK UNIFIED IDEOGRAPH
    0x8CAE, 0x9375, //#CJK UNIFIED IDEOGRAPH
    0x8CAF, 0x967A, //#CJK UNIFIED IDEOGRAPH
    0x8CB0, 0x9855, //#CJK UNIFIED IDEOGRAPH
    0x8CB1, 0x9A13, //#CJK UNIFIED IDEOGRAPH
    0x8CB2, 0x9E78, //#CJK UNIFIED IDEOGRAPH
    0x8CB3, 0x5143, //#CJK UNIFIED IDEOGRAPH
    0x8CB4, 0x539F, //#CJK UNIFIED IDEOGRAPH
    0x8CB5, 0x53B3, //#CJK UNIFIED IDEOGRAPH
    0x8CB6, 0x5E7B, //#CJK UNIFIED IDEOGRAPH
    0x8CB7, 0x5F26, //#CJK UNIFIED IDEOGRAPH
    0x8CB8, 0x6E1B, //#CJK UNIFIED IDEOGRAPH
    0x8CB9, 0x6E90, //#CJK UNIFIED IDEOGRAPH
    0x8CBA, 0x7384, //#CJK UNIFIED IDEOGRAPH
    0x8CBB, 0x73FE, //#CJK UNIFIED IDEOGRAPH
    0x8CBC, 0x7D43, //#CJK UNIFIED IDEOGRAPH
    0x8CBD, 0x8237, //#CJK UNIFIED IDEOGRAPH
    0x8CBE, 0x8A00, //#CJK UNIFIED IDEOGRAPH
    0x8CBF, 0x8AFA, //#CJK UNIFIED IDEOGRAPH
    0x8CC0, 0x9650, //#CJK UNIFIED IDEOGRAPH
    0x8CC1, 0x4E4E, //#CJK UNIFIED IDEOGRAPH
    0x8CC2, 0x500B, //#CJK UNIFIED IDEOGRAPH
    0x8CC3, 0x53E4, //#CJK UNIFIED IDEOGRAPH
    0x8CC4, 0x547C, //#CJK UNIFIED IDEOGRAPH
    0x8CC5, 0x56FA, //#CJK UNIFIED IDEOGRAPH
    0x8CC6, 0x59D1, //#CJK UNIFIED IDEOGRAPH
    0x8CC7, 0x5B64, //#CJK UNIFIED IDEOGRAPH
    0x8CC8, 0x5DF1, //#CJK UNIFIED IDEOGRAPH
    0x8CC9, 0x5EAB, //#CJK UNIFIED IDEOGRAPH
    0x8CCA, 0x5F27, //#CJK UNIFIED IDEOGRAPH
    0x8CCB, 0x6238, //#CJK UNIFIED IDEOGRAPH
    0x8CCC, 0x6545, //#CJK UNIFIED IDEOGRAPH
    0x8CCD, 0x67AF, //#CJK UNIFIED IDEOGRAPH
    0x8CCE, 0x6E56, //#CJK UNIFIED IDEOGRAPH
    0x8CCF, 0x72D0, //#CJK UNIFIED IDEOGRAPH
    0x8CD0, 0x7CCA, //#CJK UNIFIED IDEOGRAPH
    0x8CD1, 0x88B4, //#CJK UNIFIED IDEOGRAPH
    0x8CD2, 0x80A1, //#CJK UNIFIED IDEOGRAPH
    0x8CD3, 0x80E1, //#CJK UNIFIED IDEOGRAPH
    0x8CD4, 0x83F0, //#CJK UNIFIED IDEOGRAPH
    0x8CD5, 0x864E, //#CJK UNIFIED IDEOGRAPH
    0x8CD6, 0x8A87, //#CJK UNIFIED IDEOGRAPH
    0x8CD7, 0x8DE8, //#CJK UNIFIED IDEOGRAPH
    0x8CD8, 0x9237, //#CJK UNIFIED IDEOGRAPH
    0x8CD9, 0x96C7, //#CJK UNIFIED IDEOGRAPH
    0x8CDA, 0x9867, //#CJK UNIFIED IDEOGRAPH
    0x8CDB, 0x9F13, //#CJK UNIFIED IDEOGRAPH
    0x8CDC, 0x4E94, //#CJK UNIFIED IDEOGRAPH
    0x8CDD, 0x4E92, //#CJK UNIFIED IDEOGRAPH
    0x8CDE, 0x4F0D, //#CJK UNIFIED IDEOGRAPH
    0x8CDF, 0x5348, //#CJK UNIFIED IDEOGRAPH
    0x8CE0, 0x5449, //#CJK UNIFIED IDEOGRAPH
    0x8CE1, 0x543E, //#CJK UNIFIED IDEOGRAPH
    0x8CE2, 0x5A2F, //#CJK UNIFIED IDEOGRAPH
    0x8CE3, 0x5F8C, //#CJK UNIFIED IDEOGRAPH
    0x8CE4, 0x5FA1, //#CJK UNIFIED IDEOGRAPH
    0x8CE5, 0x609F, //#CJK UNIFIED IDEOGRAPH
    0x8CE6, 0x68A7, //#CJK UNIFIED IDEOGRAPH
    0x8CE7, 0x6A8E, //#CJK UNIFIED IDEOGRAPH
    0x8CE8, 0x745A, //#CJK UNIFIED IDEOGRAPH
    0x8CE9, 0x7881, //#CJK UNIFIED IDEOGRAPH
    0x8CEA, 0x8A9E, //#CJK UNIFIED IDEOGRAPH
    0x8CEB, 0x8AA4, //#CJK UNIFIED IDEOGRAPH
    0x8CEC, 0x8B77, //#CJK UNIFIED IDEOGRAPH
    0x8CED, 0x9190, //#CJK UNIFIED IDEOGRAPH
    0x8CEE, 0x4E5E, //#CJK UNIFIED IDEOGRAPH
    0x8CEF, 0x9BC9, //#CJK UNIFIED IDEOGRAPH
    0x8CF0, 0x4EA4, //#CJK UNIFIED IDEOGRAPH
    0x8CF1, 0x4F7C, //#CJK UNIFIED IDEOGRAPH
    0x8CF2, 0x4FAF, //#CJK UNIFIED IDEOGRAPH
    0x8CF3, 0x5019, //#CJK UNIFIED IDEOGRAPH
    0x8CF4, 0x5016, //#CJK UNIFIED IDEOGRAPH
    0x8CF5, 0x5149, //#CJK UNIFIED IDEOGRAPH
    0x8CF6, 0x516C, //#CJK UNIFIED IDEOGRAPH
    0x8CF7, 0x529F, //#CJK UNIFIED IDEOGRAPH
    0x8CF8, 0x52B9, //#CJK UNIFIED IDEOGRAPH
    0x8CF9, 0x52FE, //#CJK UNIFIED IDEOGRAPH
    0x8CFA, 0x539A, //#CJK UNIFIED IDEOGRAPH
    0x8CFB, 0x53E3, //#CJK UNIFIED IDEOGRAPH
    0x8CFC, 0x5411, //#CJK UNIFIED IDEOGRAPH
    0x8D40, 0x540E, //#CJK UNIFIED IDEOGRAPH
    0x8D41, 0x5589, //#CJK UNIFIED IDEOGRAPH
    0x8D42, 0x5751, //#CJK UNIFIED IDEOGRAPH
    0x8D43, 0x57A2, //#CJK UNIFIED IDEOGRAPH
    0x8D44, 0x597D, //#CJK UNIFIED IDEOGRAPH
    0x8D45, 0x5B54, //#CJK UNIFIED IDEOGRAPH
    0x8D46, 0x5B5D, //#CJK UNIFIED IDEOGRAPH
    0x8D47, 0x5B8F, //#CJK UNIFIED IDEOGRAPH
    0x8D48, 0x5DE5, //#CJK UNIFIED IDEOGRAPH
    0x8D49, 0x5DE7, //#CJK UNIFIED IDEOGRAPH
    0x8D4A, 0x5DF7, //#CJK UNIFIED IDEOGRAPH
    0x8D4B, 0x5E78, //#CJK UNIFIED IDEOGRAPH
    0x8D4C, 0x5E83, //#CJK UNIFIED IDEOGRAPH
    0x8D4D, 0x5E9A, //#CJK UNIFIED IDEOGRAPH
    0x8D4E, 0x5EB7, //#CJK UNIFIED IDEOGRAPH
    0x8D4F, 0x5F18, //#CJK UNIFIED IDEOGRAPH
    0x8D50, 0x6052, //#CJK UNIFIED IDEOGRAPH
    0x8D51, 0x614C, //#CJK UNIFIED IDEOGRAPH
    0x8D52, 0x6297, //#CJK UNIFIED IDEOGRAPH
    0x8D53, 0x62D8, //#CJK UNIFIED IDEOGRAPH
    0x8D54, 0x63A7, //#CJK UNIFIED IDEOGRAPH
    0x8D55, 0x653B, //#CJK UNIFIED IDEOGRAPH
    0x8D56, 0x6602, //#CJK UNIFIED IDEOGRAPH
    0x8D57, 0x6643, //#CJK UNIFIED IDEOGRAPH
    0x8D58, 0x66F4, //#CJK UNIFIED IDEOGRAPH
    0x8D59, 0x676D, //#CJK UNIFIED IDEOGRAPH
    0x8D5A, 0x6821, //#CJK UNIFIED IDEOGRAPH
    0x8D5B, 0x6897, //#CJK UNIFIED IDEOGRAPH
    0x8D5C, 0x69CB, //#CJK UNIFIED IDEOGRAPH
    0x8D5D, 0x6C5F, //#CJK UNIFIED IDEOGRAPH
    0x8D5E, 0x6D2A, //#CJK UNIFIED IDEOGRAPH
    0x8D5F, 0x6D69, //#CJK UNIFIED IDEOGRAPH
    0x8D60, 0x6E2F, //#CJK UNIFIED IDEOGRAPH
    0x8D61, 0x6E9D, //#CJK UNIFIED IDEOGRAPH
    0x8D62, 0x7532, //#CJK UNIFIED IDEOGRAPH
    0x8D63, 0x7687, //#CJK UNIFIED IDEOGRAPH
    0x8D64, 0x786C, //#CJK UNIFIED IDEOGRAPH
    0x8D65, 0x7A3F, //#CJK UNIFIED IDEOGRAPH
    0x8D66, 0x7CE0, //#CJK UNIFIED IDEOGRAPH
    0x8D67, 0x7D05, //#CJK UNIFIED IDEOGRAPH
    0x8D68, 0x7D18, //#CJK UNIFIED IDEOGRAPH
    0x8D69, 0x7D5E, //#CJK UNIFIED IDEOGRAPH
    0x8D6A, 0x7DB1, //#CJK UNIFIED IDEOGRAPH
    0x8D6B, 0x8015, //#CJK UNIFIED IDEOGRAPH
    0x8D6C, 0x8003, //#CJK UNIFIED IDEOGRAPH
    0x8D6D, 0x80AF, //#CJK UNIFIED IDEOGRAPH
    0x8D6E, 0x80B1, //#CJK UNIFIED IDEOGRAPH
    0x8D6F, 0x8154, //#CJK UNIFIED IDEOGRAPH
    0x8D70, 0x818F, //#CJK UNIFIED IDEOGRAPH
    0x8D71, 0x822A, //#CJK UNIFIED IDEOGRAPH
    0x8D72, 0x8352, //#CJK UNIFIED IDEOGRAPH
    0x8D73, 0x884C, //#CJK UNIFIED IDEOGRAPH
    0x8D74, 0x8861, //#CJK UNIFIED IDEOGRAPH
    0x8D75, 0x8B1B, //#CJK UNIFIED IDEOGRAPH
    0x8D76, 0x8CA2, //#CJK UNIFIED IDEOGRAPH
    0x8D77, 0x8CFC, //#CJK UNIFIED IDEOGRAPH
    0x8D78, 0x90CA, //#CJK UNIFIED IDEOGRAPH
    0x8D79, 0x9175, //#CJK UNIFIED IDEOGRAPH
    0x8D7A, 0x9271, //#CJK UNIFIED IDEOGRAPH
    0x8D7B, 0x783F, //#CJK UNIFIED IDEOGRAPH
    0x8D7C, 0x92FC, //#CJK UNIFIED IDEOGRAPH
    0x8D7D, 0x95A4, //#CJK UNIFIED IDEOGRAPH
    0x8D7E, 0x964D, //#CJK UNIFIED IDEOGRAPH
    0x8D80, 0x9805, //#CJK UNIFIED IDEOGRAPH
    0x8D81, 0x9999, //#CJK UNIFIED IDEOGRAPH
    0x8D82, 0x9AD8, //#CJK UNIFIED IDEOGRAPH
    0x8D83, 0x9D3B, //#CJK UNIFIED IDEOGRAPH
    0x8D84, 0x525B, //#CJK UNIFIED IDEOGRAPH
    0x8D85, 0x52AB, //#CJK UNIFIED IDEOGRAPH
    0x8D86, 0x53F7, //#CJK UNIFIED IDEOGRAPH
    0x8D87, 0x5408, //#CJK UNIFIED IDEOGRAPH
    0x8D88, 0x58D5, //#CJK UNIFIED IDEOGRAPH
    0x8D89, 0x62F7, //#CJK UNIFIED IDEOGRAPH
    0x8D8A, 0x6FE0, //#CJK UNIFIED IDEOGRAPH
    0x8D8B, 0x8C6A, //#CJK UNIFIED IDEOGRAPH
    0x8D8C, 0x8F5F, //#CJK UNIFIED IDEOGRAPH
    0x8D8D, 0x9EB9, //#CJK UNIFIED IDEOGRAPH
    0x8D8E, 0x514B, //#CJK UNIFIED IDEOGRAPH
    0x8D8F, 0x523B, //#CJK UNIFIED IDEOGRAPH
    0x8D90, 0x544A, //#CJK UNIFIED IDEOGRAPH
    0x8D91, 0x56FD, //#CJK UNIFIED IDEOGRAPH
    0x8D92, 0x7A40, //#CJK UNIFIED IDEOGRAPH
    0x8D93, 0x9177, //#CJK UNIFIED IDEOGRAPH
    0x8D94, 0x9D60, //#CJK UNIFIED IDEOGRAPH
    0x8D95, 0x9ED2, //#CJK UNIFIED IDEOGRAPH
    0x8D96, 0x7344, //#CJK UNIFIED IDEOGRAPH
    0x8D97, 0x6F09, //#CJK UNIFIED IDEOGRAPH
    0x8D98, 0x8170, //#CJK UNIFIED IDEOGRAPH
    0x8D99, 0x7511, //#CJK UNIFIED IDEOGRAPH
    0x8D9A, 0x5FFD, //#CJK UNIFIED IDEOGRAPH
    0x8D9B, 0x60DA, //#CJK UNIFIED IDEOGRAPH
    0x8D9C, 0x9AA8, //#CJK UNIFIED IDEOGRAPH
    0x8D9D, 0x72DB, //#CJK UNIFIED IDEOGRAPH
    0x8D9E, 0x8FBC, //#CJK UNIFIED IDEOGRAPH
    0x8D9F, 0x6B64, //#CJK UNIFIED IDEOGRAPH
    0x8DA0, 0x9803, //#CJK UNIFIED IDEOGRAPH
    0x8DA1, 0x4ECA, //#CJK UNIFIED IDEOGRAPH
    0x8DA2, 0x56F0, //#CJK UNIFIED IDEOGRAPH
    0x8DA3, 0x5764, //#CJK UNIFIED IDEOGRAPH
    0x8DA4, 0x58BE, //#CJK UNIFIED IDEOGRAPH
    0x8DA5, 0x5A5A, //#CJK UNIFIED IDEOGRAPH
    0x8DA6, 0x6068, //#CJK UNIFIED IDEOGRAPH
    0x8DA7, 0x61C7, //#CJK UNIFIED IDEOGRAPH
    0x8DA8, 0x660F, //#CJK UNIFIED IDEOGRAPH
    0x8DA9, 0x6606, //#CJK UNIFIED IDEOGRAPH
    0x8DAA, 0x6839, //#CJK UNIFIED IDEOGRAPH
    0x8DAB, 0x68B1, //#CJK UNIFIED IDEOGRAPH
    0x8DAC, 0x6DF7, //#CJK UNIFIED IDEOGRAPH
    0x8DAD, 0x75D5, //#CJK UNIFIED IDEOGRAPH
    0x8DAE, 0x7D3A, //#CJK UNIFIED IDEOGRAPH
    0x8DAF, 0x826E, //#CJK UNIFIED IDEOGRAPH
    0x8DB0, 0x9B42, //#CJK UNIFIED IDEOGRAPH
    0x8DB1, 0x4E9B, //#CJK UNIFIED IDEOGRAPH
    0x8DB2, 0x4F50, //#CJK UNIFIED IDEOGRAPH
    0x8DB3, 0x53C9, //#CJK UNIFIED IDEOGRAPH
    0x8DB4, 0x5506, //#CJK UNIFIED IDEOGRAPH
    0x8DB5, 0x5D6F, //#CJK UNIFIED IDEOGRAPH
    0x8DB6, 0x5DE6, //#CJK UNIFIED IDEOGRAPH
    0x8DB7, 0x5DEE, //#CJK UNIFIED IDEOGRAPH
    0x8DB8, 0x67FB, //#CJK UNIFIED IDEOGRAPH
    0x8DB9, 0x6C99, //#CJK UNIFIED IDEOGRAPH
    0x8DBA, 0x7473, //#CJK UNIFIED IDEOGRAPH
    0x8DBB, 0x7802, //#CJK UNIFIED IDEOGRAPH
    0x8DBC, 0x8A50, //#CJK UNIFIED IDEOGRAPH
    0x8DBD, 0x9396, //#CJK UNIFIED IDEOGRAPH
    0x8DBE, 0x88DF, //#CJK UNIFIED IDEOGRAPH
    0x8DBF, 0x5750, //#CJK UNIFIED IDEOGRAPH
    0x8DC0, 0x5EA7, //#CJK UNIFIED IDEOGRAPH
    0x8DC1, 0x632B, //#CJK UNIFIED IDEOGRAPH
    0x8DC2, 0x50B5, //#CJK UNIFIED IDEOGRAPH
    0x8DC3, 0x50AC, //#CJK UNIFIED IDEOGRAPH
    0x8DC4, 0x518D, //#CJK UNIFIED IDEOGRAPH
    0x8DC5, 0x6700, //#CJK UNIFIED IDEOGRAPH
    0x8DC6, 0x54C9, //#CJK UNIFIED IDEOGRAPH
    0x8DC7, 0x585E, //#CJK UNIFIED IDEOGRAPH
    0x8DC8, 0x59BB, //#CJK UNIFIED IDEOGRAPH
    0x8DC9, 0x5BB0, //#CJK UNIFIED IDEOGRAPH
    0x8DCA, 0x5F69, //#CJK UNIFIED IDEOGRAPH
    0x8DCB, 0x624D, //#CJK UNIFIED IDEOGRAPH
    0x8DCC, 0x63A1, //#CJK UNIFIED IDEOGRAPH
    0x8DCD, 0x683D, //#CJK UNIFIED IDEOGRAPH
    0x8DCE, 0x6B73, //#CJK UNIFIED IDEOGRAPH
    0x8DCF, 0x6E08, //#CJK UNIFIED IDEOGRAPH
    0x8DD0, 0x707D, //#CJK UNIFIED IDEOGRAPH
    0x8DD1, 0x91C7, //#CJK UNIFIED IDEOGRAPH
    0x8DD2, 0x7280, //#CJK UNIFIED IDEOGRAPH
    0x8DD3, 0x7815, //#CJK UNIFIED IDEOGRAPH
    0x8DD4, 0x7826, //#CJK UNIFIED IDEOGRAPH
    0x8DD5, 0x796D, //#CJK UNIFIED IDEOGRAPH
    0x8DD6, 0x658E, //#CJK UNIFIED IDEOGRAPH
    0x8DD7, 0x7D30, //#CJK UNIFIED IDEOGRAPH
    0x8DD8, 0x83DC, //#CJK UNIFIED IDEOGRAPH
    0x8DD9, 0x88C1, //#CJK UNIFIED IDEOGRAPH
    0x8DDA, 0x8F09, //#CJK UNIFIED IDEOGRAPH
    0x8DDB, 0x969B, //#CJK UNIFIED IDEOGRAPH
    0x8DDC, 0x5264, //#CJK UNIFIED IDEOGRAPH
    0x8DDD, 0x5728, //#CJK UNIFIED IDEOGRAPH
    0x8DDE, 0x6750, //#CJK UNIFIED IDEOGRAPH
    0x8DDF, 0x7F6A, //#CJK UNIFIED IDEOGRAPH
    0x8DE0, 0x8CA1, //#CJK UNIFIED IDEOGRAPH
    0x8DE1, 0x51B4, //#CJK UNIFIED IDEOGRAPH
    0x8DE2, 0x5742, //#CJK UNIFIED IDEOGRAPH
    0x8DE3, 0x962A, //#CJK UNIFIED IDEOGRAPH
    0x8DE4, 0x583A, //#CJK UNIFIED IDEOGRAPH
    0x8DE5, 0x698A, //#CJK UNIFIED IDEOGRAPH
    0x8DE6, 0x80B4, //#CJK UNIFIED IDEOGRAPH
    0x8DE7, 0x54B2, //#CJK UNIFIED IDEOGRAPH
    0x8DE8, 0x5D0E, //#CJK UNIFIED IDEOGRAPH
    0x8DE9, 0x57FC, //#CJK UNIFIED IDEOGRAPH
    0x8DEA, 0x7895, //#CJK UNIFIED IDEOGRAPH
    0x8DEB, 0x9DFA, //#CJK UNIFIED IDEOGRAPH
    0x8DEC, 0x4F5C, //#CJK UNIFIED IDEOGRAPH
    0x8DED, 0x524A, //#CJK UNIFIED IDEOGRAPH
    0x8DEE, 0x548B, //#CJK UNIFIED IDEOGRAPH
    0x8DEF, 0x643E, //#CJK UNIFIED IDEOGRAPH
    0x8DF0, 0x6628, //#CJK UNIFIED IDEOGRAPH
    0x8DF1, 0x6714, //#CJK UNIFIED IDEOGRAPH
    0x8DF2, 0x67F5, //#CJK UNIFIED IDEOGRAPH
    0x8DF3, 0x7A84, //#CJK UNIFIED IDEOGRAPH
    0x8DF4, 0x7B56, //#CJK UNIFIED IDEOGRAPH
    0x8DF5, 0x7D22, //#CJK UNIFIED IDEOGRAPH
    0x8DF6, 0x932F, //#CJK UNIFIED IDEOGRAPH
    0x8DF7, 0x685C, //#CJK UNIFIED IDEOGRAPH
    0x8DF8, 0x9BAD, //#CJK UNIFIED IDEOGRAPH
    0x8DF9, 0x7B39, //#CJK UNIFIED IDEOGRAPH
    0x8DFA, 0x5319, //#CJK UNIFIED IDEOGRAPH
    0x8DFB, 0x518A, //#CJK UNIFIED IDEOGRAPH
    0x8DFC, 0x5237, //#CJK UNIFIED IDEOGRAPH
    0x8E40, 0x5BDF, //#CJK UNIFIED IDEOGRAPH
    0x8E41, 0x62F6, //#CJK UNIFIED IDEOGRAPH
    0x8E42, 0x64AE, //#CJK UNIFIED IDEOGRAPH
    0x8E43, 0x64E6, //#CJK UNIFIED IDEOGRAPH
    0x8E44, 0x672D, //#CJK UNIFIED IDEOGRAPH
    0x8E45, 0x6BBA, //#CJK UNIFIED IDEOGRAPH
    0x8E46, 0x85A9, //#CJK UNIFIED IDEOGRAPH
    0x8E47, 0x96D1, //#CJK UNIFIED IDEOGRAPH
    0x8E48, 0x7690, //#CJK UNIFIED IDEOGRAPH
    0x8E49, 0x9BD6, //#CJK UNIFIED IDEOGRAPH
    0x8E4A, 0x634C, //#CJK UNIFIED IDEOGRAPH
    0x8E4B, 0x9306, //#CJK UNIFIED IDEOGRAPH
    0x8E4C, 0x9BAB, //#CJK UNIFIED IDEOGRAPH
    0x8E4D, 0x76BF, //#CJK UNIFIED IDEOGRAPH
    0x8E4E, 0x6652, //#CJK UNIFIED IDEOGRAPH
    0x8E4F, 0x4E09, //#CJK UNIFIED IDEOGRAPH
    0x8E50, 0x5098, //#CJK UNIFIED IDEOGRAPH
    0x8E51, 0x53C2, //#CJK UNIFIED IDEOGRAPH
    0x8E52, 0x5C71, //#CJK UNIFIED IDEOGRAPH
    0x8E53, 0x60E8, //#CJK UNIFIED IDEOGRAPH
    0x8E54, 0x6492, //#CJK UNIFIED IDEOGRAPH
    0x8E55, 0x6563, //#CJK UNIFIED IDEOGRAPH
    0x8E56, 0x685F, //#CJK UNIFIED IDEOGRAPH
    0x8E57, 0x71E6, //#CJK UNIFIED IDEOGRAPH
    0x8E58, 0x73CA, //#CJK UNIFIED IDEOGRAPH
    0x8E59, 0x7523, //#CJK UNIFIED IDEOGRAPH
    0x8E5A, 0x7B97, //#CJK UNIFIED IDEOGRAPH
    0x8E5B, 0x7E82, //#CJK UNIFIED IDEOGRAPH
    0x8E5C, 0x8695, //#CJK UNIFIED IDEOGRAPH
    0x8E5D, 0x8B83, //#CJK UNIFIED IDEOGRAPH
    0x8E5E, 0x8CDB, //#CJK UNIFIED IDEOGRAPH
    0x8E5F, 0x9178, //#CJK UNIFIED IDEOGRAPH
    0x8E60, 0x9910, //#CJK UNIFIED IDEOGRAPH
    0x8E61, 0x65AC, //#CJK UNIFIED IDEOGRAPH
    0x8E62, 0x66AB, //#CJK UNIFIED IDEOGRAPH
    0x8E63, 0x6B8B, //#CJK UNIFIED IDEOGRAPH
    0x8E64, 0x4ED5, //#CJK UNIFIED IDEOGRAPH
    0x8E65, 0x4ED4, //#CJK UNIFIED IDEOGRAPH
    0x8E66, 0x4F3A, //#CJK UNIFIED IDEOGRAPH
    0x8E67, 0x4F7F, //#CJK UNIFIED IDEOGRAPH
    0x8E68, 0x523A, //#CJK UNIFIED IDEOGRAPH
    0x8E69, 0x53F8, //#CJK UNIFIED IDEOGRAPH
    0x8E6A, 0x53F2, //#CJK UNIFIED IDEOGRAPH
    0x8E6B, 0x55E3, //#CJK UNIFIED IDEOGRAPH
    0x8E6C, 0x56DB, //#CJK UNIFIED IDEOGRAPH
    0x8E6D, 0x58EB, //#CJK UNIFIED IDEOGRAPH
    0x8E6E, 0x59CB, //#CJK UNIFIED IDEOGRAPH
    0x8E6F, 0x59C9, //#CJK UNIFIED IDEOGRAPH
    0x8E70, 0x59FF, //#CJK UNIFIED IDEOGRAPH
    0x8E71, 0x5B50, //#CJK UNIFIED IDEOGRAPH
    0x8E72, 0x5C4D, //#CJK UNIFIED IDEOGRAPH
    0x8E73, 0x5E02, //#CJK UNIFIED IDEOGRAPH
    0x8E74, 0x5E2B, //#CJK UNIFIED IDEOGRAPH
    0x8E75, 0x5FD7, //#CJK UNIFIED IDEOGRAPH
    0x8E76, 0x601D, //#CJK UNIFIED IDEOGRAPH
    0x8E77, 0x6307, //#CJK UNIFIED IDEOGRAPH
    0x8E78, 0x652F, //#CJK UNIFIED IDEOGRAPH
    0x8E79, 0x5B5C, //#CJK UNIFIED IDEOGRAPH
    0x8E7A, 0x65AF, //#CJK UNIFIED IDEOGRAPH
    0x8E7B, 0x65BD, //#CJK UNIFIED IDEOGRAPH
    0x8E7C, 0x65E8, //#CJK UNIFIED IDEOGRAPH
    0x8E7D, 0x679D, //#CJK UNIFIED IDEOGRAPH
    0x8E7E, 0x6B62, //#CJK UNIFIED IDEOGRAPH
    0x8E80, 0x6B7B, //#CJK UNIFIED IDEOGRAPH
    0x8E81, 0x6C0F, //#CJK UNIFIED IDEOGRAPH
    0x8E82, 0x7345, //#CJK UNIFIED IDEOGRAPH
    0x8E83, 0x7949, //#CJK UNIFIED IDEOGRAPH
    0x8E84, 0x79C1, //#CJK UNIFIED IDEOGRAPH
    0x8E85, 0x7CF8, //#CJK UNIFIED IDEOGRAPH
    0x8E86, 0x7D19, //#CJK UNIFIED IDEOGRAPH
    0x8E87, 0x7D2B, //#CJK UNIFIED IDEOGRAPH
    0x8E88, 0x80A2, //#CJK UNIFIED IDEOGRAPH
    0x8E89, 0x8102, //#CJK UNIFIED IDEOGRAPH
    0x8E8A, 0x81F3, //#CJK UNIFIED IDEOGRAPH
    0x8E8B, 0x8996, //#CJK UNIFIED IDEOGRAPH
    0x8E8C, 0x8A5E, //#CJK UNIFIED IDEOGRAPH
    0x8E8D, 0x8A69, //#CJK UNIFIED IDEOGRAPH
    0x8E8E, 0x8A66, //#CJK UNIFIED IDEOGRAPH
    0x8E8F, 0x8A8C, //#CJK UNIFIED IDEOGRAPH
    0x8E90, 0x8AEE, //#CJK UNIFIED IDEOGRAPH
    0x8E91, 0x8CC7, //#CJK UNIFIED IDEOGRAPH
    0x8E92, 0x8CDC, //#CJK UNIFIED IDEOGRAPH
    0x8E93, 0x96CC, //#CJK UNIFIED IDEOGRAPH
    0x8E94, 0x98FC, //#CJK UNIFIED IDEOGRAPH
    0x8E95, 0x6B6F, //#CJK UNIFIED IDEOGRAPH
    0x8E96, 0x4E8B, //#CJK UNIFIED IDEOGRAPH
    0x8E97, 0x4F3C, //#CJK UNIFIED IDEOGRAPH
    0x8E98, 0x4F8D, //#CJK UNIFIED IDEOGRAPH
    0x8E99, 0x5150, //#CJK UNIFIED IDEOGRAPH
    0x8E9A, 0x5B57, //#CJK UNIFIED IDEOGRAPH
    0x8E9B, 0x5BFA, //#CJK UNIFIED IDEOGRAPH
    0x8E9C, 0x6148, //#CJK UNIFIED IDEOGRAPH
    0x8E9D, 0x6301, //#CJK UNIFIED IDEOGRAPH
    0x8E9E, 0x6642, //#CJK UNIFIED IDEOGRAPH
    0x8E9F, 0x6B21, //#CJK UNIFIED IDEOGRAPH
    0x8EA0, 0x6ECB, //#CJK UNIFIED IDEOGRAPH
    0x8EA1, 0x6CBB, //#CJK UNIFIED IDEOGRAPH
    0x8EA2, 0x723E, //#CJK UNIFIED IDEOGRAPH
    0x8EA3, 0x74BD, //#CJK UNIFIED IDEOGRAPH
    0x8EA4, 0x75D4, //#CJK UNIFIED IDEOGRAPH
    0x8EA5, 0x78C1, //#CJK UNIFIED IDEOGRAPH
    0x8EA6, 0x793A, //#CJK UNIFIED IDEOGRAPH
    0x8EA7, 0x800C, //#CJK UNIFIED IDEOGRAPH
    0x8EA8, 0x8033, //#CJK UNIFIED IDEOGRAPH
    0x8EA9, 0x81EA, //#CJK UNIFIED IDEOGRAPH
    0x8EAA, 0x8494, //#CJK UNIFIED IDEOGRAPH
    0x8EAB, 0x8F9E, //#CJK UNIFIED IDEOGRAPH
    0x8EAC, 0x6C50, //#CJK UNIFIED IDEOGRAPH
    0x8EAD, 0x9E7F, //#CJK UNIFIED IDEOGRAPH
    0x8EAE, 0x5F0F, //#CJK UNIFIED IDEOGRAPH
    0x8EAF, 0x8B58, //#CJK UNIFIED IDEOGRAPH
    0x8EB0, 0x9D2B, //#CJK UNIFIED IDEOGRAPH
    0x8EB1, 0x7AFA, //#CJK UNIFIED IDEOGRAPH
    0x8EB2, 0x8EF8, //#CJK UNIFIED IDEOGRAPH
    0x8EB3, 0x5B8D, //#CJK UNIFIED IDEOGRAPH
    0x8EB4, 0x96EB, //#CJK UNIFIED IDEOGRAPH
    0x8EB5, 0x4E03, //#CJK UNIFIED IDEOGRAPH
    0x8EB6, 0x53F1, //#CJK UNIFIED IDEOGRAPH
    0x8EB7, 0x57F7, //#CJK UNIFIED IDEOGRAPH
    0x8EB8, 0x5931, //#CJK UNIFIED IDEOGRAPH
    0x8EB9, 0x5AC9, //#CJK UNIFIED IDEOGRAPH
    0x8EBA, 0x5BA4, //#CJK UNIFIED IDEOGRAPH
    0x8EBB, 0x6089, //#CJK UNIFIED IDEOGRAPH
    0x8EBC, 0x6E7F, //#CJK UNIFIED IDEOGRAPH
    0x8EBD, 0x6F06, //#CJK UNIFIED IDEOGRAPH
    0x8EBE, 0x75BE, //#CJK UNIFIED IDEOGRAPH
    0x8EBF, 0x8CEA, //#CJK UNIFIED IDEOGRAPH
    0x8EC0, 0x5B9F, //#CJK UNIFIED IDEOGRAPH
    0x8EC1, 0x8500, //#CJK UNIFIED IDEOGRAPH
    0x8EC2, 0x7BE0, //#CJK UNIFIED IDEOGRAPH
    0x8EC3, 0x5072, //#CJK UNIFIED IDEOGRAPH
    0x8EC4, 0x67F4, //#CJK UNIFIED IDEOGRAPH
    0x8EC5, 0x829D, //#CJK UNIFIED IDEOGRAPH
    0x8EC6, 0x5C61, //#CJK UNIFIED IDEOGRAPH
    0x8EC7, 0x854A, //#CJK UNIFIED IDEOGRAPH
    0x8EC8, 0x7E1E, //#CJK UNIFIED IDEOGRAPH
    0x8EC9, 0x820E, //#CJK UNIFIED IDEOGRAPH
    0x8ECA, 0x5199, //#CJK UNIFIED IDEOGRAPH
    0x8ECB, 0x5C04, //#CJK UNIFIED IDEOGRAPH
    0x8ECC, 0x6368, //#CJK UNIFIED IDEOGRAPH
    0x8ECD, 0x8D66, //#CJK UNIFIED IDEOGRAPH
    0x8ECE, 0x659C, //#CJK UNIFIED IDEOGRAPH
    0x8ECF, 0x716E, //#CJK UNIFIED IDEOGRAPH
    0x8ED0, 0x793E, //#CJK UNIFIED IDEOGRAPH
    0x8ED1, 0x7D17, //#CJK UNIFIED IDEOGRAPH
    0x8ED2, 0x8005, //#CJK UNIFIED IDEOGRAPH
    0x8ED3, 0x8B1D, //#CJK UNIFIED IDEOGRAPH
    0x8ED4, 0x8ECA, //#CJK UNIFIED IDEOGRAPH
    0x8ED5, 0x906E, //#CJK UNIFIED IDEOGRAPH
    0x8ED6, 0x86C7, //#CJK UNIFIED IDEOGRAPH
    0x8ED7, 0x90AA, //#CJK UNIFIED IDEOGRAPH
    0x8ED8, 0x501F, //#CJK UNIFIED IDEOGRAPH
    0x8ED9, 0x52FA, //#CJK UNIFIED IDEOGRAPH
    0x8EDA, 0x5C3A, //#CJK UNIFIED IDEOGRAPH
    0x8EDB, 0x6753, //#CJK UNIFIED IDEOGRAPH
    0x8EDC, 0x707C, //#CJK UNIFIED IDEOGRAPH
    0x8EDD, 0x7235, //#CJK UNIFIED IDEOGRAPH
    0x8EDE, 0x914C, //#CJK UNIFIED IDEOGRAPH
    0x8EDF, 0x91C8, //#CJK UNIFIED IDEOGRAPH
    0x8EE0, 0x932B, //#CJK UNIFIED IDEOGRAPH
    0x8EE1, 0x82E5, //#CJK UNIFIED IDEOGRAPH
    0x8EE2, 0x5BC2, //#CJK UNIFIED IDEOGRAPH
    0x8EE3, 0x5F31, //#CJK UNIFIED IDEOGRAPH
    0x8EE4, 0x60F9, //#CJK UNIFIED IDEOGRAPH
    0x8EE5, 0x4E3B, //#CJK UNIFIED IDEOGRAPH
    0x8EE6, 0x53D6, //#CJK UNIFIED IDEOGRAPH
    0x8EE7, 0x5B88, //#CJK UNIFIED IDEOGRAPH
    0x8EE8, 0x624B, //#CJK UNIFIED IDEOGRAPH
    0x8EE9, 0x6731, //#CJK UNIFIED IDEOGRAPH
    0x8EEA, 0x6B8A, //#CJK UNIFIED IDEOGRAPH
    0x8EEB, 0x72E9, //#CJK UNIFIED IDEOGRAPH
    0x8EEC, 0x73E0, //#CJK UNIFIED IDEOGRAPH
    0x8EED, 0x7A2E, //#CJK UNIFIED IDEOGRAPH
    0x8EEE, 0x816B, //#CJK UNIFIED IDEOGRAPH
    0x8EEF, 0x8DA3, //#CJK UNIFIED IDEOGRAPH
    0x8EF0, 0x9152, //#CJK UNIFIED IDEOGRAPH
    0x8EF1, 0x9996, //#CJK UNIFIED IDEOGRAPH
    0x8EF2, 0x5112, //#CJK UNIFIED IDEOGRAPH
    0x8EF3, 0x53D7, //#CJK UNIFIED IDEOGRAPH
    0x8EF4, 0x546A, //#CJK UNIFIED IDEOGRAPH
    0x8EF5, 0x5BFF, //#CJK UNIFIED IDEOGRAPH
    0x8EF6, 0x6388, //#CJK UNIFIED IDEOGRAPH
    0x8EF7, 0x6A39, //#CJK UNIFIED IDEOGRAPH
    0x8EF8, 0x7DAC, //#CJK UNIFIED IDEOGRAPH
    0x8EF9, 0x9700, //#CJK UNIFIED IDEOGRAPH
    0x8EFA, 0x56DA, //#CJK UNIFIED IDEOGRAPH
    0x8EFB, 0x53CE, //#CJK UNIFIED IDEOGRAPH
    0x8EFC, 0x5468, //#CJK UNIFIED IDEOGRAPH
    0x8F40, 0x5B97, //#CJK UNIFIED IDEOGRAPH
    0x8F41, 0x5C31, //#CJK UNIFIED IDEOGRAPH
    0x8F42, 0x5DDE, //#CJK UNIFIED IDEOGRAPH
    0x8F43, 0x4FEE, //#CJK UNIFIED IDEOGRAPH
    0x8F44, 0x6101, //#CJK UNIFIED IDEOGRAPH
    0x8F45, 0x62FE, //#CJK UNIFIED IDEOGRAPH
    0x8F46, 0x6D32, //#CJK UNIFIED IDEOGRAPH
    0x8F47, 0x79C0, //#CJK UNIFIED IDEOGRAPH
    0x8F48, 0x79CB, //#CJK UNIFIED IDEOGRAPH
    0x8F49, 0x7D42, //#CJK UNIFIED IDEOGRAPH
    0x8F4A, 0x7E4D, //#CJK UNIFIED IDEOGRAPH
    0x8F4B, 0x7FD2, //#CJK UNIFIED IDEOGRAPH
    0x8F4C, 0x81ED, //#CJK UNIFIED IDEOGRAPH
    0x8F4D, 0x821F, //#CJK UNIFIED IDEOGRAPH
    0x8F4E, 0x8490, //#CJK UNIFIED IDEOGRAPH
    0x8F4F, 0x8846, //#CJK UNIFIED IDEOGRAPH
    0x8F50, 0x8972, //#CJK UNIFIED IDEOGRAPH
    0x8F51, 0x8B90, //#CJK UNIFIED IDEOGRAPH
    0x8F52, 0x8E74, //#CJK UNIFIED IDEOGRAPH
    0x8F53, 0x8F2F, //#CJK UNIFIED IDEOGRAPH
    0x8F54, 0x9031, //#CJK UNIFIED IDEOGRAPH
    0x8F55, 0x914B, //#CJK UNIFIED IDEOGRAPH
    0x8F56, 0x916C, //#CJK UNIFIED IDEOGRAPH
    0x8F57, 0x96C6, //#CJK UNIFIED IDEOGRAPH
    0x8F58, 0x919C, //#CJK UNIFIED IDEOGRAPH
    0x8F59, 0x4EC0, //#CJK UNIFIED IDEOGRAPH
    0x8F5A, 0x4F4F, //#CJK UNIFIED IDEOGRAPH
    0x8F5B, 0x5145, //#CJK UNIFIED IDEOGRAPH
    0x8F5C, 0x5341, //#CJK UNIFIED IDEOGRAPH
    0x8F5D, 0x5F93, //#CJK UNIFIED IDEOGRAPH
    0x8F5E, 0x620E, //#CJK UNIFIED IDEOGRAPH
    0x8F5F, 0x67D4, //#CJK UNIFIED IDEOGRAPH
    0x8F60, 0x6C41, //#CJK UNIFIED IDEOGRAPH
    0x8F61, 0x6E0B, //#CJK UNIFIED IDEOGRAPH
    0x8F62, 0x7363, //#CJK UNIFIED IDEOGRAPH
    0x8F63, 0x7E26, //#CJK UNIFIED IDEOGRAPH
    0x8F64, 0x91CD, //#CJK UNIFIED IDEOGRAPH
    0x8F65, 0x9283, //#CJK UNIFIED IDEOGRAPH
    0x8F66, 0x53D4, //#CJK UNIFIED IDEOGRAPH
    0x8F67, 0x5919, //#CJK UNIFIED IDEOGRAPH
    0x8F68, 0x5BBF, //#CJK UNIFIED IDEOGRAPH
    0x8F69, 0x6DD1, //#CJK UNIFIED IDEOGRAPH
    0x8F6A, 0x795D, //#CJK UNIFIED IDEOGRAPH
    0x8F6B, 0x7E2E, //#CJK UNIFIED IDEOGRAPH
    0x8F6C, 0x7C9B, //#CJK UNIFIED IDEOGRAPH
    0x8F6D, 0x587E, //#CJK UNIFIED IDEOGRAPH
    0x8F6E, 0x719F, //#CJK UNIFIED IDEOGRAPH
    0x8F6F, 0x51FA, //#CJK UNIFIED IDEOGRAPH
    0x8F70, 0x8853, //#CJK UNIFIED IDEOGRAPH
    0x8F71, 0x8FF0, //#CJK UNIFIED IDEOGRAPH
    0x8F72, 0x4FCA, //#CJK UNIFIED IDEOGRAPH
    0x8F73, 0x5CFB, //#CJK UNIFIED IDEOGRAPH
    0x8F74, 0x6625, //#CJK UNIFIED IDEOGRAPH
    0x8F75, 0x77AC, //#CJK UNIFIED IDEOGRAPH
    0x8F76, 0x7AE3, //#CJK UNIFIED IDEOGRAPH
    0x8F77, 0x821C, //#CJK UNIFIED IDEOGRAPH
    0x8F78, 0x99FF, //#CJK UNIFIED IDEOGRAPH
    0x8F79, 0x51C6, //#CJK UNIFIED IDEOGRAPH
    0x8F7A, 0x5FAA, //#CJK UNIFIED IDEOGRAPH
    0x8F7B, 0x65EC, //#CJK UNIFIED IDEOGRAPH
    0x8F7C, 0x696F, //#CJK UNIFIED IDEOGRAPH
    0x8F7D, 0x6B89, //#CJK UNIFIED IDEOGRAPH
    0x8F7E, 0x6DF3, //#CJK UNIFIED IDEOGRAPH
    0x8F80, 0x6E96, //#CJK UNIFIED IDEOGRAPH
    0x8F81, 0x6F64, //#CJK UNIFIED IDEOGRAPH
    0x8F82, 0x76FE, //#CJK UNIFIED IDEOGRAPH
    0x8F83, 0x7D14, //#CJK UNIFIED IDEOGRAPH
    0x8F84, 0x5DE1, //#CJK UNIFIED IDEOGRAPH
    0x8F85, 0x9075, //#CJK UNIFIED IDEOGRAPH
    0x8F86, 0x9187, //#CJK UNIFIED IDEOGRAPH
    0x8F87, 0x9806, //#CJK UNIFIED IDEOGRAPH
    0x8F88, 0x51E6, //#CJK UNIFIED IDEOGRAPH
    0x8F89, 0x521D, //#CJK UNIFIED IDEOGRAPH
    0x8F8A, 0x6240, //#CJK UNIFIED IDEOGRAPH
    0x8F8B, 0x6691, //#CJK UNIFIED IDEOGRAPH
    0x8F8C, 0x66D9, //#CJK UNIFIED IDEOGRAPH
    0x8F8D, 0x6E1A, //#CJK UNIFIED IDEOGRAPH
    0x8F8E, 0x5EB6, //#CJK UNIFIED IDEOGRAPH
    0x8F8F, 0x7DD2, //#CJK UNIFIED IDEOGRAPH
    0x8F90, 0x7F72, //#CJK UNIFIED IDEOGRAPH
    0x8F91, 0x66F8, //#CJK UNIFIED IDEOGRAPH
    0x8F92, 0x85AF, //#CJK UNIFIED IDEOGRAPH
    0x8F93, 0x85F7, //#CJK UNIFIED IDEOGRAPH
    0x8F94, 0x8AF8, //#CJK UNIFIED IDEOGRAPH
    0x8F95, 0x52A9, //#CJK UNIFIED IDEOGRAPH
    0x8F96, 0x53D9, //#CJK UNIFIED IDEOGRAPH
    0x8F97, 0x5973, //#CJK UNIFIED IDEOGRAPH
    0x8F98, 0x5E8F, //#CJK UNIFIED IDEOGRAPH
    0x8F99, 0x5F90, //#CJK UNIFIED IDEOGRAPH
    0x8F9A, 0x6055, //#CJK UNIFIED IDEOGRAPH
    0x8F9B, 0x92E4, //#CJK UNIFIED IDEOGRAPH
    0x8F9C, 0x9664, //#CJK UNIFIED IDEOGRAPH
    0x8F9D, 0x50B7, //#CJK UNIFIED IDEOGRAPH
    0x8F9E, 0x511F, //#CJK UNIFIED IDEOGRAPH
    0x8F9F, 0x52DD, //#CJK UNIFIED IDEOGRAPH
    0x8FA0, 0x5320, //#CJK UNIFIED IDEOGRAPH
    0x8FA1, 0x5347, //#CJK UNIFIED IDEOGRAPH
    0x8FA2, 0x53EC, //#CJK UNIFIED IDEOGRAPH
    0x8FA3, 0x54E8, //#CJK UNIFIED IDEOGRAPH
    0x8FA4, 0x5546, //#CJK UNIFIED IDEOGRAPH
    0x8FA5, 0x5531, //#CJK UNIFIED IDEOGRAPH
    0x8FA6, 0x5617, //#CJK UNIFIED IDEOGRAPH
    0x8FA7, 0x5968, //#CJK UNIFIED IDEOGRAPH
    0x8FA8, 0x59BE, //#CJK UNIFIED IDEOGRAPH
    0x8FA9, 0x5A3C, //#CJK UNIFIED IDEOGRAPH
    0x8FAA, 0x5BB5, //#CJK UNIFIED IDEOGRAPH
    0x8FAB, 0x5C06, //#CJK UNIFIED IDEOGRAPH
    0x8FAC, 0x5C0F, //#CJK UNIFIED IDEOGRAPH
    0x8FAD, 0x5C11, //#CJK UNIFIED IDEOGRAPH
    0x8FAE, 0x5C1A, //#CJK UNIFIED IDEOGRAPH
    0x8FAF, 0x5E84, //#CJK UNIFIED IDEOGRAPH
    0x8FB0, 0x5E8A, //#CJK UNIFIED IDEOGRAPH
    0x8FB1, 0x5EE0, //#CJK UNIFIED IDEOGRAPH
    0x8FB2, 0x5F70, //#CJK UNIFIED IDEOGRAPH
    0x8FB3, 0x627F, //#CJK UNIFIED IDEOGRAPH
    0x8FB4, 0x6284, //#CJK UNIFIED IDEOGRAPH
    0x8FB5, 0x62DB, //#CJK UNIFIED IDEOGRAPH
    0x8FB6, 0x638C, //#CJK UNIFIED IDEOGRAPH
    0x8FB7, 0x6377, //#CJK UNIFIED IDEOGRAPH
    0x8FB8, 0x6607, //#CJK UNIFIED IDEOGRAPH
    0x8FB9, 0x660C, //#CJK UNIFIED IDEOGRAPH
    0x8FBA, 0x662D, //#CJK UNIFIED IDEOGRAPH
    0x8FBB, 0x6676, //#CJK UNIFIED IDEOGRAPH
    0x8FBC, 0x677E, //#CJK UNIFIED IDEOGRAPH
    0x8FBD, 0x68A2, //#CJK UNIFIED IDEOGRAPH
    0x8FBE, 0x6A1F, //#CJK UNIFIED IDEOGRAPH
    0x8FBF, 0x6A35, //#CJK UNIFIED IDEOGRAPH
    0x8FC0, 0x6CBC, //#CJK UNIFIED IDEOGRAPH
    0x8FC1, 0x6D88, //#CJK UNIFIED IDEOGRAPH
    0x8FC2, 0x6E09, //#CJK UNIFIED IDEOGRAPH
    0x8FC3, 0x6E58, //#CJK UNIFIED IDEOGRAPH
    0x8FC4, 0x713C, //#CJK UNIFIED IDEOGRAPH
    0x8FC5, 0x7126, //#CJK UNIFIED IDEOGRAPH
    0x8FC6, 0x7167, //#CJK UNIFIED IDEOGRAPH
    0x8FC7, 0x75C7, //#CJK UNIFIED IDEOGRAPH
    0x8FC8, 0x7701, //#CJK UNIFIED IDEOGRAPH
    0x8FC9, 0x785D, //#CJK UNIFIED IDEOGRAPH
    0x8FCA, 0x7901, //#CJK UNIFIED IDEOGRAPH
    0x8FCB, 0x7965, //#CJK UNIFIED IDEOGRAPH
    0x8FCC, 0x79F0, //#CJK UNIFIED IDEOGRAPH
    0x8FCD, 0x7AE0, //#CJK UNIFIED IDEOGRAPH
    0x8FCE, 0x7B11, //#CJK UNIFIED IDEOGRAPH
    0x8FCF, 0x7CA7, //#CJK UNIFIED IDEOGRAPH
    0x8FD0, 0x7D39, //#CJK UNIFIED IDEOGRAPH
    0x8FD1, 0x8096, //#CJK UNIFIED IDEOGRAPH
    0x8FD2, 0x83D6, //#CJK UNIFIED IDEOGRAPH
    0x8FD3, 0x848B, //#CJK UNIFIED IDEOGRAPH
    0x8FD4, 0x8549, //#CJK UNIFIED IDEOGRAPH
    0x8FD5, 0x885D, //#CJK UNIFIED IDEOGRAPH
    0x8FD6, 0x88F3, //#CJK UNIFIED IDEOGRAPH
    0x8FD7, 0x8A1F, //#CJK UNIFIED IDEOGRAPH
    0x8FD8, 0x8A3C, //#CJK UNIFIED IDEOGRAPH
    0x8FD9, 0x8A54, //#CJK UNIFIED IDEOGRAPH
    0x8FDA, 0x8A73, //#CJK UNIFIED IDEOGRAPH
    0x8FDB, 0x8C61, //#CJK UNIFIED IDEOGRAPH
    0x8FDC, 0x8CDE, //#CJK UNIFIED IDEOGRAPH
    0x8FDD, 0x91A4, //#CJK UNIFIED IDEOGRAPH
    0x8FDE, 0x9266, //#CJK UNIFIED IDEOGRAPH
    0x8FDF, 0x937E, //#CJK UNIFIED IDEOGRAPH
    0x8FE0, 0x9418, //#CJK UNIFIED IDEOGRAPH
    0x8FE1, 0x969C, //#CJK UNIFIED IDEOGRAPH
    0x8FE2, 0x9798, //#CJK UNIFIED IDEOGRAPH
    0x8FE3, 0x4E0A, //#CJK UNIFIED IDEOGRAPH
    0x8FE4, 0x4E08, //#CJK UNIFIED IDEOGRAPH
    0x8FE5, 0x4E1E, //#CJK UNIFIED IDEOGRAPH
    0x8FE6, 0x4E57, //#CJK UNIFIED IDEOGRAPH
    0x8FE7, 0x5197, //#CJK UNIFIED IDEOGRAPH
    0x8FE8, 0x5270, //#CJK UNIFIED IDEOGRAPH
    0x8FE9, 0x57CE, //#CJK UNIFIED IDEOGRAPH
    0x8FEA, 0x5834, //#CJK UNIFIED IDEOGRAPH
    0x8FEB, 0x58CC, //#CJK UNIFIED IDEOGRAPH
    0x8FEC, 0x5B22, //#CJK UNIFIED IDEOGRAPH
    0x8FED, 0x5E38, //#CJK UNIFIED IDEOGRAPH
    0x8FEE, 0x60C5, //#CJK UNIFIED IDEOGRAPH
    0x8FEF, 0x64FE, //#CJK UNIFIED IDEOGRAPH
    0x8FF0, 0x6761, //#CJK UNIFIED IDEOGRAPH
    0x8FF1, 0x6756, //#CJK UNIFIED IDEOGRAPH
    0x8FF2, 0x6D44, //#CJK UNIFIED IDEOGRAPH
    0x8FF3, 0x72B6, //#CJK UNIFIED IDEOGRAPH
    0x8FF4, 0x7573, //#CJK UNIFIED IDEOGRAPH
    0x8FF5, 0x7A63, //#CJK UNIFIED IDEOGRAPH
    0x8FF6, 0x84B8, //#CJK UNIFIED IDEOGRAPH
    0x8FF7, 0x8B72, //#CJK UNIFIED IDEOGRAPH
    0x8FF8, 0x91B8, //#CJK UNIFIED IDEOGRAPH
    0x8FF9, 0x9320, //#CJK UNIFIED IDEOGRAPH
    0x8FFA, 0x5631, //#CJK UNIFIED IDEOGRAPH
    0x8FFB, 0x57F4, //#CJK UNIFIED IDEOGRAPH
    0x8FFC, 0x98FE, //#CJK UNIFIED IDEOGRAPH
    0x9040, 0x62ED, //#CJK UNIFIED IDEOGRAPH
    0x9041, 0x690D, //#CJK UNIFIED IDEOGRAPH
    0x9042, 0x6B96, //#CJK UNIFIED IDEOGRAPH
    0x9043, 0x71ED, //#CJK UNIFIED IDEOGRAPH
    0x9044, 0x7E54, //#CJK UNIFIED IDEOGRAPH
    0x9045, 0x8077, //#CJK UNIFIED IDEOGRAPH
    0x9046, 0x8272, //#CJK UNIFIED IDEOGRAPH
    0x9047, 0x89E6, //#CJK UNIFIED IDEOGRAPH
    0x9048, 0x98DF, //#CJK UNIFIED IDEOGRAPH
    0x9049, 0x8755, //#CJK UNIFIED IDEOGRAPH
    0x904A, 0x8FB1, //#CJK UNIFIED IDEOGRAPH
    0x904B, 0x5C3B, //#CJK UNIFIED IDEOGRAPH
    0x904C, 0x4F38, //#CJK UNIFIED IDEOGRAPH
    0x904D, 0x4FE1, //#CJK UNIFIED IDEOGRAPH
    0x904E, 0x4FB5, //#CJK UNIFIED IDEOGRAPH
    0x904F, 0x5507, //#CJK UNIFIED IDEOGRAPH
    0x9050, 0x5A20, //#CJK UNIFIED IDEOGRAPH
    0x9051, 0x5BDD, //#CJK UNIFIED IDEOGRAPH
    0x9052, 0x5BE9, //#CJK UNIFIED IDEOGRAPH
    0x9053, 0x5FC3, //#CJK UNIFIED IDEOGRAPH
    0x9054, 0x614E, //#CJK UNIFIED IDEOGRAPH
    0x9055, 0x632F, //#CJK UNIFIED IDEOGRAPH
    0x9056, 0x65B0, //#CJK UNIFIED IDEOGRAPH
    0x9057, 0x664B, //#CJK UNIFIED IDEOGRAPH
    0x9058, 0x68EE, //#CJK UNIFIED IDEOGRAPH
    0x9059, 0x699B, //#CJK UNIFIED IDEOGRAPH
    0x905A, 0x6D78, //#CJK UNIFIED IDEOGRAPH
    0x905B, 0x6DF1, //#CJK UNIFIED IDEOGRAPH
    0x905C, 0x7533, //#CJK UNIFIED IDEOGRAPH
    0x905D, 0x75B9, //#CJK UNIFIED IDEOGRAPH
    0x905E, 0x771F, //#CJK UNIFIED IDEOGRAPH
    0x905F, 0x795E, //#CJK UNIFIED IDEOGRAPH
    0x9060, 0x79E6, //#CJK UNIFIED IDEOGRAPH
    0x9061, 0x7D33, //#CJK UNIFIED IDEOGRAPH
    0x9062, 0x81E3, //#CJK UNIFIED IDEOGRAPH
    0x9063, 0x82AF, //#CJK UNIFIED IDEOGRAPH
    0x9064, 0x85AA, //#CJK UNIFIED IDEOGRAPH
    0x9065, 0x89AA, //#CJK UNIFIED IDEOGRAPH
    0x9066, 0x8A3A, //#CJK UNIFIED IDEOGRAPH
    0x9067, 0x8EAB, //#CJK UNIFIED IDEOGRAPH
    0x9068, 0x8F9B, //#CJK UNIFIED IDEOGRAPH
    0x9069, 0x9032, //#CJK UNIFIED IDEOGRAPH
    0x906A, 0x91DD, //#CJK UNIFIED IDEOGRAPH
    0x906B, 0x9707, //#CJK UNIFIED IDEOGRAPH
    0x906C, 0x4EBA, //#CJK UNIFIED IDEOGRAPH
    0x906D, 0x4EC1, //#CJK UNIFIED IDEOGRAPH
    0x906E, 0x5203, //#CJK UNIFIED IDEOGRAPH
    0x906F, 0x5875, //#CJK UNIFIED IDEOGRAPH
    0x9070, 0x58EC, //#CJK UNIFIED IDEOGRAPH
    0x9071, 0x5C0B, //#CJK UNIFIED IDEOGRAPH
    0x9072, 0x751A, //#CJK UNIFIED IDEOGRAPH
    0x9073, 0x5C3D, //#CJK UNIFIED IDEOGRAPH
    0x9074, 0x814E, //#CJK UNIFIED IDEOGRAPH
    0x9075, 0x8A0A, //#CJK UNIFIED IDEOGRAPH
    0x9076, 0x8FC5, //#CJK UNIFIED IDEOGRAPH
    0x9077, 0x9663, //#CJK UNIFIED IDEOGRAPH
    0x9078, 0x976D, //#CJK UNIFIED IDEOGRAPH
    0x9079, 0x7B25, //#CJK UNIFIED IDEOGRAPH
    0x907A, 0x8ACF, //#CJK UNIFIED IDEOGRAPH
    0x907B, 0x9808, //#CJK UNIFIED IDEOGRAPH
    0x907C, 0x9162, //#CJK UNIFIED IDEOGRAPH
    0x907D, 0x56F3, //#CJK UNIFIED IDEOGRAPH
    0x907E, 0x53A8, //#CJK UNIFIED IDEOGRAPH
    0x9080, 0x9017, //#CJK UNIFIED IDEOGRAPH
    0x9081, 0x5439, //#CJK UNIFIED IDEOGRAPH
    0x9082, 0x5782, //#CJK UNIFIED IDEOGRAPH
    0x9083, 0x5E25, //#CJK UNIFIED IDEOGRAPH
    0x9084, 0x63A8, //#CJK UNIFIED IDEOGRAPH
    0x9085, 0x6C34, //#CJK UNIFIED IDEOGRAPH
    0x9086, 0x708A, //#CJK UNIFIED IDEOGRAPH
    0x9087, 0x7761, //#CJK UNIFIED IDEOGRAPH
    0x9088, 0x7C8B, //#CJK UNIFIED IDEOGRAPH
    0x9089, 0x7FE0, //#CJK UNIFIED IDEOGRAPH
    0x908A, 0x8870, //#CJK UNIFIED IDEOGRAPH
    0x908B, 0x9042, //#CJK UNIFIED IDEOGRAPH
    0x908C, 0x9154, //#CJK UNIFIED IDEOGRAPH
    0x908D, 0x9310, //#CJK UNIFIED IDEOGRAPH
    0x908E, 0x9318, //#CJK UNIFIED IDEOGRAPH
    0x908F, 0x968F, //#CJK UNIFIED IDEOGRAPH
    0x9090, 0x745E, //#CJK UNIFIED IDEOGRAPH
    0x9091, 0x9AC4, //#CJK UNIFIED IDEOGRAPH
    0x9092, 0x5D07, //#CJK UNIFIED IDEOGRAPH
    0x9093, 0x5D69, //#CJK UNIFIED IDEOGRAPH
    0x9094, 0x6570, //#CJK UNIFIED IDEOGRAPH
    0x9095, 0x67A2, //#CJK UNIFIED IDEOGRAPH
    0x9096, 0x8DA8, //#CJK UNIFIED IDEOGRAPH
    0x9097, 0x96DB, //#CJK UNIFIED IDEOGRAPH
    0x9098, 0x636E, //#CJK UNIFIED IDEOGRAPH
    0x9099, 0x6749, //#CJK UNIFIED IDEOGRAPH
    0x909A, 0x6919, //#CJK UNIFIED IDEOGRAPH
    0x909B, 0x83C5, //#CJK UNIFIED IDEOGRAPH
    0x909C, 0x9817, //#CJK UNIFIED IDEOGRAPH
    0x909D, 0x96C0, //#CJK UNIFIED IDEOGRAPH
    0x909E, 0x88FE, //#CJK UNIFIED IDEOGRAPH
    0x909F, 0x6F84, //#CJK UNIFIED IDEOGRAPH
    0x90A0, 0x647A, //#CJK UNIFIED IDEOGRAPH
    0x90A1, 0x5BF8, //#CJK UNIFIED IDEOGRAPH
    0x90A2, 0x4E16, //#CJK UNIFIED IDEOGRAPH
    0x90A3, 0x702C, //#CJK UNIFIED IDEOGRAPH
    0x90A4, 0x755D, //#CJK UNIFIED IDEOGRAPH
    0x90A5, 0x662F, //#CJK UNIFIED IDEOGRAPH
    0x90A6, 0x51C4, //#CJK UNIFIED IDEOGRAPH
    0x90A7, 0x5236, //#CJK UNIFIED IDEOGRAPH
    0x90A8, 0x52E2, //#CJK UNIFIED IDEOGRAPH
    0x90A9, 0x59D3, //#CJK UNIFIED IDEOGRAPH
    0x90AA, 0x5F81, //#CJK UNIFIED IDEOGRAPH
    0x90AB, 0x6027, //#CJK UNIFIED IDEOGRAPH
    0x90AC, 0x6210, //#CJK UNIFIED IDEOGRAPH
    0x90AD, 0x653F, //#CJK UNIFIED IDEOGRAPH
    0x90AE, 0x6574, //#CJK UNIFIED IDEOGRAPH
    0x90AF, 0x661F, //#CJK UNIFIED IDEOGRAPH
    0x90B0, 0x6674, //#CJK UNIFIED IDEOGRAPH
    0x90B1, 0x68F2, //#CJK UNIFIED IDEOGRAPH
    0x90B2, 0x6816, //#CJK UNIFIED IDEOGRAPH
    0x90B3, 0x6B63, //#CJK UNIFIED IDEOGRAPH
    0x90B4, 0x6E05, //#CJK UNIFIED IDEOGRAPH
    0x90B5, 0x7272, //#CJK UNIFIED IDEOGRAPH
    0x90B6, 0x751F, //#CJK UNIFIED IDEOGRAPH
    0x90B7, 0x76DB, //#CJK UNIFIED IDEOGRAPH
    0x90B8, 0x7CBE, //#CJK UNIFIED IDEOGRAPH
    0x90B9, 0x8056, //#CJK UNIFIED IDEOGRAPH
    0x90BA, 0x58F0, //#CJK UNIFIED IDEOGRAPH
    0x90BB, 0x88FD, //#CJK UNIFIED IDEOGRAPH
    0x90BC, 0x897F, //#CJK UNIFIED IDEOGRAPH
    0x90BD, 0x8AA0, //#CJK UNIFIED IDEOGRAPH
    0x90BE, 0x8A93, //#CJK UNIFIED IDEOGRAPH
    0x90BF, 0x8ACB, //#CJK UNIFIED IDEOGRAPH
    0x90C0, 0x901D, //#CJK UNIFIED IDEOGRAPH
    0x90C1, 0x9192, //#CJK UNIFIED IDEOGRAPH
    0x90C2, 0x9752, //#CJK UNIFIED IDEOGRAPH
    0x90C3, 0x9759, //#CJK UNIFIED IDEOGRAPH
    0x90C4, 0x6589, //#CJK UNIFIED IDEOGRAPH
    0x90C5, 0x7A0E, //#CJK UNIFIED IDEOGRAPH
    0x90C6, 0x8106, //#CJK UNIFIED IDEOGRAPH
    0x90C7, 0x96BB, //#CJK UNIFIED IDEOGRAPH
    0x90C8, 0x5E2D, //#CJK UNIFIED IDEOGRAPH
    0x90C9, 0x60DC, //#CJK UNIFIED IDEOGRAPH
    0x90CA, 0x621A, //#CJK UNIFIED IDEOGRAPH
    0x90CB, 0x65A5, //#CJK UNIFIED IDEOGRAPH
    0x90CC, 0x6614, //#CJK UNIFIED IDEOGRAPH
    0x90CD, 0x6790, //#CJK UNIFIED IDEOGRAPH
    0x90CE, 0x77F3, //#CJK UNIFIED IDEOGRAPH
    0x90CF, 0x7A4D, //#CJK UNIFIED IDEOGRAPH
    0x90D0, 0x7C4D, //#CJK UNIFIED IDEOGRAPH
    0x90D1, 0x7E3E, //#CJK UNIFIED IDEOGRAPH
    0x90D2, 0x810A, //#CJK UNIFIED IDEOGRAPH
    0x90D3, 0x8CAC, //#CJK UNIFIED IDEOGRAPH
    0x90D4, 0x8D64, //#CJK UNIFIED IDEOGRAPH
    0x90D5, 0x8DE1, //#CJK UNIFIED IDEOGRAPH
    0x90D6, 0x8E5F, //#CJK UNIFIED IDEOGRAPH
    0x90D7, 0x78A9, //#CJK UNIFIED IDEOGRAPH
    0x90D8, 0x5207, //#CJK UNIFIED IDEOGRAPH
    0x90D9, 0x62D9, //#CJK UNIFIED IDEOGRAPH
    0x90DA, 0x63A5, //#CJK UNIFIED IDEOGRAPH
    0x90DB, 0x6442, //#CJK UNIFIED IDEOGRAPH
    0x90DC, 0x6298, //#CJK UNIFIED IDEOGRAPH
    0x90DD, 0x8A2D, //#CJK UNIFIED IDEOGRAPH
    0x90DE, 0x7A83, //#CJK UNIFIED IDEOGRAPH
    0x90DF, 0x7BC0, //#CJK UNIFIED IDEOGRAPH
    0x90E0, 0x8AAC, //#CJK UNIFIED IDEOGRAPH
    0x90E1, 0x96EA, //#CJK UNIFIED IDEOGRAPH
    0x90E2, 0x7D76, //#CJK UNIFIED IDEOGRAPH
    0x90E3, 0x820C, //#CJK UNIFIED IDEOGRAPH
    0x90E4, 0x8749, //#CJK UNIFIED IDEOGRAPH
    0x90E5, 0x4ED9, //#CJK UNIFIED IDEOGRAPH
    0x90E6, 0x5148, //#CJK UNIFIED IDEOGRAPH
    0x90E7, 0x5343, //#CJK UNIFIED IDEOGRAPH
    0x90E8, 0x5360, //#CJK UNIFIED IDEOGRAPH
    0x90E9, 0x5BA3, //#CJK UNIFIED IDEOGRAPH
    0x90EA, 0x5C02, //#CJK UNIFIED IDEOGRAPH
    0x90EB, 0x5C16, //#CJK UNIFIED IDEOGRAPH
    0x90EC, 0x5DDD, //#CJK UNIFIED IDEOGRAPH
    0x90ED, 0x6226, //#CJK UNIFIED IDEOGRAPH
    0x90EE, 0x6247, //#CJK UNIFIED IDEOGRAPH
    0x90EF, 0x64B0, //#CJK UNIFIED IDEOGRAPH
    0x90F0, 0x6813, //#CJK UNIFIED IDEOGRAPH
    0x90F1, 0x6834, //#CJK UNIFIED IDEOGRAPH
    0x90F2, 0x6CC9, //#CJK UNIFIED IDEOGRAPH
    0x90F3, 0x6D45, //#CJK UNIFIED IDEOGRAPH
    0x90F4, 0x6D17, //#CJK UNIFIED IDEOGRAPH
    0x90F5, 0x67D3, //#CJK UNIFIED IDEOGRAPH
    0x90F6, 0x6F5C, //#CJK UNIFIED IDEOGRAPH
    0x90F7, 0x714E, //#CJK UNIFIED IDEOGRAPH
    0x90F8, 0x717D, //#CJK UNIFIED IDEOGRAPH
    0x90F9, 0x65CB, //#CJK UNIFIED IDEOGRAPH
    0x90FA, 0x7A7F, //#CJK UNIFIED IDEOGRAPH
    0x90FB, 0x7BAD, //#CJK UNIFIED IDEOGRAPH
    0x90FC, 0x7DDA, //#CJK UNIFIED IDEOGRAPH
    0x9140, 0x7E4A, //#CJK UNIFIED IDEOGRAPH
    0x9141, 0x7FA8, //#CJK UNIFIED IDEOGRAPH
    0x9142, 0x817A, //#CJK UNIFIED IDEOGRAPH
    0x9143, 0x821B, //#CJK UNIFIED IDEOGRAPH
    0x9144, 0x8239, //#CJK UNIFIED IDEOGRAPH
    0x9145, 0x85A6, //#CJK UNIFIED IDEOGRAPH
    0x9146, 0x8A6E, //#CJK UNIFIED IDEOGRAPH
    0x9147, 0x8CCE, //#CJK UNIFIED IDEOGRAPH
    0x9148, 0x8DF5, //#CJK UNIFIED IDEOGRAPH
    0x9149, 0x9078, //#CJK UNIFIED IDEOGRAPH
    0x914A, 0x9077, //#CJK UNIFIED IDEOGRAPH
    0x914B, 0x92AD, //#CJK UNIFIED IDEOGRAPH
    0x914C, 0x9291, //#CJK UNIFIED IDEOGRAPH
    0x914D, 0x9583, //#CJK UNIFIED IDEOGRAPH
    0x914E, 0x9BAE, //#CJK UNIFIED IDEOGRAPH
    0x914F, 0x524D, //#CJK UNIFIED IDEOGRAPH
    0x9150, 0x5584, //#CJK UNIFIED IDEOGRAPH
    0x9151, 0x6F38, //#CJK UNIFIED IDEOGRAPH
    0x9152, 0x7136, //#CJK UNIFIED IDEOGRAPH
    0x9153, 0x5168, //#CJK UNIFIED IDEOGRAPH
    0x9154, 0x7985, //#CJK UNIFIED IDEOGRAPH
    0x9155, 0x7E55, //#CJK UNIFIED IDEOGRAPH
    0x9156, 0x81B3, //#CJK UNIFIED IDEOGRAPH
    0x9157, 0x7CCE, //#CJK UNIFIED IDEOGRAPH
    0x9158, 0x564C, //#CJK UNIFIED IDEOGRAPH
    0x9159, 0x5851, //#CJK UNIFIED IDEOGRAPH
    0x915A, 0x5CA8, //#CJK UNIFIED IDEOGRAPH
    0x915B, 0x63AA, //#CJK UNIFIED IDEOGRAPH
    0x915C, 0x66FE, //#CJK UNIFIED IDEOGRAPH
    0x915D, 0x66FD, //#CJK UNIFIED IDEOGRAPH
    0x915E, 0x695A, //#CJK UNIFIED IDEOGRAPH
    0x915F, 0x72D9, //#CJK UNIFIED IDEOGRAPH
    0x9160, 0x758F, //#CJK UNIFIED IDEOGRAPH
    0x9161, 0x758E, //#CJK UNIFIED IDEOGRAPH
    0x9162, 0x790E, //#CJK UNIFIED IDEOGRAPH
    0x9163, 0x7956, //#CJK UNIFIED IDEOGRAPH
    0x9164, 0x79DF, //#CJK UNIFIED IDEOGRAPH
    0x9165, 0x7C97, //#CJK UNIFIED IDEOGRAPH
    0x9166, 0x7D20, //#CJK UNIFIED IDEOGRAPH
    0x9167, 0x7D44, //#CJK UNIFIED IDEOGRAPH
    0x9168, 0x8607, //#CJK UNIFIED IDEOGRAPH
    0x9169, 0x8A34, //#CJK UNIFIED IDEOGRAPH
    0x916A, 0x963B, //#CJK UNIFIED IDEOGRAPH
    0x916B, 0x9061, //#CJK UNIFIED IDEOGRAPH
    0x916C, 0x9F20, //#CJK UNIFIED IDEOGRAPH
    0x916D, 0x50E7, //#CJK UNIFIED IDEOGRAPH
    0x916E, 0x5275, //#CJK UNIFIED IDEOGRAPH
    0x916F, 0x53CC, //#CJK UNIFIED IDEOGRAPH
    0x9170, 0x53E2, //#CJK UNIFIED IDEOGRAPH
    0x9171, 0x5009, //#CJK UNIFIED IDEOGRAPH
    0x9172, 0x55AA, //#CJK UNIFIED IDEOGRAPH
    0x9173, 0x58EE, //#CJK UNIFIED IDEOGRAPH
    0x9174, 0x594F, //#CJK UNIFIED IDEOGRAPH
    0x9175, 0x723D, //#CJK UNIFIED IDEOGRAPH
    0x9176, 0x5B8B, //#CJK UNIFIED IDEOGRAPH
    0x9177, 0x5C64, //#CJK UNIFIED IDEOGRAPH
    0x9178, 0x531D, //#CJK UNIFIED IDEOGRAPH
    0x9179, 0x60E3, //#CJK UNIFIED IDEOGRAPH
    0x917A, 0x60F3, //#CJK UNIFIED IDEOGRAPH
    0x917B, 0x635C, //#CJK UNIFIED IDEOGRAPH
    0x917C, 0x6383, //#CJK UNIFIED IDEOGRAPH
    0x917D, 0x633F, //#CJK UNIFIED IDEOGRAPH
    0x917E, 0x63BB, //#CJK UNIFIED IDEOGRAPH
    0x9180, 0x64CD, //#CJK UNIFIED IDEOGRAPH
    0x9181, 0x65E9, //#CJK UNIFIED IDEOGRAPH
    0x9182, 0x66F9, //#CJK UNIFIED IDEOGRAPH
    0x9183, 0x5DE3, //#CJK UNIFIED IDEOGRAPH
    0x9184, 0x69CD, //#CJK UNIFIED IDEOGRAPH
    0x9185, 0x69FD, //#CJK UNIFIED IDEOGRAPH
    0x9186, 0x6F15, //#CJK UNIFIED IDEOGRAPH
    0x9187, 0x71E5, //#CJK UNIFIED IDEOGRAPH
    0x9188, 0x4E89, //#CJK UNIFIED IDEOGRAPH
    0x9189, 0x75E9, //#CJK UNIFIED IDEOGRAPH
    0x918A, 0x76F8, //#CJK UNIFIED IDEOGRAPH
    0x918B, 0x7A93, //#CJK UNIFIED IDEOGRAPH
    0x918C, 0x7CDF, //#CJK UNIFIED IDEOGRAPH
    0x918D, 0x7DCF, //#CJK UNIFIED IDEOGRAPH
    0x918E, 0x7D9C, //#CJK UNIFIED IDEOGRAPH
    0x918F, 0x8061, //#CJK UNIFIED IDEOGRAPH
    0x9190, 0x8349, //#CJK UNIFIED IDEOGRAPH
    0x9191, 0x8358, //#CJK UNIFIED IDEOGRAPH
    0x9192, 0x846C, //#CJK UNIFIED IDEOGRAPH
    0x9193, 0x84BC, //#CJK UNIFIED IDEOGRAPH
    0x9194, 0x85FB, //#CJK UNIFIED IDEOGRAPH
    0x9195, 0x88C5, //#CJK UNIFIED IDEOGRAPH
    0x9196, 0x8D70, //#CJK UNIFIED IDEOGRAPH
    0x9197, 0x9001, //#CJK UNIFIED IDEOGRAPH
    0x9198, 0x906D, //#CJK UNIFIED IDEOGRAPH
    0x9199, 0x9397, //#CJK UNIFIED IDEOGRAPH
    0x919A, 0x971C, //#CJK UNIFIED IDEOGRAPH
    0x919B, 0x9A12, //#CJK UNIFIED IDEOGRAPH
    0x919C, 0x50CF, //#CJK UNIFIED IDEOGRAPH
    0x919D, 0x5897, //#CJK UNIFIED IDEOGRAPH
    0x919E, 0x618E, //#CJK UNIFIED IDEOGRAPH
    0x919F, 0x81D3, //#CJK UNIFIED IDEOGRAPH
    0x91A0, 0x8535, //#CJK UNIFIED IDEOGRAPH
    0x91A1, 0x8D08, //#CJK UNIFIED IDEOGRAPH
    0x91A2, 0x9020, //#CJK UNIFIED IDEOGRAPH
    0x91A3, 0x4FC3, //#CJK UNIFIED IDEOGRAPH
    0x91A4, 0x5074, //#CJK UNIFIED IDEOGRAPH
    0x91A5, 0x5247, //#CJK UNIFIED IDEOGRAPH
    0x91A6, 0x5373, //#CJK UNIFIED IDEOGRAPH
    0x91A7, 0x606F, //#CJK UNIFIED IDEOGRAPH
    0x91A8, 0x6349, //#CJK UNIFIED IDEOGRAPH
    0x91A9, 0x675F, //#CJK UNIFIED IDEOGRAPH
    0x91AA, 0x6E2C, //#CJK UNIFIED IDEOGRAPH
    0x91AB, 0x8DB3, //#CJK UNIFIED IDEOGRAPH
    0x91AC, 0x901F, //#CJK UNIFIED IDEOGRAPH
    0x91AD, 0x4FD7, //#CJK UNIFIED IDEOGRAPH
    0x91AE, 0x5C5E, //#CJK UNIFIED IDEOGRAPH
    0x91AF, 0x8CCA, //#CJK UNIFIED IDEOGRAPH
    0x91B0, 0x65CF, //#CJK UNIFIED IDEOGRAPH
    0x91B1, 0x7D9A, //#CJK UNIFIED IDEOGRAPH
    0x91B2, 0x5352, //#CJK UNIFIED IDEOGRAPH
    0x91B3, 0x8896, //#CJK UNIFIED IDEOGRAPH
    0x91B4, 0x5176, //#CJK UNIFIED IDEOGRAPH
    0x91B5, 0x63C3, //#CJK UNIFIED IDEOGRAPH
    0x91B6, 0x5B58, //#CJK UNIFIED IDEOGRAPH
    0x91B7, 0x5B6B, //#CJK UNIFIED IDEOGRAPH
    0x91B8, 0x5C0A, //#CJK UNIFIED IDEOGRAPH
    0x91B9, 0x640D, //#CJK UNIFIED IDEOGRAPH
    0x91BA, 0x6751, //#CJK UNIFIED IDEOGRAPH
    0x91BB, 0x905C, //#CJK UNIFIED IDEOGRAPH
    0x91BC, 0x4ED6, //#CJK UNIFIED IDEOGRAPH
    0x91BD, 0x591A, //#CJK UNIFIED IDEOGRAPH
    0x91BE, 0x592A, //#CJK UNIFIED IDEOGRAPH
    0x91BF, 0x6C70, //#CJK UNIFIED IDEOGRAPH
    0x91C0, 0x8A51, //#CJK UNIFIED IDEOGRAPH
    0x91C1, 0x553E, //#CJK UNIFIED IDEOGRAPH
    0x91C2, 0x5815, //#CJK UNIFIED IDEOGRAPH
    0x91C3, 0x59A5, //#CJK UNIFIED IDEOGRAPH
    0x91C4, 0x60F0, //#CJK UNIFIED IDEOGRAPH
    0x91C5, 0x6253, //#CJK UNIFIED IDEOGRAPH
    0x91C6, 0x67C1, //#CJK UNIFIED IDEOGRAPH
    0x91C7, 0x8235, //#CJK UNIFIED IDEOGRAPH
    0x91C8, 0x6955, //#CJK UNIFIED IDEOGRAPH
    0x91C9, 0x9640, //#CJK UNIFIED IDEOGRAPH
    0x91CA, 0x99C4, //#CJK UNIFIED IDEOGRAPH
    0x91CB, 0x9A28, //#CJK UNIFIED IDEOGRAPH
    0x91CC, 0x4F53, //#CJK UNIFIED IDEOGRAPH
    0x91CD, 0x5806, //#CJK UNIFIED IDEOGRAPH
    0x91CE, 0x5BFE, //#CJK UNIFIED IDEOGRAPH
    0x91CF, 0x8010, //#CJK UNIFIED IDEOGRAPH
    0x91D0, 0x5CB1, //#CJK UNIFIED IDEOGRAPH
    0x91D1, 0x5E2F, //#CJK UNIFIED IDEOGRAPH
    0x91D2, 0x5F85, //#CJK UNIFIED IDEOGRAPH
    0x91D3, 0x6020, //#CJK UNIFIED IDEOGRAPH
    0x91D4, 0x614B, //#CJK UNIFIED IDEOGRAPH
    0x91D5, 0x6234, //#CJK UNIFIED IDEOGRAPH
    0x91D6, 0x66FF, //#CJK UNIFIED IDEOGRAPH
    0x91D7, 0x6CF0, //#CJK UNIFIED IDEOGRAPH
    0x91D8, 0x6EDE, //#CJK UNIFIED IDEOGRAPH
    0x91D9, 0x80CE, //#CJK UNIFIED IDEOGRAPH
    0x91DA, 0x817F, //#CJK UNIFIED IDEOGRAPH
    0x91DB, 0x82D4, //#CJK UNIFIED IDEOGRAPH
    0x91DC, 0x888B, //#CJK UNIFIED IDEOGRAPH
    0x91DD, 0x8CB8, //#CJK UNIFIED IDEOGRAPH
    0x91DE, 0x9000, //#CJK UNIFIED IDEOGRAPH
    0x91DF, 0x902E, //#CJK UNIFIED IDEOGRAPH
    0x91E0, 0x968A, //#CJK UNIFIED IDEOGRAPH
    0x91E1, 0x9EDB, //#CJK UNIFIED IDEOGRAPH
    0x91E2, 0x9BDB, //#CJK UNIFIED IDEOGRAPH
    0x91E3, 0x4EE3, //#CJK UNIFIED IDEOGRAPH
    0x91E4, 0x53F0, //#CJK UNIFIED IDEOGRAPH
    0x91E5, 0x5927, //#CJK UNIFIED IDEOGRAPH
    0x91E6, 0x7B2C, //#CJK UNIFIED IDEOGRAPH
    0x91E7, 0x918D, //#CJK UNIFIED IDEOGRAPH
    0x91E8, 0x984C, //#CJK UNIFIED IDEOGRAPH
    0x91E9, 0x9DF9, //#CJK UNIFIED IDEOGRAPH
    0x91EA, 0x6EDD, //#CJK UNIFIED IDEOGRAPH
    0x91EB, 0x7027, //#CJK UNIFIED IDEOGRAPH
    0x91EC, 0x5353, //#CJK UNIFIED IDEOGRAPH
    0x91ED, 0x5544, //#CJK UNIFIED IDEOGRAPH
    0x91EE, 0x5B85, //#CJK UNIFIED IDEOGRAPH
    0x91EF, 0x6258, //#CJK UNIFIED IDEOGRAPH
    0x91F0, 0x629E, //#CJK UNIFIED IDEOGRAPH
    0x91F1, 0x62D3, //#CJK UNIFIED IDEOGRAPH
    0x91F2, 0x6CA2, //#CJK UNIFIED IDEOGRAPH
    0x91F3, 0x6FEF, //#CJK UNIFIED IDEOGRAPH
    0x91F4, 0x7422, //#CJK UNIFIED IDEOGRAPH
    0x91F5, 0x8A17, //#CJK UNIFIED IDEOGRAPH
    0x91F6, 0x9438, //#CJK UNIFIED IDEOGRAPH
    0x91F7, 0x6FC1, //#CJK UNIFIED IDEOGRAPH
    0x91F8, 0x8AFE, //#CJK UNIFIED IDEOGRAPH
    0x91F9, 0x8338, //#CJK UNIFIED IDEOGRAPH
    0x91FA, 0x51E7, //#CJK UNIFIED IDEOGRAPH
    0x91FB, 0x86F8, //#CJK UNIFIED IDEOGRAPH
    0x91FC, 0x53EA, //#CJK UNIFIED IDEOGRAPH
    0x9240, 0x53E9, //#CJK UNIFIED IDEOGRAPH
    0x9241, 0x4F46, //#CJK UNIFIED IDEOGRAPH
    0x9242, 0x9054, //#CJK UNIFIED IDEOGRAPH
    0x9243, 0x8FB0, //#CJK UNIFIED IDEOGRAPH
    0x9244, 0x596A, //#CJK UNIFIED IDEOGRAPH
    0x9245, 0x8131, //#CJK UNIFIED IDEOGRAPH
    0x9246, 0x5DFD, //#CJK UNIFIED IDEOGRAPH
    0x9247, 0x7AEA, //#CJK UNIFIED IDEOGRAPH
    0x9248, 0x8FBF, //#CJK UNIFIED IDEOGRAPH
    0x9249, 0x68DA, //#CJK UNIFIED IDEOGRAPH
    0x924A, 0x8C37, //#CJK UNIFIED IDEOGRAPH
    0x924B, 0x72F8, //#CJK UNIFIED IDEOGRAPH
    0x924C, 0x9C48, //#CJK UNIFIED IDEOGRAPH
    0x924D, 0x6A3D, //#CJK UNIFIED IDEOGRAPH
    0x924E, 0x8AB0, //#CJK UNIFIED IDEOGRAPH
    0x924F, 0x4E39, //#CJK UNIFIED IDEOGRAPH
    0x9250, 0x5358, //#CJK UNIFIED IDEOGRAPH
    0x9251, 0x5606, //#CJK UNIFIED IDEOGRAPH
    0x9252, 0x5766, //#CJK UNIFIED IDEOGRAPH
    0x9253, 0x62C5, //#CJK UNIFIED IDEOGRAPH
    0x9254, 0x63A2, //#CJK UNIFIED IDEOGRAPH
    0x9255, 0x65E6, //#CJK UNIFIED IDEOGRAPH
    0x9256, 0x6B4E, //#CJK UNIFIED IDEOGRAPH
    0x9257, 0x6DE1, //#CJK UNIFIED IDEOGRAPH
    0x9258, 0x6E5B, //#CJK UNIFIED IDEOGRAPH
    0x9259, 0x70AD, //#CJK UNIFIED IDEOGRAPH
    0x925A, 0x77ED, //#CJK UNIFIED IDEOGRAPH
    0x925B, 0x7AEF, //#CJK UNIFIED IDEOGRAPH
    0x925C, 0x7BAA, //#CJK UNIFIED IDEOGRAPH
    0x925D, 0x7DBB, //#CJK UNIFIED IDEOGRAPH
    0x925E, 0x803D, //#CJK UNIFIED IDEOGRAPH
    0x925F, 0x80C6, //#CJK UNIFIED IDEOGRAPH
    0x9260, 0x86CB, //#CJK UNIFIED IDEOGRAPH
    0x9261, 0x8A95, //#CJK UNIFIED IDEOGRAPH
    0x9262, 0x935B, //#CJK UNIFIED IDEOGRAPH
    0x9263, 0x56E3, //#CJK UNIFIED IDEOGRAPH
    0x9264, 0x58C7, //#CJK UNIFIED IDEOGRAPH
    0x9265, 0x5F3E, //#CJK UNIFIED IDEOGRAPH
    0x9266, 0x65AD, //#CJK UNIFIED IDEOGRAPH
    0x9267, 0x6696, //#CJK UNIFIED IDEOGRAPH
    0x9268, 0x6A80, //#CJK UNIFIED IDEOGRAPH
    0x9269, 0x6BB5, //#CJK UNIFIED IDEOGRAPH
    0x926A, 0x7537, //#CJK UNIFIED IDEOGRAPH
    0x926B, 0x8AC7, //#CJK UNIFIED IDEOGRAPH
    0x926C, 0x5024, //#CJK UNIFIED IDEOGRAPH
    0x926D, 0x77E5, //#CJK UNIFIED IDEOGRAPH
    0x926E, 0x5730, //#CJK UNIFIED IDEOGRAPH
    0x926F, 0x5F1B, //#CJK UNIFIED IDEOGRAPH
    0x9270, 0x6065, //#CJK UNIFIED IDEOGRAPH
    0x9271, 0x667A, //#CJK UNIFIED IDEOGRAPH
    0x9272, 0x6C60, //#CJK UNIFIED IDEOGRAPH
    0x9273, 0x75F4, //#CJK UNIFIED IDEOGRAPH
    0x9274, 0x7A1A, //#CJK UNIFIED IDEOGRAPH
    0x9275, 0x7F6E, //#CJK UNIFIED IDEOGRAPH
    0x9276, 0x81F4, //#CJK UNIFIED IDEOGRAPH
    0x9277, 0x8718, //#CJK UNIFIED IDEOGRAPH
    0x9278, 0x9045, //#CJK UNIFIED IDEOGRAPH
    0x9279, 0x99B3, //#CJK UNIFIED IDEOGRAPH
    0x927A, 0x7BC9, //#CJK UNIFIED IDEOGRAPH
    0x927B, 0x755C, //#CJK UNIFIED IDEOGRAPH
    0x927C, 0x7AF9, //#CJK UNIFIED IDEOGRAPH
    0x927D, 0x7B51, //#CJK UNIFIED IDEOGRAPH
    0x927E, 0x84C4, //#CJK UNIFIED IDEOGRAPH
    0x9280, 0x9010, //#CJK UNIFIED IDEOGRAPH
    0x9281, 0x79E9, //#CJK UNIFIED IDEOGRAPH
    0x9282, 0x7A92, //#CJK UNIFIED IDEOGRAPH
    0x9283, 0x8336, //#CJK UNIFIED IDEOGRAPH
    0x9284, 0x5AE1, //#CJK UNIFIED IDEOGRAPH
    0x9285, 0x7740, //#CJK UNIFIED IDEOGRAPH
    0x9286, 0x4E2D, //#CJK UNIFIED IDEOGRAPH
    0x9287, 0x4EF2, //#CJK UNIFIED IDEOGRAPH
    0x9288, 0x5B99, //#CJK UNIFIED IDEOGRAPH
    0x9289, 0x5FE0, //#CJK UNIFIED IDEOGRAPH
    0x928A, 0x62BD, //#CJK UNIFIED IDEOGRAPH
    0x928B, 0x663C, //#CJK UNIFIED IDEOGRAPH
    0x928C, 0x67F1, //#CJK UNIFIED IDEOGRAPH
    0x928D, 0x6CE8, //#CJK UNIFIED IDEOGRAPH
    0x928E, 0x866B, //#CJK UNIFIED IDEOGRAPH
    0x928F, 0x8877, //#CJK UNIFIED IDEOGRAPH
    0x9290, 0x8A3B, //#CJK UNIFIED IDEOGRAPH
    0x9291, 0x914E, //#CJK UNIFIED IDEOGRAPH
    0x9292, 0x92F3, //#CJK UNIFIED IDEOGRAPH
    0x9293, 0x99D0, //#CJK UNIFIED IDEOGRAPH
    0x9294, 0x6A17, //#CJK UNIFIED IDEOGRAPH
    0x9295, 0x7026, //#CJK UNIFIED IDEOGRAPH
    0x9296, 0x732A, //#CJK UNIFIED IDEOGRAPH
    0x9297, 0x82E7, //#CJK UNIFIED IDEOGRAPH
    0x9298, 0x8457, //#CJK UNIFIED IDEOGRAPH
    0x9299, 0x8CAF, //#CJK UNIFIED IDEOGRAPH
    0x929A, 0x4E01, //#CJK UNIFIED IDEOGRAPH
    0x929B, 0x5146, //#CJK UNIFIED IDEOGRAPH
    0x929C, 0x51CB, //#CJK UNIFIED IDEOGRAPH
    0x929D, 0x558B, //#CJK UNIFIED IDEOGRAPH
    0x929E, 0x5BF5, //#CJK UNIFIED IDEOGRAPH
    0x929F, 0x5E16, //#CJK UNIFIED IDEOGRAPH
    0x92A0, 0x5E33, //#CJK UNIFIED IDEOGRAPH
    0x92A1, 0x5E81, //#CJK UNIFIED IDEOGRAPH
    0x92A2, 0x5F14, //#CJK UNIFIED IDEOGRAPH
    0x92A3, 0x5F35, //#CJK UNIFIED IDEOGRAPH
    0x92A4, 0x5F6B, //#CJK UNIFIED IDEOGRAPH
    0x92A5, 0x5FB4, //#CJK UNIFIED IDEOGRAPH
    0x92A6, 0x61F2, //#CJK UNIFIED IDEOGRAPH
    0x92A7, 0x6311, //#CJK UNIFIED IDEOGRAPH
    0x92A8, 0x66A2, //#CJK UNIFIED IDEOGRAPH
    0x92A9, 0x671D, //#CJK UNIFIED IDEOGRAPH
    0x92AA, 0x6F6E, //#CJK UNIFIED IDEOGRAPH
    0x92AB, 0x7252, //#CJK UNIFIED IDEOGRAPH
    0x92AC, 0x753A, //#CJK UNIFIED IDEOGRAPH
    0x92AD, 0x773A, //#CJK UNIFIED IDEOGRAPH
    0x92AE, 0x8074, //#CJK UNIFIED IDEOGRAPH
    0x92AF, 0x8139, //#CJK UNIFIED IDEOGRAPH
    0x92B0, 0x8178, //#CJK UNIFIED IDEOGRAPH
    0x92B1, 0x8776, //#CJK UNIFIED IDEOGRAPH
    0x92B2, 0x8ABF, //#CJK UNIFIED IDEOGRAPH
    0x92B3, 0x8ADC, //#CJK UNIFIED IDEOGRAPH
    0x92B4, 0x8D85, //#CJK UNIFIED IDEOGRAPH
    0x92B5, 0x8DF3, //#CJK UNIFIED IDEOGRAPH
    0x92B6, 0x929A, //#CJK UNIFIED IDEOGRAPH
    0x92B7, 0x9577, //#CJK UNIFIED IDEOGRAPH
    0x92B8, 0x9802, //#CJK UNIFIED IDEOGRAPH
    0x92B9, 0x9CE5, //#CJK UNIFIED IDEOGRAPH
    0x92BA, 0x52C5, //#CJK UNIFIED IDEOGRAPH
    0x92BB, 0x6357, //#CJK UNIFIED IDEOGRAPH
    0x92BC, 0x76F4, //#CJK UNIFIED IDEOGRAPH
    0x92BD, 0x6715, //#CJK UNIFIED IDEOGRAPH
    0x92BE, 0x6C88, //#CJK UNIFIED IDEOGRAPH
    0x92BF, 0x73CD, //#CJK UNIFIED IDEOGRAPH
    0x92C0, 0x8CC3, //#CJK UNIFIED IDEOGRAPH
    0x92C1, 0x93AE, //#CJK UNIFIED IDEOGRAPH
    0x92C2, 0x9673, //#CJK UNIFIED IDEOGRAPH
    0x92C3, 0x6D25, //#CJK UNIFIED IDEOGRAPH
    0x92C4, 0x589C, //#CJK UNIFIED IDEOGRAPH
    0x92C5, 0x690E, //#CJK UNIFIED IDEOGRAPH
    0x92C6, 0x69CC, //#CJK UNIFIED IDEOGRAPH
    0x92C7, 0x8FFD, //#CJK UNIFIED IDEOGRAPH
    0x92C8, 0x939A, //#CJK UNIFIED IDEOGRAPH
    0x92C9, 0x75DB, //#CJK UNIFIED IDEOGRAPH
    0x92CA, 0x901A, //#CJK UNIFIED IDEOGRAPH
    0x92CB, 0x585A, //#CJK UNIFIED IDEOGRAPH
    0x92CC, 0x6802, //#CJK UNIFIED IDEOGRAPH
    0x92CD, 0x63B4, //#CJK UNIFIED IDEOGRAPH
    0x92CE, 0x69FB, //#CJK UNIFIED IDEOGRAPH
    0x92CF, 0x4F43, //#CJK UNIFIED IDEOGRAPH
    0x92D0, 0x6F2C, //#CJK UNIFIED IDEOGRAPH
    0x92D1, 0x67D8, //#CJK UNIFIED IDEOGRAPH
    0x92D2, 0x8FBB, //#CJK UNIFIED IDEOGRAPH
    0x92D3, 0x8526, //#CJK UNIFIED IDEOGRAPH
    0x92D4, 0x7DB4, //#CJK UNIFIED IDEOGRAPH
    0x92D5, 0x9354, //#CJK UNIFIED IDEOGRAPH
    0x92D6, 0x693F, //#CJK UNIFIED IDEOGRAPH
    0x92D7, 0x6F70, //#CJK UNIFIED IDEOGRAPH
    0x92D8, 0x576A, //#CJK UNIFIED IDEOGRAPH
    0x92D9, 0x58F7, //#CJK UNIFIED IDEOGRAPH
    0x92DA, 0x5B2C, //#CJK UNIFIED IDEOGRAPH
    0x92DB, 0x7D2C, //#CJK UNIFIED IDEOGRAPH
    0x92DC, 0x722A, //#CJK UNIFIED IDEOGRAPH
    0x92DD, 0x540A, //#CJK UNIFIED IDEOGRAPH
    0x92DE, 0x91E3, //#CJK UNIFIED IDEOGRAPH
    0x92DF, 0x9DB4, //#CJK UNIFIED IDEOGRAPH
    0x92E0, 0x4EAD, //#CJK UNIFIED IDEOGRAPH
    0x92E1, 0x4F4E, //#CJK UNIFIED IDEOGRAPH
    0x92E2, 0x505C, //#CJK UNIFIED IDEOGRAPH
    0x92E3, 0x5075, //#CJK UNIFIED IDEOGRAPH
    0x92E4, 0x5243, //#CJK UNIFIED IDEOGRAPH
    0x92E5, 0x8C9E, //#CJK UNIFIED IDEOGRAPH
    0x92E6, 0x5448, //#CJK UNIFIED IDEOGRAPH
    0x92E7, 0x5824, //#CJK UNIFIED IDEOGRAPH
    0x92E8, 0x5B9A, //#CJK UNIFIED IDEOGRAPH
    0x92E9, 0x5E1D, //#CJK UNIFIED IDEOGRAPH
    0x92EA, 0x5E95, //#CJK UNIFIED IDEOGRAPH
    0x92EB, 0x5EAD, //#CJK UNIFIED IDEOGRAPH
    0x92EC, 0x5EF7, //#CJK UNIFIED IDEOGRAPH
    0x92ED, 0x5F1F, //#CJK UNIFIED IDEOGRAPH
    0x92EE, 0x608C, //#CJK UNIFIED IDEOGRAPH
    0x92EF, 0x62B5, //#CJK UNIFIED IDEOGRAPH
    0x92F0, 0x633A, //#CJK UNIFIED IDEOGRAPH
    0x92F1, 0x63D0, //#CJK UNIFIED IDEOGRAPH
    0x92F2, 0x68AF, //#CJK UNIFIED IDEOGRAPH
    0x92F3, 0x6C40, //#CJK UNIFIED IDEOGRAPH
    0x92F4, 0x7887, //#CJK UNIFIED IDEOGRAPH
    0x92F5, 0x798E, //#CJK UNIFIED IDEOGRAPH
    0x92F6, 0x7A0B, //#CJK UNIFIED IDEOGRAPH
    0x92F7, 0x7DE0, //#CJK UNIFIED IDEOGRAPH
    0x92F8, 0x8247, //#CJK UNIFIED IDEOGRAPH
    0x92F9, 0x8A02, //#CJK UNIFIED IDEOGRAPH
    0x92FA, 0x8AE6, //#CJK UNIFIED IDEOGRAPH
    0x92FB, 0x8E44, //#CJK UNIFIED IDEOGRAPH
    0x92FC, 0x9013, //#CJK UNIFIED IDEOGRAPH
    0x9340, 0x90B8, //#CJK UNIFIED IDEOGRAPH
    0x9341, 0x912D, //#CJK UNIFIED IDEOGRAPH
    0x9342, 0x91D8, //#CJK UNIFIED IDEOGRAPH
    0x9343, 0x9F0E, //#CJK UNIFIED IDEOGRAPH
    0x9344, 0x6CE5, //#CJK UNIFIED IDEOGRAPH
    0x9345, 0x6458, //#CJK UNIFIED IDEOGRAPH
    0x9346, 0x64E2, //#CJK UNIFIED IDEOGRAPH
    0x9347, 0x6575, //#CJK UNIFIED IDEOGRAPH
    0x9348, 0x6EF4, //#CJK UNIFIED IDEOGRAPH
    0x9349, 0x7684, //#CJK UNIFIED IDEOGRAPH
    0x934A, 0x7B1B, //#CJK UNIFIED IDEOGRAPH
    0x934B, 0x9069, //#CJK UNIFIED IDEOGRAPH
    0x934C, 0x93D1, //#CJK UNIFIED IDEOGRAPH
    0x934D, 0x6EBA, //#CJK UNIFIED IDEOGRAPH
    0x934E, 0x54F2, //#CJK UNIFIED IDEOGRAPH
    0x934F, 0x5FB9, //#CJK UNIFIED IDEOGRAPH
    0x9350, 0x64A4, //#CJK UNIFIED IDEOGRAPH
    0x9351, 0x8F4D, //#CJK UNIFIED IDEOGRAPH
    0x9352, 0x8FED, //#CJK UNIFIED IDEOGRAPH
    0x9353, 0x9244, //#CJK UNIFIED IDEOGRAPH
    0x9354, 0x5178, //#CJK UNIFIED IDEOGRAPH
    0x9355, 0x586B, //#CJK UNIFIED IDEOGRAPH
    0x9356, 0x5929, //#CJK UNIFIED IDEOGRAPH
    0x9357, 0x5C55, //#CJK UNIFIED IDEOGRAPH
    0x9358, 0x5E97, //#CJK UNIFIED IDEOGRAPH
    0x9359, 0x6DFB, //#CJK UNIFIED IDEOGRAPH
    0x935A, 0x7E8F, //#CJK UNIFIED IDEOGRAPH
    0x935B, 0x751C, //#CJK UNIFIED IDEOGRAPH
    0x935C, 0x8CBC, //#CJK UNIFIED IDEOGRAPH
    0x935D, 0x8EE2, //#CJK UNIFIED IDEOGRAPH
    0x935E, 0x985B, //#CJK UNIFIED IDEOGRAPH
    0x935F, 0x70B9, //#CJK UNIFIED IDEOGRAPH
    0x9360, 0x4F1D, //#CJK UNIFIED IDEOGRAPH
    0x9361, 0x6BBF, //#CJK UNIFIED IDEOGRAPH
    0x9362, 0x6FB1, //#CJK UNIFIED IDEOGRAPH
    0x9363, 0x7530, //#CJK UNIFIED IDEOGRAPH
    0x9364, 0x96FB, //#CJK UNIFIED IDEOGRAPH
    0x9365, 0x514E, //#CJK UNIFIED IDEOGRAPH
    0x9366, 0x5410, //#CJK UNIFIED IDEOGRAPH
    0x9367, 0x5835, //#CJK UNIFIED IDEOGRAPH
    0x9368, 0x5857, //#CJK UNIFIED IDEOGRAPH
    0x9369, 0x59AC, //#CJK UNIFIED IDEOGRAPH
    0x936A, 0x5C60, //#CJK UNIFIED IDEOGRAPH
    0x936B, 0x5F92, //#CJK UNIFIED IDEOGRAPH
    0x936C, 0x6597, //#CJK UNIFIED IDEOGRAPH
    0x936D, 0x675C, //#CJK UNIFIED IDEOGRAPH
    0x936E, 0x6E21, //#CJK UNIFIED IDEOGRAPH
    0x936F, 0x767B, //#CJK UNIFIED IDEOGRAPH
    0x9370, 0x83DF, //#CJK UNIFIED IDEOGRAPH
    0x9371, 0x8CED, //#CJK UNIFIED IDEOGRAPH
    0x9372, 0x9014, //#CJK UNIFIED IDEOGRAPH
    0x9373, 0x90FD, //#CJK UNIFIED IDEOGRAPH
    0x9374, 0x934D, //#CJK UNIFIED IDEOGRAPH
    0x9375, 0x7825, //#CJK UNIFIED IDEOGRAPH
    0x9376, 0x783A, //#CJK UNIFIED IDEOGRAPH
    0x9377, 0x52AA, //#CJK UNIFIED IDEOGRAPH
    0x9378, 0x5EA6, //#CJK UNIFIED IDEOGRAPH
    0x9379, 0x571F, //#CJK UNIFIED IDEOGRAPH
    0x937A, 0x5974, //#CJK UNIFIED IDEOGRAPH
    0x937B, 0x6012, //#CJK UNIFIED IDEOGRAPH
    0x937C, 0x5012, //#CJK UNIFIED IDEOGRAPH
    0x937D, 0x515A, //#CJK UNIFIED IDEOGRAPH
    0x937E, 0x51AC, //#CJK UNIFIED IDEOGRAPH
    0x9380, 0x51CD, //#CJK UNIFIED IDEOGRAPH
    0x9381, 0x5200, //#CJK UNIFIED IDEOGRAPH
    0x9382, 0x5510, //#CJK UNIFIED IDEOGRAPH
    0x9383, 0x5854, //#CJK UNIFIED IDEOGRAPH
    0x9384, 0x5858, //#CJK UNIFIED IDEOGRAPH
    0x9385, 0x5957, //#CJK UNIFIED IDEOGRAPH
    0x9386, 0x5B95, //#CJK UNIFIED IDEOGRAPH
    0x9387, 0x5CF6, //#CJK UNIFIED IDEOGRAPH
    0x9388, 0x5D8B, //#CJK UNIFIED IDEOGRAPH
    0x9389, 0x60BC, //#CJK UNIFIED IDEOGRAPH
    0x938A, 0x6295, //#CJK UNIFIED IDEOGRAPH
    0x938B, 0x642D, //#CJK UNIFIED IDEOGRAPH
    0x938C, 0x6771, //#CJK UNIFIED IDEOGRAPH
    0x938D, 0x6843, //#CJK UNIFIED IDEOGRAPH
    0x938E, 0x68BC, //#CJK UNIFIED IDEOGRAPH
    0x938F, 0x68DF, //#CJK UNIFIED IDEOGRAPH
    0x9390, 0x76D7, //#CJK UNIFIED IDEOGRAPH
    0x9391, 0x6DD8, //#CJK UNIFIED IDEOGRAPH
    0x9392, 0x6E6F, //#CJK UNIFIED IDEOGRAPH
    0x9393, 0x6D9B, //#CJK UNIFIED IDEOGRAPH
    0x9394, 0x706F, //#CJK UNIFIED IDEOGRAPH
    0x9395, 0x71C8, //#CJK UNIFIED IDEOGRAPH
    0x9396, 0x5F53, //#CJK UNIFIED IDEOGRAPH
    0x9397, 0x75D8, //#CJK UNIFIED IDEOGRAPH
    0x9398, 0x7977, //#CJK UNIFIED IDEOGRAPH
    0x9399, 0x7B49, //#CJK UNIFIED IDEOGRAPH
    0x939A, 0x7B54, //#CJK UNIFIED IDEOGRAPH
    0x939B, 0x7B52, //#CJK UNIFIED IDEOGRAPH
    0x939C, 0x7CD6, //#CJK UNIFIED IDEOGRAPH
    0x939D, 0x7D71, //#CJK UNIFIED IDEOGRAPH
    0x939E, 0x5230, //#CJK UNIFIED IDEOGRAPH
    0x939F, 0x8463, //#CJK UNIFIED IDEOGRAPH
    0x93A0, 0x8569, //#CJK UNIFIED IDEOGRAPH
    0x93A1, 0x85E4, //#CJK UNIFIED IDEOGRAPH
    0x93A2, 0x8A0E, //#CJK UNIFIED IDEOGRAPH
    0x93A3, 0x8B04, //#CJK UNIFIED IDEOGRAPH
    0x93A4, 0x8C46, //#CJK UNIFIED IDEOGRAPH
    0x93A5, 0x8E0F, //#CJK UNIFIED IDEOGRAPH
    0x93A6, 0x9003, //#CJK UNIFIED IDEOGRAPH
    0x93A7, 0x900F, //#CJK UNIFIED IDEOGRAPH
    0x93A8, 0x9419, //#CJK UNIFIED IDEOGRAPH
    0x93A9, 0x9676, //#CJK UNIFIED IDEOGRAPH
    0x93AA, 0x982D, //#CJK UNIFIED IDEOGRAPH
    0x93AB, 0x9A30, //#CJK UNIFIED IDEOGRAPH
    0x93AC, 0x95D8, //#CJK UNIFIED IDEOGRAPH
    0x93AD, 0x50CD, //#CJK UNIFIED IDEOGRAPH
    0x93AE, 0x52D5, //#CJK UNIFIED IDEOGRAPH
    0x93AF, 0x540C, //#CJK UNIFIED IDEOGRAPH
    0x93B0, 0x5802, //#CJK UNIFIED IDEOGRAPH
    0x93B1, 0x5C0E, //#CJK UNIFIED IDEOGRAPH
    0x93B2, 0x61A7, //#CJK UNIFIED IDEOGRAPH
    0x93B3, 0x649E, //#CJK UNIFIED IDEOGRAPH
    0x93B4, 0x6D1E, //#CJK UNIFIED IDEOGRAPH
    0x93B5, 0x77B3, //#CJK UNIFIED IDEOGRAPH
    0x93B6, 0x7AE5, //#CJK UNIFIED IDEOGRAPH
    0x93B7, 0x80F4, //#CJK UNIFIED IDEOGRAPH
    0x93B8, 0x8404, //#CJK UNIFIED IDEOGRAPH
    0x93B9, 0x9053, //#CJK UNIFIED IDEOGRAPH
    0x93BA, 0x9285, //#CJK UNIFIED IDEOGRAPH
    0x93BB, 0x5CE0, //#CJK UNIFIED IDEOGRAPH
    0x93BC, 0x9D07, //#CJK UNIFIED IDEOGRAPH
    0x93BD, 0x533F, //#CJK UNIFIED IDEOGRAPH
    0x93BE, 0x5F97, //#CJK UNIFIED IDEOGRAPH
    0x93BF, 0x5FB3, //#CJK UNIFIED IDEOGRAPH
    0x93C0, 0x6D9C, //#CJK UNIFIED IDEOGRAPH
    0x93C1, 0x7279, //#CJK UNIFIED IDEOGRAPH
    0x93C2, 0x7763, //#CJK UNIFIED IDEOGRAPH
    0x93C3, 0x79BF, //#CJK UNIFIED IDEOGRAPH
    0x93C4, 0x7BE4, //#CJK UNIFIED IDEOGRAPH
    0x93C5, 0x6BD2, //#CJK UNIFIED IDEOGRAPH
    0x93C6, 0x72EC, //#CJK UNIFIED IDEOGRAPH
    0x93C7, 0x8AAD, //#CJK UNIFIED IDEOGRAPH
    0x93C8, 0x6803, //#CJK UNIFIED IDEOGRAPH
    0x93C9, 0x6A61, //#CJK UNIFIED IDEOGRAPH
    0x93CA, 0x51F8, //#CJK UNIFIED IDEOGRAPH
    0x93CB, 0x7A81, //#CJK UNIFIED IDEOGRAPH
    0x93CC, 0x6934, //#CJK UNIFIED IDEOGRAPH
    0x93CD, 0x5C4A, //#CJK UNIFIED IDEOGRAPH
    0x93CE, 0x9CF6, //#CJK UNIFIED IDEOGRAPH
    0x93CF, 0x82EB, //#CJK UNIFIED IDEOGRAPH
    0x93D0, 0x5BC5, //#CJK UNIFIED IDEOGRAPH
    0x93D1, 0x9149, //#CJK UNIFIED IDEOGRAPH
    0x93D2, 0x701E, //#CJK UNIFIED IDEOGRAPH
    0x93D3, 0x5678, //#CJK UNIFIED IDEOGRAPH
    0x93D4, 0x5C6F, //#CJK UNIFIED IDEOGRAPH
    0x93D5, 0x60C7, //#CJK UNIFIED IDEOGRAPH
    0x93D6, 0x6566, //#CJK UNIFIED IDEOGRAPH
    0x93D7, 0x6C8C, //#CJK UNIFIED IDEOGRAPH
    0x93D8, 0x8C5A, //#CJK UNIFIED IDEOGRAPH
    0x93D9, 0x9041, //#CJK UNIFIED IDEOGRAPH
    0x93DA, 0x9813, //#CJK UNIFIED IDEOGRAPH
    0x93DB, 0x5451, //#CJK UNIFIED IDEOGRAPH
    0x93DC, 0x66C7, //#CJK UNIFIED IDEOGRAPH
    0x93DD, 0x920D, //#CJK UNIFIED IDEOGRAPH
    0x93DE, 0x5948, //#CJK UNIFIED IDEOGRAPH
    0x93DF, 0x90A3, //#CJK UNIFIED IDEOGRAPH
    0x93E0, 0x5185, //#CJK UNIFIED IDEOGRAPH
    0x93E1, 0x4E4D, //#CJK UNIFIED IDEOGRAPH
    0x93E2, 0x51EA, //#CJK UNIFIED IDEOGRAPH
    0x93E3, 0x8599, //#CJK UNIFIED IDEOGRAPH
    0x93E4, 0x8B0E, //#CJK UNIFIED IDEOGRAPH
    0x93E5, 0x7058, //#CJK UNIFIED IDEOGRAPH
    0x93E6, 0x637A, //#CJK UNIFIED IDEOGRAPH
    0x93E7, 0x934B, //#CJK UNIFIED IDEOGRAPH
    0x93E8, 0x6962, //#CJK UNIFIED IDEOGRAPH
    0x93E9, 0x99B4, //#CJK UNIFIED IDEOGRAPH
    0x93EA, 0x7E04, //#CJK UNIFIED IDEOGRAPH
    0x93EB, 0x7577, //#CJK UNIFIED IDEOGRAPH
    0x93EC, 0x5357, //#CJK UNIFIED IDEOGRAPH
    0x93ED, 0x6960, //#CJK UNIFIED IDEOGRAPH
    0x93EE, 0x8EDF, //#CJK UNIFIED IDEOGRAPH
    0x93EF, 0x96E3, //#CJK UNIFIED IDEOGRAPH
    0x93F0, 0x6C5D, //#CJK UNIFIED IDEOGRAPH
    0x93F1, 0x4E8C, //#CJK UNIFIED IDEOGRAPH
    0x93F2, 0x5C3C, //#CJK UNIFIED IDEOGRAPH
    0x93F3, 0x5F10, //#CJK UNIFIED IDEOGRAPH
    0x93F4, 0x8FE9, //#CJK UNIFIED IDEOGRAPH
    0x93F5, 0x5302, //#CJK UNIFIED IDEOGRAPH
    0x93F6, 0x8CD1, //#CJK UNIFIED IDEOGRAPH
    0x93F7, 0x8089, //#CJK UNIFIED IDEOGRAPH
    0x93F8, 0x8679, //#CJK UNIFIED IDEOGRAPH
    0x93F9, 0x5EFF, //#CJK UNIFIED IDEOGRAPH
    0x93FA, 0x65E5, //#CJK UNIFIED IDEOGRAPH
    0x93FB, 0x4E73, //#CJK UNIFIED IDEOGRAPH
    0x93FC, 0x5165, //#CJK UNIFIED IDEOGRAPH
    0x9440, 0x5982, //#CJK UNIFIED IDEOGRAPH
    0x9441, 0x5C3F, //#CJK UNIFIED IDEOGRAPH
    0x9442, 0x97EE, //#CJK UNIFIED IDEOGRAPH
    0x9443, 0x4EFB, //#CJK UNIFIED IDEOGRAPH
    0x9444, 0x598A, //#CJK UNIFIED IDEOGRAPH
    0x9445, 0x5FCD, //#CJK UNIFIED IDEOGRAPH
    0x9446, 0x8A8D, //#CJK UNIFIED IDEOGRAPH
    0x9447, 0x6FE1, //#CJK UNIFIED IDEOGRAPH
    0x9448, 0x79B0, //#CJK UNIFIED IDEOGRAPH
    0x9449, 0x7962, //#CJK UNIFIED IDEOGRAPH
    0x944A, 0x5BE7, //#CJK UNIFIED IDEOGRAPH
    0x944B, 0x8471, //#CJK UNIFIED IDEOGRAPH
    0x944C, 0x732B, //#CJK UNIFIED IDEOGRAPH
    0x944D, 0x71B1, //#CJK UNIFIED IDEOGRAPH
    0x944E, 0x5E74, //#CJK UNIFIED IDEOGRAPH
    0x944F, 0x5FF5, //#CJK UNIFIED IDEOGRAPH
    0x9450, 0x637B, //#CJK UNIFIED IDEOGRAPH
    0x9451, 0x649A, //#CJK UNIFIED IDEOGRAPH
    0x9452, 0x71C3, //#CJK UNIFIED IDEOGRAPH
    0x9453, 0x7C98, //#CJK UNIFIED IDEOGRAPH
    0x9454, 0x4E43, //#CJK UNIFIED IDEOGRAPH
    0x9455, 0x5EFC, //#CJK UNIFIED IDEOGRAPH
    0x9456, 0x4E4B, //#CJK UNIFIED IDEOGRAPH
    0x9457, 0x57DC, //#CJK UNIFIED IDEOGRAPH
    0x9458, 0x56A2, //#CJK UNIFIED IDEOGRAPH
    0x9459, 0x60A9, //#CJK UNIFIED IDEOGRAPH
    0x945A, 0x6FC3, //#CJK UNIFIED IDEOGRAPH
    0x945B, 0x7D0D, //#CJK UNIFIED IDEOGRAPH
    0x945C, 0x80FD, //#CJK UNIFIED IDEOGRAPH
    0x945D, 0x8133, //#CJK UNIFIED IDEOGRAPH
    0x945E, 0x81BF, //#CJK UNIFIED IDEOGRAPH
    0x945F, 0x8FB2, //#CJK UNIFIED IDEOGRAPH
    0x9460, 0x8997, //#CJK UNIFIED IDEOGRAPH
    0x9461, 0x86A4, //#CJK UNIFIED IDEOGRAPH
    0x9462, 0x5DF4, //#CJK UNIFIED IDEOGRAPH
    0x9463, 0x628A, //#CJK UNIFIED IDEOGRAPH
    0x9464, 0x64AD, //#CJK UNIFIED IDEOGRAPH
    0x9465, 0x8987, //#CJK UNIFIED IDEOGRAPH
    0x9466, 0x6777, //#CJK UNIFIED IDEOGRAPH
    0x9467, 0x6CE2, //#CJK UNIFIED IDEOGRAPH
    0x9468, 0x6D3E, //#CJK UNIFIED IDEOGRAPH
    0x9469, 0x7436, //#CJK UNIFIED IDEOGRAPH
    0x946A, 0x7834, //#CJK UNIFIED IDEOGRAPH
    0x946B, 0x5A46, //#CJK UNIFIED IDEOGRAPH
    0x946C, 0x7F75, //#CJK UNIFIED IDEOGRAPH
    0x946D, 0x82AD, //#CJK UNIFIED IDEOGRAPH
    0x946E, 0x99AC, //#CJK UNIFIED IDEOGRAPH
    0x946F, 0x4FF3, //#CJK UNIFIED IDEOGRAPH
    0x9470, 0x5EC3, //#CJK UNIFIED IDEOGRAPH
    0x9471, 0x62DD, //#CJK UNIFIED IDEOGRAPH
    0x9472, 0x6392, //#CJK UNIFIED IDEOGRAPH
    0x9473, 0x6557, //#CJK UNIFIED IDEOGRAPH
    0x9474, 0x676F, //#CJK UNIFIED IDEOGRAPH
    0x9475, 0x76C3, //#CJK UNIFIED IDEOGRAPH
    0x9476, 0x724C, //#CJK UNIFIED IDEOGRAPH
    0x9477, 0x80CC, //#CJK UNIFIED IDEOGRAPH
    0x9478, 0x80BA, //#CJK UNIFIED IDEOGRAPH
    0x9479, 0x8F29, //#CJK UNIFIED IDEOGRAPH
    0x947A, 0x914D, //#CJK UNIFIED IDEOGRAPH
    0x947B, 0x500D, //#CJK UNIFIED IDEOGRAPH
    0x947C, 0x57F9, //#CJK UNIFIED IDEOGRAPH
    0x947D, 0x5A92, //#CJK UNIFIED IDEOGRAPH
    0x947E, 0x6885, //#CJK UNIFIED IDEOGRAPH
    0x9480, 0x6973, //#CJK UNIFIED IDEOGRAPH
    0x9481, 0x7164, //#CJK UNIFIED IDEOGRAPH
    0x9482, 0x72FD, //#CJK UNIFIED IDEOGRAPH
    0x9483, 0x8CB7, //#CJK UNIFIED IDEOGRAPH
    0x9484, 0x58F2, //#CJK UNIFIED IDEOGRAPH
    0x9485, 0x8CE0, //#CJK UNIFIED IDEOGRAPH
    0x9486, 0x966A, //#CJK UNIFIED IDEOGRAPH
    0x9487, 0x9019, //#CJK UNIFIED IDEOGRAPH
    0x9488, 0x877F, //#CJK UNIFIED IDEOGRAPH
    0x9489, 0x79E4, //#CJK UNIFIED IDEOGRAPH
    0x948A, 0x77E7, //#CJK UNIFIED IDEOGRAPH
    0x948B, 0x8429, //#CJK UNIFIED IDEOGRAPH
    0x948C, 0x4F2F, //#CJK UNIFIED IDEOGRAPH
    0x948D, 0x5265, //#CJK UNIFIED IDEOGRAPH
    0x948E, 0x535A, //#CJK UNIFIED IDEOGRAPH
    0x948F, 0x62CD, //#CJK UNIFIED IDEOGRAPH
    0x9490, 0x67CF, //#CJK UNIFIED IDEOGRAPH
    0x9491, 0x6CCA, //#CJK UNIFIED IDEOGRAPH
    0x9492, 0x767D, //#CJK UNIFIED IDEOGRAPH
    0x9493, 0x7B94, //#CJK UNIFIED IDEOGRAPH
    0x9494, 0x7C95, //#CJK UNIFIED IDEOGRAPH
    0x9495, 0x8236, //#CJK UNIFIED IDEOGRAPH
    0x9496, 0x8584, //#CJK UNIFIED IDEOGRAPH
    0x9497, 0x8FEB, //#CJK UNIFIED IDEOGRAPH
    0x9498, 0x66DD, //#CJK UNIFIED IDEOGRAPH
    0x9499, 0x6F20, //#CJK UNIFIED IDEOGRAPH
    0x949A, 0x7206, //#CJK UNIFIED IDEOGRAPH
    0x949B, 0x7E1B, //#CJK UNIFIED IDEOGRAPH
    0x949C, 0x83AB, //#CJK UNIFIED IDEOGRAPH
    0x949D, 0x99C1, //#CJK UNIFIED IDEOGRAPH
    0x949E, 0x9EA6, //#CJK UNIFIED IDEOGRAPH
    0x949F, 0x51FD, //#CJK UNIFIED IDEOGRAPH
    0x94A0, 0x7BB1, //#CJK UNIFIED IDEOGRAPH
    0x94A1, 0x7872, //#CJK UNIFIED IDEOGRAPH
    0x94A2, 0x7BB8, //#CJK UNIFIED IDEOGRAPH
    0x94A3, 0x8087, //#CJK UNIFIED IDEOGRAPH
    0x94A4, 0x7B48, //#CJK UNIFIED IDEOGRAPH
    0x94A5, 0x6AE8, //#CJK UNIFIED IDEOGRAPH
    0x94A6, 0x5E61, //#CJK UNIFIED IDEOGRAPH
    0x94A7, 0x808C, //#CJK UNIFIED IDEOGRAPH
    0x94A8, 0x7551, //#CJK UNIFIED IDEOGRAPH
    0x94A9, 0x7560, //#CJK UNIFIED IDEOGRAPH
    0x94AA, 0x516B, //#CJK UNIFIED IDEOGRAPH
    0x94AB, 0x9262, //#CJK UNIFIED IDEOGRAPH
    0x94AC, 0x6E8C, //#CJK UNIFIED IDEOGRAPH
    0x94AD, 0x767A, //#CJK UNIFIED IDEOGRAPH
    0x94AE, 0x9197, //#CJK UNIFIED IDEOGRAPH
    0x94AF, 0x9AEA, //#CJK UNIFIED IDEOGRAPH
    0x94B0, 0x4F10, //#CJK UNIFIED IDEOGRAPH
    0x94B1, 0x7F70, //#CJK UNIFIED IDEOGRAPH
    0x94B2, 0x629C, //#CJK UNIFIED IDEOGRAPH
    0x94B3, 0x7B4F, //#CJK UNIFIED IDEOGRAPH
    0x94B4, 0x95A5, //#CJK UNIFIED IDEOGRAPH
    0x94B5, 0x9CE9, //#CJK UNIFIED IDEOGRAPH
    0x94B6, 0x567A, //#CJK UNIFIED IDEOGRAPH
    0x94B7, 0x5859, //#CJK UNIFIED IDEOGRAPH
    0x94B8, 0x86E4, //#CJK UNIFIED IDEOGRAPH
    0x94B9, 0x96BC, //#CJK UNIFIED IDEOGRAPH
    0x94BA, 0x4F34, //#CJK UNIFIED IDEOGRAPH
    0x94BB, 0x5224, //#CJK UNIFIED IDEOGRAPH
    0x94BC, 0x534A, //#CJK UNIFIED IDEOGRAPH
    0x94BD, 0x53CD, //#CJK UNIFIED IDEOGRAPH
    0x94BE, 0x53DB, //#CJK UNIFIED IDEOGRAPH
    0x94BF, 0x5E06, //#CJK UNIFIED IDEOGRAPH
    0x94C0, 0x642C, //#CJK UNIFIED IDEOGRAPH
    0x94C1, 0x6591, //#CJK UNIFIED IDEOGRAPH
    0x94C2, 0x677F, //#CJK UNIFIED IDEOGRAPH
    0x94C3, 0x6C3E, //#CJK UNIFIED IDEOGRAPH
    0x94C4, 0x6C4E, //#CJK UNIFIED IDEOGRAPH
    0x94C5, 0x7248, //#CJK UNIFIED IDEOGRAPH
    0x94C6, 0x72AF, //#CJK UNIFIED IDEOGRAPH
    0x94C7, 0x73ED, //#CJK UNIFIED IDEOGRAPH
    0x94C8, 0x7554, //#CJK UNIFIED IDEOGRAPH
    0x94C9, 0x7E41, //#CJK UNIFIED IDEOGRAPH
    0x94CA, 0x822C, //#CJK UNIFIED IDEOGRAPH
    0x94CB, 0x85E9, //#CJK UNIFIED IDEOGRAPH
    0x94CC, 0x8CA9, //#CJK UNIFIED IDEOGRAPH
    0x94CD, 0x7BC4, //#CJK UNIFIED IDEOGRAPH
    0x94CE, 0x91C6, //#CJK UNIFIED IDEOGRAPH
    0x94CF, 0x7169, //#CJK UNIFIED IDEOGRAPH
    0x94D0, 0x9812, //#CJK UNIFIED IDEOGRAPH
    0x94D1, 0x98EF, //#CJK UNIFIED IDEOGRAPH
    0x94D2, 0x633D, //#CJK UNIFIED IDEOGRAPH
    0x94D3, 0x6669, //#CJK UNIFIED IDEOGRAPH
    0x94D4, 0x756A, //#CJK UNIFIED IDEOGRAPH
    0x94D5, 0x76E4, //#CJK UNIFIED IDEOGRAPH
    0x94D6, 0x78D0, //#CJK UNIFIED IDEOGRAPH
    0x94D7, 0x8543, //#CJK UNIFIED IDEOGRAPH
    0x94D8, 0x86EE, //#CJK UNIFIED IDEOGRAPH
    0x94D9, 0x532A, //#CJK UNIFIED IDEOGRAPH
    0x94DA, 0x5351, //#CJK UNIFIED IDEOGRAPH
    0x94DB, 0x5426, //#CJK UNIFIED IDEOGRAPH
    0x94DC, 0x5983, //#CJK UNIFIED IDEOGRAPH
    0x94DD, 0x5E87, //#CJK UNIFIED IDEOGRAPH
    0x94DE, 0x5F7C, //#CJK UNIFIED IDEOGRAPH
    0x94DF, 0x60B2, //#CJK UNIFIED IDEOGRAPH
    0x94E0, 0x6249, //#CJK UNIFIED IDEOGRAPH
    0x94E1, 0x6279, //#CJK UNIFIED IDEOGRAPH
    0x94E2, 0x62AB, //#CJK UNIFIED IDEOGRAPH
    0x94E3, 0x6590, //#CJK UNIFIED IDEOGRAPH
    0x94E4, 0x6BD4, //#CJK UNIFIED IDEOGRAPH
    0x94E5, 0x6CCC, //#CJK UNIFIED IDEOGRAPH
    0x94E6, 0x75B2, //#CJK UNIFIED IDEOGRAPH
    0x94E7, 0x76AE, //#CJK UNIFIED IDEOGRAPH
    0x94E8, 0x7891, //#CJK UNIFIED IDEOGRAPH
    0x94E9, 0x79D8, //#CJK UNIFIED IDEOGRAPH
    0x94EA, 0x7DCB, //#CJK UNIFIED IDEOGRAPH
    0x94EB, 0x7F77, //#CJK UNIFIED IDEOGRAPH
    0x94EC, 0x80A5, //#CJK UNIFIED IDEOGRAPH
    0x94ED, 0x88AB, //#CJK UNIFIED IDEOGRAPH
    0x94EE, 0x8AB9, //#CJK UNIFIED IDEOGRAPH
    0x94EF, 0x8CBB, //#CJK UNIFIED IDEOGRAPH
    0x94F0, 0x907F, //#CJK UNIFIED IDEOGRAPH
    0x94F1, 0x975E, //#CJK UNIFIED IDEOGRAPH
    0x94F2, 0x98DB, //#CJK UNIFIED IDEOGRAPH
    0x94F3, 0x6A0B, //#CJK UNIFIED IDEOGRAPH
    0x94F4, 0x7C38, //#CJK UNIFIED IDEOGRAPH
    0x94F5, 0x5099, //#CJK UNIFIED IDEOGRAPH
    0x94F6, 0x5C3E, //#CJK UNIFIED IDEOGRAPH
    0x94F7, 0x5FAE, //#CJK UNIFIED IDEOGRAPH
    0x94F8, 0x6787, //#CJK UNIFIED IDEOGRAPH
    0x94F9, 0x6BD8, //#CJK UNIFIED IDEOGRAPH
    0x94FA, 0x7435, //#CJK UNIFIED IDEOGRAPH
    0x94FB, 0x7709, //#CJK UNIFIED IDEOGRAPH
    0x94FC, 0x7F8E, //#CJK UNIFIED IDEOGRAPH
    0x9540, 0x9F3B, //#CJK UNIFIED IDEOGRAPH
    0x9541, 0x67CA, //#CJK UNIFIED IDEOGRAPH
    0x9542, 0x7A17, //#CJK UNIFIED IDEOGRAPH
    0x9543, 0x5339, //#CJK UNIFIED IDEOGRAPH
    0x9544, 0x758B, //#CJK UNIFIED IDEOGRAPH
    0x9545, 0x9AED, //#CJK UNIFIED IDEOGRAPH
    0x9546, 0x5F66, //#CJK UNIFIED IDEOGRAPH
    0x9547, 0x819D, //#CJK UNIFIED IDEOGRAPH
    0x9548, 0x83F1, //#CJK UNIFIED IDEOGRAPH
    0x9549, 0x8098, //#CJK UNIFIED IDEOGRAPH
    0x954A, 0x5F3C, //#CJK UNIFIED IDEOGRAPH
    0x954B, 0x5FC5, //#CJK UNIFIED IDEOGRAPH
    0x954C, 0x7562, //#CJK UNIFIED IDEOGRAPH
    0x954D, 0x7B46, //#CJK UNIFIED IDEOGRAPH
    0x954E, 0x903C, //#CJK UNIFIED IDEOGRAPH
    0x954F, 0x6867, //#CJK UNIFIED IDEOGRAPH
    0x9550, 0x59EB, //#CJK UNIFIED IDEOGRAPH
    0x9551, 0x5A9B, //#CJK UNIFIED IDEOGRAPH
    0x9552, 0x7D10, //#CJK UNIFIED IDEOGRAPH
    0x9553, 0x767E, //#CJK UNIFIED IDEOGRAPH
    0x9554, 0x8B2C, //#CJK UNIFIED IDEOGRAPH
    0x9555, 0x4FF5, //#CJK UNIFIED IDEOGRAPH
    0x9556, 0x5F6A, //#CJK UNIFIED IDEOGRAPH
    0x9557, 0x6A19, //#CJK UNIFIED IDEOGRAPH
    0x9558, 0x6C37, //#CJK UNIFIED IDEOGRAPH
    0x9559, 0x6F02, //#CJK UNIFIED IDEOGRAPH
    0x955A, 0x74E2, //#CJK UNIFIED IDEOGRAPH
    0x955B, 0x7968, //#CJK UNIFIED IDEOGRAPH
    0x955C, 0x8868, //#CJK UNIFIED IDEOGRAPH
    0x955D, 0x8A55, //#CJK UNIFIED IDEOGRAPH
    0x955E, 0x8C79, //#CJK UNIFIED IDEOGRAPH
    0x955F, 0x5EDF, //#CJK UNIFIED IDEOGRAPH
    0x9560, 0x63CF, //#CJK UNIFIED IDEOGRAPH
    0x9561, 0x75C5, //#CJK UNIFIED IDEOGRAPH
    0x9562, 0x79D2, //#CJK UNIFIED IDEOGRAPH
    0x9563, 0x82D7, //#CJK UNIFIED IDEOGRAPH
    0x9564, 0x9328, //#CJK UNIFIED IDEOGRAPH
    0x9565, 0x92F2, //#CJK UNIFIED IDEOGRAPH
    0x9566, 0x849C, //#CJK UNIFIED IDEOGRAPH
    0x9567, 0x86ED, //#CJK UNIFIED IDEOGRAPH
    0x9568, 0x9C2D, //#CJK UNIFIED IDEOGRAPH
    0x9569, 0x54C1, //#CJK UNIFIED IDEOGRAPH
    0x956A, 0x5F6C, //#CJK UNIFIED IDEOGRAPH
    0x956B, 0x658C, //#CJK UNIFIED IDEOGRAPH
    0x956C, 0x6D5C, //#CJK UNIFIED IDEOGRAPH
    0x956D, 0x7015, //#CJK UNIFIED IDEOGRAPH
    0x956E, 0x8CA7, //#CJK UNIFIED IDEOGRAPH
    0x956F, 0x8CD3, //#CJK UNIFIED IDEOGRAPH
    0x9570, 0x983B, //#CJK UNIFIED IDEOGRAPH
    0x9571, 0x654F, //#CJK UNIFIED IDEOGRAPH
    0x9572, 0x74F6, //#CJK UNIFIED IDEOGRAPH
    0x9573, 0x4E0D, //#CJK UNIFIED IDEOGRAPH
    0x9574, 0x4ED8, //#CJK UNIFIED IDEOGRAPH
    0x9575, 0x57E0, //#CJK UNIFIED IDEOGRAPH
    0x9576, 0x592B, //#CJK UNIFIED IDEOGRAPH
    0x9577, 0x5A66, //#CJK UNIFIED IDEOGRAPH
    0x9578, 0x5BCC, //#CJK UNIFIED IDEOGRAPH
    0x9579, 0x51A8, //#CJK UNIFIED IDEOGRAPH
    0x957A, 0x5E03, //#CJK UNIFIED IDEOGRAPH
    0x957B, 0x5E9C, //#CJK UNIFIED IDEOGRAPH
    0x957C, 0x6016, //#CJK UNIFIED IDEOGRAPH
    0x957D, 0x6276, //#CJK UNIFIED IDEOGRAPH
    0x957E, 0x6577, //#CJK UNIFIED IDEOGRAPH
    0x9580, 0x65A7, //#CJK UNIFIED IDEOGRAPH
    0x9581, 0x666E, //#CJK UNIFIED IDEOGRAPH
    0x9582, 0x6D6E, //#CJK UNIFIED IDEOGRAPH
    0x9583, 0x7236, //#CJK UNIFIED IDEOGRAPH
    0x9584, 0x7B26, //#CJK UNIFIED IDEOGRAPH
    0x9585, 0x8150, //#CJK UNIFIED IDEOGRAPH
    0x9586, 0x819A, //#CJK UNIFIED IDEOGRAPH
    0x9587, 0x8299, //#CJK UNIFIED IDEOGRAPH
    0x9588, 0x8B5C, //#CJK UNIFIED IDEOGRAPH
    0x9589, 0x8CA0, //#CJK UNIFIED IDEOGRAPH
    0x958A, 0x8CE6, //#CJK UNIFIED IDEOGRAPH
    0x958B, 0x8D74, //#CJK UNIFIED IDEOGRAPH
    0x958C, 0x961C, //#CJK UNIFIED IDEOGRAPH
    0x958D, 0x9644, //#CJK UNIFIED IDEOGRAPH
    0x958E, 0x4FAE, //#CJK UNIFIED IDEOGRAPH
    0x958F, 0x64AB, //#CJK UNIFIED IDEOGRAPH
    0x9590, 0x6B66, //#CJK UNIFIED IDEOGRAPH
    0x9591, 0x821E, //#CJK UNIFIED IDEOGRAPH
    0x9592, 0x8461, //#CJK UNIFIED IDEOGRAPH
    0x9593, 0x856A, //#CJK UNIFIED IDEOGRAPH
    0x9594, 0x90E8, //#CJK UNIFIED IDEOGRAPH
    0x9595, 0x5C01, //#CJK UNIFIED IDEOGRAPH
    0x9596, 0x6953, //#CJK UNIFIED IDEOGRAPH
    0x9597, 0x98A8, //#CJK UNIFIED IDEOGRAPH
    0x9598, 0x847A, //#CJK UNIFIED IDEOGRAPH
    0x9599, 0x8557, //#CJK UNIFIED IDEOGRAPH
    0x959A, 0x4F0F, //#CJK UNIFIED IDEOGRAPH
    0x959B, 0x526F, //#CJK UNIFIED IDEOGRAPH
    0x959C, 0x5FA9, //#CJK UNIFIED IDEOGRAPH
    0x959D, 0x5E45, //#CJK UNIFIED IDEOGRAPH
    0x959E, 0x670D, //#CJK UNIFIED IDEOGRAPH
    0x959F, 0x798F, //#CJK UNIFIED IDEOGRAPH
    0x95A0, 0x8179, //#CJK UNIFIED IDEOGRAPH
    0x95A1, 0x8907, //#CJK UNIFIED IDEOGRAPH
    0x95A2, 0x8986, //#CJK UNIFIED IDEOGRAPH
    0x95A3, 0x6DF5, //#CJK UNIFIED IDEOGRAPH
    0x95A4, 0x5F17, //#CJK UNIFIED IDEOGRAPH
    0x95A5, 0x6255, //#CJK UNIFIED IDEOGRAPH
    0x95A6, 0x6CB8, //#CJK UNIFIED IDEOGRAPH
    0x95A7, 0x4ECF, //#CJK UNIFIED IDEOGRAPH
    0x95A8, 0x7269, //#CJK UNIFIED IDEOGRAPH
    0x95A9, 0x9B92, //#CJK UNIFIED IDEOGRAPH
    0x95AA, 0x5206, //#CJK UNIFIED IDEOGRAPH
    0x95AB, 0x543B, //#CJK UNIFIED IDEOGRAPH
    0x95AC, 0x5674, //#CJK UNIFIED IDEOGRAPH
    0x95AD, 0x58B3, //#CJK UNIFIED IDEOGRAPH
    0x95AE, 0x61A4, //#CJK UNIFIED IDEOGRAPH
    0x95AF, 0x626E, //#CJK UNIFIED IDEOGRAPH
    0x95B0, 0x711A, //#CJK UNIFIED IDEOGRAPH
    0x95B1, 0x596E, //#CJK UNIFIED IDEOGRAPH
    0x95B2, 0x7C89, //#CJK UNIFIED IDEOGRAPH
    0x95B3, 0x7CDE, //#CJK UNIFIED IDEOGRAPH
    0x95B4, 0x7D1B, //#CJK UNIFIED IDEOGRAPH
    0x95B5, 0x96F0, //#CJK UNIFIED IDEOGRAPH
    0x95B6, 0x6587, //#CJK UNIFIED IDEOGRAPH
    0x95B7, 0x805E, //#CJK UNIFIED IDEOGRAPH
    0x95B8, 0x4E19, //#CJK UNIFIED IDEOGRAPH
    0x95B9, 0x4F75, //#CJK UNIFIED IDEOGRAPH
    0x95BA, 0x5175, //#CJK UNIFIED IDEOGRAPH
    0x95BB, 0x5840, //#CJK UNIFIED IDEOGRAPH
    0x95BC, 0x5E63, //#CJK UNIFIED IDEOGRAPH
    0x95BD, 0x5E73, //#CJK UNIFIED IDEOGRAPH
    0x95BE, 0x5F0A, //#CJK UNIFIED IDEOGRAPH
    0x95BF, 0x67C4, //#CJK UNIFIED IDEOGRAPH
    0x95C0, 0x4E26, //#CJK UNIFIED IDEOGRAPH
    0x95C1, 0x853D, //#CJK UNIFIED IDEOGRAPH
    0x95C2, 0x9589, //#CJK UNIFIED IDEOGRAPH
    0x95C3, 0x965B, //#CJK UNIFIED IDEOGRAPH
    0x95C4, 0x7C73, //#CJK UNIFIED IDEOGRAPH
    0x95C5, 0x9801, //#CJK UNIFIED IDEOGRAPH
    0x95C6, 0x50FB, //#CJK UNIFIED IDEOGRAPH
    0x95C7, 0x58C1, //#CJK UNIFIED IDEOGRAPH
    0x95C8, 0x7656, //#CJK UNIFIED IDEOGRAPH
    0x95C9, 0x78A7, //#CJK UNIFIED IDEOGRAPH
    0x95CA, 0x5225, //#CJK UNIFIED IDEOGRAPH
    0x95CB, 0x77A5, //#CJK UNIFIED IDEOGRAPH
    0x95CC, 0x8511, //#CJK UNIFIED IDEOGRAPH
    0x95CD, 0x7B86, //#CJK UNIFIED IDEOGRAPH
    0x95CE, 0x504F, //#CJK UNIFIED IDEOGRAPH
    0x95CF, 0x5909, //#CJK UNIFIED IDEOGRAPH
    0x95D0, 0x7247, //#CJK UNIFIED IDEOGRAPH
    0x95D1, 0x7BC7, //#CJK UNIFIED IDEOGRAPH
    0x95D2, 0x7DE8, //#CJK UNIFIED IDEOGRAPH
    0x95D3, 0x8FBA, //#CJK UNIFIED IDEOGRAPH
    0x95D4, 0x8FD4, //#CJK UNIFIED IDEOGRAPH
    0x95D5, 0x904D, //#CJK UNIFIED IDEOGRAPH
    0x95D6, 0x4FBF, //#CJK UNIFIED IDEOGRAPH
    0x95D7, 0x52C9, //#CJK UNIFIED IDEOGRAPH
    0x95D8, 0x5A29, //#CJK UNIFIED IDEOGRAPH
    0x95D9, 0x5F01, //#CJK UNIFIED IDEOGRAPH
    0x95DA, 0x97AD, //#CJK UNIFIED IDEOGRAPH
    0x95DB, 0x4FDD, //#CJK UNIFIED IDEOGRAPH
    0x95DC, 0x8217, //#CJK UNIFIED IDEOGRAPH
    0x95DD, 0x92EA, //#CJK UNIFIED IDEOGRAPH
    0x95DE, 0x5703, //#CJK UNIFIED IDEOGRAPH
    0x95DF, 0x6355, //#CJK UNIFIED IDEOGRAPH
    0x95E0, 0x6B69, //#CJK UNIFIED IDEOGRAPH
    0x95E1, 0x752B, //#CJK UNIFIED IDEOGRAPH
    0x95E2, 0x88DC, //#CJK UNIFIED IDEOGRAPH
    0x95E3, 0x8F14, //#CJK UNIFIED IDEOGRAPH
    0x95E4, 0x7A42, //#CJK UNIFIED IDEOGRAPH
    0x95E5, 0x52DF, //#CJK UNIFIED IDEOGRAPH
    0x95E6, 0x5893, //#CJK UNIFIED IDEOGRAPH
    0x95E7, 0x6155, //#CJK UNIFIED IDEOGRAPH
    0x95E8, 0x620A, //#CJK UNIFIED IDEOGRAPH
    0x95E9, 0x66AE, //#CJK UNIFIED IDEOGRAPH
    0x95EA, 0x6BCD, //#CJK UNIFIED IDEOGRAPH
    0x95EB, 0x7C3F, //#CJK UNIFIED IDEOGRAPH
    0x95EC, 0x83E9, //#CJK UNIFIED IDEOGRAPH
    0x95ED, 0x5023, //#CJK UNIFIED IDEOGRAPH
    0x95EE, 0x4FF8, //#CJK UNIFIED IDEOGRAPH
    0x95EF, 0x5305, //#CJK UNIFIED IDEOGRAPH
    0x95F0, 0x5446, //#CJK UNIFIED IDEOGRAPH
    0x95F1, 0x5831, //#CJK UNIFIED IDEOGRAPH
    0x95F2, 0x5949, //#CJK UNIFIED IDEOGRAPH
    0x95F3, 0x5B9D, //#CJK UNIFIED IDEOGRAPH
    0x95F4, 0x5CF0, //#CJK UNIFIED IDEOGRAPH
    0x95F5, 0x5CEF, //#CJK UNIFIED IDEOGRAPH
    0x95F6, 0x5D29, //#CJK UNIFIED IDEOGRAPH
    0x95F7, 0x5E96, //#CJK UNIFIED IDEOGRAPH
    0x95F8, 0x62B1, //#CJK UNIFIED IDEOGRAPH
    0x95F9, 0x6367, //#CJK UNIFIED IDEOGRAPH
    0x95FA, 0x653E, //#CJK UNIFIED IDEOGRAPH
    0x95FB, 0x65B9, //#CJK UNIFIED IDEOGRAPH
    0x95FC, 0x670B, //#CJK UNIFIED IDEOGRAPH
    0x9640, 0x6CD5, //#CJK UNIFIED IDEOGRAPH
    0x9641, 0x6CE1, //#CJK UNIFIED IDEOGRAPH
    0x9642, 0x70F9, //#CJK UNIFIED IDEOGRAPH
    0x9643, 0x7832, //#CJK UNIFIED IDEOGRAPH
    0x9644, 0x7E2B, //#CJK UNIFIED IDEOGRAPH
    0x9645, 0x80DE, //#CJK UNIFIED IDEOGRAPH
    0x9646, 0x82B3, //#CJK UNIFIED IDEOGRAPH
    0x9647, 0x840C, //#CJK UNIFIED IDEOGRAPH
    0x9648, 0x84EC, //#CJK UNIFIED IDEOGRAPH
    0x9649, 0x8702, //#CJK UNIFIED IDEOGRAPH
    0x964A, 0x8912, //#CJK UNIFIED IDEOGRAPH
    0x964B, 0x8A2A, //#CJK UNIFIED IDEOGRAPH
    0x964C, 0x8C4A, //#CJK UNIFIED IDEOGRAPH
    0x964D, 0x90A6, //#CJK UNIFIED IDEOGRAPH
    0x964E, 0x92D2, //#CJK UNIFIED IDEOGRAPH
    0x964F, 0x98FD, //#CJK UNIFIED IDEOGRAPH
    0x9650, 0x9CF3, //#CJK UNIFIED IDEOGRAPH
    0x9651, 0x9D6C, //#CJK UNIFIED IDEOGRAPH
    0x9652, 0x4E4F, //#CJK UNIFIED IDEOGRAPH
    0x9653, 0x4EA1, //#CJK UNIFIED IDEOGRAPH
    0x9654, 0x508D, //#CJK UNIFIED IDEOGRAPH
    0x9655, 0x5256, //#CJK UNIFIED IDEOGRAPH
    0x9656, 0x574A, //#CJK UNIFIED IDEOGRAPH
    0x9657, 0x59A8, //#CJK UNIFIED IDEOGRAPH
    0x9658, 0x5E3D, //#CJK UNIFIED IDEOGRAPH
    0x9659, 0x5FD8, //#CJK UNIFIED IDEOGRAPH
    0x965A, 0x5FD9, //#CJK UNIFIED IDEOGRAPH
    0x965B, 0x623F, //#CJK UNIFIED IDEOGRAPH
    0x965C, 0x66B4, //#CJK UNIFIED IDEOGRAPH
    0x965D, 0x671B, //#CJK UNIFIED IDEOGRAPH
    0x965E, 0x67D0, //#CJK UNIFIED IDEOGRAPH
    0x965F, 0x68D2, //#CJK UNIFIED IDEOGRAPH
    0x9660, 0x5192, //#CJK UNIFIED IDEOGRAPH
    0x9661, 0x7D21, //#CJK UNIFIED IDEOGRAPH
    0x9662, 0x80AA, //#CJK UNIFIED IDEOGRAPH
    0x9663, 0x81A8, //#CJK UNIFIED IDEOGRAPH
    0x9664, 0x8B00, //#CJK UNIFIED IDEOGRAPH
    0x9665, 0x8C8C, //#CJK UNIFIED IDEOGRAPH
    0x9666, 0x8CBF, //#CJK UNIFIED IDEOGRAPH
    0x9667, 0x927E, //#CJK UNIFIED IDEOGRAPH
    0x9668, 0x9632, //#CJK UNIFIED IDEOGRAPH
    0x9669, 0x5420, //#CJK UNIFIED IDEOGRAPH
    0x966A, 0x982C, //#CJK UNIFIED IDEOGRAPH
    0x966B, 0x5317, //#CJK UNIFIED IDEOGRAPH
    0x966C, 0x50D5, //#CJK UNIFIED IDEOGRAPH
    0x966D, 0x535C, //#CJK UNIFIED IDEOGRAPH
    0x966E, 0x58A8, //#CJK UNIFIED IDEOGRAPH
    0x966F, 0x64B2, //#CJK UNIFIED IDEOGRAPH
    0x9670, 0x6734, //#CJK UNIFIED IDEOGRAPH
    0x9671, 0x7267, //#CJK UNIFIED IDEOGRAPH
    0x9672, 0x7766, //#CJK UNIFIED IDEOGRAPH
    0x9673, 0x7A46, //#CJK UNIFIED IDEOGRAPH
    0x9674, 0x91E6, //#CJK UNIFIED IDEOGRAPH
    0x9675, 0x52C3, //#CJK UNIFIED IDEOGRAPH
    0x9676, 0x6CA1, //#CJK UNIFIED IDEOGRAPH
    0x9677, 0x6B86, //#CJK UNIFIED IDEOGRAPH
    0x9678, 0x5800, //#CJK UNIFIED IDEOGRAPH
    0x9679, 0x5E4C, //#CJK UNIFIED IDEOGRAPH
    0x967A, 0x5954, //#CJK UNIFIED IDEOGRAPH
    0x967B, 0x672C, //#CJK UNIFIED IDEOGRAPH
    0x967C, 0x7FFB, //#CJK UNIFIED IDEOGRAPH
    0x967D, 0x51E1, //#CJK UNIFIED IDEOGRAPH
    0x967E, 0x76C6, //#CJK UNIFIED IDEOGRAPH
    0x9680, 0x6469, //#CJK UNIFIED IDEOGRAPH
    0x9681, 0x78E8, //#CJK UNIFIED IDEOGRAPH
    0x9682, 0x9B54, //#CJK UNIFIED IDEOGRAPH
    0x9683, 0x9EBB, //#CJK UNIFIED IDEOGRAPH
    0x9684, 0x57CB, //#CJK UNIFIED IDEOGRAPH
    0x9685, 0x59B9, //#CJK UNIFIED IDEOGRAPH
    0x9686, 0x6627, //#CJK UNIFIED IDEOGRAPH
    0x9687, 0x679A, //#CJK UNIFIED IDEOGRAPH
    0x9688, 0x6BCE, //#CJK UNIFIED IDEOGRAPH
    0x9689, 0x54E9, //#CJK UNIFIED IDEOGRAPH
    0x968A, 0x69D9, //#CJK UNIFIED IDEOGRAPH
    0x968B, 0x5E55, //#CJK UNIFIED IDEOGRAPH
    0x968C, 0x819C, //#CJK UNIFIED IDEOGRAPH
    0x968D, 0x6795, //#CJK UNIFIED IDEOGRAPH
    0x968E, 0x9BAA, //#CJK UNIFIED IDEOGRAPH
    0x968F, 0x67FE, //#CJK UNIFIED IDEOGRAPH
    0x9690, 0x9C52, //#CJK UNIFIED IDEOGRAPH
    0x9691, 0x685D, //#CJK UNIFIED IDEOGRAPH
    0x9692, 0x4EA6, //#CJK UNIFIED IDEOGRAPH
    0x9693, 0x4FE3, //#CJK UNIFIED IDEOGRAPH
    0x9694, 0x53C8, //#CJK UNIFIED IDEOGRAPH
    0x9695, 0x62B9, //#CJK UNIFIED IDEOGRAPH
    0x9696, 0x672B, //#CJK UNIFIED IDEOGRAPH
    0x9697, 0x6CAB, //#CJK UNIFIED IDEOGRAPH
    0x9698, 0x8FC4, //#CJK UNIFIED IDEOGRAPH
    0x9699, 0x4FAD, //#CJK UNIFIED IDEOGRAPH
    0x969A, 0x7E6D, //#CJK UNIFIED IDEOGRAPH
    0x969B, 0x9EBF, //#CJK UNIFIED IDEOGRAPH
    0x969C, 0x4E07, //#CJK UNIFIED IDEOGRAPH
    0x969D, 0x6162, //#CJK UNIFIED IDEOGRAPH
    0x969E, 0x6E80, //#CJK UNIFIED IDEOGRAPH
    0x969F, 0x6F2B, //#CJK UNIFIED IDEOGRAPH
    0x96A0, 0x8513, //#CJK UNIFIED IDEOGRAPH
    0x96A1, 0x5473, //#CJK UNIFIED IDEOGRAPH
    0x96A2, 0x672A, //#CJK UNIFIED IDEOGRAPH
    0x96A3, 0x9B45, //#CJK UNIFIED IDEOGRAPH
    0x96A4, 0x5DF3, //#CJK UNIFIED IDEOGRAPH
    0x96A5, 0x7B95, //#CJK UNIFIED IDEOGRAPH
    0x96A6, 0x5CAC, //#CJK UNIFIED IDEOGRAPH
    0x96A7, 0x5BC6, //#CJK UNIFIED IDEOGRAPH
    0x96A8, 0x871C, //#CJK UNIFIED IDEOGRAPH
    0x96A9, 0x6E4A, //#CJK UNIFIED IDEOGRAPH
    0x96AA, 0x84D1, //#CJK UNIFIED IDEOGRAPH
    0x96AB, 0x7A14, //#CJK UNIFIED IDEOGRAPH
    0x96AC, 0x8108, //#CJK UNIFIED IDEOGRAPH
    0x96AD, 0x5999, //#CJK UNIFIED IDEOGRAPH
    0x96AE, 0x7C8D, //#CJK UNIFIED IDEOGRAPH
    0x96AF, 0x6C11, //#CJK UNIFIED IDEOGRAPH
    0x96B0, 0x7720, //#CJK UNIFIED IDEOGRAPH
    0x96B1, 0x52D9, //#CJK UNIFIED IDEOGRAPH
    0x96B2, 0x5922, //#CJK UNIFIED IDEOGRAPH
    0x96B3, 0x7121, //#CJK UNIFIED IDEOGRAPH
    0x96B4, 0x725F, //#CJK UNIFIED IDEOGRAPH
    0x96B5, 0x77DB, //#CJK UNIFIED IDEOGRAPH
    0x96B6, 0x9727, //#CJK UNIFIED IDEOGRAPH
    0x96B7, 0x9D61, //#CJK UNIFIED IDEOGRAPH
    0x96B8, 0x690B, //#CJK UNIFIED IDEOGRAPH
    0x96B9, 0x5A7F, //#CJK UNIFIED IDEOGRAPH
    0x96BA, 0x5A18, //#CJK UNIFIED IDEOGRAPH
    0x96BB, 0x51A5, //#CJK UNIFIED IDEOGRAPH
    0x96BC, 0x540D, //#CJK UNIFIED IDEOGRAPH
    0x96BD, 0x547D, //#CJK UNIFIED IDEOGRAPH
    0x96BE, 0x660E, //#CJK UNIFIED IDEOGRAPH
    0x96BF, 0x76DF, //#CJK UNIFIED IDEOGRAPH
    0x96C0, 0x8FF7, //#CJK UNIFIED IDEOGRAPH
    0x96C1, 0x9298, //#CJK UNIFIED IDEOGRAPH
    0x96C2, 0x9CF4, //#CJK UNIFIED IDEOGRAPH
    0x96C3, 0x59EA, //#CJK UNIFIED IDEOGRAPH
    0x96C4, 0x725D, //#CJK UNIFIED IDEOGRAPH
    0x96C5, 0x6EC5, //#CJK UNIFIED IDEOGRAPH
    0x96C6, 0x514D, //#CJK UNIFIED IDEOGRAPH
    0x96C7, 0x68C9, //#CJK UNIFIED IDEOGRAPH
    0x96C8, 0x7DBF, //#CJK UNIFIED IDEOGRAPH
    0x96C9, 0x7DEC, //#CJK UNIFIED IDEOGRAPH
    0x96CA, 0x9762, //#CJK UNIFIED IDEOGRAPH
    0x96CB, 0x9EBA, //#CJK UNIFIED IDEOGRAPH
    0x96CC, 0x6478, //#CJK UNIFIED IDEOGRAPH
    0x96CD, 0x6A21, //#CJK UNIFIED IDEOGRAPH
    0x96CE, 0x8302, //#CJK UNIFIED IDEOGRAPH
    0x96CF, 0x5984, //#CJK UNIFIED IDEOGRAPH
    0x96D0, 0x5B5F, //#CJK UNIFIED IDEOGRAPH
    0x96D1, 0x6BDB, //#CJK UNIFIED IDEOGRAPH
    0x96D2, 0x731B, //#CJK UNIFIED IDEOGRAPH
    0x96D3, 0x76F2, //#CJK UNIFIED IDEOGRAPH
    0x96D4, 0x7DB2, //#CJK UNIFIED IDEOGRAPH
    0x96D5, 0x8017, //#CJK UNIFIED IDEOGRAPH
    0x96D6, 0x8499, //#CJK UNIFIED IDEOGRAPH
    0x96D7, 0x5132, //#CJK UNIFIED IDEOGRAPH
    0x96D8, 0x6728, //#CJK UNIFIED IDEOGRAPH
    0x96D9, 0x9ED9, //#CJK UNIFIED IDEOGRAPH
    0x96DA, 0x76EE, //#CJK UNIFIED IDEOGRAPH
    0x96DB, 0x6762, //#CJK UNIFIED IDEOGRAPH
    0x96DC, 0x52FF, //#CJK UNIFIED IDEOGRAPH
    0x96DD, 0x9905, //#CJK UNIFIED IDEOGRAPH
    0x96DE, 0x5C24, //#CJK UNIFIED IDEOGRAPH
    0x96DF, 0x623B, //#CJK UNIFIED IDEOGRAPH
    0x96E0, 0x7C7E, //#CJK UNIFIED IDEOGRAPH
    0x96E1, 0x8CB0, //#CJK UNIFIED IDEOGRAPH
    0x96E2, 0x554F, //#CJK UNIFIED IDEOGRAPH
    0x96E3, 0x60B6, //#CJK UNIFIED IDEOGRAPH
    0x96E4, 0x7D0B, //#CJK UNIFIED IDEOGRAPH
    0x96E5, 0x9580, //#CJK UNIFIED IDEOGRAPH
    0x96E6, 0x5301, //#CJK UNIFIED IDEOGRAPH
    0x96E7, 0x4E5F, //#CJK UNIFIED IDEOGRAPH
    0x96E8, 0x51B6, //#CJK UNIFIED IDEOGRAPH
    0x96E9, 0x591C, //#CJK UNIFIED IDEOGRAPH
    0x96EA, 0x723A, //#CJK UNIFIED IDEOGRAPH
    0x96EB, 0x8036, //#CJK UNIFIED IDEOGRAPH
    0x96EC, 0x91CE, //#CJK UNIFIED IDEOGRAPH
    0x96ED, 0x5F25, //#CJK UNIFIED IDEOGRAPH
    0x96EE, 0x77E2, //#CJK UNIFIED IDEOGRAPH
    0x96EF, 0x5384, //#CJK UNIFIED IDEOGRAPH
    0x96F0, 0x5F79, //#CJK UNIFIED IDEOGRAPH
    0x96F1, 0x7D04, //#CJK UNIFIED IDEOGRAPH
    0x96F2, 0x85AC, //#CJK UNIFIED IDEOGRAPH
    0x96F3, 0x8A33, //#CJK UNIFIED IDEOGRAPH
    0x96F4, 0x8E8D, //#CJK UNIFIED IDEOGRAPH
    0x96F5, 0x9756, //#CJK UNIFIED IDEOGRAPH
    0x96F6, 0x67F3, //#CJK UNIFIED IDEOGRAPH
    0x96F7, 0x85AE, //#CJK UNIFIED IDEOGRAPH
    0x96F8, 0x9453, //#CJK UNIFIED IDEOGRAPH
    0x96F9, 0x6109, //#CJK UNIFIED IDEOGRAPH
    0x96FA, 0x6108, //#CJK UNIFIED IDEOGRAPH
    0x96FB, 0x6CB9, //#CJK UNIFIED IDEOGRAPH
    0x96FC, 0x7652, //#CJK UNIFIED IDEOGRAPH
    0x9740, 0x8AED, //#CJK UNIFIED IDEOGRAPH
    0x9741, 0x8F38, //#CJK UNIFIED IDEOGRAPH
    0x9742, 0x552F, //#CJK UNIFIED IDEOGRAPH
    0x9743, 0x4F51, //#CJK UNIFIED IDEOGRAPH
    0x9744, 0x512A, //#CJK UNIFIED IDEOGRAPH
    0x9745, 0x52C7, //#CJK UNIFIED IDEOGRAPH
    0x9746, 0x53CB, //#CJK UNIFIED IDEOGRAPH
    0x9747, 0x5BA5, //#CJK UNIFIED IDEOGRAPH
    0x9748, 0x5E7D, //#CJK UNIFIED IDEOGRAPH
    0x9749, 0x60A0, //#CJK UNIFIED IDEOGRAPH
    0x974A, 0x6182, //#CJK UNIFIED IDEOGRAPH
    0x974B, 0x63D6, //#CJK UNIFIED IDEOGRAPH
    0x974C, 0x6709, //#CJK UNIFIED IDEOGRAPH
    0x974D, 0x67DA, //#CJK UNIFIED IDEOGRAPH
    0x974E, 0x6E67, //#CJK UNIFIED IDEOGRAPH
    0x974F, 0x6D8C, //#CJK UNIFIED IDEOGRAPH
    0x9750, 0x7336, //#CJK UNIFIED IDEOGRAPH
    0x9751, 0x7337, //#CJK UNIFIED IDEOGRAPH
    0x9752, 0x7531, //#CJK UNIFIED IDEOGRAPH
    0x9753, 0x7950, //#CJK UNIFIED IDEOGRAPH
    0x9754, 0x88D5, //#CJK UNIFIED IDEOGRAPH
    0x9755, 0x8A98, //#CJK UNIFIED IDEOGRAPH
    0x9756, 0x904A, //#CJK UNIFIED IDEOGRAPH
    0x9757, 0x9091, //#CJK UNIFIED IDEOGRAPH
    0x9758, 0x90F5, //#CJK UNIFIED IDEOGRAPH
    0x9759, 0x96C4, //#CJK UNIFIED IDEOGRAPH
    0x975A, 0x878D, //#CJK UNIFIED IDEOGRAPH
    0x975B, 0x5915, //#CJK UNIFIED IDEOGRAPH
    0x975C, 0x4E88, //#CJK UNIFIED IDEOGRAPH
    0x975D, 0x4F59, //#CJK UNIFIED IDEOGRAPH
    0x975E, 0x4E0E, //#CJK UNIFIED IDEOGRAPH
    0x975F, 0x8A89, //#CJK UNIFIED IDEOGRAPH
    0x9760, 0x8F3F, //#CJK UNIFIED IDEOGRAPH
    0x9761, 0x9810, //#CJK UNIFIED IDEOGRAPH
    0x9762, 0x50AD, //#CJK UNIFIED IDEOGRAPH
    0x9763, 0x5E7C, //#CJK UNIFIED IDEOGRAPH
    0x9764, 0x5996, //#CJK UNIFIED IDEOGRAPH
    0x9765, 0x5BB9, //#CJK UNIFIED IDEOGRAPH
    0x9766, 0x5EB8, //#CJK UNIFIED IDEOGRAPH
    0x9767, 0x63DA, //#CJK UNIFIED IDEOGRAPH
    0x9768, 0x63FA, //#CJK UNIFIED IDEOGRAPH
    0x9769, 0x64C1, //#CJK UNIFIED IDEOGRAPH
    0x976A, 0x66DC, //#CJK UNIFIED IDEOGRAPH
    0x976B, 0x694A, //#CJK UNIFIED IDEOGRAPH
    0x976C, 0x69D8, //#CJK UNIFIED IDEOGRAPH
    0x976D, 0x6D0B, //#CJK UNIFIED IDEOGRAPH
    0x976E, 0x6EB6, //#CJK UNIFIED IDEOGRAPH
    0x976F, 0x7194, //#CJK UNIFIED IDEOGRAPH
    0x9770, 0x7528, //#CJK UNIFIED IDEOGRAPH
    0x9771, 0x7AAF, //#CJK UNIFIED IDEOGRAPH
    0x9772, 0x7F8A, //#CJK UNIFIED IDEOGRAPH
    0x9773, 0x8000, //#CJK UNIFIED IDEOGRAPH
    0x9774, 0x8449, //#CJK UNIFIED IDEOGRAPH
    0x9775, 0x84C9, //#CJK UNIFIED IDEOGRAPH
    0x9776, 0x8981, //#CJK UNIFIED IDEOGRAPH
    0x9777, 0x8B21, //#CJK UNIFIED IDEOGRAPH
    0x9778, 0x8E0A, //#CJK UNIFIED IDEOGRAPH
    0x9779, 0x9065, //#CJK UNIFIED IDEOGRAPH
    0x977A, 0x967D, //#CJK UNIFIED IDEOGRAPH
    0x977B, 0x990A, //#CJK UNIFIED IDEOGRAPH
    0x977C, 0x617E, //#CJK UNIFIED IDEOGRAPH
    0x977D, 0x6291, //#CJK UNIFIED IDEOGRAPH
    0x977E, 0x6B32, //#CJK UNIFIED IDEOGRAPH
    0x9780, 0x6C83, //#CJK UNIFIED IDEOGRAPH
    0x9781, 0x6D74, //#CJK UNIFIED IDEOGRAPH
    0x9782, 0x7FCC, //#CJK UNIFIED IDEOGRAPH
    0x9783, 0x7FFC, //#CJK UNIFIED IDEOGRAPH
    0x9784, 0x6DC0, //#CJK UNIFIED IDEOGRAPH
    0x9785, 0x7F85, //#CJK UNIFIED IDEOGRAPH
    0x9786, 0x87BA, //#CJK UNIFIED IDEOGRAPH
    0x9787, 0x88F8, //#CJK UNIFIED IDEOGRAPH
    0x9788, 0x6765, //#CJK UNIFIED IDEOGRAPH
    0x9789, 0x83B1, //#CJK UNIFIED IDEOGRAPH
    0x978A, 0x983C, //#CJK UNIFIED IDEOGRAPH
    0x978B, 0x96F7, //#CJK UNIFIED IDEOGRAPH
    0x978C, 0x6D1B, //#CJK UNIFIED IDEOGRAPH
    0x978D, 0x7D61, //#CJK UNIFIED IDEOGRAPH
    0x978E, 0x843D, //#CJK UNIFIED IDEOGRAPH
    0x978F, 0x916A, //#CJK UNIFIED IDEOGRAPH
    0x9790, 0x4E71, //#CJK UNIFIED IDEOGRAPH
    0x9791, 0x5375, //#CJK UNIFIED IDEOGRAPH
    0x9792, 0x5D50, //#CJK UNIFIED IDEOGRAPH
    0x9793, 0x6B04, //#CJK UNIFIED IDEOGRAPH
    0x9794, 0x6FEB, //#CJK UNIFIED IDEOGRAPH
    0x9795, 0x85CD, //#CJK UNIFIED IDEOGRAPH
    0x9796, 0x862D, //#CJK UNIFIED IDEOGRAPH
    0x9797, 0x89A7, //#CJK UNIFIED IDEOGRAPH
    0x9798, 0x5229, //#CJK UNIFIED IDEOGRAPH
    0x9799, 0x540F, //#CJK UNIFIED IDEOGRAPH
    0x979A, 0x5C65, //#CJK UNIFIED IDEOGRAPH
    0x979B, 0x674E, //#CJK UNIFIED IDEOGRAPH
    0x979C, 0x68A8, //#CJK UNIFIED IDEOGRAPH
    0x979D, 0x7406, //#CJK UNIFIED IDEOGRAPH
    0x979E, 0x7483, //#CJK UNIFIED IDEOGRAPH
    0x979F, 0x75E2, //#CJK UNIFIED IDEOGRAPH
    0x97A0, 0x88CF, //#CJK UNIFIED IDEOGRAPH
    0x97A1, 0x88E1, //#CJK UNIFIED IDEOGRAPH
    0x97A2, 0x91CC, //#CJK UNIFIED IDEOGRAPH
    0x97A3, 0x96E2, //#CJK UNIFIED IDEOGRAPH
    0x97A4, 0x9678, //#CJK UNIFIED IDEOGRAPH
    0x97A5, 0x5F8B, //#CJK UNIFIED IDEOGRAPH
    0x97A6, 0x7387, //#CJK UNIFIED IDEOGRAPH
    0x97A7, 0x7ACB, //#CJK UNIFIED IDEOGRAPH
    0x97A8, 0x844E, //#CJK UNIFIED IDEOGRAPH
    0x97A9, 0x63A0, //#CJK UNIFIED IDEOGRAPH
    0x97AA, 0x7565, //#CJK UNIFIED IDEOGRAPH
    0x97AB, 0x5289, //#CJK UNIFIED IDEOGRAPH
    0x97AC, 0x6D41, //#CJK UNIFIED IDEOGRAPH
    0x97AD, 0x6E9C, //#CJK UNIFIED IDEOGRAPH
    0x97AE, 0x7409, //#CJK UNIFIED IDEOGRAPH
    0x97AF, 0x7559, //#CJK UNIFIED IDEOGRAPH
    0x97B0, 0x786B, //#CJK UNIFIED IDEOGRAPH
    0x97B1, 0x7C92, //#CJK UNIFIED IDEOGRAPH
    0x97B2, 0x9686, //#CJK UNIFIED IDEOGRAPH
    0x97B3, 0x7ADC, //#CJK UNIFIED IDEOGRAPH
    0x97B4, 0x9F8D, //#CJK UNIFIED IDEOGRAPH
    0x97B5, 0x4FB6, //#CJK UNIFIED IDEOGRAPH
    0x97B6, 0x616E, //#CJK UNIFIED IDEOGRAPH
    0x97B7, 0x65C5, //#CJK UNIFIED IDEOGRAPH
    0x97B8, 0x865C, //#CJK UNIFIED IDEOGRAPH
    0x97B9, 0x4E86, //#CJK UNIFIED IDEOGRAPH
    0x97BA, 0x4EAE, //#CJK UNIFIED IDEOGRAPH
    0x97BB, 0x50DA, //#CJK UNIFIED IDEOGRAPH
    0x97BC, 0x4E21, //#CJK UNIFIED IDEOGRAPH
    0x97BD, 0x51CC, //#CJK UNIFIED IDEOGRAPH
    0x97BE, 0x5BEE, //#CJK UNIFIED IDEOGRAPH
    0x97BF, 0x6599, //#CJK UNIFIED IDEOGRAPH
    0x97C0, 0x6881, //#CJK UNIFIED IDEOGRAPH
    0x97C1, 0x6DBC, //#CJK UNIFIED IDEOGRAPH
    0x97C2, 0x731F, //#CJK UNIFIED IDEOGRAPH
    0x97C3, 0x7642, //#CJK UNIFIED IDEOGRAPH
    0x97C4, 0x77AD, //#CJK UNIFIED IDEOGRAPH
    0x97C5, 0x7A1C, //#CJK UNIFIED IDEOGRAPH
    0x97C6, 0x7CE7, //#CJK UNIFIED IDEOGRAPH
    0x97C7, 0x826F, //#CJK UNIFIED IDEOGRAPH
    0x97C8, 0x8AD2, //#CJK UNIFIED IDEOGRAPH
    0x97C9, 0x907C, //#CJK UNIFIED IDEOGRAPH
    0x97CA, 0x91CF, //#CJK UNIFIED IDEOGRAPH
    0x97CB, 0x9675, //#CJK UNIFIED IDEOGRAPH
    0x97CC, 0x9818, //#CJK UNIFIED IDEOGRAPH
    0x97CD, 0x529B, //#CJK UNIFIED IDEOGRAPH
    0x97CE, 0x7DD1, //#CJK UNIFIED IDEOGRAPH
    0x97CF, 0x502B, //#CJK UNIFIED IDEOGRAPH
    0x97D0, 0x5398, //#CJK UNIFIED IDEOGRAPH
    0x97D1, 0x6797, //#CJK UNIFIED IDEOGRAPH
    0x97D2, 0x6DCB, //#CJK UNIFIED IDEOGRAPH
    0x97D3, 0x71D0, //#CJK UNIFIED IDEOGRAPH
    0x97D4, 0x7433, //#CJK UNIFIED IDEOGRAPH
    0x97D5, 0x81E8, //#CJK UNIFIED IDEOGRAPH
    0x97D6, 0x8F2A, //#CJK UNIFIED IDEOGRAPH
    0x97D7, 0x96A3, //#CJK UNIFIED IDEOGRAPH
    0x97D8, 0x9C57, //#CJK UNIFIED IDEOGRAPH
    0x97D9, 0x9E9F, //#CJK UNIFIED IDEOGRAPH
    0x97DA, 0x7460, //#CJK UNIFIED IDEOGRAPH
    0x97DB, 0x5841, //#CJK UNIFIED IDEOGRAPH
    0x97DC, 0x6D99, //#CJK UNIFIED IDEOGRAPH
    0x97DD, 0x7D2F, //#CJK UNIFIED IDEOGRAPH
    0x97DE, 0x985E, //#CJK UNIFIED IDEOGRAPH
    0x97DF, 0x4EE4, //#CJK UNIFIED IDEOGRAPH
    0x97E0, 0x4F36, //#CJK UNIFIED IDEOGRAPH
    0x97E1, 0x4F8B, //#CJK UNIFIED IDEOGRAPH
    0x97E2, 0x51B7, //#CJK UNIFIED IDEOGRAPH
    0x97E3, 0x52B1, //#CJK UNIFIED IDEOGRAPH
    0x97E4, 0x5DBA, //#CJK UNIFIED IDEOGRAPH
    0x97E5, 0x601C, //#CJK UNIFIED IDEOGRAPH
    0x97E6, 0x73B2, //#CJK UNIFIED IDEOGRAPH
    0x97E7, 0x793C, //#CJK UNIFIED IDEOGRAPH
    0x97E8, 0x82D3, //#CJK UNIFIED IDEOGRAPH
    0x97E9, 0x9234, //#CJK UNIFIED IDEOGRAPH
    0x97EA, 0x96B7, //#CJK UNIFIED IDEOGRAPH
    0x97EB, 0x96F6, //#CJK UNIFIED IDEOGRAPH
    0x97EC, 0x970A, //#CJK UNIFIED IDEOGRAPH
    0x97ED, 0x9E97, //#CJK UNIFIED IDEOGRAPH
    0x97EE, 0x9F62, //#CJK UNIFIED IDEOGRAPH
    0x97EF, 0x66A6, //#CJK UNIFIED IDEOGRAPH
    0x97F0, 0x6B74, //#CJK UNIFIED IDEOGRAPH
    0x97F1, 0x5217, //#CJK UNIFIED IDEOGRAPH
    0x97F2, 0x52A3, //#CJK UNIFIED IDEOGRAPH
    0x97F3, 0x70C8, //#CJK UNIFIED IDEOGRAPH
    0x97F4, 0x88C2, //#CJK UNIFIED IDEOGRAPH
    0x97F5, 0x5EC9, //#CJK UNIFIED IDEOGRAPH
    0x97F6, 0x604B, //#CJK UNIFIED IDEOGRAPH
    0x97F7, 0x6190, //#CJK UNIFIED IDEOGRAPH
    0x97F8, 0x6F23, //#CJK UNIFIED IDEOGRAPH
    0x97F9, 0x7149, //#CJK UNIFIED IDEOGRAPH
    0x97FA, 0x7C3E, //#CJK UNIFIED IDEOGRAPH
    0x97FB, 0x7DF4, //#CJK UNIFIED IDEOGRAPH
    0x97FC, 0x806F, //#CJK UNIFIED IDEOGRAPH
    0x9840, 0x84EE, //#CJK UNIFIED IDEOGRAPH
    0x9841, 0x9023, //#CJK UNIFIED IDEOGRAPH
    0x9842, 0x932C, //#CJK UNIFIED IDEOGRAPH
    0x9843, 0x5442, //#CJK UNIFIED IDEOGRAPH
    0x9844, 0x9B6F, //#CJK UNIFIED IDEOGRAPH
    0x9845, 0x6AD3, //#CJK UNIFIED IDEOGRAPH
    0x9846, 0x7089, //#CJK UNIFIED IDEOGRAPH
    0x9847, 0x8CC2, //#CJK UNIFIED IDEOGRAPH
    0x9848, 0x8DEF, //#CJK UNIFIED IDEOGRAPH
    0x9849, 0x9732, //#CJK UNIFIED IDEOGRAPH
    0x984A, 0x52B4, //#CJK UNIFIED IDEOGRAPH
    0x984B, 0x5A41, //#CJK UNIFIED IDEOGRAPH
    0x984C, 0x5ECA, //#CJK UNIFIED IDEOGRAPH
    0x984D, 0x5F04, //#CJK UNIFIED IDEOGRAPH
    0x984E, 0x6717, //#CJK UNIFIED IDEOGRAPH
    0x984F, 0x697C, //#CJK UNIFIED IDEOGRAPH
    0x9850, 0x6994, //#CJK UNIFIED IDEOGRAPH
    0x9851, 0x6D6A, //#CJK UNIFIED IDEOGRAPH
    0x9852, 0x6F0F, //#CJK UNIFIED IDEOGRAPH
    0x9853, 0x7262, //#CJK UNIFIED IDEOGRAPH
    0x9854, 0x72FC, //#CJK UNIFIED IDEOGRAPH
    0x9855, 0x7BED, //#CJK UNIFIED IDEOGRAPH
    0x9856, 0x8001, //#CJK UNIFIED IDEOGRAPH
    0x9857, 0x807E, //#CJK UNIFIED IDEOGRAPH
    0x9858, 0x874B, //#CJK UNIFIED IDEOGRAPH
    0x9859, 0x90CE, //#CJK UNIFIED IDEOGRAPH
    0x985A, 0x516D, //#CJK UNIFIED IDEOGRAPH
    0x985B, 0x9E93, //#CJK UNIFIED IDEOGRAPH
    0x985C, 0x7984, //#CJK UNIFIED IDEOGRAPH
    0x985D, 0x808B, //#CJK UNIFIED IDEOGRAPH
    0x985E, 0x9332, //#CJK UNIFIED IDEOGRAPH
    0x985F, 0x8AD6, //#CJK UNIFIED IDEOGRAPH
    0x9860, 0x502D, //#CJK UNIFIED IDEOGRAPH
    0x9861, 0x548C, //#CJK UNIFIED IDEOGRAPH
    0x9862, 0x8A71, //#CJK UNIFIED IDEOGRAPH
    0x9863, 0x6B6A, //#CJK UNIFIED IDEOGRAPH
    0x9864, 0x8CC4, //#CJK UNIFIED IDEOGRAPH
    0x9865, 0x8107, //#CJK UNIFIED IDEOGRAPH
    0x9866, 0x60D1, //#CJK UNIFIED IDEOGRAPH
    0x9867, 0x67A0, //#CJK UNIFIED IDEOGRAPH
    0x9868, 0x9DF2, //#CJK UNIFIED IDEOGRAPH
    0x9869, 0x4E99, //#CJK UNIFIED IDEOGRAPH
    0x986A, 0x4E98, //#CJK UNIFIED IDEOGRAPH
    0x986B, 0x9C10, //#CJK UNIFIED IDEOGRAPH
    0x986C, 0x8A6B, //#CJK UNIFIED IDEOGRAPH
    0x986D, 0x85C1, //#CJK UNIFIED IDEOGRAPH
    0x986E, 0x8568, //#CJK UNIFIED IDEOGRAPH
    0x986F, 0x6900, //#CJK UNIFIED IDEOGRAPH
    0x9870, 0x6E7E, //#CJK UNIFIED IDEOGRAPH
    0x9871, 0x7897, //#CJK UNIFIED IDEOGRAPH
    0x9872, 0x8155, //#CJK UNIFIED IDEOGRAPH
    0x989F, 0x5F0C, //#CJK UNIFIED IDEOGRAPH
    0x98A0, 0x4E10, //#CJK UNIFIED IDEOGRAPH
    0x98A1, 0x4E15, //#CJK UNIFIED IDEOGRAPH
    0x98A2, 0x4E2A, //#CJK UNIFIED IDEOGRAPH
    0x98A3, 0x4E31, //#CJK UNIFIED IDEOGRAPH
    0x98A4, 0x4E36, //#CJK UNIFIED IDEOGRAPH
    0x98A5, 0x4E3C, //#CJK UNIFIED IDEOGRAPH
    0x98A6, 0x4E3F, //#CJK UNIFIED IDEOGRAPH
    0x98A7, 0x4E42, //#CJK UNIFIED IDEOGRAPH
    0x98A8, 0x4E56, //#CJK UNIFIED IDEOGRAPH
    0x98A9, 0x4E58, //#CJK UNIFIED IDEOGRAPH
    0x98AA, 0x4E82, //#CJK UNIFIED IDEOGRAPH
    0x98AB, 0x4E85, //#CJK UNIFIED IDEOGRAPH
    0x98AC, 0x8C6B, //#CJK UNIFIED IDEOGRAPH
    0x98AD, 0x4E8A, //#CJK UNIFIED IDEOGRAPH
    0x98AE, 0x8212, //#CJK UNIFIED IDEOGRAPH
    0x98AF, 0x5F0D, //#CJK UNIFIED IDEOGRAPH
    0x98B0, 0x4E8E, //#CJK UNIFIED IDEOGRAPH
    0x98B1, 0x4E9E, //#CJK UNIFIED IDEOGRAPH
    0x98B2, 0x4E9F, //#CJK UNIFIED IDEOGRAPH
    0x98B3, 0x4EA0, //#CJK UNIFIED IDEOGRAPH
    0x98B4, 0x4EA2, //#CJK UNIFIED IDEOGRAPH
    0x98B5, 0x4EB0, //#CJK UNIFIED IDEOGRAPH
    0x98B6, 0x4EB3, //#CJK UNIFIED IDEOGRAPH
    0x98B7, 0x4EB6, //#CJK UNIFIED IDEOGRAPH
    0x98B8, 0x4ECE, //#CJK UNIFIED IDEOGRAPH
    0x98B9, 0x4ECD, //#CJK UNIFIED IDEOGRAPH
    0x98BA, 0x4EC4, //#CJK UNIFIED IDEOGRAPH
    0x98BB, 0x4EC6, //#CJK UNIFIED IDEOGRAPH
    0x98BC, 0x4EC2, //#CJK UNIFIED IDEOGRAPH
    0x98BD, 0x4ED7, //#CJK UNIFIED IDEOGRAPH
    0x98BE, 0x4EDE, //#CJK UNIFIED IDEOGRAPH
    0x98BF, 0x4EED, //#CJK UNIFIED IDEOGRAPH
    0x98C0, 0x4EDF, //#CJK UNIFIED IDEOGRAPH
    0x98C1, 0x4EF7, //#CJK UNIFIED IDEOGRAPH
    0x98C2, 0x4F09, //#CJK UNIFIED IDEOGRAPH
    0x98C3, 0x4F5A, //#CJK UNIFIED IDEOGRAPH
    0x98C4, 0x4F30, //#CJK UNIFIED IDEOGRAPH
    0x98C5, 0x4F5B, //#CJK UNIFIED IDEOGRAPH
    0x98C6, 0x4F5D, //#CJK UNIFIED IDEOGRAPH
    0x98C7, 0x4F57, //#CJK UNIFIED IDEOGRAPH
    0x98C8, 0x4F47, //#CJK UNIFIED IDEOGRAPH
    0x98C9, 0x4F76, //#CJK UNIFIED IDEOGRAPH
    0x98CA, 0x4F88, //#CJK UNIFIED IDEOGRAPH
    0x98CB, 0x4F8F, //#CJK UNIFIED IDEOGRAPH
    0x98CC, 0x4F98, //#CJK UNIFIED IDEOGRAPH
    0x98CD, 0x4F7B, //#CJK UNIFIED IDEOGRAPH
    0x98CE, 0x4F69, //#CJK UNIFIED IDEOGRAPH
    0x98CF, 0x4F70, //#CJK UNIFIED IDEOGRAPH
    0x98D0, 0x4F91, //#CJK UNIFIED IDEOGRAPH
    0x98D1, 0x4F6F, //#CJK UNIFIED IDEOGRAPH
    0x98D2, 0x4F86, //#CJK UNIFIED IDEOGRAPH
    0x98D3, 0x4F96, //#CJK UNIFIED IDEOGRAPH
    0x98D4, 0x5118, //#CJK UNIFIED IDEOGRAPH
    0x98D5, 0x4FD4, //#CJK UNIFIED IDEOGRAPH
    0x98D6, 0x4FDF, //#CJK UNIFIED IDEOGRAPH
    0x98D7, 0x4FCE, //#CJK UNIFIED IDEOGRAPH
    0x98D8, 0x4FD8, //#CJK UNIFIED IDEOGRAPH
    0x98D9, 0x4FDB, //#CJK UNIFIED IDEOGRAPH
    0x98DA, 0x4FD1, //#CJK UNIFIED IDEOGRAPH
    0x98DB, 0x4FDA, //#CJK UNIFIED IDEOGRAPH
    0x98DC, 0x4FD0, //#CJK UNIFIED IDEOGRAPH
    0x98DD, 0x4FE4, //#CJK UNIFIED IDEOGRAPH
    0x98DE, 0x4FE5, //#CJK UNIFIED IDEOGRAPH
    0x98DF, 0x501A, //#CJK UNIFIED IDEOGRAPH
    0x98E0, 0x5028, //#CJK UNIFIED IDEOGRAPH
    0x98E1, 0x5014, //#CJK UNIFIED IDEOGRAPH
    0x98E2, 0x502A, //#CJK UNIFIED IDEOGRAPH
    0x98E3, 0x5025, //#CJK UNIFIED IDEOGRAPH
    0x98E4, 0x5005, //#CJK UNIFIED IDEOGRAPH
    0x98E5, 0x4F1C, //#CJK UNIFIED IDEOGRAPH
    0x98E6, 0x4FF6, //#CJK UNIFIED IDEOGRAPH
    0x98E7, 0x5021, //#CJK UNIFIED IDEOGRAPH
    0x98E8, 0x5029, //#CJK UNIFIED IDEOGRAPH
    0x98E9, 0x502C, //#CJK UNIFIED IDEOGRAPH
    0x98EA, 0x4FFE, //#CJK UNIFIED IDEOGRAPH
    0x98EB, 0x4FEF, //#CJK UNIFIED IDEOGRAPH
    0x98EC, 0x5011, //#CJK UNIFIED IDEOGRAPH
    0x98ED, 0x5006, //#CJK UNIFIED IDEOGRAPH
    0x98EE, 0x5043, //#CJK UNIFIED IDEOGRAPH
    0x98EF, 0x5047, //#CJK UNIFIED IDEOGRAPH
    0x98F0, 0x6703, //#CJK UNIFIED IDEOGRAPH
    0x98F1, 0x5055, //#CJK UNIFIED IDEOGRAPH
    0x98F2, 0x5050, //#CJK UNIFIED IDEOGRAPH
    0x98F3, 0x5048, //#CJK UNIFIED IDEOGRAPH
    0x98F4, 0x505A, //#CJK UNIFIED IDEOGRAPH
    0x98F5, 0x5056, //#CJK UNIFIED IDEOGRAPH
    0x98F6, 0x506C, //#CJK UNIFIED IDEOGRAPH
    0x98F7, 0x5078, //#CJK UNIFIED IDEOGRAPH
    0x98F8, 0x5080, //#CJK UNIFIED IDEOGRAPH
    0x98F9, 0x509A, //#CJK UNIFIED IDEOGRAPH
    0x98FA, 0x5085, //#CJK UNIFIED IDEOGRAPH
    0x98FB, 0x50B4, //#CJK UNIFIED IDEOGRAPH
    0x98FC, 0x50B2, //#CJK UNIFIED IDEOGRAPH
    0x9940, 0x50C9, //#CJK UNIFIED IDEOGRAPH
    0x9941, 0x50CA, //#CJK UNIFIED IDEOGRAPH
    0x9942, 0x50B3, //#CJK UNIFIED IDEOGRAPH
    0x9943, 0x50C2, //#CJK UNIFIED IDEOGRAPH
    0x9944, 0x50D6, //#CJK UNIFIED IDEOGRAPH
    0x9945, 0x50DE, //#CJK UNIFIED IDEOGRAPH
    0x9946, 0x50E5, //#CJK UNIFIED IDEOGRAPH
    0x9947, 0x50ED, //#CJK UNIFIED IDEOGRAPH
    0x9948, 0x50E3, //#CJK UNIFIED IDEOGRAPH
    0x9949, 0x50EE, //#CJK UNIFIED IDEOGRAPH
    0x994A, 0x50F9, //#CJK UNIFIED IDEOGRAPH
    0x994B, 0x50F5, //#CJK UNIFIED IDEOGRAPH
    0x994C, 0x5109, //#CJK UNIFIED IDEOGRAPH
    0x994D, 0x5101, //#CJK UNIFIED IDEOGRAPH
    0x994E, 0x5102, //#CJK UNIFIED IDEOGRAPH
    0x994F, 0x5116, //#CJK UNIFIED IDEOGRAPH
    0x9950, 0x5115, //#CJK UNIFIED IDEOGRAPH
    0x9951, 0x5114, //#CJK UNIFIED IDEOGRAPH
    0x9952, 0x511A, //#CJK UNIFIED IDEOGRAPH
    0x9953, 0x5121, //#CJK UNIFIED IDEOGRAPH
    0x9954, 0x513A, //#CJK UNIFIED IDEOGRAPH
    0x9955, 0x5137, //#CJK UNIFIED IDEOGRAPH
    0x9956, 0x513C, //#CJK UNIFIED IDEOGRAPH
    0x9957, 0x513B, //#CJK UNIFIED IDEOGRAPH
    0x9958, 0x513F, //#CJK UNIFIED IDEOGRAPH
    0x9959, 0x5140, //#CJK UNIFIED IDEOGRAPH
    0x995A, 0x5152, //#CJK UNIFIED IDEOGRAPH
    0x995B, 0x514C, //#CJK UNIFIED IDEOGRAPH
    0x995C, 0x5154, //#CJK UNIFIED IDEOGRAPH
    0x995D, 0x5162, //#CJK UNIFIED IDEOGRAPH
    0x995E, 0x7AF8, //#CJK UNIFIED IDEOGRAPH
    0x995F, 0x5169, //#CJK UNIFIED IDEOGRAPH
    0x9960, 0x516A, //#CJK UNIFIED IDEOGRAPH
    0x9961, 0x516E, //#CJK UNIFIED IDEOGRAPH
    0x9962, 0x5180, //#CJK UNIFIED IDEOGRAPH
    0x9963, 0x5182, //#CJK UNIFIED IDEOGRAPH
    0x9964, 0x56D8, //#CJK UNIFIED IDEOGRAPH
    0x9965, 0x518C, //#CJK UNIFIED IDEOGRAPH
    0x9966, 0x5189, //#CJK UNIFIED IDEOGRAPH
    0x9967, 0x518F, //#CJK UNIFIED IDEOGRAPH
    0x9968, 0x5191, //#CJK UNIFIED IDEOGRAPH
    0x9969, 0x5193, //#CJK UNIFIED IDEOGRAPH
    0x996A, 0x5195, //#CJK UNIFIED IDEOGRAPH
    0x996B, 0x5196, //#CJK UNIFIED IDEOGRAPH
    0x996C, 0x51A4, //#CJK UNIFIED IDEOGRAPH
    0x996D, 0x51A6, //#CJK UNIFIED IDEOGRAPH
    0x996E, 0x51A2, //#CJK UNIFIED IDEOGRAPH
    0x996F, 0x51A9, //#CJK UNIFIED IDEOGRAPH
    0x9970, 0x51AA, //#CJK UNIFIED IDEOGRAPH
    0x9971, 0x51AB, //#CJK UNIFIED IDEOGRAPH
    0x9972, 0x51B3, //#CJK UNIFIED IDEOGRAPH
    0x9973, 0x51B1, //#CJK UNIFIED IDEOGRAPH
    0x9974, 0x51B2, //#CJK UNIFIED IDEOGRAPH
    0x9975, 0x51B0, //#CJK UNIFIED IDEOGRAPH
    0x9976, 0x51B5, //#CJK UNIFIED IDEOGRAPH
    0x9977, 0x51BD, //#CJK UNIFIED IDEOGRAPH
    0x9978, 0x51C5, //#CJK UNIFIED IDEOGRAPH
    0x9979, 0x51C9, //#CJK UNIFIED IDEOGRAPH
    0x997A, 0x51DB, //#CJK UNIFIED IDEOGRAPH
    0x997B, 0x51E0, //#CJK UNIFIED IDEOGRAPH
    0x997C, 0x8655, //#CJK UNIFIED IDEOGRAPH
    0x997D, 0x51E9, //#CJK UNIFIED IDEOGRAPH
    0x997E, 0x51ED, //#CJK UNIFIED IDEOGRAPH
    0x9980, 0x51F0, //#CJK UNIFIED IDEOGRAPH
    0x9981, 0x51F5, //#CJK UNIFIED IDEOGRAPH
    0x9982, 0x51FE, //#CJK UNIFIED IDEOGRAPH
    0x9983, 0x5204, //#CJK UNIFIED IDEOGRAPH
    0x9984, 0x520B, //#CJK UNIFIED IDEOGRAPH
    0x9985, 0x5214, //#CJK UNIFIED IDEOGRAPH
    0x9986, 0x520E, //#CJK UNIFIED IDEOGRAPH
    0x9987, 0x5227, //#CJK UNIFIED IDEOGRAPH
    0x9988, 0x522A, //#CJK UNIFIED IDEOGRAPH
    0x9989, 0x522E, //#CJK UNIFIED IDEOGRAPH
    0x998A, 0x5233, //#CJK UNIFIED IDEOGRAPH
    0x998B, 0x5239, //#CJK UNIFIED IDEOGRAPH
    0x998C, 0x524F, //#CJK UNIFIED IDEOGRAPH
    0x998D, 0x5244, //#CJK UNIFIED IDEOGRAPH
    0x998E, 0x524B, //#CJK UNIFIED IDEOGRAPH
    0x998F, 0x524C, //#CJK UNIFIED IDEOGRAPH
    0x9990, 0x525E, //#CJK UNIFIED IDEOGRAPH
    0x9991, 0x5254, //#CJK UNIFIED IDEOGRAPH
    0x9992, 0x526A, //#CJK UNIFIED IDEOGRAPH
    0x9993, 0x5274, //#CJK UNIFIED IDEOGRAPH
    0x9994, 0x5269, //#CJK UNIFIED IDEOGRAPH
    0x9995, 0x5273, //#CJK UNIFIED IDEOGRAPH
    0x9996, 0x527F, //#CJK UNIFIED IDEOGRAPH
    0x9997, 0x527D, //#CJK UNIFIED IDEOGRAPH
    0x9998, 0x528D, //#CJK UNIFIED IDEOGRAPH
    0x9999, 0x5294, //#CJK UNIFIED IDEOGRAPH
    0x999A, 0x5292, //#CJK UNIFIED IDEOGRAPH
    0x999B, 0x5271, //#CJK UNIFIED IDEOGRAPH
    0x999C, 0x5288, //#CJK UNIFIED IDEOGRAPH
    0x999D, 0x5291, //#CJK UNIFIED IDEOGRAPH
    0x999E, 0x8FA8, //#CJK UNIFIED IDEOGRAPH
    0x999F, 0x8FA7, //#CJK UNIFIED IDEOGRAPH
    0x99A0, 0x52AC, //#CJK UNIFIED IDEOGRAPH
    0x99A1, 0x52AD, //#CJK UNIFIED IDEOGRAPH
    0x99A2, 0x52BC, //#CJK UNIFIED IDEOGRAPH
    0x99A3, 0x52B5, //#CJK UNIFIED IDEOGRAPH
    0x99A4, 0x52C1, //#CJK UNIFIED IDEOGRAPH
    0x99A5, 0x52CD, //#CJK UNIFIED IDEOGRAPH
    0x99A6, 0x52D7, //#CJK UNIFIED IDEOGRAPH
    0x99A7, 0x52DE, //#CJK UNIFIED IDEOGRAPH
    0x99A8, 0x52E3, //#CJK UNIFIED IDEOGRAPH
    0x99A9, 0x52E6, //#CJK UNIFIED IDEOGRAPH
    0x99AA, 0x98ED, //#CJK UNIFIED IDEOGRAPH
    0x99AB, 0x52E0, //#CJK UNIFIED IDEOGRAPH
    0x99AC, 0x52F3, //#CJK UNIFIED IDEOGRAPH
    0x99AD, 0x52F5, //#CJK UNIFIED IDEOGRAPH
    0x99AE, 0x52F8, //#CJK UNIFIED IDEOGRAPH
    0x99AF, 0x52F9, //#CJK UNIFIED IDEOGRAPH
    0x99B0, 0x5306, //#CJK UNIFIED IDEOGRAPH
    0x99B1, 0x5308, //#CJK UNIFIED IDEOGRAPH
    0x99B2, 0x7538, //#CJK UNIFIED IDEOGRAPH
    0x99B3, 0x530D, //#CJK UNIFIED IDEOGRAPH
    0x99B4, 0x5310, //#CJK UNIFIED IDEOGRAPH
    0x99B5, 0x530F, //#CJK UNIFIED IDEOGRAPH
    0x99B6, 0x5315, //#CJK UNIFIED IDEOGRAPH
    0x99B7, 0x531A, //#CJK UNIFIED IDEOGRAPH
    0x99B8, 0x5323, //#CJK UNIFIED IDEOGRAPH
    0x99B9, 0x532F, //#CJK UNIFIED IDEOGRAPH
    0x99BA, 0x5331, //#CJK UNIFIED IDEOGRAPH
    0x99BB, 0x5333, //#CJK UNIFIED IDEOGRAPH
    0x99BC, 0x5338, //#CJK UNIFIED IDEOGRAPH
    0x99BD, 0x5340, //#CJK UNIFIED IDEOGRAPH
    0x99BE, 0x5346, //#CJK UNIFIED IDEOGRAPH
    0x99BF, 0x5345, //#CJK UNIFIED IDEOGRAPH
    0x99C0, 0x4E17, //#CJK UNIFIED IDEOGRAPH
    0x99C1, 0x5349, //#CJK UNIFIED IDEOGRAPH
    0x99C2, 0x534D, //#CJK UNIFIED IDEOGRAPH
    0x99C3, 0x51D6, //#CJK UNIFIED IDEOGRAPH
    0x99C4, 0x535E, //#CJK UNIFIED IDEOGRAPH
    0x99C5, 0x5369, //#CJK UNIFIED IDEOGRAPH
    0x99C6, 0x536E, //#CJK UNIFIED IDEOGRAPH
    0x99C7, 0x5918, //#CJK UNIFIED IDEOGRAPH
    0x99C8, 0x537B, //#CJK UNIFIED IDEOGRAPH
    0x99C9, 0x5377, //#CJK UNIFIED IDEOGRAPH
    0x99CA, 0x5382, //#CJK UNIFIED IDEOGRAPH
    0x99CB, 0x5396, //#CJK UNIFIED IDEOGRAPH
    0x99CC, 0x53A0, //#CJK UNIFIED IDEOGRAPH
    0x99CD, 0x53A6, //#CJK UNIFIED IDEOGRAPH
    0x99CE, 0x53A5, //#CJK UNIFIED IDEOGRAPH
    0x99CF, 0x53AE, //#CJK UNIFIED IDEOGRAPH
    0x99D0, 0x53B0, //#CJK UNIFIED IDEOGRAPH
    0x99D1, 0x53B6, //#CJK UNIFIED IDEOGRAPH
    0x99D2, 0x53C3, //#CJK UNIFIED IDEOGRAPH
    0x99D3, 0x7C12, //#CJK UNIFIED IDEOGRAPH
    0x99D4, 0x96D9, //#CJK UNIFIED IDEOGRAPH
    0x99D5, 0x53DF, //#CJK UNIFIED IDEOGRAPH
    0x99D6, 0x66FC, //#CJK UNIFIED IDEOGRAPH
    0x99D7, 0x71EE, //#CJK UNIFIED IDEOGRAPH
    0x99D8, 0x53EE, //#CJK UNIFIED IDEOGRAPH
    0x99D9, 0x53E8, //#CJK UNIFIED IDEOGRAPH
    0x99DA, 0x53ED, //#CJK UNIFIED IDEOGRAPH
    0x99DB, 0x53FA, //#CJK UNIFIED IDEOGRAPH
    0x99DC, 0x5401, //#CJK UNIFIED IDEOGRAPH
    0x99DD, 0x543D, //#CJK UNIFIED IDEOGRAPH
    0x99DE, 0x5440, //#CJK UNIFIED IDEOGRAPH
    0x99DF, 0x542C, //#CJK UNIFIED IDEOGRAPH
    0x99E0, 0x542D, //#CJK UNIFIED IDEOGRAPH
    0x99E1, 0x543C, //#CJK UNIFIED IDEOGRAPH
    0x99E2, 0x542E, //#CJK UNIFIED IDEOGRAPH
    0x99E3, 0x5436, //#CJK UNIFIED IDEOGRAPH
    0x99E4, 0x5429, //#CJK UNIFIED IDEOGRAPH
    0x99E5, 0x541D, //#CJK UNIFIED IDEOGRAPH
    0x99E6, 0x544E, //#CJK UNIFIED IDEOGRAPH
    0x99E7, 0x548F, //#CJK UNIFIED IDEOGRAPH
    0x99E8, 0x5475, //#CJK UNIFIED IDEOGRAPH
    0x99E9, 0x548E, //#CJK UNIFIED IDEOGRAPH
    0x99EA, 0x545F, //#CJK UNIFIED IDEOGRAPH
    0x99EB, 0x5471, //#CJK UNIFIED IDEOGRAPH
    0x99EC, 0x5477, //#CJK UNIFIED IDEOGRAPH
    0x99ED, 0x5470, //#CJK UNIFIED IDEOGRAPH
    0x99EE, 0x5492, //#CJK UNIFIED IDEOGRAPH
    0x99EF, 0x547B, //#CJK UNIFIED IDEOGRAPH
    0x99F0, 0x5480, //#CJK UNIFIED IDEOGRAPH
    0x99F1, 0x5476, //#CJK UNIFIED IDEOGRAPH
    0x99F2, 0x5484, //#CJK UNIFIED IDEOGRAPH
    0x99F3, 0x5490, //#CJK UNIFIED IDEOGRAPH
    0x99F4, 0x5486, //#CJK UNIFIED IDEOGRAPH
    0x99F5, 0x54C7, //#CJK UNIFIED IDEOGRAPH
    0x99F6, 0x54A2, //#CJK UNIFIED IDEOGRAPH
    0x99F7, 0x54B8, //#CJK UNIFIED IDEOGRAPH
    0x99F8, 0x54A5, //#CJK UNIFIED IDEOGRAPH
    0x99F9, 0x54AC, //#CJK UNIFIED IDEOGRAPH
    0x99FA, 0x54C4, //#CJK UNIFIED IDEOGRAPH
    0x99FB, 0x54C8, //#CJK UNIFIED IDEOGRAPH
    0x99FC, 0x54A8, //#CJK UNIFIED IDEOGRAPH
    0x9A40, 0x54AB, //#CJK UNIFIED IDEOGRAPH
    0x9A41, 0x54C2, //#CJK UNIFIED IDEOGRAPH
    0x9A42, 0x54A4, //#CJK UNIFIED IDEOGRAPH
    0x9A43, 0x54BE, //#CJK UNIFIED IDEOGRAPH
    0x9A44, 0x54BC, //#CJK UNIFIED IDEOGRAPH
    0x9A45, 0x54D8, //#CJK UNIFIED IDEOGRAPH
    0x9A46, 0x54E5, //#CJK UNIFIED IDEOGRAPH
    0x9A47, 0x54E6, //#CJK UNIFIED IDEOGRAPH
    0x9A48, 0x550F, //#CJK UNIFIED IDEOGRAPH
    0x9A49, 0x5514, //#CJK UNIFIED IDEOGRAPH
    0x9A4A, 0x54FD, //#CJK UNIFIED IDEOGRAPH
    0x9A4B, 0x54EE, //#CJK UNIFIED IDEOGRAPH
    0x9A4C, 0x54ED, //#CJK UNIFIED IDEOGRAPH
    0x9A4D, 0x54FA, //#CJK UNIFIED IDEOGRAPH
    0x9A4E, 0x54E2, //#CJK UNIFIED IDEOGRAPH
    0x9A4F, 0x5539, //#CJK UNIFIED IDEOGRAPH
    0x9A50, 0x5540, //#CJK UNIFIED IDEOGRAPH
    0x9A51, 0x5563, //#CJK UNIFIED IDEOGRAPH
    0x9A52, 0x554C, //#CJK UNIFIED IDEOGRAPH
    0x9A53, 0x552E, //#CJK UNIFIED IDEOGRAPH
    0x9A54, 0x555C, //#CJK UNIFIED IDEOGRAPH
    0x9A55, 0x5545, //#CJK UNIFIED IDEOGRAPH
    0x9A56, 0x5556, //#CJK UNIFIED IDEOGRAPH
    0x9A57, 0x5557, //#CJK UNIFIED IDEOGRAPH
    0x9A58, 0x5538, //#CJK UNIFIED IDEOGRAPH
    0x9A59, 0x5533, //#CJK UNIFIED IDEOGRAPH
    0x9A5A, 0x555D, //#CJK UNIFIED IDEOGRAPH
    0x9A5B, 0x5599, //#CJK UNIFIED IDEOGRAPH
    0x9A5C, 0x5580, //#CJK UNIFIED IDEOGRAPH
    0x9A5D, 0x54AF, //#CJK UNIFIED IDEOGRAPH
    0x9A5E, 0x558A, //#CJK UNIFIED IDEOGRAPH
    0x9A5F, 0x559F, //#CJK UNIFIED IDEOGRAPH
    0x9A60, 0x557B, //#CJK UNIFIED IDEOGRAPH
    0x9A61, 0x557E, //#CJK UNIFIED IDEOGRAPH
    0x9A62, 0x5598, //#CJK UNIFIED IDEOGRAPH
    0x9A63, 0x559E, //#CJK UNIFIED IDEOGRAPH
    0x9A64, 0x55AE, //#CJK UNIFIED IDEOGRAPH
    0x9A65, 0x557C, //#CJK UNIFIED IDEOGRAPH
    0x9A66, 0x5583, //#CJK UNIFIED IDEOGRAPH
    0x9A67, 0x55A9, //#CJK UNIFIED IDEOGRAPH
    0x9A68, 0x5587, //#CJK UNIFIED IDEOGRAPH
    0x9A69, 0x55A8, //#CJK UNIFIED IDEOGRAPH
    0x9A6A, 0x55DA, //#CJK UNIFIED IDEOGRAPH
    0x9A6B, 0x55C5, //#CJK UNIFIED IDEOGRAPH
    0x9A6C, 0x55DF, //#CJK UNIFIED IDEOGRAPH
    0x9A6D, 0x55C4, //#CJK UNIFIED IDEOGRAPH
    0x9A6E, 0x55DC, //#CJK UNIFIED IDEOGRAPH
    0x9A6F, 0x55E4, //#CJK UNIFIED IDEOGRAPH
    0x9A70, 0x55D4, //#CJK UNIFIED IDEOGRAPH
    0x9A71, 0x5614, //#CJK UNIFIED IDEOGRAPH
    0x9A72, 0x55F7, //#CJK UNIFIED IDEOGRAPH
    0x9A73, 0x5616, //#CJK UNIFIED IDEOGRAPH
    0x9A74, 0x55FE, //#CJK UNIFIED IDEOGRAPH
    0x9A75, 0x55FD, //#CJK UNIFIED IDEOGRAPH
    0x9A76, 0x561B, //#CJK UNIFIED IDEOGRAPH
    0x9A77, 0x55F9, //#CJK UNIFIED IDEOGRAPH
    0x9A78, 0x564E, //#CJK UNIFIED IDEOGRAPH
    0x9A79, 0x5650, //#CJK UNIFIED IDEOGRAPH
    0x9A7A, 0x71DF, //#CJK UNIFIED IDEOGRAPH
    0x9A7B, 0x5634, //#CJK UNIFIED IDEOGRAPH
    0x9A7C, 0x5636, //#CJK UNIFIED IDEOGRAPH
    0x9A7D, 0x5632, //#CJK UNIFIED IDEOGRAPH
    0x9A7E, 0x5638, //#CJK UNIFIED IDEOGRAPH
    0x9A80, 0x566B, //#CJK UNIFIED IDEOGRAPH
    0x9A81, 0x5664, //#CJK UNIFIED IDEOGRAPH
    0x9A82, 0x562F, //#CJK UNIFIED IDEOGRAPH
    0x9A83, 0x566C, //#CJK UNIFIED IDEOGRAPH
    0x9A84, 0x566A, //#CJK UNIFIED IDEOGRAPH
    0x9A85, 0x5686, //#CJK UNIFIED IDEOGRAPH
    0x9A86, 0x5680, //#CJK UNIFIED IDEOGRAPH
    0x9A87, 0x568A, //#CJK UNIFIED IDEOGRAPH
    0x9A88, 0x56A0, //#CJK UNIFIED IDEOGRAPH
    0x9A89, 0x5694, //#CJK UNIFIED IDEOGRAPH
    0x9A8A, 0x568F, //#CJK UNIFIED IDEOGRAPH
    0x9A8B, 0x56A5, //#CJK UNIFIED IDEOGRAPH
    0x9A8C, 0x56AE, //#CJK UNIFIED IDEOGRAPH
    0x9A8D, 0x56B6, //#CJK UNIFIED IDEOGRAPH
    0x9A8E, 0x56B4, //#CJK UNIFIED IDEOGRAPH
    0x9A8F, 0x56C2, //#CJK UNIFIED IDEOGRAPH
    0x9A90, 0x56BC, //#CJK UNIFIED IDEOGRAPH
    0x9A91, 0x56C1, //#CJK UNIFIED IDEOGRAPH
    0x9A92, 0x56C3, //#CJK UNIFIED IDEOGRAPH
    0x9A93, 0x56C0, //#CJK UNIFIED IDEOGRAPH
    0x9A94, 0x56C8, //#CJK UNIFIED IDEOGRAPH
    0x9A95, 0x56CE, //#CJK UNIFIED IDEOGRAPH
    0x9A96, 0x56D1, //#CJK UNIFIED IDEOGRAPH
    0x9A97, 0x56D3, //#CJK UNIFIED IDEOGRAPH
    0x9A98, 0x56D7, //#CJK UNIFIED IDEOGRAPH
    0x9A99, 0x56EE, //#CJK UNIFIED IDEOGRAPH
    0x9A9A, 0x56F9, //#CJK UNIFIED IDEOGRAPH
    0x9A9B, 0x5700, //#CJK UNIFIED IDEOGRAPH
    0x9A9C, 0x56FF, //#CJK UNIFIED IDEOGRAPH
    0x9A9D, 0x5704, //#CJK UNIFIED IDEOGRAPH
    0x9A9E, 0x5709, //#CJK UNIFIED IDEOGRAPH
    0x9A9F, 0x5708, //#CJK UNIFIED IDEOGRAPH
    0x9AA0, 0x570B, //#CJK UNIFIED IDEOGRAPH
    0x9AA1, 0x570D, //#CJK UNIFIED IDEOGRAPH
    0x9AA2, 0x5713, //#CJK UNIFIED IDEOGRAPH
    0x9AA3, 0x5718, //#CJK UNIFIED IDEOGRAPH
    0x9AA4, 0x5716, //#CJK UNIFIED IDEOGRAPH
    0x9AA5, 0x55C7, //#CJK UNIFIED IDEOGRAPH
    0x9AA6, 0x571C, //#CJK UNIFIED IDEOGRAPH
    0x9AA7, 0x5726, //#CJK UNIFIED IDEOGRAPH
    0x9AA8, 0x5737, //#CJK UNIFIED IDEOGRAPH
    0x9AA9, 0x5738, //#CJK UNIFIED IDEOGRAPH
    0x9AAA, 0x574E, //#CJK UNIFIED IDEOGRAPH
    0x9AAB, 0x573B, //#CJK UNIFIED IDEOGRAPH
    0x9AAC, 0x5740, //#CJK UNIFIED IDEOGRAPH
    0x9AAD, 0x574F, //#CJK UNIFIED IDEOGRAPH
    0x9AAE, 0x5769, //#CJK UNIFIED IDEOGRAPH
    0x9AAF, 0x57C0, //#CJK UNIFIED IDEOGRAPH
    0x9AB0, 0x5788, //#CJK UNIFIED IDEOGRAPH
    0x9AB1, 0x5761, //#CJK UNIFIED IDEOGRAPH
    0x9AB2, 0x577F, //#CJK UNIFIED IDEOGRAPH
    0x9AB3, 0x5789, //#CJK UNIFIED IDEOGRAPH
    0x9AB4, 0x5793, //#CJK UNIFIED IDEOGRAPH
    0x9AB5, 0x57A0, //#CJK UNIFIED IDEOGRAPH
    0x9AB6, 0x57B3, //#CJK UNIFIED IDEOGRAPH
    0x9AB7, 0x57A4, //#CJK UNIFIED IDEOGRAPH
    0x9AB8, 0x57AA, //#CJK UNIFIED IDEOGRAPH
    0x9AB9, 0x57B0, //#CJK UNIFIED IDEOGRAPH
    0x9ABA, 0x57C3, //#CJK UNIFIED IDEOGRAPH
    0x9ABB, 0x57C6, //#CJK UNIFIED IDEOGRAPH
    0x9ABC, 0x57D4, //#CJK UNIFIED IDEOGRAPH
    0x9ABD, 0x57D2, //#CJK UNIFIED IDEOGRAPH
    0x9ABE, 0x57D3, //#CJK UNIFIED IDEOGRAPH
    0x9ABF, 0x580A, //#CJK UNIFIED IDEOGRAPH
    0x9AC0, 0x57D6, //#CJK UNIFIED IDEOGRAPH
    0x9AC1, 0x57E3, //#CJK UNIFIED IDEOGRAPH
    0x9AC2, 0x580B, //#CJK UNIFIED IDEOGRAPH
    0x9AC3, 0x5819, //#CJK UNIFIED IDEOGRAPH
    0x9AC4, 0x581D, //#CJK UNIFIED IDEOGRAPH
    0x9AC5, 0x5872, //#CJK UNIFIED IDEOGRAPH
    0x9AC6, 0x5821, //#CJK UNIFIED IDEOGRAPH
    0x9AC7, 0x5862, //#CJK UNIFIED IDEOGRAPH
    0x9AC8, 0x584B, //#CJK UNIFIED IDEOGRAPH
    0x9AC9, 0x5870, //#CJK UNIFIED IDEOGRAPH
    0x9ACA, 0x6BC0, //#CJK UNIFIED IDEOGRAPH
    0x9ACB, 0x5852, //#CJK UNIFIED IDEOGRAPH
    0x9ACC, 0x583D, //#CJK UNIFIED IDEOGRAPH
    0x9ACD, 0x5879, //#CJK UNIFIED IDEOGRAPH
    0x9ACE, 0x5885, //#CJK UNIFIED IDEOGRAPH
    0x9ACF, 0x58B9, //#CJK UNIFIED IDEOGRAPH
    0x9AD0, 0x589F, //#CJK UNIFIED IDEOGRAPH
    0x9AD1, 0x58AB, //#CJK UNIFIED IDEOGRAPH
    0x9AD2, 0x58BA, //#CJK UNIFIED IDEOGRAPH
    0x9AD3, 0x58DE, //#CJK UNIFIED IDEOGRAPH
    0x9AD4, 0x58BB, //#CJK UNIFIED IDEOGRAPH
    0x9AD5, 0x58B8, //#CJK UNIFIED IDEOGRAPH
    0x9AD6, 0x58AE, //#CJK UNIFIED IDEOGRAPH
    0x9AD7, 0x58C5, //#CJK UNIFIED IDEOGRAPH
    0x9AD8, 0x58D3, //#CJK UNIFIED IDEOGRAPH
    0x9AD9, 0x58D1, //#CJK UNIFIED IDEOGRAPH
    0x9ADA, 0x58D7, //#CJK UNIFIED IDEOGRAPH
    0x9ADB, 0x58D9, //#CJK UNIFIED IDEOGRAPH
    0x9ADC, 0x58D8, //#CJK UNIFIED IDEOGRAPH
    0x9ADD, 0x58E5, //#CJK UNIFIED IDEOGRAPH
    0x9ADE, 0x58DC, //#CJK UNIFIED IDEOGRAPH
    0x9ADF, 0x58E4, //#CJK UNIFIED IDEOGRAPH
    0x9AE0, 0x58DF, //#CJK UNIFIED IDEOGRAPH
    0x9AE1, 0x58EF, //#CJK UNIFIED IDEOGRAPH
    0x9AE2, 0x58FA, //#CJK UNIFIED IDEOGRAPH
    0x9AE3, 0x58F9, //#CJK UNIFIED IDEOGRAPH
    0x9AE4, 0x58FB, //#CJK UNIFIED IDEOGRAPH
    0x9AE5, 0x58FC, //#CJK UNIFIED IDEOGRAPH
    0x9AE6, 0x58FD, //#CJK UNIFIED IDEOGRAPH
    0x9AE7, 0x5902, //#CJK UNIFIED IDEOGRAPH
    0x9AE8, 0x590A, //#CJK UNIFIED IDEOGRAPH
    0x9AE9, 0x5910, //#CJK UNIFIED IDEOGRAPH
    0x9AEA, 0x591B, //#CJK UNIFIED IDEOGRAPH
    0x9AEB, 0x68A6, //#CJK UNIFIED IDEOGRAPH
    0x9AEC, 0x5925, //#CJK UNIFIED IDEOGRAPH
    0x9AED, 0x592C, //#CJK UNIFIED IDEOGRAPH
    0x9AEE, 0x592D, //#CJK UNIFIED IDEOGRAPH
    0x9AEF, 0x5932, //#CJK UNIFIED IDEOGRAPH
    0x9AF0, 0x5938, //#CJK UNIFIED IDEOGRAPH
    0x9AF1, 0x593E, //#CJK UNIFIED IDEOGRAPH
    0x9AF2, 0x7AD2, //#CJK UNIFIED IDEOGRAPH
    0x9AF3, 0x5955, //#CJK UNIFIED IDEOGRAPH
    0x9AF4, 0x5950, //#CJK UNIFIED IDEOGRAPH
    0x9AF5, 0x594E, //#CJK UNIFIED IDEOGRAPH
    0x9AF6, 0x595A, //#CJK UNIFIED IDEOGRAPH
    0x9AF7, 0x5958, //#CJK UNIFIED IDEOGRAPH
    0x9AF8, 0x5962, //#CJK UNIFIED IDEOGRAPH
    0x9AF9, 0x5960, //#CJK UNIFIED IDEOGRAPH
    0x9AFA, 0x5967, //#CJK UNIFIED IDEOGRAPH
    0x9AFB, 0x596C, //#CJK UNIFIED IDEOGRAPH
    0x9AFC, 0x5969, //#CJK UNIFIED IDEOGRAPH
    0x9B40, 0x5978, //#CJK UNIFIED IDEOGRAPH
    0x9B41, 0x5981, //#CJK UNIFIED IDEOGRAPH
    0x9B42, 0x599D, //#CJK UNIFIED IDEOGRAPH
    0x9B43, 0x4F5E, //#CJK UNIFIED IDEOGRAPH
    0x9B44, 0x4FAB, //#CJK UNIFIED IDEOGRAPH
    0x9B45, 0x59A3, //#CJK UNIFIED IDEOGRAPH
    0x9B46, 0x59B2, //#CJK UNIFIED IDEOGRAPH
    0x9B47, 0x59C6, //#CJK UNIFIED IDEOGRAPH
    0x9B48, 0x59E8, //#CJK UNIFIED IDEOGRAPH
    0x9B49, 0x59DC, //#CJK UNIFIED IDEOGRAPH
    0x9B4A, 0x598D, //#CJK UNIFIED IDEOGRAPH
    0x9B4B, 0x59D9, //#CJK UNIFIED IDEOGRAPH
    0x9B4C, 0x59DA, //#CJK UNIFIED IDEOGRAPH
    0x9B4D, 0x5A25, //#CJK UNIFIED IDEOGRAPH
    0x9B4E, 0x5A1F, //#CJK UNIFIED IDEOGRAPH
    0x9B4F, 0x5A11, //#CJK UNIFIED IDEOGRAPH
    0x9B50, 0x5A1C, //#CJK UNIFIED IDEOGRAPH
    0x9B51, 0x5A09, //#CJK UNIFIED IDEOGRAPH
    0x9B52, 0x5A1A, //#CJK UNIFIED IDEOGRAPH
    0x9B53, 0x5A40, //#CJK UNIFIED IDEOGRAPH
    0x9B54, 0x5A6C, //#CJK UNIFIED IDEOGRAPH
    0x9B55, 0x5A49, //#CJK UNIFIED IDEOGRAPH
    0x9B56, 0x5A35, //#CJK UNIFIED IDEOGRAPH
    0x9B57, 0x5A36, //#CJK UNIFIED IDEOGRAPH
    0x9B58, 0x5A62, //#CJK UNIFIED IDEOGRAPH
    0x9B59, 0x5A6A, //#CJK UNIFIED IDEOGRAPH
    0x9B5A, 0x5A9A, //#CJK UNIFIED IDEOGRAPH
    0x9B5B, 0x5ABC, //#CJK UNIFIED IDEOGRAPH
    0x9B5C, 0x5ABE, //#CJK UNIFIED IDEOGRAPH
    0x9B5D, 0x5ACB, //#CJK UNIFIED IDEOGRAPH
    0x9B5E, 0x5AC2, //#CJK UNIFIED IDEOGRAPH
    0x9B5F, 0x5ABD, //#CJK UNIFIED IDEOGRAPH
    0x9B60, 0x5AE3, //#CJK UNIFIED IDEOGRAPH
    0x9B61, 0x5AD7, //#CJK UNIFIED IDEOGRAPH
    0x9B62, 0x5AE6, //#CJK UNIFIED IDEOGRAPH
    0x9B63, 0x5AE9, //#CJK UNIFIED IDEOGRAPH
    0x9B64, 0x5AD6, //#CJK UNIFIED IDEOGRAPH
    0x9B65, 0x5AFA, //#CJK UNIFIED IDEOGRAPH
    0x9B66, 0x5AFB, //#CJK UNIFIED IDEOGRAPH
    0x9B67, 0x5B0C, //#CJK UNIFIED IDEOGRAPH
    0x9B68, 0x5B0B, //#CJK UNIFIED IDEOGRAPH
    0x9B69, 0x5B16, //#CJK UNIFIED IDEOGRAPH
    0x9B6A, 0x5B32, //#CJK UNIFIED IDEOGRAPH
    0x9B6B, 0x5AD0, //#CJK UNIFIED IDEOGRAPH
    0x9B6C, 0x5B2A, //#CJK UNIFIED IDEOGRAPH
    0x9B6D, 0x5B36, //#CJK UNIFIED IDEOGRAPH
    0x9B6E, 0x5B3E, //#CJK UNIFIED IDEOGRAPH
    0x9B6F, 0x5B43, //#CJK UNIFIED IDEOGRAPH
    0x9B70, 0x5B45, //#CJK UNIFIED IDEOGRAPH
    0x9B71, 0x5B40, //#CJK UNIFIED IDEOGRAPH
    0x9B72, 0x5B51, //#CJK UNIFIED IDEOGRAPH
    0x9B73, 0x5B55, //#CJK UNIFIED IDEOGRAPH
    0x9B74, 0x5B5A, //#CJK UNIFIED IDEOGRAPH
    0x9B75, 0x5B5B, //#CJK UNIFIED IDEOGRAPH
    0x9B76, 0x5B65, //#CJK UNIFIED IDEOGRAPH
    0x9B77, 0x5B69, //#CJK UNIFIED IDEOGRAPH
    0x9B78, 0x5B70, //#CJK UNIFIED IDEOGRAPH
    0x9B79, 0x5B73, //#CJK UNIFIED IDEOGRAPH
    0x9B7A, 0x5B75, //#CJK UNIFIED IDEOGRAPH
    0x9B7B, 0x5B78, //#CJK UNIFIED IDEOGRAPH
    0x9B7C, 0x6588, //#CJK UNIFIED IDEOGRAPH
    0x9B7D, 0x5B7A, //#CJK UNIFIED IDEOGRAPH
    0x9B7E, 0x5B80, //#CJK UNIFIED IDEOGRAPH
    0x9B80, 0x5B83, //#CJK UNIFIED IDEOGRAPH
    0x9B81, 0x5BA6, //#CJK UNIFIED IDEOGRAPH
    0x9B82, 0x5BB8, //#CJK UNIFIED IDEOGRAPH
    0x9B83, 0x5BC3, //#CJK UNIFIED IDEOGRAPH
    0x9B84, 0x5BC7, //#CJK UNIFIED IDEOGRAPH
    0x9B85, 0x5BC9, //#CJK UNIFIED IDEOGRAPH
    0x9B86, 0x5BD4, //#CJK UNIFIED IDEOGRAPH
    0x9B87, 0x5BD0, //#CJK UNIFIED IDEOGRAPH
    0x9B88, 0x5BE4, //#CJK UNIFIED IDEOGRAPH
    0x9B89, 0x5BE6, //#CJK UNIFIED IDEOGRAPH
    0x9B8A, 0x5BE2, //#CJK UNIFIED IDEOGRAPH
    0x9B8B, 0x5BDE, //#CJK UNIFIED IDEOGRAPH
    0x9B8C, 0x5BE5, //#CJK UNIFIED IDEOGRAPH
    0x9B8D, 0x5BEB, //#CJK UNIFIED IDEOGRAPH
    0x9B8E, 0x5BF0, //#CJK UNIFIED IDEOGRAPH
    0x9B8F, 0x5BF6, //#CJK UNIFIED IDEOGRAPH
    0x9B90, 0x5BF3, //#CJK UNIFIED IDEOGRAPH
    0x9B91, 0x5C05, //#CJK UNIFIED IDEOGRAPH
    0x9B92, 0x5C07, //#CJK UNIFIED IDEOGRAPH
    0x9B93, 0x5C08, //#CJK UNIFIED IDEOGRAPH
    0x9B94, 0x5C0D, //#CJK UNIFIED IDEOGRAPH
    0x9B95, 0x5C13, //#CJK UNIFIED IDEOGRAPH
    0x9B96, 0x5C20, //#CJK UNIFIED IDEOGRAPH
    0x9B97, 0x5C22, //#CJK UNIFIED IDEOGRAPH
    0x9B98, 0x5C28, //#CJK UNIFIED IDEOGRAPH
    0x9B99, 0x5C38, //#CJK UNIFIED IDEOGRAPH
    0x9B9A, 0x5C39, //#CJK UNIFIED IDEOGRAPH
    0x9B9B, 0x5C41, //#CJK UNIFIED IDEOGRAPH
    0x9B9C, 0x5C46, //#CJK UNIFIED IDEOGRAPH
    0x9B9D, 0x5C4E, //#CJK UNIFIED IDEOGRAPH
    0x9B9E, 0x5C53, //#CJK UNIFIED IDEOGRAPH
    0x9B9F, 0x5C50, //#CJK UNIFIED IDEOGRAPH
    0x9BA0, 0x5C4F, //#CJK UNIFIED IDEOGRAPH
    0x9BA1, 0x5B71, //#CJK UNIFIED IDEOGRAPH
    0x9BA2, 0x5C6C, //#CJK UNIFIED IDEOGRAPH
    0x9BA3, 0x5C6E, //#CJK UNIFIED IDEOGRAPH
    0x9BA4, 0x4E62, //#CJK UNIFIED IDEOGRAPH
    0x9BA5, 0x5C76, //#CJK UNIFIED IDEOGRAPH
    0x9BA6, 0x5C79, //#CJK UNIFIED IDEOGRAPH
    0x9BA7, 0x5C8C, //#CJK UNIFIED IDEOGRAPH
    0x9BA8, 0x5C91, //#CJK UNIFIED IDEOGRAPH
    0x9BA9, 0x5C94, //#CJK UNIFIED IDEOGRAPH
    0x9BAA, 0x599B, //#CJK UNIFIED IDEOGRAPH
    0x9BAB, 0x5CAB, //#CJK UNIFIED IDEOGRAPH
    0x9BAC, 0x5CBB, //#CJK UNIFIED IDEOGRAPH
    0x9BAD, 0x5CB6, //#CJK UNIFIED IDEOGRAPH
    0x9BAE, 0x5CBC, //#CJK UNIFIED IDEOGRAPH
    0x9BAF, 0x5CB7, //#CJK UNIFIED IDEOGRAPH
    0x9BB0, 0x5CC5, //#CJK UNIFIED IDEOGRAPH
    0x9BB1, 0x5CBE, //#CJK UNIFIED IDEOGRAPH
    0x9BB2, 0x5CC7, //#CJK UNIFIED IDEOGRAPH
    0x9BB3, 0x5CD9, //#CJK UNIFIED IDEOGRAPH
    0x9BB4, 0x5CE9, //#CJK UNIFIED IDEOGRAPH
    0x9BB5, 0x5CFD, //#CJK UNIFIED IDEOGRAPH
    0x9BB6, 0x5CFA, //#CJK UNIFIED IDEOGRAPH
    0x9BB7, 0x5CED, //#CJK UNIFIED IDEOGRAPH
    0x9BB8, 0x5D8C, //#CJK UNIFIED IDEOGRAPH
    0x9BB9, 0x5CEA, //#CJK UNIFIED IDEOGRAPH
    0x9BBA, 0x5D0B, //#CJK UNIFIED IDEOGRAPH
    0x9BBB, 0x5D15, //#CJK UNIFIED IDEOGRAPH
    0x9BBC, 0x5D17, //#CJK UNIFIED IDEOGRAPH
    0x9BBD, 0x5D5C, //#CJK UNIFIED IDEOGRAPH
    0x9BBE, 0x5D1F, //#CJK UNIFIED IDEOGRAPH
    0x9BBF, 0x5D1B, //#CJK UNIFIED IDEOGRAPH
    0x9BC0, 0x5D11, //#CJK UNIFIED IDEOGRAPH
    0x9BC1, 0x5D14, //#CJK UNIFIED IDEOGRAPH
    0x9BC2, 0x5D22, //#CJK UNIFIED IDEOGRAPH
    0x9BC3, 0x5D1A, //#CJK UNIFIED IDEOGRAPH
    0x9BC4, 0x5D19, //#CJK UNIFIED IDEOGRAPH
    0x9BC5, 0x5D18, //#CJK UNIFIED IDEOGRAPH
    0x9BC6, 0x5D4C, //#CJK UNIFIED IDEOGRAPH
    0x9BC7, 0x5D52, //#CJK UNIFIED IDEOGRAPH
    0x9BC8, 0x5D4E, //#CJK UNIFIED IDEOGRAPH
    0x9BC9, 0x5D4B, //#CJK UNIFIED IDEOGRAPH
    0x9BCA, 0x5D6C, //#CJK UNIFIED IDEOGRAPH
    0x9BCB, 0x5D73, //#CJK UNIFIED IDEOGRAPH
    0x9BCC, 0x5D76, //#CJK UNIFIED IDEOGRAPH
    0x9BCD, 0x5D87, //#CJK UNIFIED IDEOGRAPH
    0x9BCE, 0x5D84, //#CJK UNIFIED IDEOGRAPH
    0x9BCF, 0x5D82, //#CJK UNIFIED IDEOGRAPH
    0x9BD0, 0x5DA2, //#CJK UNIFIED IDEOGRAPH
    0x9BD1, 0x5D9D, //#CJK UNIFIED IDEOGRAPH
    0x9BD2, 0x5DAC, //#CJK UNIFIED IDEOGRAPH
    0x9BD3, 0x5DAE, //#CJK UNIFIED IDEOGRAPH
    0x9BD4, 0x5DBD, //#CJK UNIFIED IDEOGRAPH
    0x9BD5, 0x5D90, //#CJK UNIFIED IDEOGRAPH
    0x9BD6, 0x5DB7, //#CJK UNIFIED IDEOGRAPH
    0x9BD7, 0x5DBC, //#CJK UNIFIED IDEOGRAPH
    0x9BD8, 0x5DC9, //#CJK UNIFIED IDEOGRAPH
    0x9BD9, 0x5DCD, //#CJK UNIFIED IDEOGRAPH
    0x9BDA, 0x5DD3, //#CJK UNIFIED IDEOGRAPH
    0x9BDB, 0x5DD2, //#CJK UNIFIED IDEOGRAPH
    0x9BDC, 0x5DD6, //#CJK UNIFIED IDEOGRAPH
    0x9BDD, 0x5DDB, //#CJK UNIFIED IDEOGRAPH
    0x9BDE, 0x5DEB, //#CJK UNIFIED IDEOGRAPH
    0x9BDF, 0x5DF2, //#CJK UNIFIED IDEOGRAPH
    0x9BE0, 0x5DF5, //#CJK UNIFIED IDEOGRAPH
    0x9BE1, 0x5E0B, //#CJK UNIFIED IDEOGRAPH
    0x9BE2, 0x5E1A, //#CJK UNIFIED IDEOGRAPH
    0x9BE3, 0x5E19, //#CJK UNIFIED IDEOGRAPH
    0x9BE4, 0x5E11, //#CJK UNIFIED IDEOGRAPH
    0x9BE5, 0x5E1B, //#CJK UNIFIED IDEOGRAPH
    0x9BE6, 0x5E36, //#CJK UNIFIED IDEOGRAPH
    0x9BE7, 0x5E37, //#CJK UNIFIED IDEOGRAPH
    0x9BE8, 0x5E44, //#CJK UNIFIED IDEOGRAPH
    0x9BE9, 0x5E43, //#CJK UNIFIED IDEOGRAPH
    0x9BEA, 0x5E40, //#CJK UNIFIED IDEOGRAPH
    0x9BEB, 0x5E4E, //#CJK UNIFIED IDEOGRAPH
    0x9BEC, 0x5E57, //#CJK UNIFIED IDEOGRAPH
    0x9BED, 0x5E54, //#CJK UNIFIED IDEOGRAPH
    0x9BEE, 0x5E5F, //#CJK UNIFIED IDEOGRAPH
    0x9BEF, 0x5E62, //#CJK UNIFIED IDEOGRAPH
    0x9BF0, 0x5E64, //#CJK UNIFIED IDEOGRAPH
    0x9BF1, 0x5E47, //#CJK UNIFIED IDEOGRAPH
    0x9BF2, 0x5E75, //#CJK UNIFIED IDEOGRAPH
    0x9BF3, 0x5E76, //#CJK UNIFIED IDEOGRAPH
    0x9BF4, 0x5E7A, //#CJK UNIFIED IDEOGRAPH
    0x9BF5, 0x9EBC, //#CJK UNIFIED IDEOGRAPH
    0x9BF6, 0x5E7F, //#CJK UNIFIED IDEOGRAPH
    0x9BF7, 0x5EA0, //#CJK UNIFIED IDEOGRAPH
    0x9BF8, 0x5EC1, //#CJK UNIFIED IDEOGRAPH
    0x9BF9, 0x5EC2, //#CJK UNIFIED IDEOGRAPH
    0x9BFA, 0x5EC8, //#CJK UNIFIED IDEOGRAPH
    0x9BFB, 0x5ED0, //#CJK UNIFIED IDEOGRAPH
    0x9BFC, 0x5ECF, //#CJK UNIFIED IDEOGRAPH
    0x9C40, 0x5ED6, //#CJK UNIFIED IDEOGRAPH
    0x9C41, 0x5EE3, //#CJK UNIFIED IDEOGRAPH
    0x9C42, 0x5EDD, //#CJK UNIFIED IDEOGRAPH
    0x9C43, 0x5EDA, //#CJK UNIFIED IDEOGRAPH
    0x9C44, 0x5EDB, //#CJK UNIFIED IDEOGRAPH
    0x9C45, 0x5EE2, //#CJK UNIFIED IDEOGRAPH
    0x9C46, 0x5EE1, //#CJK UNIFIED IDEOGRAPH
    0x9C47, 0x5EE8, //#CJK UNIFIED IDEOGRAPH
    0x9C48, 0x5EE9, //#CJK UNIFIED IDEOGRAPH
    0x9C49, 0x5EEC, //#CJK UNIFIED IDEOGRAPH
    0x9C4A, 0x5EF1, //#CJK UNIFIED IDEOGRAPH
    0x9C4B, 0x5EF3, //#CJK UNIFIED IDEOGRAPH
    0x9C4C, 0x5EF0, //#CJK UNIFIED IDEOGRAPH
    0x9C4D, 0x5EF4, //#CJK UNIFIED IDEOGRAPH
    0x9C4E, 0x5EF8, //#CJK UNIFIED IDEOGRAPH
    0x9C4F, 0x5EFE, //#CJK UNIFIED IDEOGRAPH
    0x9C50, 0x5F03, //#CJK UNIFIED IDEOGRAPH
    0x9C51, 0x5F09, //#CJK UNIFIED IDEOGRAPH
    0x9C52, 0x5F5D, //#CJK UNIFIED IDEOGRAPH
    0x9C53, 0x5F5C, //#CJK UNIFIED IDEOGRAPH
    0x9C54, 0x5F0B, //#CJK UNIFIED IDEOGRAPH
    0x9C55, 0x5F11, //#CJK UNIFIED IDEOGRAPH
    0x9C56, 0x5F16, //#CJK UNIFIED IDEOGRAPH
    0x9C57, 0x5F29, //#CJK UNIFIED IDEOGRAPH
    0x9C58, 0x5F2D, //#CJK UNIFIED IDEOGRAPH
    0x9C59, 0x5F38, //#CJK UNIFIED IDEOGRAPH
    0x9C5A, 0x5F41, //#CJK UNIFIED IDEOGRAPH
    0x9C5B, 0x5F48, //#CJK UNIFIED IDEOGRAPH
    0x9C5C, 0x5F4C, //#CJK UNIFIED IDEOGRAPH
    0x9C5D, 0x5F4E, //#CJK UNIFIED IDEOGRAPH
    0x9C5E, 0x5F2F, //#CJK UNIFIED IDEOGRAPH
    0x9C5F, 0x5F51, //#CJK UNIFIED IDEOGRAPH
    0x9C60, 0x5F56, //#CJK UNIFIED IDEOGRAPH
    0x9C61, 0x5F57, //#CJK UNIFIED IDEOGRAPH
    0x9C62, 0x5F59, //#CJK UNIFIED IDEOGRAPH
    0x9C63, 0x5F61, //#CJK UNIFIED IDEOGRAPH
    0x9C64, 0x5F6D, //#CJK UNIFIED IDEOGRAPH
    0x9C65, 0x5F73, //#CJK UNIFIED IDEOGRAPH
    0x9C66, 0x5F77, //#CJK UNIFIED IDEOGRAPH
    0x9C67, 0x5F83, //#CJK UNIFIED IDEOGRAPH
    0x9C68, 0x5F82, //#CJK UNIFIED IDEOGRAPH
    0x9C69, 0x5F7F, //#CJK UNIFIED IDEOGRAPH
    0x9C6A, 0x5F8A, //#CJK UNIFIED IDEOGRAPH
    0x9C6B, 0x5F88, //#CJK UNIFIED IDEOGRAPH
    0x9C6C, 0x5F91, //#CJK UNIFIED IDEOGRAPH
    0x9C6D, 0x5F87, //#CJK UNIFIED IDEOGRAPH
    0x9C6E, 0x5F9E, //#CJK UNIFIED IDEOGRAPH
    0x9C6F, 0x5F99, //#CJK UNIFIED IDEOGRAPH
    0x9C70, 0x5F98, //#CJK UNIFIED IDEOGRAPH
    0x9C71, 0x5FA0, //#CJK UNIFIED IDEOGRAPH
    0x9C72, 0x5FA8, //#CJK UNIFIED IDEOGRAPH
    0x9C73, 0x5FAD, //#CJK UNIFIED IDEOGRAPH
    0x9C74, 0x5FBC, //#CJK UNIFIED IDEOGRAPH
    0x9C75, 0x5FD6, //#CJK UNIFIED IDEOGRAPH
    0x9C76, 0x5FFB, //#CJK UNIFIED IDEOGRAPH
    0x9C77, 0x5FE4, //#CJK UNIFIED IDEOGRAPH
    0x9C78, 0x5FF8, //#CJK UNIFIED IDEOGRAPH
    0x9C79, 0x5FF1, //#CJK UNIFIED IDEOGRAPH
    0x9C7A, 0x5FDD, //#CJK UNIFIED IDEOGRAPH
    0x9C7B, 0x60B3, //#CJK UNIFIED IDEOGRAPH
    0x9C7C, 0x5FFF, //#CJK UNIFIED IDEOGRAPH
    0x9C7D, 0x6021, //#CJK UNIFIED IDEOGRAPH
    0x9C7E, 0x6060, //#CJK UNIFIED IDEOGRAPH
    0x9C80, 0x6019, //#CJK UNIFIED IDEOGRAPH
    0x9C81, 0x6010, //#CJK UNIFIED IDEOGRAPH
    0x9C82, 0x6029, //#CJK UNIFIED IDEOGRAPH
    0x9C83, 0x600E, //#CJK UNIFIED IDEOGRAPH
    0x9C84, 0x6031, //#CJK UNIFIED IDEOGRAPH
    0x9C85, 0x601B, //#CJK UNIFIED IDEOGRAPH
    0x9C86, 0x6015, //#CJK UNIFIED IDEOGRAPH
    0x9C87, 0x602B, //#CJK UNIFIED IDEOGRAPH
    0x9C88, 0x6026, //#CJK UNIFIED IDEOGRAPH
    0x9C89, 0x600F, //#CJK UNIFIED IDEOGRAPH
    0x9C8A, 0x603A, //#CJK UNIFIED IDEOGRAPH
    0x9C8B, 0x605A, //#CJK UNIFIED IDEOGRAPH
    0x9C8C, 0x6041, //#CJK UNIFIED IDEOGRAPH
    0x9C8D, 0x606A, //#CJK UNIFIED IDEOGRAPH
    0x9C8E, 0x6077, //#CJK UNIFIED IDEOGRAPH
    0x9C8F, 0x605F, //#CJK UNIFIED IDEOGRAPH
    0x9C90, 0x604A, //#CJK UNIFIED IDEOGRAPH
    0x9C91, 0x6046, //#CJK UNIFIED IDEOGRAPH
    0x9C92, 0x604D, //#CJK UNIFIED IDEOGRAPH
    0x9C93, 0x6063, //#CJK UNIFIED IDEOGRAPH
    0x9C94, 0x6043, //#CJK UNIFIED IDEOGRAPH
    0x9C95, 0x6064, //#CJK UNIFIED IDEOGRAPH
    0x9C96, 0x6042, //#CJK UNIFIED IDEOGRAPH
    0x9C97, 0x606C, //#CJK UNIFIED IDEOGRAPH
    0x9C98, 0x606B, //#CJK UNIFIED IDEOGRAPH
    0x9C99, 0x6059, //#CJK UNIFIED IDEOGRAPH
    0x9C9A, 0x6081, //#CJK UNIFIED IDEOGRAPH
    0x9C9B, 0x608D, //#CJK UNIFIED IDEOGRAPH
    0x9C9C, 0x60E7, //#CJK UNIFIED IDEOGRAPH
    0x9C9D, 0x6083, //#CJK UNIFIED IDEOGRAPH
    0x9C9E, 0x609A, //#CJK UNIFIED IDEOGRAPH
    0x9C9F, 0x6084, //#CJK UNIFIED IDEOGRAPH
    0x9CA0, 0x609B, //#CJK UNIFIED IDEOGRAPH
    0x9CA1, 0x6096, //#CJK UNIFIED IDEOGRAPH
    0x9CA2, 0x6097, //#CJK UNIFIED IDEOGRAPH
    0x9CA3, 0x6092, //#CJK UNIFIED IDEOGRAPH
    0x9CA4, 0x60A7, //#CJK UNIFIED IDEOGRAPH
    0x9CA5, 0x608B, //#CJK UNIFIED IDEOGRAPH
    0x9CA6, 0x60E1, //#CJK UNIFIED IDEOGRAPH
    0x9CA7, 0x60B8, //#CJK UNIFIED IDEOGRAPH
    0x9CA8, 0x60E0, //#CJK UNIFIED IDEOGRAPH
    0x9CA9, 0x60D3, //#CJK UNIFIED IDEOGRAPH
    0x9CAA, 0x60B4, //#CJK UNIFIED IDEOGRAPH
    0x9CAB, 0x5FF0, //#CJK UNIFIED IDEOGRAPH
    0x9CAC, 0x60BD, //#CJK UNIFIED IDEOGRAPH
    0x9CAD, 0x60C6, //#CJK UNIFIED IDEOGRAPH
    0x9CAE, 0x60B5, //#CJK UNIFIED IDEOGRAPH
    0x9CAF, 0x60D8, //#CJK UNIFIED IDEOGRAPH
    0x9CB0, 0x614D, //#CJK UNIFIED IDEOGRAPH
    0x9CB1, 0x6115, //#CJK UNIFIED IDEOGRAPH
    0x9CB2, 0x6106, //#CJK UNIFIED IDEOGRAPH
    0x9CB3, 0x60F6, //#CJK UNIFIED IDEOGRAPH
    0x9CB4, 0x60F7, //#CJK UNIFIED IDEOGRAPH
    0x9CB5, 0x6100, //#CJK UNIFIED IDEOGRAPH
    0x9CB6, 0x60F4, //#CJK UNIFIED IDEOGRAPH
    0x9CB7, 0x60FA, //#CJK UNIFIED IDEOGRAPH
    0x9CB8, 0x6103, //#CJK UNIFIED IDEOGRAPH
    0x9CB9, 0x6121, //#CJK UNIFIED IDEOGRAPH
    0x9CBA, 0x60FB, //#CJK UNIFIED IDEOGRAPH
    0x9CBB, 0x60F1, //#CJK UNIFIED IDEOGRAPH
    0x9CBC, 0x610D, //#CJK UNIFIED IDEOGRAPH
    0x9CBD, 0x610E, //#CJK UNIFIED IDEOGRAPH
    0x9CBE, 0x6147, //#CJK UNIFIED IDEOGRAPH
    0x9CBF, 0x613E, //#CJK UNIFIED IDEOGRAPH
    0x9CC0, 0x6128, //#CJK UNIFIED IDEOGRAPH
    0x9CC1, 0x6127, //#CJK UNIFIED IDEOGRAPH
    0x9CC2, 0x614A, //#CJK UNIFIED IDEOGRAPH
    0x9CC3, 0x613F, //#CJK UNIFIED IDEOGRAPH
    0x9CC4, 0x613C, //#CJK UNIFIED IDEOGRAPH
    0x9CC5, 0x612C, //#CJK UNIFIED IDEOGRAPH
    0x9CC6, 0x6134, //#CJK UNIFIED IDEOGRAPH
    0x9CC7, 0x613D, //#CJK UNIFIED IDEOGRAPH
    0x9CC8, 0x6142, //#CJK UNIFIED IDEOGRAPH
    0x9CC9, 0x6144, //#CJK UNIFIED IDEOGRAPH
    0x9CCA, 0x6173, //#CJK UNIFIED IDEOGRAPH
    0x9CCB, 0x6177, //#CJK UNIFIED IDEOGRAPH
    0x9CCC, 0x6158, //#CJK UNIFIED IDEOGRAPH
    0x9CCD, 0x6159, //#CJK UNIFIED IDEOGRAPH
    0x9CCE, 0x615A, //#CJK UNIFIED IDEOGRAPH
    0x9CCF, 0x616B, //#CJK UNIFIED IDEOGRAPH
    0x9CD0, 0x6174, //#CJK UNIFIED IDEOGRAPH
    0x9CD1, 0x616F, //#CJK UNIFIED IDEOGRAPH
    0x9CD2, 0x6165, //#CJK UNIFIED IDEOGRAPH
    0x9CD3, 0x6171, //#CJK UNIFIED IDEOGRAPH
    0x9CD4, 0x615F, //#CJK UNIFIED IDEOGRAPH
    0x9CD5, 0x615D, //#CJK UNIFIED IDEOGRAPH
    0x9CD6, 0x6153, //#CJK UNIFIED IDEOGRAPH
    0x9CD7, 0x6175, //#CJK UNIFIED IDEOGRAPH
    0x9CD8, 0x6199, //#CJK UNIFIED IDEOGRAPH
    0x9CD9, 0x6196, //#CJK UNIFIED IDEOGRAPH
    0x9CDA, 0x6187, //#CJK UNIFIED IDEOGRAPH
    0x9CDB, 0x61AC, //#CJK UNIFIED IDEOGRAPH
    0x9CDC, 0x6194, //#CJK UNIFIED IDEOGRAPH
    0x9CDD, 0x619A, //#CJK UNIFIED IDEOGRAPH
    0x9CDE, 0x618A, //#CJK UNIFIED IDEOGRAPH
    0x9CDF, 0x6191, //#CJK UNIFIED IDEOGRAPH
    0x9CE0, 0x61AB, //#CJK UNIFIED IDEOGRAPH
    0x9CE1, 0x61AE, //#CJK UNIFIED IDEOGRAPH
    0x9CE2, 0x61CC, //#CJK UNIFIED IDEOGRAPH
    0x9CE3, 0x61CA, //#CJK UNIFIED IDEOGRAPH
    0x9CE4, 0x61C9, //#CJK UNIFIED IDEOGRAPH
    0x9CE5, 0x61F7, //#CJK UNIFIED IDEOGRAPH
    0x9CE6, 0x61C8, //#CJK UNIFIED IDEOGRAPH
    0x9CE7, 0x61C3, //#CJK UNIFIED IDEOGRAPH
    0x9CE8, 0x61C6, //#CJK UNIFIED IDEOGRAPH
    0x9CE9, 0x61BA, //#CJK UNIFIED IDEOGRAPH
    0x9CEA, 0x61CB, //#CJK UNIFIED IDEOGRAPH
    0x9CEB, 0x7F79, //#CJK UNIFIED IDEOGRAPH
    0x9CEC, 0x61CD, //#CJK UNIFIED IDEOGRAPH
    0x9CED, 0x61E6, //#CJK UNIFIED IDEOGRAPH
    0x9CEE, 0x61E3, //#CJK UNIFIED IDEOGRAPH
    0x9CEF, 0x61F6, //#CJK UNIFIED IDEOGRAPH
    0x9CF0, 0x61FA, //#CJK UNIFIED IDEOGRAPH
    0x9CF1, 0x61F4, //#CJK UNIFIED IDEOGRAPH
    0x9CF2, 0x61FF, //#CJK UNIFIED IDEOGRAPH
    0x9CF3, 0x61FD, //#CJK UNIFIED IDEOGRAPH
    0x9CF4, 0x61FC, //#CJK UNIFIED IDEOGRAPH
    0x9CF5, 0x61FE, //#CJK UNIFIED IDEOGRAPH
    0x9CF6, 0x6200, //#CJK UNIFIED IDEOGRAPH
    0x9CF7, 0x6208, //#CJK UNIFIED IDEOGRAPH
    0x9CF8, 0x6209, //#CJK UNIFIED IDEOGRAPH
    0x9CF9, 0x620D, //#CJK UNIFIED IDEOGRAPH
    0x9CFA, 0x620C, //#CJK UNIFIED IDEOGRAPH
    0x9CFB, 0x6214, //#CJK UNIFIED IDEOGRAPH
    0x9CFC, 0x621B, //#CJK UNIFIED IDEOGRAPH
    0x9D40, 0x621E, //#CJK UNIFIED IDEOGRAPH
    0x9D41, 0x6221, //#CJK UNIFIED IDEOGRAPH
    0x9D42, 0x622A, //#CJK UNIFIED IDEOGRAPH
    0x9D43, 0x622E, //#CJK UNIFIED IDEOGRAPH
    0x9D44, 0x6230, //#CJK UNIFIED IDEOGRAPH
    0x9D45, 0x6232, //#CJK UNIFIED IDEOGRAPH
    0x9D46, 0x6233, //#CJK UNIFIED IDEOGRAPH
    0x9D47, 0x6241, //#CJK UNIFIED IDEOGRAPH
    0x9D48, 0x624E, //#CJK UNIFIED IDEOGRAPH
    0x9D49, 0x625E, //#CJK UNIFIED IDEOGRAPH
    0x9D4A, 0x6263, //#CJK UNIFIED IDEOGRAPH
    0x9D4B, 0x625B, //#CJK UNIFIED IDEOGRAPH
    0x9D4C, 0x6260, //#CJK UNIFIED IDEOGRAPH
    0x9D4D, 0x6268, //#CJK UNIFIED IDEOGRAPH
    0x9D4E, 0x627C, //#CJK UNIFIED IDEOGRAPH
    0x9D4F, 0x6282, //#CJK UNIFIED IDEOGRAPH
    0x9D50, 0x6289, //#CJK UNIFIED IDEOGRAPH
    0x9D51, 0x627E, //#CJK UNIFIED IDEOGRAPH
    0x9D52, 0x6292, //#CJK UNIFIED IDEOGRAPH
    0x9D53, 0x6293, //#CJK UNIFIED IDEOGRAPH
    0x9D54, 0x6296, //#CJK UNIFIED IDEOGRAPH
    0x9D55, 0x62D4, //#CJK UNIFIED IDEOGRAPH
    0x9D56, 0x6283, //#CJK UNIFIED IDEOGRAPH
    0x9D57, 0x6294, //#CJK UNIFIED IDEOGRAPH
    0x9D58, 0x62D7, //#CJK UNIFIED IDEOGRAPH
    0x9D59, 0x62D1, //#CJK UNIFIED IDEOGRAPH
    0x9D5A, 0x62BB, //#CJK UNIFIED IDEOGRAPH
    0x9D5B, 0x62CF, //#CJK UNIFIED IDEOGRAPH
    0x9D5C, 0x62FF, //#CJK UNIFIED IDEOGRAPH
    0x9D5D, 0x62C6, //#CJK UNIFIED IDEOGRAPH
    0x9D5E, 0x64D4, //#CJK UNIFIED IDEOGRAPH
    0x9D5F, 0x62C8, //#CJK UNIFIED IDEOGRAPH
    0x9D60, 0x62DC, //#CJK UNIFIED IDEOGRAPH
    0x9D61, 0x62CC, //#CJK UNIFIED IDEOGRAPH
    0x9D62, 0x62CA, //#CJK UNIFIED IDEOGRAPH
    0x9D63, 0x62C2, //#CJK UNIFIED IDEOGRAPH
    0x9D64, 0x62C7, //#CJK UNIFIED IDEOGRAPH
    0x9D65, 0x629B, //#CJK UNIFIED IDEOGRAPH
    0x9D66, 0x62C9, //#CJK UNIFIED IDEOGRAPH
    0x9D67, 0x630C, //#CJK UNIFIED IDEOGRAPH
    0x9D68, 0x62EE, //#CJK UNIFIED IDEOGRAPH
    0x9D69, 0x62F1, //#CJK UNIFIED IDEOGRAPH
    0x9D6A, 0x6327, //#CJK UNIFIED IDEOGRAPH
    0x9D6B, 0x6302, //#CJK UNIFIED IDEOGRAPH
    0x9D6C, 0x6308, //#CJK UNIFIED IDEOGRAPH
    0x9D6D, 0x62EF, //#CJK UNIFIED IDEOGRAPH
    0x9D6E, 0x62F5, //#CJK UNIFIED IDEOGRAPH
    0x9D6F, 0x6350, //#CJK UNIFIED IDEOGRAPH
    0x9D70, 0x633E, //#CJK UNIFIED IDEOGRAPH
    0x9D71, 0x634D, //#CJK UNIFIED IDEOGRAPH
    0x9D72, 0x641C, //#CJK UNIFIED IDEOGRAPH
    0x9D73, 0x634F, //#CJK UNIFIED IDEOGRAPH
    0x9D74, 0x6396, //#CJK UNIFIED IDEOGRAPH
    0x9D75, 0x638E, //#CJK UNIFIED IDEOGRAPH
    0x9D76, 0x6380, //#CJK UNIFIED IDEOGRAPH
    0x9D77, 0x63AB, //#CJK UNIFIED IDEOGRAPH
    0x9D78, 0x6376, //#CJK UNIFIED IDEOGRAPH
    0x9D79, 0x63A3, //#CJK UNIFIED IDEOGRAPH
    0x9D7A, 0x638F, //#CJK UNIFIED IDEOGRAPH
    0x9D7B, 0x6389, //#CJK UNIFIED IDEOGRAPH
    0x9D7C, 0x639F, //#CJK UNIFIED IDEOGRAPH
    0x9D7D, 0x63B5, //#CJK UNIFIED IDEOGRAPH
    0x9D7E, 0x636B, //#CJK UNIFIED IDEOGRAPH
    0x9D80, 0x6369, //#CJK UNIFIED IDEOGRAPH
    0x9D81, 0x63BE, //#CJK UNIFIED IDEOGRAPH
    0x9D82, 0x63E9, //#CJK UNIFIED IDEOGRAPH
    0x9D83, 0x63C0, //#CJK UNIFIED IDEOGRAPH
    0x9D84, 0x63C6, //#CJK UNIFIED IDEOGRAPH
    0x9D85, 0x63E3, //#CJK UNIFIED IDEOGRAPH
    0x9D86, 0x63C9, //#CJK UNIFIED IDEOGRAPH
    0x9D87, 0x63D2, //#CJK UNIFIED IDEOGRAPH
    0x9D88, 0x63F6, //#CJK UNIFIED IDEOGRAPH
    0x9D89, 0x63C4, //#CJK UNIFIED IDEOGRAPH
    0x9D8A, 0x6416, //#CJK UNIFIED IDEOGRAPH
    0x9D8B, 0x6434, //#CJK UNIFIED IDEOGRAPH
    0x9D8C, 0x6406, //#CJK UNIFIED IDEOGRAPH
    0x9D8D, 0x6413, //#CJK UNIFIED IDEOGRAPH
    0x9D8E, 0x6426, //#CJK UNIFIED IDEOGRAPH
    0x9D8F, 0x6436, //#CJK UNIFIED IDEOGRAPH
    0x9D90, 0x651D, //#CJK UNIFIED IDEOGRAPH
    0x9D91, 0x6417, //#CJK UNIFIED IDEOGRAPH
    0x9D92, 0x6428, //#CJK UNIFIED IDEOGRAPH
    0x9D93, 0x640F, //#CJK UNIFIED IDEOGRAPH
    0x9D94, 0x6467, //#CJK UNIFIED IDEOGRAPH
    0x9D95, 0x646F, //#CJK UNIFIED IDEOGRAPH
    0x9D96, 0x6476, //#CJK UNIFIED IDEOGRAPH
    0x9D97, 0x644E, //#CJK UNIFIED IDEOGRAPH
    0x9D98, 0x652A, //#CJK UNIFIED IDEOGRAPH
    0x9D99, 0x6495, //#CJK UNIFIED IDEOGRAPH
    0x9D9A, 0x6493, //#CJK UNIFIED IDEOGRAPH
    0x9D9B, 0x64A5, //#CJK UNIFIED IDEOGRAPH
    0x9D9C, 0x64A9, //#CJK UNIFIED IDEOGRAPH
    0x9D9D, 0x6488, //#CJK UNIFIED IDEOGRAPH
    0x9D9E, 0x64BC, //#CJK UNIFIED IDEOGRAPH
    0x9D9F, 0x64DA, //#CJK UNIFIED IDEOGRAPH
    0x9DA0, 0x64D2, //#CJK UNIFIED IDEOGRAPH
    0x9DA1, 0x64C5, //#CJK UNIFIED IDEOGRAPH
    0x9DA2, 0x64C7, //#CJK UNIFIED IDEOGRAPH
    0x9DA3, 0x64BB, //#CJK UNIFIED IDEOGRAPH
    0x9DA4, 0x64D8, //#CJK UNIFIED IDEOGRAPH
    0x9DA5, 0x64C2, //#CJK UNIFIED IDEOGRAPH
    0x9DA6, 0x64F1, //#CJK UNIFIED IDEOGRAPH
    0x9DA7, 0x64E7, //#CJK UNIFIED IDEOGRAPH
    0x9DA8, 0x8209, //#CJK UNIFIED IDEOGRAPH
    0x9DA9, 0x64E0, //#CJK UNIFIED IDEOGRAPH
    0x9DAA, 0x64E1, //#CJK UNIFIED IDEOGRAPH
    0x9DAB, 0x62AC, //#CJK UNIFIED IDEOGRAPH
    0x9DAC, 0x64E3, //#CJK UNIFIED IDEOGRAPH
    0x9DAD, 0x64EF, //#CJK UNIFIED IDEOGRAPH
    0x9DAE, 0x652C, //#CJK UNIFIED IDEOGRAPH
    0x9DAF, 0x64F6, //#CJK UNIFIED IDEOGRAPH
    0x9DB0, 0x64F4, //#CJK UNIFIED IDEOGRAPH
    0x9DB1, 0x64F2, //#CJK UNIFIED IDEOGRAPH
    0x9DB2, 0x64FA, //#CJK UNIFIED IDEOGRAPH
    0x9DB3, 0x6500, //#CJK UNIFIED IDEOGRAPH
    0x9DB4, 0x64FD, //#CJK UNIFIED IDEOGRAPH
    0x9DB5, 0x6518, //#CJK UNIFIED IDEOGRAPH
    0x9DB6, 0x651C, //#CJK UNIFIED IDEOGRAPH
    0x9DB7, 0x6505, //#CJK UNIFIED IDEOGRAPH
    0x9DB8, 0x6524, //#CJK UNIFIED IDEOGRAPH
    0x9DB9, 0x6523, //#CJK UNIFIED IDEOGRAPH
    0x9DBA, 0x652B, //#CJK UNIFIED IDEOGRAPH
    0x9DBB, 0x6534, //#CJK UNIFIED IDEOGRAPH
    0x9DBC, 0x6535, //#CJK UNIFIED IDEOGRAPH
    0x9DBD, 0x6537, //#CJK UNIFIED IDEOGRAPH
    0x9DBE, 0x6536, //#CJK UNIFIED IDEOGRAPH
    0x9DBF, 0x6538, //#CJK UNIFIED IDEOGRAPH
    0x9DC0, 0x754B, //#CJK UNIFIED IDEOGRAPH
    0x9DC1, 0x6548, //#CJK UNIFIED IDEOGRAPH
    0x9DC2, 0x6556, //#CJK UNIFIED IDEOGRAPH
    0x9DC3, 0x6555, //#CJK UNIFIED IDEOGRAPH
    0x9DC4, 0x654D, //#CJK UNIFIED IDEOGRAPH
    0x9DC5, 0x6558, //#CJK UNIFIED IDEOGRAPH
    0x9DC6, 0x655E, //#CJK UNIFIED IDEOGRAPH
    0x9DC7, 0x655D, //#CJK UNIFIED IDEOGRAPH
    0x9DC8, 0x6572, //#CJK UNIFIED IDEOGRAPH
    0x9DC9, 0x6578, //#CJK UNIFIED IDEOGRAPH
    0x9DCA, 0x6582, //#CJK UNIFIED IDEOGRAPH
    0x9DCB, 0x6583, //#CJK UNIFIED IDEOGRAPH
    0x9DCC, 0x8B8A, //#CJK UNIFIED IDEOGRAPH
    0x9DCD, 0x659B, //#CJK UNIFIED IDEOGRAPH
    0x9DCE, 0x659F, //#CJK UNIFIED IDEOGRAPH
    0x9DCF, 0x65AB, //#CJK UNIFIED IDEOGRAPH
    0x9DD0, 0x65B7, //#CJK UNIFIED IDEOGRAPH
    0x9DD1, 0x65C3, //#CJK UNIFIED IDEOGRAPH
    0x9DD2, 0x65C6, //#CJK UNIFIED IDEOGRAPH
    0x9DD3, 0x65C1, //#CJK UNIFIED IDEOGRAPH
    0x9DD4, 0x65C4, //#CJK UNIFIED IDEOGRAPH
    0x9DD5, 0x65CC, //#CJK UNIFIED IDEOGRAPH
    0x9DD6, 0x65D2, //#CJK UNIFIED IDEOGRAPH
    0x9DD7, 0x65DB, //#CJK UNIFIED IDEOGRAPH
    0x9DD8, 0x65D9, //#CJK UNIFIED IDEOGRAPH
    0x9DD9, 0x65E0, //#CJK UNIFIED IDEOGRAPH
    0x9DDA, 0x65E1, //#CJK UNIFIED IDEOGRAPH
    0x9DDB, 0x65F1, //#CJK UNIFIED IDEOGRAPH
    0x9DDC, 0x6772, //#CJK UNIFIED IDEOGRAPH
    0x9DDD, 0x660A, //#CJK UNIFIED IDEOGRAPH
    0x9DDE, 0x6603, //#CJK UNIFIED IDEOGRAPH
    0x9DDF, 0x65FB, //#CJK UNIFIED IDEOGRAPH
    0x9DE0, 0x6773, //#CJK UNIFIED IDEOGRAPH
    0x9DE1, 0x6635, //#CJK UNIFIED IDEOGRAPH
    0x9DE2, 0x6636, //#CJK UNIFIED IDEOGRAPH
    0x9DE3, 0x6634, //#CJK UNIFIED IDEOGRAPH
    0x9DE4, 0x661C, //#CJK UNIFIED IDEOGRAPH
    0x9DE5, 0x664F, //#CJK UNIFIED IDEOGRAPH
    0x9DE6, 0x6644, //#CJK UNIFIED IDEOGRAPH
    0x9DE7, 0x6649, //#CJK UNIFIED IDEOGRAPH
    0x9DE8, 0x6641, //#CJK UNIFIED IDEOGRAPH
    0x9DE9, 0x665E, //#CJK UNIFIED IDEOGRAPH
    0x9DEA, 0x665D, //#CJK UNIFIED IDEOGRAPH
    0x9DEB, 0x6664, //#CJK UNIFIED IDEOGRAPH
    0x9DEC, 0x6667, //#CJK UNIFIED IDEOGRAPH
    0x9DED, 0x6668, //#CJK UNIFIED IDEOGRAPH
    0x9DEE, 0x665F, //#CJK UNIFIED IDEOGRAPH
    0x9DEF, 0x6662, //#CJK UNIFIED IDEOGRAPH
    0x9DF0, 0x6670, //#CJK UNIFIED IDEOGRAPH
    0x9DF1, 0x6683, //#CJK UNIFIED IDEOGRAPH
    0x9DF2, 0x6688, //#CJK UNIFIED IDEOGRAPH
    0x9DF3, 0x668E, //#CJK UNIFIED IDEOGRAPH
    0x9DF4, 0x6689, //#CJK UNIFIED IDEOGRAPH
    0x9DF5, 0x6684, //#CJK UNIFIED IDEOGRAPH
    0x9DF6, 0x6698, //#CJK UNIFIED IDEOGRAPH
    0x9DF7, 0x669D, //#CJK UNIFIED IDEOGRAPH
    0x9DF8, 0x66C1, //#CJK UNIFIED IDEOGRAPH
    0x9DF9, 0x66B9, //#CJK UNIFIED IDEOGRAPH
    0x9DFA, 0x66C9, //#CJK UNIFIED IDEOGRAPH
    0x9DFB, 0x66BE, //#CJK UNIFIED IDEOGRAPH
    0x9DFC, 0x66BC, //#CJK UNIFIED IDEOGRAPH
    0x9E40, 0x66C4, //#CJK UNIFIED IDEOGRAPH
    0x9E41, 0x66B8, //#CJK UNIFIED IDEOGRAPH
    0x9E42, 0x66D6, //#CJK UNIFIED IDEOGRAPH
    0x9E43, 0x66DA, //#CJK UNIFIED IDEOGRAPH
    0x9E44, 0x66E0, //#CJK UNIFIED IDEOGRAPH
    0x9E45, 0x663F, //#CJK UNIFIED IDEOGRAPH
    0x9E46, 0x66E6, //#CJK UNIFIED IDEOGRAPH
    0x9E47, 0x66E9, //#CJK UNIFIED IDEOGRAPH
    0x9E48, 0x66F0, //#CJK UNIFIED IDEOGRAPH
    0x9E49, 0x66F5, //#CJK UNIFIED IDEOGRAPH
    0x9E4A, 0x66F7, //#CJK UNIFIED IDEOGRAPH
    0x9E4B, 0x670F, //#CJK UNIFIED IDEOGRAPH
    0x9E4C, 0x6716, //#CJK UNIFIED IDEOGRAPH
    0x9E4D, 0x671E, //#CJK UNIFIED IDEOGRAPH
    0x9E4E, 0x6726, //#CJK UNIFIED IDEOGRAPH
    0x9E4F, 0x6727, //#CJK UNIFIED IDEOGRAPH
    0x9E50, 0x9738, //#CJK UNIFIED IDEOGRAPH
    0x9E51, 0x672E, //#CJK UNIFIED IDEOGRAPH
    0x9E52, 0x673F, //#CJK UNIFIED IDEOGRAPH
    0x9E53, 0x6736, //#CJK UNIFIED IDEOGRAPH
    0x9E54, 0x6741, //#CJK UNIFIED IDEOGRAPH
    0x9E55, 0x6738, //#CJK UNIFIED IDEOGRAPH
    0x9E56, 0x6737, //#CJK UNIFIED IDEOGRAPH
    0x9E57, 0x6746, //#CJK UNIFIED IDEOGRAPH
    0x9E58, 0x675E, //#CJK UNIFIED IDEOGRAPH
    0x9E59, 0x6760, //#CJK UNIFIED IDEOGRAPH
    0x9E5A, 0x6759, //#CJK UNIFIED IDEOGRAPH
    0x9E5B, 0x6763, //#CJK UNIFIED IDEOGRAPH
    0x9E5C, 0x6764, //#CJK UNIFIED IDEOGRAPH
    0x9E5D, 0x6789, //#CJK UNIFIED IDEOGRAPH
    0x9E5E, 0x6770, //#CJK UNIFIED IDEOGRAPH
    0x9E5F, 0x67A9, //#CJK UNIFIED IDEOGRAPH
    0x9E60, 0x677C, //#CJK UNIFIED IDEOGRAPH
    0x9E61, 0x676A, //#CJK UNIFIED IDEOGRAPH
    0x9E62, 0x678C, //#CJK UNIFIED IDEOGRAPH
    0x9E63, 0x678B, //#CJK UNIFIED IDEOGRAPH
    0x9E64, 0x67A6, //#CJK UNIFIED IDEOGRAPH
    0x9E65, 0x67A1, //#CJK UNIFIED IDEOGRAPH
    0x9E66, 0x6785, //#CJK UNIFIED IDEOGRAPH
    0x9E67, 0x67B7, //#CJK UNIFIED IDEOGRAPH
    0x9E68, 0x67EF, //#CJK UNIFIED IDEOGRAPH
    0x9E69, 0x67B4, //#CJK UNIFIED IDEOGRAPH
    0x9E6A, 0x67EC, //#CJK UNIFIED IDEOGRAPH
    0x9E6B, 0x67B3, //#CJK UNIFIED IDEOGRAPH
    0x9E6C, 0x67E9, //#CJK UNIFIED IDEOGRAPH
    0x9E6D, 0x67B8, //#CJK UNIFIED IDEOGRAPH
    0x9E6E, 0x67E4, //#CJK UNIFIED IDEOGRAPH
    0x9E6F, 0x67DE, //#CJK UNIFIED IDEOGRAPH
    0x9E70, 0x67DD, //#CJK UNIFIED IDEOGRAPH
    0x9E71, 0x67E2, //#CJK UNIFIED IDEOGRAPH
    0x9E72, 0x67EE, //#CJK UNIFIED IDEOGRAPH
    0x9E73, 0x67B9, //#CJK UNIFIED IDEOGRAPH
    0x9E74, 0x67CE, //#CJK UNIFIED IDEOGRAPH
    0x9E75, 0x67C6, //#CJK UNIFIED IDEOGRAPH
    0x9E76, 0x67E7, //#CJK UNIFIED IDEOGRAPH
    0x9E77, 0x6A9C, //#CJK UNIFIED IDEOGRAPH
    0x9E78, 0x681E, //#CJK UNIFIED IDEOGRAPH
    0x9E79, 0x6846, //#CJK UNIFIED IDEOGRAPH
    0x9E7A, 0x6829, //#CJK UNIFIED IDEOGRAPH
    0x9E7B, 0x6840, //#CJK UNIFIED IDEOGRAPH
    0x9E7C, 0x684D, //#CJK UNIFIED IDEOGRAPH
    0x9E7D, 0x6832, //#CJK UNIFIED IDEOGRAPH
    0x9E7E, 0x684E, //#CJK UNIFIED IDEOGRAPH
    0x9E80, 0x68B3, //#CJK UNIFIED IDEOGRAPH
    0x9E81, 0x682B, //#CJK UNIFIED IDEOGRAPH
    0x9E82, 0x6859, //#CJK UNIFIED IDEOGRAPH
    0x9E83, 0x6863, //#CJK UNIFIED IDEOGRAPH
    0x9E84, 0x6877, //#CJK UNIFIED IDEOGRAPH
    0x9E85, 0x687F, //#CJK UNIFIED IDEOGRAPH
    0x9E86, 0x689F, //#CJK UNIFIED IDEOGRAPH
    0x9E87, 0x688F, //#CJK UNIFIED IDEOGRAPH
    0x9E88, 0x68AD, //#CJK UNIFIED IDEOGRAPH
    0x9E89, 0x6894, //#CJK UNIFIED IDEOGRAPH
    0x9E8A, 0x689D, //#CJK UNIFIED IDEOGRAPH
    0x9E8B, 0x689B, //#CJK UNIFIED IDEOGRAPH
    0x9E8C, 0x6883, //#CJK UNIFIED IDEOGRAPH
    0x9E8D, 0x6AAE, //#CJK UNIFIED IDEOGRAPH
    0x9E8E, 0x68B9, //#CJK UNIFIED IDEOGRAPH
    0x9E8F, 0x6874, //#CJK UNIFIED IDEOGRAPH
    0x9E90, 0x68B5, //#CJK UNIFIED IDEOGRAPH
    0x9E91, 0x68A0, //#CJK UNIFIED IDEOGRAPH
    0x9E92, 0x68BA, //#CJK UNIFIED IDEOGRAPH
    0x9E93, 0x690F, //#CJK UNIFIED IDEOGRAPH
    0x9E94, 0x688D, //#CJK UNIFIED IDEOGRAPH
    0x9E95, 0x687E, //#CJK UNIFIED IDEOGRAPH
    0x9E96, 0x6901, //#CJK UNIFIED IDEOGRAPH
    0x9E97, 0x68CA, //#CJK UNIFIED IDEOGRAPH
    0x9E98, 0x6908, //#CJK UNIFIED IDEOGRAPH
    0x9E99, 0x68D8, //#CJK UNIFIED IDEOGRAPH
    0x9E9A, 0x6922, //#CJK UNIFIED IDEOGRAPH
    0x9E9B, 0x6926, //#CJK UNIFIED IDEOGRAPH
    0x9E9C, 0x68E1, //#CJK UNIFIED IDEOGRAPH
    0x9E9D, 0x690C, //#CJK UNIFIED IDEOGRAPH
    0x9E9E, 0x68CD, //#CJK UNIFIED IDEOGRAPH
    0x9E9F, 0x68D4, //#CJK UNIFIED IDEOGRAPH
    0x9EA0, 0x68E7, //#CJK UNIFIED IDEOGRAPH
    0x9EA1, 0x68D5, //#CJK UNIFIED IDEOGRAPH
    0x9EA2, 0x6936, //#CJK UNIFIED IDEOGRAPH
    0x9EA3, 0x6912, //#CJK UNIFIED IDEOGRAPH
    0x9EA4, 0x6904, //#CJK UNIFIED IDEOGRAPH
    0x9EA5, 0x68D7, //#CJK UNIFIED IDEOGRAPH
    0x9EA6, 0x68E3, //#CJK UNIFIED IDEOGRAPH
    0x9EA7, 0x6925, //#CJK UNIFIED IDEOGRAPH
    0x9EA8, 0x68F9, //#CJK UNIFIED IDEOGRAPH
    0x9EA9, 0x68E0, //#CJK UNIFIED IDEOGRAPH
    0x9EAA, 0x68EF, //#CJK UNIFIED IDEOGRAPH
    0x9EAB, 0x6928, //#CJK UNIFIED IDEOGRAPH
    0x9EAC, 0x692A, //#CJK UNIFIED IDEOGRAPH
    0x9EAD, 0x691A, //#CJK UNIFIED IDEOGRAPH
    0x9EAE, 0x6923, //#CJK UNIFIED IDEOGRAPH
    0x9EAF, 0x6921, //#CJK UNIFIED IDEOGRAPH
    0x9EB0, 0x68C6, //#CJK UNIFIED IDEOGRAPH
    0x9EB1, 0x6979, //#CJK UNIFIED IDEOGRAPH
    0x9EB2, 0x6977, //#CJK UNIFIED IDEOGRAPH
    0x9EB3, 0x695C, //#CJK UNIFIED IDEOGRAPH
    0x9EB4, 0x6978, //#CJK UNIFIED IDEOGRAPH
    0x9EB5, 0x696B, //#CJK UNIFIED IDEOGRAPH
    0x9EB6, 0x6954, //#CJK UNIFIED IDEOGRAPH
    0x9EB7, 0x697E, //#CJK UNIFIED IDEOGRAPH
    0x9EB8, 0x696E, //#CJK UNIFIED IDEOGRAPH
    0x9EB9, 0x6939, //#CJK UNIFIED IDEOGRAPH
    0x9EBA, 0x6974, //#CJK UNIFIED IDEOGRAPH
    0x9EBB, 0x693D, //#CJK UNIFIED IDEOGRAPH
    0x9EBC, 0x6959, //#CJK UNIFIED IDEOGRAPH
    0x9EBD, 0x6930, //#CJK UNIFIED IDEOGRAPH
    0x9EBE, 0x6961, //#CJK UNIFIED IDEOGRAPH
    0x9EBF, 0x695E, //#CJK UNIFIED IDEOGRAPH
    0x9EC0, 0x695D, //#CJK UNIFIED IDEOGRAPH
    0x9EC1, 0x6981, //#CJK UNIFIED IDEOGRAPH
    0x9EC2, 0x696A, //#CJK UNIFIED IDEOGRAPH
    0x9EC3, 0x69B2, //#CJK UNIFIED IDEOGRAPH
    0x9EC4, 0x69AE, //#CJK UNIFIED IDEOGRAPH
    0x9EC5, 0x69D0, //#CJK UNIFIED IDEOGRAPH
    0x9EC6, 0x69BF, //#CJK UNIFIED IDEOGRAPH
    0x9EC7, 0x69C1, //#CJK UNIFIED IDEOGRAPH
    0x9EC8, 0x69D3, //#CJK UNIFIED IDEOGRAPH
    0x9EC9, 0x69BE, //#CJK UNIFIED IDEOGRAPH
    0x9ECA, 0x69CE, //#CJK UNIFIED IDEOGRAPH
    0x9ECB, 0x5BE8, //#CJK UNIFIED IDEOGRAPH
    0x9ECC, 0x69CA, //#CJK UNIFIED IDEOGRAPH
    0x9ECD, 0x69DD, //#CJK UNIFIED IDEOGRAPH
    0x9ECE, 0x69BB, //#CJK UNIFIED IDEOGRAPH
    0x9ECF, 0x69C3, //#CJK UNIFIED IDEOGRAPH
    0x9ED0, 0x69A7, //#CJK UNIFIED IDEOGRAPH
    0x9ED1, 0x6A2E, //#CJK UNIFIED IDEOGRAPH
    0x9ED2, 0x6991, //#CJK UNIFIED IDEOGRAPH
    0x9ED3, 0x69A0, //#CJK UNIFIED IDEOGRAPH
    0x9ED4, 0x699C, //#CJK UNIFIED IDEOGRAPH
    0x9ED5, 0x6995, //#CJK UNIFIED IDEOGRAPH
    0x9ED6, 0x69B4, //#CJK UNIFIED IDEOGRAPH
    0x9ED7, 0x69DE, //#CJK UNIFIED IDEOGRAPH
    0x9ED8, 0x69E8, //#CJK UNIFIED IDEOGRAPH
    0x9ED9, 0x6A02, //#CJK UNIFIED IDEOGRAPH
    0x9EDA, 0x6A1B, //#CJK UNIFIED IDEOGRAPH
    0x9EDB, 0x69FF, //#CJK UNIFIED IDEOGRAPH
    0x9EDC, 0x6B0A, //#CJK UNIFIED IDEOGRAPH
    0x9EDD, 0x69F9, //#CJK UNIFIED IDEOGRAPH
    0x9EDE, 0x69F2, //#CJK UNIFIED IDEOGRAPH
    0x9EDF, 0x69E7, //#CJK UNIFIED IDEOGRAPH
    0x9EE0, 0x6A05, //#CJK UNIFIED IDEOGRAPH
    0x9EE1, 0x69B1, //#CJK UNIFIED IDEOGRAPH
    0x9EE2, 0x6A1E, //#CJK UNIFIED IDEOGRAPH
    0x9EE3, 0x69ED, //#CJK UNIFIED IDEOGRAPH
    0x9EE4, 0x6A14, //#CJK UNIFIED IDEOGRAPH
    0x9EE5, 0x69EB, //#CJK UNIFIED IDEOGRAPH
    0x9EE6, 0x6A0A, //#CJK UNIFIED IDEOGRAPH
    0x9EE7, 0x6A12, //#CJK UNIFIED IDEOGRAPH
    0x9EE8, 0x6AC1, //#CJK UNIFIED IDEOGRAPH
    0x9EE9, 0x6A23, //#CJK UNIFIED IDEOGRAPH
    0x9EEA, 0x6A13, //#CJK UNIFIED IDEOGRAPH
    0x9EEB, 0x6A44, //#CJK UNIFIED IDEOGRAPH
    0x9EEC, 0x6A0C, //#CJK UNIFIED IDEOGRAPH
    0x9EED, 0x6A72, //#CJK UNIFIED IDEOGRAPH
    0x9EEE, 0x6A36, //#CJK UNIFIED IDEOGRAPH
    0x9EEF, 0x6A78, //#CJK UNIFIED IDEOGRAPH
    0x9EF0, 0x6A47, //#CJK UNIFIED IDEOGRAPH
    0x9EF1, 0x6A62, //#CJK UNIFIED IDEOGRAPH
    0x9EF2, 0x6A59, //#CJK UNIFIED IDEOGRAPH
    0x9EF3, 0x6A66, //#CJK UNIFIED IDEOGRAPH
    0x9EF4, 0x6A48, //#CJK UNIFIED IDEOGRAPH
    0x9EF5, 0x6A38, //#CJK UNIFIED IDEOGRAPH
    0x9EF6, 0x6A22, //#CJK UNIFIED IDEOGRAPH
    0x9EF7, 0x6A90, //#CJK UNIFIED IDEOGRAPH
    0x9EF8, 0x6A8D, //#CJK UNIFIED IDEOGRAPH
    0x9EF9, 0x6AA0, //#CJK UNIFIED IDEOGRAPH
    0x9EFA, 0x6A84, //#CJK UNIFIED IDEOGRAPH
    0x9EFB, 0x6AA2, //#CJK UNIFIED IDEOGRAPH
    0x9EFC, 0x6AA3, //#CJK UNIFIED IDEOGRAPH
    0x9F40, 0x6A97, //#CJK UNIFIED IDEOGRAPH
    0x9F41, 0x8617, //#CJK UNIFIED IDEOGRAPH
    0x9F42, 0x6ABB, //#CJK UNIFIED IDEOGRAPH
    0x9F43, 0x6AC3, //#CJK UNIFIED IDEOGRAPH
    0x9F44, 0x6AC2, //#CJK UNIFIED IDEOGRAPH
    0x9F45, 0x6AB8, //#CJK UNIFIED IDEOGRAPH
    0x9F46, 0x6AB3, //#CJK UNIFIED IDEOGRAPH
    0x9F47, 0x6AAC, //#CJK UNIFIED IDEOGRAPH
    0x9F48, 0x6ADE, //#CJK UNIFIED IDEOGRAPH
    0x9F49, 0x6AD1, //#CJK UNIFIED IDEOGRAPH
    0x9F4A, 0x6ADF, //#CJK UNIFIED IDEOGRAPH
    0x9F4B, 0x6AAA, //#CJK UNIFIED IDEOGRAPH
    0x9F4C, 0x6ADA, //#CJK UNIFIED IDEOGRAPH
    0x9F4D, 0x6AEA, //#CJK UNIFIED IDEOGRAPH
    0x9F4E, 0x6AFB, //#CJK UNIFIED IDEOGRAPH
    0x9F4F, 0x6B05, //#CJK UNIFIED IDEOGRAPH
    0x9F50, 0x8616, //#CJK UNIFIED IDEOGRAPH
    0x9F51, 0x6AFA, //#CJK UNIFIED IDEOGRAPH
    0x9F52, 0x6B12, //#CJK UNIFIED IDEOGRAPH
    0x9F53, 0x6B16, //#CJK UNIFIED IDEOGRAPH
    0x9F54, 0x9B31, //#CJK UNIFIED IDEOGRAPH
    0x9F55, 0x6B1F, //#CJK UNIFIED IDEOGRAPH
    0x9F56, 0x6B38, //#CJK UNIFIED IDEOGRAPH
    0x9F57, 0x6B37, //#CJK UNIFIED IDEOGRAPH
    0x9F58, 0x76DC, //#CJK UNIFIED IDEOGRAPH
    0x9F59, 0x6B39, //#CJK UNIFIED IDEOGRAPH
    0x9F5A, 0x98EE, //#CJK UNIFIED IDEOGRAPH
    0x9F5B, 0x6B47, //#CJK UNIFIED IDEOGRAPH
    0x9F5C, 0x6B43, //#CJK UNIFIED IDEOGRAPH
    0x9F5D, 0x6B49, //#CJK UNIFIED IDEOGRAPH
    0x9F5E, 0x6B50, //#CJK UNIFIED IDEOGRAPH
    0x9F5F, 0x6B59, //#CJK UNIFIED IDEOGRAPH
    0x9F60, 0x6B54, //#CJK UNIFIED IDEOGRAPH
    0x9F61, 0x6B5B, //#CJK UNIFIED IDEOGRAPH
    0x9F62, 0x6B5F, //#CJK UNIFIED IDEOGRAPH
    0x9F63, 0x6B61, //#CJK UNIFIED IDEOGRAPH
    0x9F64, 0x6B78, //#CJK UNIFIED IDEOGRAPH
    0x9F65, 0x6B79, //#CJK UNIFIED IDEOGRAPH
    0x9F66, 0x6B7F, //#CJK UNIFIED IDEOGRAPH
    0x9F67, 0x6B80, //#CJK UNIFIED IDEOGRAPH
    0x9F68, 0x6B84, //#CJK UNIFIED IDEOGRAPH
    0x9F69, 0x6B83, //#CJK UNIFIED IDEOGRAPH
    0x9F6A, 0x6B8D, //#CJK UNIFIED IDEOGRAPH
    0x9F6B, 0x6B98, //#CJK UNIFIED IDEOGRAPH
    0x9F6C, 0x6B95, //#CJK UNIFIED IDEOGRAPH
    0x9F6D, 0x6B9E, //#CJK UNIFIED IDEOGRAPH
    0x9F6E, 0x6BA4, //#CJK UNIFIED IDEOGRAPH
    0x9F6F, 0x6BAA, //#CJK UNIFIED IDEOGRAPH
    0x9F70, 0x6BAB, //#CJK UNIFIED IDEOGRAPH
    0x9F71, 0x6BAF, //#CJK UNIFIED IDEOGRAPH
    0x9F72, 0x6BB2, //#CJK UNIFIED IDEOGRAPH
    0x9F73, 0x6BB1, //#CJK UNIFIED IDEOGRAPH
    0x9F74, 0x6BB3, //#CJK UNIFIED IDEOGRAPH
    0x9F75, 0x6BB7, //#CJK UNIFIED IDEOGRAPH
    0x9F76, 0x6BBC, //#CJK UNIFIED IDEOGRAPH
    0x9F77, 0x6BC6, //#CJK UNIFIED IDEOGRAPH
    0x9F78, 0x6BCB, //#CJK UNIFIED IDEOGRAPH
    0x9F79, 0x6BD3, //#CJK UNIFIED IDEOGRAPH
    0x9F7A, 0x6BDF, //#CJK UNIFIED IDEOGRAPH
    0x9F7B, 0x6BEC, //#CJK UNIFIED IDEOGRAPH
    0x9F7C, 0x6BEB, //#CJK UNIFIED IDEOGRAPH
    0x9F7D, 0x6BF3, //#CJK UNIFIED IDEOGRAPH
    0x9F7E, 0x6BEF, //#CJK UNIFIED IDEOGRAPH
    0x9F80, 0x9EBE, //#CJK UNIFIED IDEOGRAPH
    0x9F81, 0x6C08, //#CJK UNIFIED IDEOGRAPH
    0x9F82, 0x6C13, //#CJK UNIFIED IDEOGRAPH
    0x9F83, 0x6C14, //#CJK UNIFIED IDEOGRAPH
    0x9F84, 0x6C1B, //#CJK UNIFIED IDEOGRAPH
    0x9F85, 0x6C24, //#CJK UNIFIED IDEOGRAPH
    0x9F86, 0x6C23, //#CJK UNIFIED IDEOGRAPH
    0x9F87, 0x6C5E, //#CJK UNIFIED IDEOGRAPH
    0x9F88, 0x6C55, //#CJK UNIFIED IDEOGRAPH
    0x9F89, 0x6C62, //#CJK UNIFIED IDEOGRAPH
    0x9F8A, 0x6C6A, //#CJK UNIFIED IDEOGRAPH
    0x9F8B, 0x6C82, //#CJK UNIFIED IDEOGRAPH
    0x9F8C, 0x6C8D, //#CJK UNIFIED IDEOGRAPH
    0x9F8D, 0x6C9A, //#CJK UNIFIED IDEOGRAPH
    0x9F8E, 0x6C81, //#CJK UNIFIED IDEOGRAPH
    0x9F8F, 0x6C9B, //#CJK UNIFIED IDEOGRAPH
    0x9F90, 0x6C7E, //#CJK UNIFIED IDEOGRAPH
    0x9F91, 0x6C68, //#CJK UNIFIED IDEOGRAPH
    0x9F92, 0x6C73, //#CJK UNIFIED IDEOGRAPH
    0x9F93, 0x6C92, //#CJK UNIFIED IDEOGRAPH
    0x9F94, 0x6C90, //#CJK UNIFIED IDEOGRAPH
    0x9F95, 0x6CC4, //#CJK UNIFIED IDEOGRAPH
    0x9F96, 0x6CF1, //#CJK UNIFIED IDEOGRAPH
    0x9F97, 0x6CD3, //#CJK UNIFIED IDEOGRAPH
    0x9F98, 0x6CBD, //#CJK UNIFIED IDEOGRAPH
    0x9F99, 0x6CD7, //#CJK UNIFIED IDEOGRAPH
    0x9F9A, 0x6CC5, //#CJK UNIFIED IDEOGRAPH
    0x9F9B, 0x6CDD, //#CJK UNIFIED IDEOGRAPH
    0x9F9C, 0x6CAE, //#CJK UNIFIED IDEOGRAPH
    0x9F9D, 0x6CB1, //#CJK UNIFIED IDEOGRAPH
    0x9F9E, 0x6CBE, //#CJK UNIFIED IDEOGRAPH
    0x9F9F, 0x6CBA, //#CJK UNIFIED IDEOGRAPH
    0x9FA0, 0x6CDB, //#CJK UNIFIED IDEOGRAPH
    0x9FA1, 0x6CEF, //#CJK UNIFIED IDEOGRAPH
    0x9FA2, 0x6CD9, //#CJK UNIFIED IDEOGRAPH
    0x9FA3, 0x6CEA, //#CJK UNIFIED IDEOGRAPH
    0x9FA4, 0x6D1F, //#CJK UNIFIED IDEOGRAPH
    0x9FA5, 0x884D, //#CJK UNIFIED IDEOGRAPH
    0x9FA6, 0x6D36, //#CJK UNIFIED IDEOGRAPH
    0x9FA7, 0x6D2B, //#CJK UNIFIED IDEOGRAPH
    0x9FA8, 0x6D3D, //#CJK UNIFIED IDEOGRAPH
    0x9FA9, 0x6D38, //#CJK UNIFIED IDEOGRAPH
    0x9FAA, 0x6D19, //#CJK UNIFIED IDEOGRAPH
    0x9FAB, 0x6D35, //#CJK UNIFIED IDEOGRAPH
    0x9FAC, 0x6D33, //#CJK UNIFIED IDEOGRAPH
    0x9FAD, 0x6D12, //#CJK UNIFIED IDEOGRAPH
    0x9FAE, 0x6D0C, //#CJK UNIFIED IDEOGRAPH
    0x9FAF, 0x6D63, //#CJK UNIFIED IDEOGRAPH
    0x9FB0, 0x6D93, //#CJK UNIFIED IDEOGRAPH
    0x9FB1, 0x6D64, //#CJK UNIFIED IDEOGRAPH
    0x9FB2, 0x6D5A, //#CJK UNIFIED IDEOGRAPH
    0x9FB3, 0x6D79, //#CJK UNIFIED IDEOGRAPH
    0x9FB4, 0x6D59, //#CJK UNIFIED IDEOGRAPH
    0x9FB5, 0x6D8E, //#CJK UNIFIED IDEOGRAPH
    0x9FB6, 0x6D95, //#CJK UNIFIED IDEOGRAPH
    0x9FB7, 0x6FE4, //#CJK UNIFIED IDEOGRAPH
    0x9FB8, 0x6D85, //#CJK UNIFIED IDEOGRAPH
    0x9FB9, 0x6DF9, //#CJK UNIFIED IDEOGRAPH
    0x9FBA, 0x6E15, //#CJK UNIFIED IDEOGRAPH
    0x9FBB, 0x6E0A, //#CJK UNIFIED IDEOGRAPH
    0x9FBC, 0x6DB5, //#CJK UNIFIED IDEOGRAPH
    0x9FBD, 0x6DC7, //#CJK UNIFIED IDEOGRAPH
    0x9FBE, 0x6DE6, //#CJK UNIFIED IDEOGRAPH
    0x9FBF, 0x6DB8, //#CJK UNIFIED IDEOGRAPH
    0x9FC0, 0x6DC6, //#CJK UNIFIED IDEOGRAPH
    0x9FC1, 0x6DEC, //#CJK UNIFIED IDEOGRAPH
    0x9FC2, 0x6DDE, //#CJK UNIFIED IDEOGRAPH
    0x9FC3, 0x6DCC, //#CJK UNIFIED IDEOGRAPH
    0x9FC4, 0x6DE8, //#CJK UNIFIED IDEOGRAPH
    0x9FC5, 0x6DD2, //#CJK UNIFIED IDEOGRAPH
    0x9FC6, 0x6DC5, //#CJK UNIFIED IDEOGRAPH
    0x9FC7, 0x6DFA, //#CJK UNIFIED IDEOGRAPH
    0x9FC8, 0x6DD9, //#CJK UNIFIED IDEOGRAPH
    0x9FC9, 0x6DE4, //#CJK UNIFIED IDEOGRAPH
    0x9FCA, 0x6DD5, //#CJK UNIFIED IDEOGRAPH
    0x9FCB, 0x6DEA, //#CJK UNIFIED IDEOGRAPH
    0x9FCC, 0x6DEE, //#CJK UNIFIED IDEOGRAPH
    0x9FCD, 0x6E2D, //#CJK UNIFIED IDEOGRAPH
    0x9FCE, 0x6E6E, //#CJK UNIFIED IDEOGRAPH
    0x9FCF, 0x6E2E, //#CJK UNIFIED IDEOGRAPH
    0x9FD0, 0x6E19, //#CJK UNIFIED IDEOGRAPH
    0x9FD1, 0x6E72, //#CJK UNIFIED IDEOGRAPH
    0x9FD2, 0x6E5F, //#CJK UNIFIED IDEOGRAPH
    0x9FD3, 0x6E3E, //#CJK UNIFIED IDEOGRAPH
    0x9FD4, 0x6E23, //#CJK UNIFIED IDEOGRAPH
    0x9FD5, 0x6E6B, //#CJK UNIFIED IDEOGRAPH
    0x9FD6, 0x6E2B, //#CJK UNIFIED IDEOGRAPH
    0x9FD7, 0x6E76, //#CJK UNIFIED IDEOGRAPH
    0x9FD8, 0x6E4D, //#CJK UNIFIED IDEOGRAPH
    0x9FD9, 0x6E1F, //#CJK UNIFIED IDEOGRAPH
    0x9FDA, 0x6E43, //#CJK UNIFIED IDEOGRAPH
    0x9FDB, 0x6E3A, //#CJK UNIFIED IDEOGRAPH
    0x9FDC, 0x6E4E, //#CJK UNIFIED IDEOGRAPH
    0x9FDD, 0x6E24, //#CJK UNIFIED IDEOGRAPH
    0x9FDE, 0x6EFF, //#CJK UNIFIED IDEOGRAPH
    0x9FDF, 0x6E1D, //#CJK UNIFIED IDEOGRAPH
    0x9FE0, 0x6E38, //#CJK UNIFIED IDEOGRAPH
    0x9FE1, 0x6E82, //#CJK UNIFIED IDEOGRAPH
    0x9FE2, 0x6EAA, //#CJK UNIFIED IDEOGRAPH
    0x9FE3, 0x6E98, //#CJK UNIFIED IDEOGRAPH
    0x9FE4, 0x6EC9, //#CJK UNIFIED IDEOGRAPH
    0x9FE5, 0x6EB7, //#CJK UNIFIED IDEOGRAPH
    0x9FE6, 0x6ED3, //#CJK UNIFIED IDEOGRAPH
    0x9FE7, 0x6EBD, //#CJK UNIFIED IDEOGRAPH
    0x9FE8, 0x6EAF, //#CJK UNIFIED IDEOGRAPH
    0x9FE9, 0x6EC4, //#CJK UNIFIED IDEOGRAPH
    0x9FEA, 0x6EB2, //#CJK UNIFIED IDEOGRAPH
    0x9FEB, 0x6ED4, //#CJK UNIFIED IDEOGRAPH
    0x9FEC, 0x6ED5, //#CJK UNIFIED IDEOGRAPH
    0x9FED, 0x6E8F, //#CJK UNIFIED IDEOGRAPH
    0x9FEE, 0x6EA5, //#CJK UNIFIED IDEOGRAPH
    0x9FEF, 0x6EC2, //#CJK UNIFIED IDEOGRAPH
    0x9FF0, 0x6E9F, //#CJK UNIFIED IDEOGRAPH
    0x9FF1, 0x6F41, //#CJK UNIFIED IDEOGRAPH
    0x9FF2, 0x6F11, //#CJK UNIFIED IDEOGRAPH
    0x9FF3, 0x704C, //#CJK UNIFIED IDEOGRAPH
    0x9FF4, 0x6EEC, //#CJK UNIFIED IDEOGRAPH
    0x9FF5, 0x6EF8, //#CJK UNIFIED IDEOGRAPH
    0x9FF6, 0x6EFE, //#CJK UNIFIED IDEOGRAPH
    0x9FF7, 0x6F3F, //#CJK UNIFIED IDEOGRAPH
    0x9FF8, 0x6EF2, //#CJK UNIFIED IDEOGRAPH
    0x9FF9, 0x6F31, //#CJK UNIFIED IDEOGRAPH
    0x9FFA, 0x6EEF, //#CJK UNIFIED IDEOGRAPH
    0x9FFB, 0x6F32, //#CJK UNIFIED IDEOGRAPH
    0x9FFC, 0x6ECC, //#CJK UNIFIED IDEOGRAPH
    0xE040, 0x6F3E, //#CJK UNIFIED IDEOGRAPH
    0xE041, 0x6F13, //#CJK UNIFIED IDEOGRAPH
    0xE042, 0x6EF7, //#CJK UNIFIED IDEOGRAPH
    0xE043, 0x6F86, //#CJK UNIFIED IDEOGRAPH
    0xE044, 0x6F7A, //#CJK UNIFIED IDEOGRAPH
    0xE045, 0x6F78, //#CJK UNIFIED IDEOGRAPH
    0xE046, 0x6F81, //#CJK UNIFIED IDEOGRAPH
    0xE047, 0x6F80, //#CJK UNIFIED IDEOGRAPH
    0xE048, 0x6F6F, //#CJK UNIFIED IDEOGRAPH
    0xE049, 0x6F5B, //#CJK UNIFIED IDEOGRAPH
    0xE04A, 0x6FF3, //#CJK UNIFIED IDEOGRAPH
    0xE04B, 0x6F6D, //#CJK UNIFIED IDEOGRAPH
    0xE04C, 0x6F82, //#CJK UNIFIED IDEOGRAPH
    0xE04D, 0x6F7C, //#CJK UNIFIED IDEOGRAPH
    0xE04E, 0x6F58, //#CJK UNIFIED IDEOGRAPH
    0xE04F, 0x6F8E, //#CJK UNIFIED IDEOGRAPH
    0xE050, 0x6F91, //#CJK UNIFIED IDEOGRAPH
    0xE051, 0x6FC2, //#CJK UNIFIED IDEOGRAPH
    0xE052, 0x6F66, //#CJK UNIFIED IDEOGRAPH
    0xE053, 0x6FB3, //#CJK UNIFIED IDEOGRAPH
    0xE054, 0x6FA3, //#CJK UNIFIED IDEOGRAPH
    0xE055, 0x6FA1, //#CJK UNIFIED IDEOGRAPH
    0xE056, 0x6FA4, //#CJK UNIFIED IDEOGRAPH
    0xE057, 0x6FB9, //#CJK UNIFIED IDEOGRAPH
    0xE058, 0x6FC6, //#CJK UNIFIED IDEOGRAPH
    0xE059, 0x6FAA, //#CJK UNIFIED IDEOGRAPH
    0xE05A, 0x6FDF, //#CJK UNIFIED IDEOGRAPH
    0xE05B, 0x6FD5, //#CJK UNIFIED IDEOGRAPH
    0xE05C, 0x6FEC, //#CJK UNIFIED IDEOGRAPH
    0xE05D, 0x6FD4, //#CJK UNIFIED IDEOGRAPH
    0xE05E, 0x6FD8, //#CJK UNIFIED IDEOGRAPH
    0xE05F, 0x6FF1, //#CJK UNIFIED IDEOGRAPH
    0xE060, 0x6FEE, //#CJK UNIFIED IDEOGRAPH
    0xE061, 0x6FDB, //#CJK UNIFIED IDEOGRAPH
    0xE062, 0x7009, //#CJK UNIFIED IDEOGRAPH
    0xE063, 0x700B, //#CJK UNIFIED IDEOGRAPH
    0xE064, 0x6FFA, //#CJK UNIFIED IDEOGRAPH
    0xE065, 0x7011, //#CJK UNIFIED IDEOGRAPH
    0xE066, 0x7001, //#CJK UNIFIED IDEOGRAPH
    0xE067, 0x700F, //#CJK UNIFIED IDEOGRAPH
    0xE068, 0x6FFE, //#CJK UNIFIED IDEOGRAPH
    0xE069, 0x701B, //#CJK UNIFIED IDEOGRAPH
    0xE06A, 0x701A, //#CJK UNIFIED IDEOGRAPH
    0xE06B, 0x6F74, //#CJK UNIFIED IDEOGRAPH
    0xE06C, 0x701D, //#CJK UNIFIED IDEOGRAPH
    0xE06D, 0x7018, //#CJK UNIFIED IDEOGRAPH
    0xE06E, 0x701F, //#CJK UNIFIED IDEOGRAPH
    0xE06F, 0x7030, //#CJK UNIFIED IDEOGRAPH
    0xE070, 0x703E, //#CJK UNIFIED IDEOGRAPH
    0xE071, 0x7032, //#CJK UNIFIED IDEOGRAPH
    0xE072, 0x7051, //#CJK UNIFIED IDEOGRAPH
    0xE073, 0x7063, //#CJK UNIFIED IDEOGRAPH
    0xE074, 0x7099, //#CJK UNIFIED IDEOGRAPH
    0xE075, 0x7092, //#CJK UNIFIED IDEOGRAPH
    0xE076, 0x70AF, //#CJK UNIFIED IDEOGRAPH
    0xE077, 0x70F1, //#CJK UNIFIED IDEOGRAPH
    0xE078, 0x70AC, //#CJK UNIFIED IDEOGRAPH
    0xE079, 0x70B8, //#CJK UNIFIED IDEOGRAPH
    0xE07A, 0x70B3, //#CJK UNIFIED IDEOGRAPH
    0xE07B, 0x70AE, //#CJK UNIFIED IDEOGRAPH
    0xE07C, 0x70DF, //#CJK UNIFIED IDEOGRAPH
    0xE07D, 0x70CB, //#CJK UNIFIED IDEOGRAPH
    0xE07E, 0x70DD, //#CJK UNIFIED IDEOGRAPH
    0xE080, 0x70D9, //#CJK UNIFIED IDEOGRAPH
    0xE081, 0x7109, //#CJK UNIFIED IDEOGRAPH
    0xE082, 0x70FD, //#CJK UNIFIED IDEOGRAPH
    0xE083, 0x711C, //#CJK UNIFIED IDEOGRAPH
    0xE084, 0x7119, //#CJK UNIFIED IDEOGRAPH
    0xE085, 0x7165, //#CJK UNIFIED IDEOGRAPH
    0xE086, 0x7155, //#CJK UNIFIED IDEOGRAPH
    0xE087, 0x7188, //#CJK UNIFIED IDEOGRAPH
    0xE088, 0x7166, //#CJK UNIFIED IDEOGRAPH
    0xE089, 0x7162, //#CJK UNIFIED IDEOGRAPH
    0xE08A, 0x714C, //#CJK UNIFIED IDEOGRAPH
    0xE08B, 0x7156, //#CJK UNIFIED IDEOGRAPH
    0xE08C, 0x716C, //#CJK UNIFIED IDEOGRAPH
    0xE08D, 0x718F, //#CJK UNIFIED IDEOGRAPH
    0xE08E, 0x71FB, //#CJK UNIFIED IDEOGRAPH
    0xE08F, 0x7184, //#CJK UNIFIED IDEOGRAPH
    0xE090, 0x7195, //#CJK UNIFIED IDEOGRAPH
    0xE091, 0x71A8, //#CJK UNIFIED IDEOGRAPH
    0xE092, 0x71AC, //#CJK UNIFIED IDEOGRAPH
    0xE093, 0x71D7, //#CJK UNIFIED IDEOGRAPH
    0xE094, 0x71B9, //#CJK UNIFIED IDEOGRAPH
    0xE095, 0x71BE, //#CJK UNIFIED IDEOGRAPH
    0xE096, 0x71D2, //#CJK UNIFIED IDEOGRAPH
    0xE097, 0x71C9, //#CJK UNIFIED IDEOGRAPH
    0xE098, 0x71D4, //#CJK UNIFIED IDEOGRAPH
    0xE099, 0x71CE, //#CJK UNIFIED IDEOGRAPH
    0xE09A, 0x71E0, //#CJK UNIFIED IDEOGRAPH
    0xE09B, 0x71EC, //#CJK UNIFIED IDEOGRAPH
    0xE09C, 0x71E7, //#CJK UNIFIED IDEOGRAPH
    0xE09D, 0x71F5, //#CJK UNIFIED IDEOGRAPH
    0xE09E, 0x71FC, //#CJK UNIFIED IDEOGRAPH
    0xE09F, 0x71F9, //#CJK UNIFIED IDEOGRAPH
    0xE0A0, 0x71FF, //#CJK UNIFIED IDEOGRAPH
    0xE0A1, 0x720D, //#CJK UNIFIED IDEOGRAPH
    0xE0A2, 0x7210, //#CJK UNIFIED IDEOGRAPH
    0xE0A3, 0x721B, //#CJK UNIFIED IDEOGRAPH
    0xE0A4, 0x7228, //#CJK UNIFIED IDEOGRAPH
    0xE0A5, 0x722D, //#CJK UNIFIED IDEOGRAPH
    0xE0A6, 0x722C, //#CJK UNIFIED IDEOGRAPH
    0xE0A7, 0x7230, //#CJK UNIFIED IDEOGRAPH
    0xE0A8, 0x7232, //#CJK UNIFIED IDEOGRAPH
    0xE0A9, 0x723B, //#CJK UNIFIED IDEOGRAPH
    0xE0AA, 0x723C, //#CJK UNIFIED IDEOGRAPH
    0xE0AB, 0x723F, //#CJK UNIFIED IDEOGRAPH
    0xE0AC, 0x7240, //#CJK UNIFIED IDEOGRAPH
    0xE0AD, 0x7246, //#CJK UNIFIED IDEOGRAPH
    0xE0AE, 0x724B, //#CJK UNIFIED IDEOGRAPH
    0xE0AF, 0x7258, //#CJK UNIFIED IDEOGRAPH
    0xE0B0, 0x7274, //#CJK UNIFIED IDEOGRAPH
    0xE0B1, 0x727E, //#CJK UNIFIED IDEOGRAPH
    0xE0B2, 0x7282, //#CJK UNIFIED IDEOGRAPH
    0xE0B3, 0x7281, //#CJK UNIFIED IDEOGRAPH
    0xE0B4, 0x7287, //#CJK UNIFIED IDEOGRAPH
    0xE0B5, 0x7292, //#CJK UNIFIED IDEOGRAPH
    0xE0B6, 0x7296, //#CJK UNIFIED IDEOGRAPH
    0xE0B7, 0x72A2, //#CJK UNIFIED IDEOGRAPH
    0xE0B8, 0x72A7, //#CJK UNIFIED IDEOGRAPH
    0xE0B9, 0x72B9, //#CJK UNIFIED IDEOGRAPH
    0xE0BA, 0x72B2, //#CJK UNIFIED IDEOGRAPH
    0xE0BB, 0x72C3, //#CJK UNIFIED IDEOGRAPH
    0xE0BC, 0x72C6, //#CJK UNIFIED IDEOGRAPH
    0xE0BD, 0x72C4, //#CJK UNIFIED IDEOGRAPH
    0xE0BE, 0x72CE, //#CJK UNIFIED IDEOGRAPH
    0xE0BF, 0x72D2, //#CJK UNIFIED IDEOGRAPH
    0xE0C0, 0x72E2, //#CJK UNIFIED IDEOGRAPH
    0xE0C1, 0x72E0, //#CJK UNIFIED IDEOGRAPH
    0xE0C2, 0x72E1, //#CJK UNIFIED IDEOGRAPH
    0xE0C3, 0x72F9, //#CJK UNIFIED IDEOGRAPH
    0xE0C4, 0x72F7, //#CJK UNIFIED IDEOGRAPH
    0xE0C5, 0x500F, //#CJK UNIFIED IDEOGRAPH
    0xE0C6, 0x7317, //#CJK UNIFIED IDEOGRAPH
    0xE0C7, 0x730A, //#CJK UNIFIED IDEOGRAPH
    0xE0C8, 0x731C, //#CJK UNIFIED IDEOGRAPH
    0xE0C9, 0x7316, //#CJK UNIFIED IDEOGRAPH
    0xE0CA, 0x731D, //#CJK UNIFIED IDEOGRAPH
    0xE0CB, 0x7334, //#CJK UNIFIED IDEOGRAPH
    0xE0CC, 0x732F, //#CJK UNIFIED IDEOGRAPH
    0xE0CD, 0x7329, //#CJK UNIFIED IDEOGRAPH
    0xE0CE, 0x7325, //#CJK UNIFIED IDEOGRAPH
    0xE0CF, 0x733E, //#CJK UNIFIED IDEOGRAPH
    0xE0D0, 0x734E, //#CJK UNIFIED IDEOGRAPH
    0xE0D1, 0x734F, //#CJK UNIFIED IDEOGRAPH
    0xE0D2, 0x9ED8, //#CJK UNIFIED IDEOGRAPH
    0xE0D3, 0x7357, //#CJK UNIFIED IDEOGRAPH
    0xE0D4, 0x736A, //#CJK UNIFIED IDEOGRAPH
    0xE0D5, 0x7368, //#CJK UNIFIED IDEOGRAPH
    0xE0D6, 0x7370, //#CJK UNIFIED IDEOGRAPH
    0xE0D7, 0x7378, //#CJK UNIFIED IDEOGRAPH
    0xE0D8, 0x7375, //#CJK UNIFIED IDEOGRAPH
    0xE0D9, 0x737B, //#CJK UNIFIED IDEOGRAPH
    0xE0DA, 0x737A, //#CJK UNIFIED IDEOGRAPH
    0xE0DB, 0x73C8, //#CJK UNIFIED IDEOGRAPH
    0xE0DC, 0x73B3, //#CJK UNIFIED IDEOGRAPH
    0xE0DD, 0x73CE, //#CJK UNIFIED IDEOGRAPH
    0xE0DE, 0x73BB, //#CJK UNIFIED IDEOGRAPH
    0xE0DF, 0x73C0, //#CJK UNIFIED IDEOGRAPH
    0xE0E0, 0x73E5, //#CJK UNIFIED IDEOGRAPH
    0xE0E1, 0x73EE, //#CJK UNIFIED IDEOGRAPH
    0xE0E2, 0x73DE, //#CJK UNIFIED IDEOGRAPH
    0xE0E3, 0x74A2, //#CJK UNIFIED IDEOGRAPH
    0xE0E4, 0x7405, //#CJK UNIFIED IDEOGRAPH
    0xE0E5, 0x746F, //#CJK UNIFIED IDEOGRAPH
    0xE0E6, 0x7425, //#CJK UNIFIED IDEOGRAPH
    0xE0E7, 0x73F8, //#CJK UNIFIED IDEOGRAPH
    0xE0E8, 0x7432, //#CJK UNIFIED IDEOGRAPH
    0xE0E9, 0x743A, //#CJK UNIFIED IDEOGRAPH
    0xE0EA, 0x7455, //#CJK UNIFIED IDEOGRAPH
    0xE0EB, 0x743F, //#CJK UNIFIED IDEOGRAPH
    0xE0EC, 0x745F, //#CJK UNIFIED IDEOGRAPH
    0xE0ED, 0x7459, //#CJK UNIFIED IDEOGRAPH
    0xE0EE, 0x7441, //#CJK UNIFIED IDEOGRAPH
    0xE0EF, 0x745C, //#CJK UNIFIED IDEOGRAPH
    0xE0F0, 0x7469, //#CJK UNIFIED IDEOGRAPH
    0xE0F1, 0x7470, //#CJK UNIFIED IDEOGRAPH
    0xE0F2, 0x7463, //#CJK UNIFIED IDEOGRAPH
    0xE0F3, 0x746A, //#CJK UNIFIED IDEOGRAPH
    0xE0F4, 0x7476, //#CJK UNIFIED IDEOGRAPH
    0xE0F5, 0x747E, //#CJK UNIFIED IDEOGRAPH
    0xE0F6, 0x748B, //#CJK UNIFIED IDEOGRAPH
    0xE0F7, 0x749E, //#CJK UNIFIED IDEOGRAPH
    0xE0F8, 0x74A7, //#CJK UNIFIED IDEOGRAPH
    0xE0F9, 0x74CA, //#CJK UNIFIED IDEOGRAPH
    0xE0FA, 0x74CF, //#CJK UNIFIED IDEOGRAPH
    0xE0FB, 0x74D4, //#CJK UNIFIED IDEOGRAPH
    0xE0FC, 0x73F1, //#CJK UNIFIED IDEOGRAPH
    0xE140, 0x74E0, //#CJK UNIFIED IDEOGRAPH
    0xE141, 0x74E3, //#CJK UNIFIED IDEOGRAPH
    0xE142, 0x74E7, //#CJK UNIFIED IDEOGRAPH
    0xE143, 0x74E9, //#CJK UNIFIED IDEOGRAPH
    0xE144, 0x74EE, //#CJK UNIFIED IDEOGRAPH
    0xE145, 0x74F2, //#CJK UNIFIED IDEOGRAPH
    0xE146, 0x74F0, //#CJK UNIFIED IDEOGRAPH
    0xE147, 0x74F1, //#CJK UNIFIED IDEOGRAPH
    0xE148, 0x74F8, //#CJK UNIFIED IDEOGRAPH
    0xE149, 0x74F7, //#CJK UNIFIED IDEOGRAPH
    0xE14A, 0x7504, //#CJK UNIFIED IDEOGRAPH
    0xE14B, 0x7503, //#CJK UNIFIED IDEOGRAPH
    0xE14C, 0x7505, //#CJK UNIFIED IDEOGRAPH
    0xE14D, 0x750C, //#CJK UNIFIED IDEOGRAPH
    0xE14E, 0x750E, //#CJK UNIFIED IDEOGRAPH
    0xE14F, 0x750D, //#CJK UNIFIED IDEOGRAPH
    0xE150, 0x7515, //#CJK UNIFIED IDEOGRAPH
    0xE151, 0x7513, //#CJK UNIFIED IDEOGRAPH
    0xE152, 0x751E, //#CJK UNIFIED IDEOGRAPH
    0xE153, 0x7526, //#CJK UNIFIED IDEOGRAPH
    0xE154, 0x752C, //#CJK UNIFIED IDEOGRAPH
    0xE155, 0x753C, //#CJK UNIFIED IDEOGRAPH
    0xE156, 0x7544, //#CJK UNIFIED IDEOGRAPH
    0xE157, 0x754D, //#CJK UNIFIED IDEOGRAPH
    0xE158, 0x754A, //#CJK UNIFIED IDEOGRAPH
    0xE159, 0x7549, //#CJK UNIFIED IDEOGRAPH
    0xE15A, 0x755B, //#CJK UNIFIED IDEOGRAPH
    0xE15B, 0x7546, //#CJK UNIFIED IDEOGRAPH
    0xE15C, 0x755A, //#CJK UNIFIED IDEOGRAPH
    0xE15D, 0x7569, //#CJK UNIFIED IDEOGRAPH
    0xE15E, 0x7564, //#CJK UNIFIED IDEOGRAPH
    0xE15F, 0x7567, //#CJK UNIFIED IDEOGRAPH
    0xE160, 0x756B, //#CJK UNIFIED IDEOGRAPH
    0xE161, 0x756D, //#CJK UNIFIED IDEOGRAPH
    0xE162, 0x7578, //#CJK UNIFIED IDEOGRAPH
    0xE163, 0x7576, //#CJK UNIFIED IDEOGRAPH
    0xE164, 0x7586, //#CJK UNIFIED IDEOGRAPH
    0xE165, 0x7587, //#CJK UNIFIED IDEOGRAPH
    0xE166, 0x7574, //#CJK UNIFIED IDEOGRAPH
    0xE167, 0x758A, //#CJK UNIFIED IDEOGRAPH
    0xE168, 0x7589, //#CJK UNIFIED IDEOGRAPH
    0xE169, 0x7582, //#CJK UNIFIED IDEOGRAPH
    0xE16A, 0x7594, //#CJK UNIFIED IDEOGRAPH
    0xE16B, 0x759A, //#CJK UNIFIED IDEOGRAPH
    0xE16C, 0x759D, //#CJK UNIFIED IDEOGRAPH
    0xE16D, 0x75A5, //#CJK UNIFIED IDEOGRAPH
    0xE16E, 0x75A3, //#CJK UNIFIED IDEOGRAPH
    0xE16F, 0x75C2, //#CJK UNIFIED IDEOGRAPH
    0xE170, 0x75B3, //#CJK UNIFIED IDEOGRAPH
    0xE171, 0x75C3, //#CJK UNIFIED IDEOGRAPH
    0xE172, 0x75B5, //#CJK UNIFIED IDEOGRAPH
    0xE173, 0x75BD, //#CJK UNIFIED IDEOGRAPH
    0xE174, 0x75B8, //#CJK UNIFIED IDEOGRAPH
    0xE175, 0x75BC, //#CJK UNIFIED IDEOGRAPH
    0xE176, 0x75B1, //#CJK UNIFIED IDEOGRAPH
    0xE177, 0x75CD, //#CJK UNIFIED IDEOGRAPH
    0xE178, 0x75CA, //#CJK UNIFIED IDEOGRAPH
    0xE179, 0x75D2, //#CJK UNIFIED IDEOGRAPH
    0xE17A, 0x75D9, //#CJK UNIFIED IDEOGRAPH
    0xE17B, 0x75E3, //#CJK UNIFIED IDEOGRAPH
    0xE17C, 0x75DE, //#CJK UNIFIED IDEOGRAPH
    0xE17D, 0x75FE, //#CJK UNIFIED IDEOGRAPH
    0xE17E, 0x75FF, //#CJK UNIFIED IDEOGRAPH
    0xE180, 0x75FC, //#CJK UNIFIED IDEOGRAPH
    0xE181, 0x7601, //#CJK UNIFIED IDEOGRAPH
    0xE182, 0x75F0, //#CJK UNIFIED IDEOGRAPH
    0xE183, 0x75FA, //#CJK UNIFIED IDEOGRAPH
    0xE184, 0x75F2, //#CJK UNIFIED IDEOGRAPH
    0xE185, 0x75F3, //#CJK UNIFIED IDEOGRAPH
    0xE186, 0x760B, //#CJK UNIFIED IDEOGRAPH
    0xE187, 0x760D, //#CJK UNIFIED IDEOGRAPH
    0xE188, 0x7609, //#CJK UNIFIED IDEOGRAPH
    0xE189, 0x761F, //#CJK UNIFIED IDEOGRAPH
    0xE18A, 0x7627, //#CJK UNIFIED IDEOGRAPH
    0xE18B, 0x7620, //#CJK UNIFIED IDEOGRAPH
    0xE18C, 0x7621, //#CJK UNIFIED IDEOGRAPH
    0xE18D, 0x7622, //#CJK UNIFIED IDEOGRAPH
    0xE18E, 0x7624, //#CJK UNIFIED IDEOGRAPH
    0xE18F, 0x7634, //#CJK UNIFIED IDEOGRAPH
    0xE190, 0x7630, //#CJK UNIFIED IDEOGRAPH
    0xE191, 0x763B, //#CJK UNIFIED IDEOGRAPH
    0xE192, 0x7647, //#CJK UNIFIED IDEOGRAPH
    0xE193, 0x7648, //#CJK UNIFIED IDEOGRAPH
    0xE194, 0x7646, //#CJK UNIFIED IDEOGRAPH
    0xE195, 0x765C, //#CJK UNIFIED IDEOGRAPH
    0xE196, 0x7658, //#CJK UNIFIED IDEOGRAPH
    0xE197, 0x7661, //#CJK UNIFIED IDEOGRAPH
    0xE198, 0x7662, //#CJK UNIFIED IDEOGRAPH
    0xE199, 0x7668, //#CJK UNIFIED IDEOGRAPH
    0xE19A, 0x7669, //#CJK UNIFIED IDEOGRAPH
    0xE19B, 0x766A, //#CJK UNIFIED IDEOGRAPH
    0xE19C, 0x7667, //#CJK UNIFIED IDEOGRAPH
    0xE19D, 0x766C, //#CJK UNIFIED IDEOGRAPH
    0xE19E, 0x7670, //#CJK UNIFIED IDEOGRAPH
    0xE19F, 0x7672, //#CJK UNIFIED IDEOGRAPH
    0xE1A0, 0x7676, //#CJK UNIFIED IDEOGRAPH
    0xE1A1, 0x7678, //#CJK UNIFIED IDEOGRAPH
    0xE1A2, 0x767C, //#CJK UNIFIED IDEOGRAPH
    0xE1A3, 0x7680, //#CJK UNIFIED IDEOGRAPH
    0xE1A4, 0x7683, //#CJK UNIFIED IDEOGRAPH
    0xE1A5, 0x7688, //#CJK UNIFIED IDEOGRAPH
    0xE1A6, 0x768B, //#CJK UNIFIED IDEOGRAPH
    0xE1A7, 0x768E, //#CJK UNIFIED IDEOGRAPH
    0xE1A8, 0x7696, //#CJK UNIFIED IDEOGRAPH
    0xE1A9, 0x7693, //#CJK UNIFIED IDEOGRAPH
    0xE1AA, 0x7699, //#CJK UNIFIED IDEOGRAPH
    0xE1AB, 0x769A, //#CJK UNIFIED IDEOGRAPH
    0xE1AC, 0x76B0, //#CJK UNIFIED IDEOGRAPH
    0xE1AD, 0x76B4, //#CJK UNIFIED IDEOGRAPH
    0xE1AE, 0x76B8, //#CJK UNIFIED IDEOGRAPH
    0xE1AF, 0x76B9, //#CJK UNIFIED IDEOGRAPH
    0xE1B0, 0x76BA, //#CJK UNIFIED IDEOGRAPH
    0xE1B1, 0x76C2, //#CJK UNIFIED IDEOGRAPH
    0xE1B2, 0x76CD, //#CJK UNIFIED IDEOGRAPH
    0xE1B3, 0x76D6, //#CJK UNIFIED IDEOGRAPH
    0xE1B4, 0x76D2, //#CJK UNIFIED IDEOGRAPH
    0xE1B5, 0x76DE, //#CJK UNIFIED IDEOGRAPH
    0xE1B6, 0x76E1, //#CJK UNIFIED IDEOGRAPH
    0xE1B7, 0x76E5, //#CJK UNIFIED IDEOGRAPH
    0xE1B8, 0x76E7, //#CJK UNIFIED IDEOGRAPH
    0xE1B9, 0x76EA, //#CJK UNIFIED IDEOGRAPH
    0xE1BA, 0x862F, //#CJK UNIFIED IDEOGRAPH
    0xE1BB, 0x76FB, //#CJK UNIFIED IDEOGRAPH
    0xE1BC, 0x7708, //#CJK UNIFIED IDEOGRAPH
    0xE1BD, 0x7707, //#CJK UNIFIED IDEOGRAPH
    0xE1BE, 0x7704, //#CJK UNIFIED IDEOGRAPH
    0xE1BF, 0x7729, //#CJK UNIFIED IDEOGRAPH
    0xE1C0, 0x7724, //#CJK UNIFIED IDEOGRAPH
    0xE1C1, 0x771E, //#CJK UNIFIED IDEOGRAPH
    0xE1C2, 0x7725, //#CJK UNIFIED IDEOGRAPH
    0xE1C3, 0x7726, //#CJK UNIFIED IDEOGRAPH
    0xE1C4, 0x771B, //#CJK UNIFIED IDEOGRAPH
    0xE1C5, 0x7737, //#CJK UNIFIED IDEOGRAPH
    0xE1C6, 0x7738, //#CJK UNIFIED IDEOGRAPH
    0xE1C7, 0x7747, //#CJK UNIFIED IDEOGRAPH
    0xE1C8, 0x775A, //#CJK UNIFIED IDEOGRAPH
    0xE1C9, 0x7768, //#CJK UNIFIED IDEOGRAPH
    0xE1CA, 0x776B, //#CJK UNIFIED IDEOGRAPH
    0xE1CB, 0x775B, //#CJK UNIFIED IDEOGRAPH
    0xE1CC, 0x7765, //#CJK UNIFIED IDEOGRAPH
    0xE1CD, 0x777F, //#CJK UNIFIED IDEOGRAPH
    0xE1CE, 0x777E, //#CJK UNIFIED IDEOGRAPH
    0xE1CF, 0x7779, //#CJK UNIFIED IDEOGRAPH
    0xE1D0, 0x778E, //#CJK UNIFIED IDEOGRAPH
    0xE1D1, 0x778B, //#CJK UNIFIED IDEOGRAPH
    0xE1D2, 0x7791, //#CJK UNIFIED IDEOGRAPH
    0xE1D3, 0x77A0, //#CJK UNIFIED IDEOGRAPH
    0xE1D4, 0x779E, //#CJK UNIFIED IDEOGRAPH
    0xE1D5, 0x77B0, //#CJK UNIFIED IDEOGRAPH
    0xE1D6, 0x77B6, //#CJK UNIFIED IDEOGRAPH
    0xE1D7, 0x77B9, //#CJK UNIFIED IDEOGRAPH
    0xE1D8, 0x77BF, //#CJK UNIFIED IDEOGRAPH
    0xE1D9, 0x77BC, //#CJK UNIFIED IDEOGRAPH
    0xE1DA, 0x77BD, //#CJK UNIFIED IDEOGRAPH
    0xE1DB, 0x77BB, //#CJK UNIFIED IDEOGRAPH
    0xE1DC, 0x77C7, //#CJK UNIFIED IDEOGRAPH
    0xE1DD, 0x77CD, //#CJK UNIFIED IDEOGRAPH
    0xE1DE, 0x77D7, //#CJK UNIFIED IDEOGRAPH
    0xE1DF, 0x77DA, //#CJK UNIFIED IDEOGRAPH
    0xE1E0, 0x77DC, //#CJK UNIFIED IDEOGRAPH
    0xE1E1, 0x77E3, //#CJK UNIFIED IDEOGRAPH
    0xE1E2, 0x77EE, //#CJK UNIFIED IDEOGRAPH
    0xE1E3, 0x77FC, //#CJK UNIFIED IDEOGRAPH
    0xE1E4, 0x780C, //#CJK UNIFIED IDEOGRAPH
    0xE1E5, 0x7812, //#CJK UNIFIED IDEOGRAPH
    0xE1E6, 0x7926, //#CJK UNIFIED IDEOGRAPH
    0xE1E7, 0x7820, //#CJK UNIFIED IDEOGRAPH
    0xE1E8, 0x792A, //#CJK UNIFIED IDEOGRAPH
    0xE1E9, 0x7845, //#CJK UNIFIED IDEOGRAPH
    0xE1EA, 0x788E, //#CJK UNIFIED IDEOGRAPH
    0xE1EB, 0x7874, //#CJK UNIFIED IDEOGRAPH
    0xE1EC, 0x7886, //#CJK UNIFIED IDEOGRAPH
    0xE1ED, 0x787C, //#CJK UNIFIED IDEOGRAPH
    0xE1EE, 0x789A, //#CJK UNIFIED IDEOGRAPH
    0xE1EF, 0x788C, //#CJK UNIFIED IDEOGRAPH
    0xE1F0, 0x78A3, //#CJK UNIFIED IDEOGRAPH
    0xE1F1, 0x78B5, //#CJK UNIFIED IDEOGRAPH
    0xE1F2, 0x78AA, //#CJK UNIFIED IDEOGRAPH
    0xE1F3, 0x78AF, //#CJK UNIFIED IDEOGRAPH
    0xE1F4, 0x78D1, //#CJK UNIFIED IDEOGRAPH
    0xE1F5, 0x78C6, //#CJK UNIFIED IDEOGRAPH
    0xE1F6, 0x78CB, //#CJK UNIFIED IDEOGRAPH
    0xE1F7, 0x78D4, //#CJK UNIFIED IDEOGRAPH
    0xE1F8, 0x78BE, //#CJK UNIFIED IDEOGRAPH
    0xE1F9, 0x78BC, //#CJK UNIFIED IDEOGRAPH
    0xE1FA, 0x78C5, //#CJK UNIFIED IDEOGRAPH
    0xE1FB, 0x78CA, //#CJK UNIFIED IDEOGRAPH
    0xE1FC, 0x78EC, //#CJK UNIFIED IDEOGRAPH
    0xE240, 0x78E7, //#CJK UNIFIED IDEOGRAPH
    0xE241, 0x78DA, //#CJK UNIFIED IDEOGRAPH
    0xE242, 0x78FD, //#CJK UNIFIED IDEOGRAPH
    0xE243, 0x78F4, //#CJK UNIFIED IDEOGRAPH
    0xE244, 0x7907, //#CJK UNIFIED IDEOGRAPH
    0xE245, 0x7912, //#CJK UNIFIED IDEOGRAPH
    0xE246, 0x7911, //#CJK UNIFIED IDEOGRAPH
    0xE247, 0x7919, //#CJK UNIFIED IDEOGRAPH
    0xE248, 0x792C, //#CJK UNIFIED IDEOGRAPH
    0xE249, 0x792B, //#CJK UNIFIED IDEOGRAPH
    0xE24A, 0x7940, //#CJK UNIFIED IDEOGRAPH
    0xE24B, 0x7960, //#CJK UNIFIED IDEOGRAPH
    0xE24C, 0x7957, //#CJK UNIFIED IDEOGRAPH
    0xE24D, 0x795F, //#CJK UNIFIED IDEOGRAPH
    0xE24E, 0x795A, //#CJK UNIFIED IDEOGRAPH
    0xE24F, 0x7955, //#CJK UNIFIED IDEOGRAPH
    0xE250, 0x7953, //#CJK UNIFIED IDEOGRAPH
    0xE251, 0x797A, //#CJK UNIFIED IDEOGRAPH
    0xE252, 0x797F, //#CJK UNIFIED IDEOGRAPH
    0xE253, 0x798A, //#CJK UNIFIED IDEOGRAPH
    0xE254, 0x799D, //#CJK UNIFIED IDEOGRAPH
    0xE255, 0x79A7, //#CJK UNIFIED IDEOGRAPH
    0xE256, 0x9F4B, //#CJK UNIFIED IDEOGRAPH
    0xE257, 0x79AA, //#CJK UNIFIED IDEOGRAPH
    0xE258, 0x79AE, //#CJK UNIFIED IDEOGRAPH
    0xE259, 0x79B3, //#CJK UNIFIED IDEOGRAPH
    0xE25A, 0x79B9, //#CJK UNIFIED IDEOGRAPH
    0xE25B, 0x79BA, //#CJK UNIFIED IDEOGRAPH
    0xE25C, 0x79C9, //#CJK UNIFIED IDEOGRAPH
    0xE25D, 0x79D5, //#CJK UNIFIED IDEOGRAPH
    0xE25E, 0x79E7, //#CJK UNIFIED IDEOGRAPH
    0xE25F, 0x79EC, //#CJK UNIFIED IDEOGRAPH
    0xE260, 0x79E1, //#CJK UNIFIED IDEOGRAPH
    0xE261, 0x79E3, //#CJK UNIFIED IDEOGRAPH
    0xE262, 0x7A08, //#CJK UNIFIED IDEOGRAPH
    0xE263, 0x7A0D, //#CJK UNIFIED IDEOGRAPH
    0xE264, 0x7A18, //#CJK UNIFIED IDEOGRAPH
    0xE265, 0x7A19, //#CJK UNIFIED IDEOGRAPH
    0xE266, 0x7A20, //#CJK UNIFIED IDEOGRAPH
    0xE267, 0x7A1F, //#CJK UNIFIED IDEOGRAPH
    0xE268, 0x7980, //#CJK UNIFIED IDEOGRAPH
    0xE269, 0x7A31, //#CJK UNIFIED IDEOGRAPH
    0xE26A, 0x7A3B, //#CJK UNIFIED IDEOGRAPH
    0xE26B, 0x7A3E, //#CJK UNIFIED IDEOGRAPH
    0xE26C, 0x7A37, //#CJK UNIFIED IDEOGRAPH
    0xE26D, 0x7A43, //#CJK UNIFIED IDEOGRAPH
    0xE26E, 0x7A57, //#CJK UNIFIED IDEOGRAPH
    0xE26F, 0x7A49, //#CJK UNIFIED IDEOGRAPH
    0xE270, 0x7A61, //#CJK UNIFIED IDEOGRAPH
    0xE271, 0x7A62, //#CJK UNIFIED IDEOGRAPH
    0xE272, 0x7A69, //#CJK UNIFIED IDEOGRAPH
    0xE273, 0x9F9D, //#CJK UNIFIED IDEOGRAPH
    0xE274, 0x7A70, //#CJK UNIFIED IDEOGRAPH
    0xE275, 0x7A79, //#CJK UNIFIED IDEOGRAPH
    0xE276, 0x7A7D, //#CJK UNIFIED IDEOGRAPH
    0xE277, 0x7A88, //#CJK UNIFIED IDEOGRAPH
    0xE278, 0x7A97, //#CJK UNIFIED IDEOGRAPH
    0xE279, 0x7A95, //#CJK UNIFIED IDEOGRAPH
    0xE27A, 0x7A98, //#CJK UNIFIED IDEOGRAPH
    0xE27B, 0x7A96, //#CJK UNIFIED IDEOGRAPH
    0xE27C, 0x7AA9, //#CJK UNIFIED IDEOGRAPH
    0xE27D, 0x7AC8, //#CJK UNIFIED IDEOGRAPH
    0xE27E, 0x7AB0, //#CJK UNIFIED IDEOGRAPH
    0xE280, 0x7AB6, //#CJK UNIFIED IDEOGRAPH
    0xE281, 0x7AC5, //#CJK UNIFIED IDEOGRAPH
    0xE282, 0x7AC4, //#CJK UNIFIED IDEOGRAPH
    0xE283, 0x7ABF, //#CJK UNIFIED IDEOGRAPH
    0xE284, 0x9083, //#CJK UNIFIED IDEOGRAPH
    0xE285, 0x7AC7, //#CJK UNIFIED IDEOGRAPH
    0xE286, 0x7ACA, //#CJK UNIFIED IDEOGRAPH
    0xE287, 0x7ACD, //#CJK UNIFIED IDEOGRAPH
    0xE288, 0x7ACF, //#CJK UNIFIED IDEOGRAPH
    0xE289, 0x7AD5, //#CJK UNIFIED IDEOGRAPH
    0xE28A, 0x7AD3, //#CJK UNIFIED IDEOGRAPH
    0xE28B, 0x7AD9, //#CJK UNIFIED IDEOGRAPH
    0xE28C, 0x7ADA, //#CJK UNIFIED IDEOGRAPH
    0xE28D, 0x7ADD, //#CJK UNIFIED IDEOGRAPH
    0xE28E, 0x7AE1, //#CJK UNIFIED IDEOGRAPH
    0xE28F, 0x7AE2, //#CJK UNIFIED IDEOGRAPH
    0xE290, 0x7AE6, //#CJK UNIFIED IDEOGRAPH
    0xE291, 0x7AED, //#CJK UNIFIED IDEOGRAPH
    0xE292, 0x7AF0, //#CJK UNIFIED IDEOGRAPH
    0xE293, 0x7B02, //#CJK UNIFIED IDEOGRAPH
    0xE294, 0x7B0F, //#CJK UNIFIED IDEOGRAPH
    0xE295, 0x7B0A, //#CJK UNIFIED IDEOGRAPH
    0xE296, 0x7B06, //#CJK UNIFIED IDEOGRAPH
    0xE297, 0x7B33, //#CJK UNIFIED IDEOGRAPH
    0xE298, 0x7B18, //#CJK UNIFIED IDEOGRAPH
    0xE299, 0x7B19, //#CJK UNIFIED IDEOGRAPH
    0xE29A, 0x7B1E, //#CJK UNIFIED IDEOGRAPH
    0xE29B, 0x7B35, //#CJK UNIFIED IDEOGRAPH
    0xE29C, 0x7B28, //#CJK UNIFIED IDEOGRAPH
    0xE29D, 0x7B36, //#CJK UNIFIED IDEOGRAPH
    0xE29E, 0x7B50, //#CJK UNIFIED IDEOGRAPH
    0xE29F, 0x7B7A, //#CJK UNIFIED IDEOGRAPH
    0xE2A0, 0x7B04, //#CJK UNIFIED IDEOGRAPH
    0xE2A1, 0x7B4D, //#CJK UNIFIED IDEOGRAPH
    0xE2A2, 0x7B0B, //#CJK UNIFIED IDEOGRAPH
    0xE2A3, 0x7B4C, //#CJK UNIFIED IDEOGRAPH
    0xE2A4, 0x7B45, //#CJK UNIFIED IDEOGRAPH
    0xE2A5, 0x7B75, //#CJK UNIFIED IDEOGRAPH
    0xE2A6, 0x7B65, //#CJK UNIFIED IDEOGRAPH
    0xE2A7, 0x7B74, //#CJK UNIFIED IDEOGRAPH
    0xE2A8, 0x7B67, //#CJK UNIFIED IDEOGRAPH
    0xE2A9, 0x7B70, //#CJK UNIFIED IDEOGRAPH
    0xE2AA, 0x7B71, //#CJK UNIFIED IDEOGRAPH
    0xE2AB, 0x7B6C, //#CJK UNIFIED IDEOGRAPH
    0xE2AC, 0x7B6E, //#CJK UNIFIED IDEOGRAPH
    0xE2AD, 0x7B9D, //#CJK UNIFIED IDEOGRAPH
    0xE2AE, 0x7B98, //#CJK UNIFIED IDEOGRAPH
    0xE2AF, 0x7B9F, //#CJK UNIFIED IDEOGRAPH
    0xE2B0, 0x7B8D, //#CJK UNIFIED IDEOGRAPH
    0xE2B1, 0x7B9C, //#CJK UNIFIED IDEOGRAPH
    0xE2B2, 0x7B9A, //#CJK UNIFIED IDEOGRAPH
    0xE2B3, 0x7B8B, //#CJK UNIFIED IDEOGRAPH
    0xE2B4, 0x7B92, //#CJK UNIFIED IDEOGRAPH
    0xE2B5, 0x7B8F, //#CJK UNIFIED IDEOGRAPH
    0xE2B6, 0x7B5D, //#CJK UNIFIED IDEOGRAPH
    0xE2B7, 0x7B99, //#CJK UNIFIED IDEOGRAPH
    0xE2B8, 0x7BCB, //#CJK UNIFIED IDEOGRAPH
    0xE2B9, 0x7BC1, //#CJK UNIFIED IDEOGRAPH
    0xE2BA, 0x7BCC, //#CJK UNIFIED IDEOGRAPH
    0xE2BB, 0x7BCF, //#CJK UNIFIED IDEOGRAPH
    0xE2BC, 0x7BB4, //#CJK UNIFIED IDEOGRAPH
    0xE2BD, 0x7BC6, //#CJK UNIFIED IDEOGRAPH
    0xE2BE, 0x7BDD, //#CJK UNIFIED IDEOGRAPH
    0xE2BF, 0x7BE9, //#CJK UNIFIED IDEOGRAPH
    0xE2C0, 0x7C11, //#CJK UNIFIED IDEOGRAPH
    0xE2C1, 0x7C14, //#CJK UNIFIED IDEOGRAPH
    0xE2C2, 0x7BE6, //#CJK UNIFIED IDEOGRAPH
    0xE2C3, 0x7BE5, //#CJK UNIFIED IDEOGRAPH
    0xE2C4, 0x7C60, //#CJK UNIFIED IDEOGRAPH
    0xE2C5, 0x7C00, //#CJK UNIFIED IDEOGRAPH
    0xE2C6, 0x7C07, //#CJK UNIFIED IDEOGRAPH
    0xE2C7, 0x7C13, //#CJK UNIFIED IDEOGRAPH
    0xE2C8, 0x7BF3, //#CJK UNIFIED IDEOGRAPH
    0xE2C9, 0x7BF7, //#CJK UNIFIED IDEOGRAPH
    0xE2CA, 0x7C17, //#CJK UNIFIED IDEOGRAPH
    0xE2CB, 0x7C0D, //#CJK UNIFIED IDEOGRAPH
    0xE2CC, 0x7BF6, //#CJK UNIFIED IDEOGRAPH
    0xE2CD, 0x7C23, //#CJK UNIFIED IDEOGRAPH
    0xE2CE, 0x7C27, //#CJK UNIFIED IDEOGRAPH
    0xE2CF, 0x7C2A, //#CJK UNIFIED IDEOGRAPH
    0xE2D0, 0x7C1F, //#CJK UNIFIED IDEOGRAPH
    0xE2D1, 0x7C37, //#CJK UNIFIED IDEOGRAPH
    0xE2D2, 0x7C2B, //#CJK UNIFIED IDEOGRAPH
    0xE2D3, 0x7C3D, //#CJK UNIFIED IDEOGRAPH
    0xE2D4, 0x7C4C, //#CJK UNIFIED IDEOGRAPH
    0xE2D5, 0x7C43, //#CJK UNIFIED IDEOGRAPH
    0xE2D6, 0x7C54, //#CJK UNIFIED IDEOGRAPH
    0xE2D7, 0x7C4F, //#CJK UNIFIED IDEOGRAPH
    0xE2D8, 0x7C40, //#CJK UNIFIED IDEOGRAPH
    0xE2D9, 0x7C50, //#CJK UNIFIED IDEOGRAPH
    0xE2DA, 0x7C58, //#CJK UNIFIED IDEOGRAPH
    0xE2DB, 0x7C5F, //#CJK UNIFIED IDEOGRAPH
    0xE2DC, 0x7C64, //#CJK UNIFIED IDEOGRAPH
    0xE2DD, 0x7C56, //#CJK UNIFIED IDEOGRAPH
    0xE2DE, 0x7C65, //#CJK UNIFIED IDEOGRAPH
    0xE2DF, 0x7C6C, //#CJK UNIFIED IDEOGRAPH
    0xE2E0, 0x7C75, //#CJK UNIFIED IDEOGRAPH
    0xE2E1, 0x7C83, //#CJK UNIFIED IDEOGRAPH
    0xE2E2, 0x7C90, //#CJK UNIFIED IDEOGRAPH
    0xE2E3, 0x7CA4, //#CJK UNIFIED IDEOGRAPH
    0xE2E4, 0x7CAD, //#CJK UNIFIED IDEOGRAPH
    0xE2E5, 0x7CA2, //#CJK UNIFIED IDEOGRAPH
    0xE2E6, 0x7CAB, //#CJK UNIFIED IDEOGRAPH
    0xE2E7, 0x7CA1, //#CJK UNIFIED IDEOGRAPH
    0xE2E8, 0x7CA8, //#CJK UNIFIED IDEOGRAPH
    0xE2E9, 0x7CB3, //#CJK UNIFIED IDEOGRAPH
    0xE2EA, 0x7CB2, //#CJK UNIFIED IDEOGRAPH
    0xE2EB, 0x7CB1, //#CJK UNIFIED IDEOGRAPH
    0xE2EC, 0x7CAE, //#CJK UNIFIED IDEOGRAPH
    0xE2ED, 0x7CB9, //#CJK UNIFIED IDEOGRAPH
    0xE2EE, 0x7CBD, //#CJK UNIFIED IDEOGRAPH
    0xE2EF, 0x7CC0, //#CJK UNIFIED IDEOGRAPH
    0xE2F0, 0x7CC5, //#CJK UNIFIED IDEOGRAPH
    0xE2F1, 0x7CC2, //#CJK UNIFIED IDEOGRAPH
    0xE2F2, 0x7CD8, //#CJK UNIFIED IDEOGRAPH
    0xE2F3, 0x7CD2, //#CJK UNIFIED IDEOGRAPH
    0xE2F4, 0x7CDC, //#CJK UNIFIED IDEOGRAPH
    0xE2F5, 0x7CE2, //#CJK UNIFIED IDEOGRAPH
    0xE2F6, 0x9B3B, //#CJK UNIFIED IDEOGRAPH
    0xE2F7, 0x7CEF, //#CJK UNIFIED IDEOGRAPH
    0xE2F8, 0x7CF2, //#CJK UNIFIED IDEOGRAPH
    0xE2F9, 0x7CF4, //#CJK UNIFIED IDEOGRAPH
    0xE2FA, 0x7CF6, //#CJK UNIFIED IDEOGRAPH
    0xE2FB, 0x7CFA, //#CJK UNIFIED IDEOGRAPH
    0xE2FC, 0x7D06, //#CJK UNIFIED IDEOGRAPH
    0xE340, 0x7D02, //#CJK UNIFIED IDEOGRAPH
    0xE341, 0x7D1C, //#CJK UNIFIED IDEOGRAPH
    0xE342, 0x7D15, //#CJK UNIFIED IDEOGRAPH
    0xE343, 0x7D0A, //#CJK UNIFIED IDEOGRAPH
    0xE344, 0x7D45, //#CJK UNIFIED IDEOGRAPH
    0xE345, 0x7D4B, //#CJK UNIFIED IDEOGRAPH
    0xE346, 0x7D2E, //#CJK UNIFIED IDEOGRAPH
    0xE347, 0x7D32, //#CJK UNIFIED IDEOGRAPH
    0xE348, 0x7D3F, //#CJK UNIFIED IDEOGRAPH
    0xE349, 0x7D35, //#CJK UNIFIED IDEOGRAPH
    0xE34A, 0x7D46, //#CJK UNIFIED IDEOGRAPH
    0xE34B, 0x7D73, //#CJK UNIFIED IDEOGRAPH
    0xE34C, 0x7D56, //#CJK UNIFIED IDEOGRAPH
    0xE34D, 0x7D4E, //#CJK UNIFIED IDEOGRAPH
    0xE34E, 0x7D72, //#CJK UNIFIED IDEOGRAPH
    0xE34F, 0x7D68, //#CJK UNIFIED IDEOGRAPH
    0xE350, 0x7D6E, //#CJK UNIFIED IDEOGRAPH
    0xE351, 0x7D4F, //#CJK UNIFIED IDEOGRAPH
    0xE352, 0x7D63, //#CJK UNIFIED IDEOGRAPH
    0xE353, 0x7D93, //#CJK UNIFIED IDEOGRAPH
    0xE354, 0x7D89, //#CJK UNIFIED IDEOGRAPH
    0xE355, 0x7D5B, //#CJK UNIFIED IDEOGRAPH
    0xE356, 0x7D8F, //#CJK UNIFIED IDEOGRAPH
    0xE357, 0x7D7D, //#CJK UNIFIED IDEOGRAPH
    0xE358, 0x7D9B, //#CJK UNIFIED IDEOGRAPH
    0xE359, 0x7DBA, //#CJK UNIFIED IDEOGRAPH
    0xE35A, 0x7DAE, //#CJK UNIFIED IDEOGRAPH
    0xE35B, 0x7DA3, //#CJK UNIFIED IDEOGRAPH
    0xE35C, 0x7DB5, //#CJK UNIFIED IDEOGRAPH
    0xE35D, 0x7DC7, //#CJK UNIFIED IDEOGRAPH
    0xE35E, 0x7DBD, //#CJK UNIFIED IDEOGRAPH
    0xE35F, 0x7DAB, //#CJK UNIFIED IDEOGRAPH
    0xE360, 0x7E3D, //#CJK UNIFIED IDEOGRAPH
    0xE361, 0x7DA2, //#CJK UNIFIED IDEOGRAPH
    0xE362, 0x7DAF, //#CJK UNIFIED IDEOGRAPH
    0xE363, 0x7DDC, //#CJK UNIFIED IDEOGRAPH
    0xE364, 0x7DB8, //#CJK UNIFIED IDEOGRAPH
    0xE365, 0x7D9F, //#CJK UNIFIED IDEOGRAPH
    0xE366, 0x7DB0, //#CJK UNIFIED IDEOGRAPH
    0xE367, 0x7DD8, //#CJK UNIFIED IDEOGRAPH
    0xE368, 0x7DDD, //#CJK UNIFIED IDEOGRAPH
    0xE369, 0x7DE4, //#CJK UNIFIED IDEOGRAPH
    0xE36A, 0x7DDE, //#CJK UNIFIED IDEOGRAPH
    0xE36B, 0x7DFB, //#CJK UNIFIED IDEOGRAPH
    0xE36C, 0x7DF2, //#CJK UNIFIED IDEOGRAPH
    0xE36D, 0x7DE1, //#CJK UNIFIED IDEOGRAPH
    0xE36E, 0x7E05, //#CJK UNIFIED IDEOGRAPH
    0xE36F, 0x7E0A, //#CJK UNIFIED IDEOGRAPH
    0xE370, 0x7E23, //#CJK UNIFIED IDEOGRAPH
    0xE371, 0x7E21, //#CJK UNIFIED IDEOGRAPH
    0xE372, 0x7E12, //#CJK UNIFIED IDEOGRAPH
    0xE373, 0x7E31, //#CJK UNIFIED IDEOGRAPH
    0xE374, 0x7E1F, //#CJK UNIFIED IDEOGRAPH
    0xE375, 0x7E09, //#CJK UNIFIED IDEOGRAPH
    0xE376, 0x7E0B, //#CJK UNIFIED IDEOGRAPH
    0xE377, 0x7E22, //#CJK UNIFIED IDEOGRAPH
    0xE378, 0x7E46, //#CJK UNIFIED IDEOGRAPH
    0xE379, 0x7E66, //#CJK UNIFIED IDEOGRAPH
    0xE37A, 0x7E3B, //#CJK UNIFIED IDEOGRAPH
    0xE37B, 0x7E35, //#CJK UNIFIED IDEOGRAPH
    0xE37C, 0x7E39, //#CJK UNIFIED IDEOGRAPH
    0xE37D, 0x7E43, //#CJK UNIFIED IDEOGRAPH
    0xE37E, 0x7E37, //#CJK UNIFIED IDEOGRAPH
    0xE380, 0x7E32, //#CJK UNIFIED IDEOGRAPH
    0xE381, 0x7E3A, //#CJK UNIFIED IDEOGRAPH
    0xE382, 0x7E67, //#CJK UNIFIED IDEOGRAPH
    0xE383, 0x7E5D, //#CJK UNIFIED IDEOGRAPH
    0xE384, 0x7E56, //#CJK UNIFIED IDEOGRAPH
    0xE385, 0x7E5E, //#CJK UNIFIED IDEOGRAPH
    0xE386, 0x7E59, //#CJK UNIFIED IDEOGRAPH
    0xE387, 0x7E5A, //#CJK UNIFIED IDEOGRAPH
    0xE388, 0x7E79, //#CJK UNIFIED IDEOGRAPH
    0xE389, 0x7E6A, //#CJK UNIFIED IDEOGRAPH
    0xE38A, 0x7E69, //#CJK UNIFIED IDEOGRAPH
    0xE38B, 0x7E7C, //#CJK UNIFIED IDEOGRAPH
    0xE38C, 0x7E7B, //#CJK UNIFIED IDEOGRAPH
    0xE38D, 0x7E83, //#CJK UNIFIED IDEOGRAPH
    0xE38E, 0x7DD5, //#CJK UNIFIED IDEOGRAPH
    0xE38F, 0x7E7D, //#CJK UNIFIED IDEOGRAPH
    0xE390, 0x8FAE, //#CJK UNIFIED IDEOGRAPH
    0xE391, 0x7E7F, //#CJK UNIFIED IDEOGRAPH
    0xE392, 0x7E88, //#CJK UNIFIED IDEOGRAPH
    0xE393, 0x7E89, //#CJK UNIFIED IDEOGRAPH
    0xE394, 0x7E8C, //#CJK UNIFIED IDEOGRAPH
    0xE395, 0x7E92, //#CJK UNIFIED IDEOGRAPH
    0xE396, 0x7E90, //#CJK UNIFIED IDEOGRAPH
    0xE397, 0x7E93, //#CJK UNIFIED IDEOGRAPH
    0xE398, 0x7E94, //#CJK UNIFIED IDEOGRAPH
    0xE399, 0x7E96, //#CJK UNIFIED IDEOGRAPH
    0xE39A, 0x7E8E, //#CJK UNIFIED IDEOGRAPH
    0xE39B, 0x7E9B, //#CJK UNIFIED IDEOGRAPH
    0xE39C, 0x7E9C, //#CJK UNIFIED IDEOGRAPH
    0xE39D, 0x7F38, //#CJK UNIFIED IDEOGRAPH
    0xE39E, 0x7F3A, //#CJK UNIFIED IDEOGRAPH
    0xE39F, 0x7F45, //#CJK UNIFIED IDEOGRAPH
    0xE3A0, 0x7F4C, //#CJK UNIFIED IDEOGRAPH
    0xE3A1, 0x7F4D, //#CJK UNIFIED IDEOGRAPH
    0xE3A2, 0x7F4E, //#CJK UNIFIED IDEOGRAPH
    0xE3A3, 0x7F50, //#CJK UNIFIED IDEOGRAPH
    0xE3A4, 0x7F51, //#CJK UNIFIED IDEOGRAPH
    0xE3A5, 0x7F55, //#CJK UNIFIED IDEOGRAPH
    0xE3A6, 0x7F54, //#CJK UNIFIED IDEOGRAPH
    0xE3A7, 0x7F58, //#CJK UNIFIED IDEOGRAPH
    0xE3A8, 0x7F5F, //#CJK UNIFIED IDEOGRAPH
    0xE3A9, 0x7F60, //#CJK UNIFIED IDEOGRAPH
    0xE3AA, 0x7F68, //#CJK UNIFIED IDEOGRAPH
    0xE3AB, 0x7F69, //#CJK UNIFIED IDEOGRAPH
    0xE3AC, 0x7F67, //#CJK UNIFIED IDEOGRAPH
    0xE3AD, 0x7F78, //#CJK UNIFIED IDEOGRAPH
    0xE3AE, 0x7F82, //#CJK UNIFIED IDEOGRAPH
    0xE3AF, 0x7F86, //#CJK UNIFIED IDEOGRAPH
    0xE3B0, 0x7F83, //#CJK UNIFIED IDEOGRAPH
    0xE3B1, 0x7F88, //#CJK UNIFIED IDEOGRAPH
    0xE3B2, 0x7F87, //#CJK UNIFIED IDEOGRAPH
    0xE3B3, 0x7F8C, //#CJK UNIFIED IDEOGRAPH
    0xE3B4, 0x7F94, //#CJK UNIFIED IDEOGRAPH
    0xE3B5, 0x7F9E, //#CJK UNIFIED IDEOGRAPH
    0xE3B6, 0x7F9D, //#CJK UNIFIED IDEOGRAPH
    0xE3B7, 0x7F9A, //#CJK UNIFIED IDEOGRAPH
    0xE3B8, 0x7FA3, //#CJK UNIFIED IDEOGRAPH
    0xE3B9, 0x7FAF, //#CJK UNIFIED IDEOGRAPH
    0xE3BA, 0x7FB2, //#CJK UNIFIED IDEOGRAPH
    0xE3BB, 0x7FB9, //#CJK UNIFIED IDEOGRAPH
    0xE3BC, 0x7FAE, //#CJK UNIFIED IDEOGRAPH
    0xE3BD, 0x7FB6, //#CJK UNIFIED IDEOGRAPH
    0xE3BE, 0x7FB8, //#CJK UNIFIED IDEOGRAPH
    0xE3BF, 0x8B71, //#CJK UNIFIED IDEOGRAPH
    0xE3C0, 0x7FC5, //#CJK UNIFIED IDEOGRAPH
    0xE3C1, 0x7FC6, //#CJK UNIFIED IDEOGRAPH
    0xE3C2, 0x7FCA, //#CJK UNIFIED IDEOGRAPH
    0xE3C3, 0x7FD5, //#CJK UNIFIED IDEOGRAPH
    0xE3C4, 0x7FD4, //#CJK UNIFIED IDEOGRAPH
    0xE3C5, 0x7FE1, //#CJK UNIFIED IDEOGRAPH
    0xE3C6, 0x7FE6, //#CJK UNIFIED IDEOGRAPH
    0xE3C7, 0x7FE9, //#CJK UNIFIED IDEOGRAPH
    0xE3C8, 0x7FF3, //#CJK UNIFIED IDEOGRAPH
    0xE3C9, 0x7FF9, //#CJK UNIFIED IDEOGRAPH
    0xE3CA, 0x98DC, //#CJK UNIFIED IDEOGRAPH
    0xE3CB, 0x8006, //#CJK UNIFIED IDEOGRAPH
    0xE3CC, 0x8004, //#CJK UNIFIED IDEOGRAPH
    0xE3CD, 0x800B, //#CJK UNIFIED IDEOGRAPH
    0xE3CE, 0x8012, //#CJK UNIFIED IDEOGRAPH
    0xE3CF, 0x8018, //#CJK UNIFIED IDEOGRAPH
    0xE3D0, 0x8019, //#CJK UNIFIED IDEOGRAPH
    0xE3D1, 0x801C, //#CJK UNIFIED IDEOGRAPH
    0xE3D2, 0x8021, //#CJK UNIFIED IDEOGRAPH
    0xE3D3, 0x8028, //#CJK UNIFIED IDEOGRAPH
    0xE3D4, 0x803F, //#CJK UNIFIED IDEOGRAPH
    0xE3D5, 0x803B, //#CJK UNIFIED IDEOGRAPH
    0xE3D6, 0x804A, //#CJK UNIFIED IDEOGRAPH
    0xE3D7, 0x8046, //#CJK UNIFIED IDEOGRAPH
    0xE3D8, 0x8052, //#CJK UNIFIED IDEOGRAPH
    0xE3D9, 0x8058, //#CJK UNIFIED IDEOGRAPH
    0xE3DA, 0x805A, //#CJK UNIFIED IDEOGRAPH
    0xE3DB, 0x805F, //#CJK UNIFIED IDEOGRAPH
    0xE3DC, 0x8062, //#CJK UNIFIED IDEOGRAPH
    0xE3DD, 0x8068, //#CJK UNIFIED IDEOGRAPH
    0xE3DE, 0x8073, //#CJK UNIFIED IDEOGRAPH
    0xE3DF, 0x8072, //#CJK UNIFIED IDEOGRAPH
    0xE3E0, 0x8070, //#CJK UNIFIED IDEOGRAPH
    0xE3E1, 0x8076, //#CJK UNIFIED IDEOGRAPH
    0xE3E2, 0x8079, //#CJK UNIFIED IDEOGRAPH
    0xE3E3, 0x807D, //#CJK UNIFIED IDEOGRAPH
    0xE3E4, 0x807F, //#CJK UNIFIED IDEOGRAPH
    0xE3E5, 0x8084, //#CJK UNIFIED IDEOGRAPH
    0xE3E6, 0x8086, //#CJK UNIFIED IDEOGRAPH
    0xE3E7, 0x8085, //#CJK UNIFIED IDEOGRAPH
    0xE3E8, 0x809B, //#CJK UNIFIED IDEOGRAPH
    0xE3E9, 0x8093, //#CJK UNIFIED IDEOGRAPH
    0xE3EA, 0x809A, //#CJK UNIFIED IDEOGRAPH
    0xE3EB, 0x80AD, //#CJK UNIFIED IDEOGRAPH
    0xE3EC, 0x5190, //#CJK UNIFIED IDEOGRAPH
    0xE3ED, 0x80AC, //#CJK UNIFIED IDEOGRAPH
    0xE3EE, 0x80DB, //#CJK UNIFIED IDEOGRAPH
    0xE3EF, 0x80E5, //#CJK UNIFIED IDEOGRAPH
    0xE3F0, 0x80D9, //#CJK UNIFIED IDEOGRAPH
    0xE3F1, 0x80DD, //#CJK UNIFIED IDEOGRAPH
    0xE3F2, 0x80C4, //#CJK UNIFIED IDEOGRAPH
    0xE3F3, 0x80DA, //#CJK UNIFIED IDEOGRAPH
    0xE3F4, 0x80D6, //#CJK UNIFIED IDEOGRAPH
    0xE3F5, 0x8109, //#CJK UNIFIED IDEOGRAPH
    0xE3F6, 0x80EF, //#CJK UNIFIED IDEOGRAPH
    0xE3F7, 0x80F1, //#CJK UNIFIED IDEOGRAPH
    0xE3F8, 0x811B, //#CJK UNIFIED IDEOGRAPH
    0xE3F9, 0x8129, //#CJK UNIFIED IDEOGRAPH
    0xE3FA, 0x8123, //#CJK UNIFIED IDEOGRAPH
    0xE3FB, 0x812F, //#CJK UNIFIED IDEOGRAPH
    0xE3FC, 0x814B, //#CJK UNIFIED IDEOGRAPH
    0xE440, 0x968B, //#CJK UNIFIED IDEOGRAPH
    0xE441, 0x8146, //#CJK UNIFIED IDEOGRAPH
    0xE442, 0x813E, //#CJK UNIFIED IDEOGRAPH
    0xE443, 0x8153, //#CJK UNIFIED IDEOGRAPH
    0xE444, 0x8151, //#CJK UNIFIED IDEOGRAPH
    0xE445, 0x80FC, //#CJK UNIFIED IDEOGRAPH
    0xE446, 0x8171, //#CJK UNIFIED IDEOGRAPH
    0xE447, 0x816E, //#CJK UNIFIED IDEOGRAPH
    0xE448, 0x8165, //#CJK UNIFIED IDEOGRAPH
    0xE449, 0x8166, //#CJK UNIFIED IDEOGRAPH
    0xE44A, 0x8174, //#CJK UNIFIED IDEOGRAPH
    0xE44B, 0x8183, //#CJK UNIFIED IDEOGRAPH
    0xE44C, 0x8188, //#CJK UNIFIED IDEOGRAPH
    0xE44D, 0x818A, //#CJK UNIFIED IDEOGRAPH
    0xE44E, 0x8180, //#CJK UNIFIED IDEOGRAPH
    0xE44F, 0x8182, //#CJK UNIFIED IDEOGRAPH
    0xE450, 0x81A0, //#CJK UNIFIED IDEOGRAPH
    0xE451, 0x8195, //#CJK UNIFIED IDEOGRAPH
    0xE452, 0x81A4, //#CJK UNIFIED IDEOGRAPH
    0xE453, 0x81A3, //#CJK UNIFIED IDEOGRAPH
    0xE454, 0x815F, //#CJK UNIFIED IDEOGRAPH
    0xE455, 0x8193, //#CJK UNIFIED IDEOGRAPH
    0xE456, 0x81A9, //#CJK UNIFIED IDEOGRAPH
    0xE457, 0x81B0, //#CJK UNIFIED IDEOGRAPH
    0xE458, 0x81B5, //#CJK UNIFIED IDEOGRAPH
    0xE459, 0x81BE, //#CJK UNIFIED IDEOGRAPH
    0xE45A, 0x81B8, //#CJK UNIFIED IDEOGRAPH
    0xE45B, 0x81BD, //#CJK UNIFIED IDEOGRAPH
    0xE45C, 0x81C0, //#CJK UNIFIED IDEOGRAPH
    0xE45D, 0x81C2, //#CJK UNIFIED IDEOGRAPH
    0xE45E, 0x81BA, //#CJK UNIFIED IDEOGRAPH
    0xE45F, 0x81C9, //#CJK UNIFIED IDEOGRAPH
    0xE460, 0x81CD, //#CJK UNIFIED IDEOGRAPH
    0xE461, 0x81D1, //#CJK UNIFIED IDEOGRAPH
    0xE462, 0x81D9, //#CJK UNIFIED IDEOGRAPH
    0xE463, 0x81D8, //#CJK UNIFIED IDEOGRAPH
    0xE464, 0x81C8, //#CJK UNIFIED IDEOGRAPH
    0xE465, 0x81DA, //#CJK UNIFIED IDEOGRAPH
    0xE466, 0x81DF, //#CJK UNIFIED IDEOGRAPH
    0xE467, 0x81E0, //#CJK UNIFIED IDEOGRAPH
    0xE468, 0x81E7, //#CJK UNIFIED IDEOGRAPH
    0xE469, 0x81FA, //#CJK UNIFIED IDEOGRAPH
    0xE46A, 0x81FB, //#CJK UNIFIED IDEOGRAPH
    0xE46B, 0x81FE, //#CJK UNIFIED IDEOGRAPH
    0xE46C, 0x8201, //#CJK UNIFIED IDEOGRAPH
    0xE46D, 0x8202, //#CJK UNIFIED IDEOGRAPH
    0xE46E, 0x8205, //#CJK UNIFIED IDEOGRAPH
    0xE46F, 0x8207, //#CJK UNIFIED IDEOGRAPH
    0xE470, 0x820A, //#CJK UNIFIED IDEOGRAPH
    0xE471, 0x820D, //#CJK UNIFIED IDEOGRAPH
    0xE472, 0x8210, //#CJK UNIFIED IDEOGRAPH
    0xE473, 0x8216, //#CJK UNIFIED IDEOGRAPH
    0xE474, 0x8229, //#CJK UNIFIED IDEOGRAPH
    0xE475, 0x822B, //#CJK UNIFIED IDEOGRAPH
    0xE476, 0x8238, //#CJK UNIFIED IDEOGRAPH
    0xE477, 0x8233, //#CJK UNIFIED IDEOGRAPH
    0xE478, 0x8240, //#CJK UNIFIED IDEOGRAPH
    0xE479, 0x8259, //#CJK UNIFIED IDEOGRAPH
    0xE47A, 0x8258, //#CJK UNIFIED IDEOGRAPH
    0xE47B, 0x825D, //#CJK UNIFIED IDEOGRAPH
    0xE47C, 0x825A, //#CJK UNIFIED IDEOGRAPH
    0xE47D, 0x825F, //#CJK UNIFIED IDEOGRAPH
    0xE47E, 0x8264, //#CJK UNIFIED IDEOGRAPH
    0xE480, 0x8262, //#CJK UNIFIED IDEOGRAPH
    0xE481, 0x8268, //#CJK UNIFIED IDEOGRAPH
    0xE482, 0x826A, //#CJK UNIFIED IDEOGRAPH
    0xE483, 0x826B, //#CJK UNIFIED IDEOGRAPH
    0xE484, 0x822E, //#CJK UNIFIED IDEOGRAPH
    0xE485, 0x8271, //#CJK UNIFIED IDEOGRAPH
    0xE486, 0x8277, //#CJK UNIFIED IDEOGRAPH
    0xE487, 0x8278, //#CJK UNIFIED IDEOGRAPH
    0xE488, 0x827E, //#CJK UNIFIED IDEOGRAPH
    0xE489, 0x828D, //#CJK UNIFIED IDEOGRAPH
    0xE48A, 0x8292, //#CJK UNIFIED IDEOGRAPH
    0xE48B, 0x82AB, //#CJK UNIFIED IDEOGRAPH
    0xE48C, 0x829F, //#CJK UNIFIED IDEOGRAPH
    0xE48D, 0x82BB, //#CJK UNIFIED IDEOGRAPH
    0xE48E, 0x82AC, //#CJK UNIFIED IDEOGRAPH
    0xE48F, 0x82E1, //#CJK UNIFIED IDEOGRAPH
    0xE490, 0x82E3, //#CJK UNIFIED IDEOGRAPH
    0xE491, 0x82DF, //#CJK UNIFIED IDEOGRAPH
    0xE492, 0x82D2, //#CJK UNIFIED IDEOGRAPH
    0xE493, 0x82F4, //#CJK UNIFIED IDEOGRAPH
    0xE494, 0x82F3, //#CJK UNIFIED IDEOGRAPH
    0xE495, 0x82FA, //#CJK UNIFIED IDEOGRAPH
    0xE496, 0x8393, //#CJK UNIFIED IDEOGRAPH
    0xE497, 0x8303, //#CJK UNIFIED IDEOGRAPH
    0xE498, 0x82FB, //#CJK UNIFIED IDEOGRAPH
    0xE499, 0x82F9, //#CJK UNIFIED IDEOGRAPH
    0xE49A, 0x82DE, //#CJK UNIFIED IDEOGRAPH
    0xE49B, 0x8306, //#CJK UNIFIED IDEOGRAPH
    0xE49C, 0x82DC, //#CJK UNIFIED IDEOGRAPH
    0xE49D, 0x8309, //#CJK UNIFIED IDEOGRAPH
    0xE49E, 0x82D9, //#CJK UNIFIED IDEOGRAPH
    0xE49F, 0x8335, //#CJK UNIFIED IDEOGRAPH
    0xE4A0, 0x8334, //#CJK UNIFIED IDEOGRAPH
    0xE4A1, 0x8316, //#CJK UNIFIED IDEOGRAPH
    0xE4A2, 0x8332, //#CJK UNIFIED IDEOGRAPH
    0xE4A3, 0x8331, //#CJK UNIFIED IDEOGRAPH
    0xE4A4, 0x8340, //#CJK UNIFIED IDEOGRAPH
    0xE4A5, 0x8339, //#CJK UNIFIED IDEOGRAPH
    0xE4A6, 0x8350, //#CJK UNIFIED IDEOGRAPH
    0xE4A7, 0x8345, //#CJK UNIFIED IDEOGRAPH
    0xE4A8, 0x832F, //#CJK UNIFIED IDEOGRAPH
    0xE4A9, 0x832B, //#CJK UNIFIED IDEOGRAPH
    0xE4AA, 0x8317, //#CJK UNIFIED IDEOGRAPH
    0xE4AB, 0x8318, //#CJK UNIFIED IDEOGRAPH
    0xE4AC, 0x8385, //#CJK UNIFIED IDEOGRAPH
    0xE4AD, 0x839A, //#CJK UNIFIED IDEOGRAPH
    0xE4AE, 0x83AA, //#CJK UNIFIED IDEOGRAPH
    0xE4AF, 0x839F, //#CJK UNIFIED IDEOGRAPH
    0xE4B0, 0x83A2, //#CJK UNIFIED IDEOGRAPH
    0xE4B1, 0x8396, //#CJK UNIFIED IDEOGRAPH
    0xE4B2, 0x8323, //#CJK UNIFIED IDEOGRAPH
    0xE4B3, 0x838E, //#CJK UNIFIED IDEOGRAPH
    0xE4B4, 0x8387, //#CJK UNIFIED IDEOGRAPH
    0xE4B5, 0x838A, //#CJK UNIFIED IDEOGRAPH
    0xE4B6, 0x837C, //#CJK UNIFIED IDEOGRAPH
    0xE4B7, 0x83B5, //#CJK UNIFIED IDEOGRAPH
    0xE4B8, 0x8373, //#CJK UNIFIED IDEOGRAPH
    0xE4B9, 0x8375, //#CJK UNIFIED IDEOGRAPH
    0xE4BA, 0x83A0, //#CJK UNIFIED IDEOGRAPH
    0xE4BB, 0x8389, //#CJK UNIFIED IDEOGRAPH
    0xE4BC, 0x83A8, //#CJK UNIFIED IDEOGRAPH
    0xE4BD, 0x83F4, //#CJK UNIFIED IDEOGRAPH
    0xE4BE, 0x8413, //#CJK UNIFIED IDEOGRAPH
    0xE4BF, 0x83EB, //#CJK UNIFIED IDEOGRAPH
    0xE4C0, 0x83CE, //#CJK UNIFIED IDEOGRAPH
    0xE4C1, 0x83FD, //#CJK UNIFIED IDEOGRAPH
    0xE4C2, 0x8403, //#CJK UNIFIED IDEOGRAPH
    0xE4C3, 0x83D8, //#CJK UNIFIED IDEOGRAPH
    0xE4C4, 0x840B, //#CJK UNIFIED IDEOGRAPH
    0xE4C5, 0x83C1, //#CJK UNIFIED IDEOGRAPH
    0xE4C6, 0x83F7, //#CJK UNIFIED IDEOGRAPH
    0xE4C7, 0x8407, //#CJK UNIFIED IDEOGRAPH
    0xE4C8, 0x83E0, //#CJK UNIFIED IDEOGRAPH
    0xE4C9, 0x83F2, //#CJK UNIFIED IDEOGRAPH
    0xE4CA, 0x840D, //#CJK UNIFIED IDEOGRAPH
    0xE4CB, 0x8422, //#CJK UNIFIED IDEOGRAPH
    0xE4CC, 0x8420, //#CJK UNIFIED IDEOGRAPH
    0xE4CD, 0x83BD, //#CJK UNIFIED IDEOGRAPH
    0xE4CE, 0x8438, //#CJK UNIFIED IDEOGRAPH
    0xE4CF, 0x8506, //#CJK UNIFIED IDEOGRAPH
    0xE4D0, 0x83FB, //#CJK UNIFIED IDEOGRAPH
    0xE4D1, 0x846D, //#CJK UNIFIED IDEOGRAPH
    0xE4D2, 0x842A, //#CJK UNIFIED IDEOGRAPH
    0xE4D3, 0x843C, //#CJK UNIFIED IDEOGRAPH
    0xE4D4, 0x855A, //#CJK UNIFIED IDEOGRAPH
    0xE4D5, 0x8484, //#CJK UNIFIED IDEOGRAPH
    0xE4D6, 0x8477, //#CJK UNIFIED IDEOGRAPH
    0xE4D7, 0x846B, //#CJK UNIFIED IDEOGRAPH
    0xE4D8, 0x84AD, //#CJK UNIFIED IDEOGRAPH
    0xE4D9, 0x846E, //#CJK UNIFIED IDEOGRAPH
    0xE4DA, 0x8482, //#CJK UNIFIED IDEOGRAPH
    0xE4DB, 0x8469, //#CJK UNIFIED IDEOGRAPH
    0xE4DC, 0x8446, //#CJK UNIFIED IDEOGRAPH
    0xE4DD, 0x842C, //#CJK UNIFIED IDEOGRAPH
    0xE4DE, 0x846F, //#CJK UNIFIED IDEOGRAPH
    0xE4DF, 0x8479, //#CJK UNIFIED IDEOGRAPH
    0xE4E0, 0x8435, //#CJK UNIFIED IDEOGRAPH
    0xE4E1, 0x84CA, //#CJK UNIFIED IDEOGRAPH
    0xE4E2, 0x8462, //#CJK UNIFIED IDEOGRAPH
    0xE4E3, 0x84B9, //#CJK UNIFIED IDEOGRAPH
    0xE4E4, 0x84BF, //#CJK UNIFIED IDEOGRAPH
    0xE4E5, 0x849F, //#CJK UNIFIED IDEOGRAPH
    0xE4E6, 0x84D9, //#CJK UNIFIED IDEOGRAPH
    0xE4E7, 0x84CD, //#CJK UNIFIED IDEOGRAPH
    0xE4E8, 0x84BB, //#CJK UNIFIED IDEOGRAPH
    0xE4E9, 0x84DA, //#CJK UNIFIED IDEOGRAPH
    0xE4EA, 0x84D0, //#CJK UNIFIED IDEOGRAPH
    0xE4EB, 0x84C1, //#CJK UNIFIED IDEOGRAPH
    0xE4EC, 0x84C6, //#CJK UNIFIED IDEOGRAPH
    0xE4ED, 0x84D6, //#CJK UNIFIED IDEOGRAPH
    0xE4EE, 0x84A1, //#CJK UNIFIED IDEOGRAPH
    0xE4EF, 0x8521, //#CJK UNIFIED IDEOGRAPH
    0xE4F0, 0x84FF, //#CJK UNIFIED IDEOGRAPH
    0xE4F1, 0x84F4, //#CJK UNIFIED IDEOGRAPH
    0xE4F2, 0x8517, //#CJK UNIFIED IDEOGRAPH
    0xE4F3, 0x8518, //#CJK UNIFIED IDEOGRAPH
    0xE4F4, 0x852C, //#CJK UNIFIED IDEOGRAPH
    0xE4F5, 0x851F, //#CJK UNIFIED IDEOGRAPH
    0xE4F6, 0x8515, //#CJK UNIFIED IDEOGRAPH
    0xE4F7, 0x8514, //#CJK UNIFIED IDEOGRAPH
    0xE4F8, 0x84FC, //#CJK UNIFIED IDEOGRAPH
    0xE4F9, 0x8540, //#CJK UNIFIED IDEOGRAPH
    0xE4FA, 0x8563, //#CJK UNIFIED IDEOGRAPH
    0xE4FB, 0x8558, //#CJK UNIFIED IDEOGRAPH
    0xE4FC, 0x8548, //#CJK UNIFIED IDEOGRAPH
    0xE540, 0x8541, //#CJK UNIFIED IDEOGRAPH
    0xE541, 0x8602, //#CJK UNIFIED IDEOGRAPH
    0xE542, 0x854B, //#CJK UNIFIED IDEOGRAPH
    0xE543, 0x8555, //#CJK UNIFIED IDEOGRAPH
    0xE544, 0x8580, //#CJK UNIFIED IDEOGRAPH
    0xE545, 0x85A4, //#CJK UNIFIED IDEOGRAPH
    0xE546, 0x8588, //#CJK UNIFIED IDEOGRAPH
    0xE547, 0x8591, //#CJK UNIFIED IDEOGRAPH
    0xE548, 0x858A, //#CJK UNIFIED IDEOGRAPH
    0xE549, 0x85A8, //#CJK UNIFIED IDEOGRAPH
    0xE54A, 0x856D, //#CJK UNIFIED IDEOGRAPH
    0xE54B, 0x8594, //#CJK UNIFIED IDEOGRAPH
    0xE54C, 0x859B, //#CJK UNIFIED IDEOGRAPH
    0xE54D, 0x85EA, //#CJK UNIFIED IDEOGRAPH
    0xE54E, 0x8587, //#CJK UNIFIED IDEOGRAPH
    0xE54F, 0x859C, //#CJK UNIFIED IDEOGRAPH
    0xE550, 0x8577, //#CJK UNIFIED IDEOGRAPH
    0xE551, 0x857E, //#CJK UNIFIED IDEOGRAPH
    0xE552, 0x8590, //#CJK UNIFIED IDEOGRAPH
    0xE553, 0x85C9, //#CJK UNIFIED IDEOGRAPH
    0xE554, 0x85BA, //#CJK UNIFIED IDEOGRAPH
    0xE555, 0x85CF, //#CJK UNIFIED IDEOGRAPH
    0xE556, 0x85B9, //#CJK UNIFIED IDEOGRAPH
    0xE557, 0x85D0, //#CJK UNIFIED IDEOGRAPH
    0xE558, 0x85D5, //#CJK UNIFIED IDEOGRAPH
    0xE559, 0x85DD, //#CJK UNIFIED IDEOGRAPH
    0xE55A, 0x85E5, //#CJK UNIFIED IDEOGRAPH
    0xE55B, 0x85DC, //#CJK UNIFIED IDEOGRAPH
    0xE55C, 0x85F9, //#CJK UNIFIED IDEOGRAPH
    0xE55D, 0x860A, //#CJK UNIFIED IDEOGRAPH
    0xE55E, 0x8613, //#CJK UNIFIED IDEOGRAPH
    0xE55F, 0x860B, //#CJK UNIFIED IDEOGRAPH
    0xE560, 0x85FE, //#CJK UNIFIED IDEOGRAPH
    0xE561, 0x85FA, //#CJK UNIFIED IDEOGRAPH
    0xE562, 0x8606, //#CJK UNIFIED IDEOGRAPH
    0xE563, 0x8622, //#CJK UNIFIED IDEOGRAPH
    0xE564, 0x861A, //#CJK UNIFIED IDEOGRAPH
    0xE565, 0x8630, //#CJK UNIFIED IDEOGRAPH
    0xE566, 0x863F, //#CJK UNIFIED IDEOGRAPH
    0xE567, 0x864D, //#CJK UNIFIED IDEOGRAPH
    0xE568, 0x4E55, //#CJK UNIFIED IDEOGRAPH
    0xE569, 0x8654, //#CJK UNIFIED IDEOGRAPH
    0xE56A, 0x865F, //#CJK UNIFIED IDEOGRAPH
    0xE56B, 0x8667, //#CJK UNIFIED IDEOGRAPH
    0xE56C, 0x8671, //#CJK UNIFIED IDEOGRAPH
    0xE56D, 0x8693, //#CJK UNIFIED IDEOGRAPH
    0xE56E, 0x86A3, //#CJK UNIFIED IDEOGRAPH
    0xE56F, 0x86A9, //#CJK UNIFIED IDEOGRAPH
    0xE570, 0x86AA, //#CJK UNIFIED IDEOGRAPH
    0xE571, 0x868B, //#CJK UNIFIED IDEOGRAPH
    0xE572, 0x868C, //#CJK UNIFIED IDEOGRAPH
    0xE573, 0x86B6, //#CJK UNIFIED IDEOGRAPH
    0xE574, 0x86AF, //#CJK UNIFIED IDEOGRAPH
    0xE575, 0x86C4, //#CJK UNIFIED IDEOGRAPH
    0xE576, 0x86C6, //#CJK UNIFIED IDEOGRAPH
    0xE577, 0x86B0, //#CJK UNIFIED IDEOGRAPH
    0xE578, 0x86C9, //#CJK UNIFIED IDEOGRAPH
    0xE579, 0x8823, //#CJK UNIFIED IDEOGRAPH
    0xE57A, 0x86AB, //#CJK UNIFIED IDEOGRAPH
    0xE57B, 0x86D4, //#CJK UNIFIED IDEOGRAPH
    0xE57C, 0x86DE, //#CJK UNIFIED IDEOGRAPH
    0xE57D, 0x86E9, //#CJK UNIFIED IDEOGRAPH
    0xE57E, 0x86EC, //#CJK UNIFIED IDEOGRAPH
    0xE580, 0x86DF, //#CJK UNIFIED IDEOGRAPH
    0xE581, 0x86DB, //#CJK UNIFIED IDEOGRAPH
    0xE582, 0x86EF, //#CJK UNIFIED IDEOGRAPH
    0xE583, 0x8712, //#CJK UNIFIED IDEOGRAPH
    0xE584, 0x8706, //#CJK UNIFIED IDEOGRAPH
    0xE585, 0x8708, //#CJK UNIFIED IDEOGRAPH
    0xE586, 0x8700, //#CJK UNIFIED IDEOGRAPH
    0xE587, 0x8703, //#CJK UNIFIED IDEOGRAPH
    0xE588, 0x86FB, //#CJK UNIFIED IDEOGRAPH
    0xE589, 0x8711, //#CJK UNIFIED IDEOGRAPH
    0xE58A, 0x8709, //#CJK UNIFIED IDEOGRAPH
    0xE58B, 0x870D, //#CJK UNIFIED IDEOGRAPH
    0xE58C, 0x86F9, //#CJK UNIFIED IDEOGRAPH
    0xE58D, 0x870A, //#CJK UNIFIED IDEOGRAPH
    0xE58E, 0x8734, //#CJK UNIFIED IDEOGRAPH
    0xE58F, 0x873F, //#CJK UNIFIED IDEOGRAPH
    0xE590, 0x8737, //#CJK UNIFIED IDEOGRAPH
    0xE591, 0x873B, //#CJK UNIFIED IDEOGRAPH
    0xE592, 0x8725, //#CJK UNIFIED IDEOGRAPH
    0xE593, 0x8729, //#CJK UNIFIED IDEOGRAPH
    0xE594, 0x871A, //#CJK UNIFIED IDEOGRAPH
    0xE595, 0x8760, //#CJK UNIFIED IDEOGRAPH
    0xE596, 0x875F, //#CJK UNIFIED IDEOGRAPH
    0xE597, 0x8778, //#CJK UNIFIED IDEOGRAPH
    0xE598, 0x874C, //#CJK UNIFIED IDEOGRAPH
    0xE599, 0x874E, //#CJK UNIFIED IDEOGRAPH
    0xE59A, 0x8774, //#CJK UNIFIED IDEOGRAPH
    0xE59B, 0x8757, //#CJK UNIFIED IDEOGRAPH
    0xE59C, 0x8768, //#CJK UNIFIED IDEOGRAPH
    0xE59D, 0x876E, //#CJK UNIFIED IDEOGRAPH
    0xE59E, 0x8759, //#CJK UNIFIED IDEOGRAPH
    0xE59F, 0x8753, //#CJK UNIFIED IDEOGRAPH
    0xE5A0, 0x8763, //#CJK UNIFIED IDEOGRAPH
    0xE5A1, 0x876A, //#CJK UNIFIED IDEOGRAPH
    0xE5A2, 0x8805, //#CJK UNIFIED IDEOGRAPH
    0xE5A3, 0x87A2, //#CJK UNIFIED IDEOGRAPH
    0xE5A4, 0x879F, //#CJK UNIFIED IDEOGRAPH
    0xE5A5, 0x8782, //#CJK UNIFIED IDEOGRAPH
    0xE5A6, 0x87AF, //#CJK UNIFIED IDEOGRAPH
    0xE5A7, 0x87CB, //#CJK UNIFIED IDEOGRAPH
    0xE5A8, 0x87BD, //#CJK UNIFIED IDEOGRAPH
    0xE5A9, 0x87C0, //#CJK UNIFIED IDEOGRAPH
    0xE5AA, 0x87D0, //#CJK UNIFIED IDEOGRAPH
    0xE5AB, 0x96D6, //#CJK UNIFIED IDEOGRAPH
    0xE5AC, 0x87AB, //#CJK UNIFIED IDEOGRAPH
    0xE5AD, 0x87C4, //#CJK UNIFIED IDEOGRAPH
    0xE5AE, 0x87B3, //#CJK UNIFIED IDEOGRAPH
    0xE5AF, 0x87C7, //#CJK UNIFIED IDEOGRAPH
    0xE5B0, 0x87C6, //#CJK UNIFIED IDEOGRAPH
    0xE5B1, 0x87BB, //#CJK UNIFIED IDEOGRAPH
    0xE5B2, 0x87EF, //#CJK UNIFIED IDEOGRAPH
    0xE5B3, 0x87F2, //#CJK UNIFIED IDEOGRAPH
    0xE5B4, 0x87E0, //#CJK UNIFIED IDEOGRAPH
    0xE5B5, 0x880F, //#CJK UNIFIED IDEOGRAPH
    0xE5B6, 0x880D, //#CJK UNIFIED IDEOGRAPH
    0xE5B7, 0x87FE, //#CJK UNIFIED IDEOGRAPH
    0xE5B8, 0x87F6, //#CJK UNIFIED IDEOGRAPH
    0xE5B9, 0x87F7, //#CJK UNIFIED IDEOGRAPH
    0xE5BA, 0x880E, //#CJK UNIFIED IDEOGRAPH
    0xE5BB, 0x87D2, //#CJK UNIFIED IDEOGRAPH
    0xE5BC, 0x8811, //#CJK UNIFIED IDEOGRAPH
    0xE5BD, 0x8816, //#CJK UNIFIED IDEOGRAPH
    0xE5BE, 0x8815, //#CJK UNIFIED IDEOGRAPH
    0xE5BF, 0x8822, //#CJK UNIFIED IDEOGRAPH
    0xE5C0, 0x8821, //#CJK UNIFIED IDEOGRAPH
    0xE5C1, 0x8831, //#CJK UNIFIED IDEOGRAPH
    0xE5C2, 0x8836, //#CJK UNIFIED IDEOGRAPH
    0xE5C3, 0x8839, //#CJK UNIFIED IDEOGRAPH
    0xE5C4, 0x8827, //#CJK UNIFIED IDEOGRAPH
    0xE5C5, 0x883B, //#CJK UNIFIED IDEOGRAPH
    0xE5C6, 0x8844, //#CJK UNIFIED IDEOGRAPH
    0xE5C7, 0x8842, //#CJK UNIFIED IDEOGRAPH
    0xE5C8, 0x8852, //#CJK UNIFIED IDEOGRAPH
    0xE5C9, 0x8859, //#CJK UNIFIED IDEOGRAPH
    0xE5CA, 0x885E, //#CJK UNIFIED IDEOGRAPH
    0xE5CB, 0x8862, //#CJK UNIFIED IDEOGRAPH
    0xE5CC, 0x886B, //#CJK UNIFIED IDEOGRAPH
    0xE5CD, 0x8881, //#CJK UNIFIED IDEOGRAPH
    0xE5CE, 0x887E, //#CJK UNIFIED IDEOGRAPH
    0xE5CF, 0x889E, //#CJK UNIFIED IDEOGRAPH
    0xE5D0, 0x8875, //#CJK UNIFIED IDEOGRAPH
    0xE5D1, 0x887D, //#CJK UNIFIED IDEOGRAPH
    0xE5D2, 0x88B5, //#CJK UNIFIED IDEOGRAPH
    0xE5D3, 0x8872, //#CJK UNIFIED IDEOGRAPH
    0xE5D4, 0x8882, //#CJK UNIFIED IDEOGRAPH
    0xE5D5, 0x8897, //#CJK UNIFIED IDEOGRAPH
    0xE5D6, 0x8892, //#CJK UNIFIED IDEOGRAPH
    0xE5D7, 0x88AE, //#CJK UNIFIED IDEOGRAPH
    0xE5D8, 0x8899, //#CJK UNIFIED IDEOGRAPH
    0xE5D9, 0x88A2, //#CJK UNIFIED IDEOGRAPH
    0xE5DA, 0x888D, //#CJK UNIFIED IDEOGRAPH
    0xE5DB, 0x88A4, //#CJK UNIFIED IDEOGRAPH
    0xE5DC, 0x88B0, //#CJK UNIFIED IDEOGRAPH
    0xE5DD, 0x88BF, //#CJK UNIFIED IDEOGRAPH
    0xE5DE, 0x88B1, //#CJK UNIFIED IDEOGRAPH
    0xE5DF, 0x88C3, //#CJK UNIFIED IDEOGRAPH
    0xE5E0, 0x88C4, //#CJK UNIFIED IDEOGRAPH
    0xE5E1, 0x88D4, //#CJK UNIFIED IDEOGRAPH
    0xE5E2, 0x88D8, //#CJK UNIFIED IDEOGRAPH
    0xE5E3, 0x88D9, //#CJK UNIFIED IDEOGRAPH
    0xE5E4, 0x88DD, //#CJK UNIFIED IDEOGRAPH
    0xE5E5, 0x88F9, //#CJK UNIFIED IDEOGRAPH
    0xE5E6, 0x8902, //#CJK UNIFIED IDEOGRAPH
    0xE5E7, 0x88FC, //#CJK UNIFIED IDEOGRAPH
    0xE5E8, 0x88F4, //#CJK UNIFIED IDEOGRAPH
    0xE5E9, 0x88E8, //#CJK UNIFIED IDEOGRAPH
    0xE5EA, 0x88F2, //#CJK UNIFIED IDEOGRAPH
    0xE5EB, 0x8904, //#CJK UNIFIED IDEOGRAPH
    0xE5EC, 0x890C, //#CJK UNIFIED IDEOGRAPH
    0xE5ED, 0x890A, //#CJK UNIFIED IDEOGRAPH
    0xE5EE, 0x8913, //#CJK UNIFIED IDEOGRAPH
    0xE5EF, 0x8943, //#CJK UNIFIED IDEOGRAPH
    0xE5F0, 0x891E, //#CJK UNIFIED IDEOGRAPH
    0xE5F1, 0x8925, //#CJK UNIFIED IDEOGRAPH
    0xE5F2, 0x892A, //#CJK UNIFIED IDEOGRAPH
    0xE5F3, 0x892B, //#CJK UNIFIED IDEOGRAPH
    0xE5F4, 0x8941, //#CJK UNIFIED IDEOGRAPH
    0xE5F5, 0x8944, //#CJK UNIFIED IDEOGRAPH
    0xE5F6, 0x893B, //#CJK UNIFIED IDEOGRAPH
    0xE5F7, 0x8936, //#CJK UNIFIED IDEOGRAPH
    0xE5F8, 0x8938, //#CJK UNIFIED IDEOGRAPH
    0xE5F9, 0x894C, //#CJK UNIFIED IDEOGRAPH
    0xE5FA, 0x891D, //#CJK UNIFIED IDEOGRAPH
    0xE5FB, 0x8960, //#CJK UNIFIED IDEOGRAPH
    0xE5FC, 0x895E, //#CJK UNIFIED IDEOGRAPH
    0xE640, 0x8966, //#CJK UNIFIED IDEOGRAPH
    0xE641, 0x8964, //#CJK UNIFIED IDEOGRAPH
    0xE642, 0x896D, //#CJK UNIFIED IDEOGRAPH
    0xE643, 0x896A, //#CJK UNIFIED IDEOGRAPH
    0xE644, 0x896F, //#CJK UNIFIED IDEOGRAPH
    0xE645, 0x8974, //#CJK UNIFIED IDEOGRAPH
    0xE646, 0x8977, //#CJK UNIFIED IDEOGRAPH
    0xE647, 0x897E, //#CJK UNIFIED IDEOGRAPH
    0xE648, 0x8983, //#CJK UNIFIED IDEOGRAPH
    0xE649, 0x8988, //#CJK UNIFIED IDEOGRAPH
    0xE64A, 0x898A, //#CJK UNIFIED IDEOGRAPH
    0xE64B, 0x8993, //#CJK UNIFIED IDEOGRAPH
    0xE64C, 0x8998, //#CJK UNIFIED IDEOGRAPH
    0xE64D, 0x89A1, //#CJK UNIFIED IDEOGRAPH
    0xE64E, 0x89A9, //#CJK UNIFIED IDEOGRAPH
    0xE64F, 0x89A6, //#CJK UNIFIED IDEOGRAPH
    0xE650, 0x89AC, //#CJK UNIFIED IDEOGRAPH
    0xE651, 0x89AF, //#CJK UNIFIED IDEOGRAPH
    0xE652, 0x89B2, //#CJK UNIFIED IDEOGRAPH
    0xE653, 0x89BA, //#CJK UNIFIED IDEOGRAPH
    0xE654, 0x89BD, //#CJK UNIFIED IDEOGRAPH
    0xE655, 0x89BF, //#CJK UNIFIED IDEOGRAPH
    0xE656, 0x89C0, //#CJK UNIFIED IDEOGRAPH
    0xE657, 0x89DA, //#CJK UNIFIED IDEOGRAPH
    0xE658, 0x89DC, //#CJK UNIFIED IDEOGRAPH
    0xE659, 0x89DD, //#CJK UNIFIED IDEOGRAPH
    0xE65A, 0x89E7, //#CJK UNIFIED IDEOGRAPH
    0xE65B, 0x89F4, //#CJK UNIFIED IDEOGRAPH
    0xE65C, 0x89F8, //#CJK UNIFIED IDEOGRAPH
    0xE65D, 0x8A03, //#CJK UNIFIED IDEOGRAPH
    0xE65E, 0x8A16, //#CJK UNIFIED IDEOGRAPH
    0xE65F, 0x8A10, //#CJK UNIFIED IDEOGRAPH
    0xE660, 0x8A0C, //#CJK UNIFIED IDEOGRAPH
    0xE661, 0x8A1B, //#CJK UNIFIED IDEOGRAPH
    0xE662, 0x8A1D, //#CJK UNIFIED IDEOGRAPH
    0xE663, 0x8A25, //#CJK UNIFIED IDEOGRAPH
    0xE664, 0x8A36, //#CJK UNIFIED IDEOGRAPH
    0xE665, 0x8A41, //#CJK UNIFIED IDEOGRAPH
    0xE666, 0x8A5B, //#CJK UNIFIED IDEOGRAPH
    0xE667, 0x8A52, //#CJK UNIFIED IDEOGRAPH
    0xE668, 0x8A46, //#CJK UNIFIED IDEOGRAPH
    0xE669, 0x8A48, //#CJK UNIFIED IDEOGRAPH
    0xE66A, 0x8A7C, //#CJK UNIFIED IDEOGRAPH
    0xE66B, 0x8A6D, //#CJK UNIFIED IDEOGRAPH
    0xE66C, 0x8A6C, //#CJK UNIFIED IDEOGRAPH
    0xE66D, 0x8A62, //#CJK UNIFIED IDEOGRAPH
    0xE66E, 0x8A85, //#CJK UNIFIED IDEOGRAPH
    0xE66F, 0x8A82, //#CJK UNIFIED IDEOGRAPH
    0xE670, 0x8A84, //#CJK UNIFIED IDEOGRAPH
    0xE671, 0x8AA8, //#CJK UNIFIED IDEOGRAPH
    0xE672, 0x8AA1, //#CJK UNIFIED IDEOGRAPH
    0xE673, 0x8A91, //#CJK UNIFIED IDEOGRAPH
    0xE674, 0x8AA5, //#CJK UNIFIED IDEOGRAPH
    0xE675, 0x8AA6, //#CJK UNIFIED IDEOGRAPH
    0xE676, 0x8A9A, //#CJK UNIFIED IDEOGRAPH
    0xE677, 0x8AA3, //#CJK UNIFIED IDEOGRAPH
    0xE678, 0x8AC4, //#CJK UNIFIED IDEOGRAPH
    0xE679, 0x8ACD, //#CJK UNIFIED IDEOGRAPH
    0xE67A, 0x8AC2, //#CJK UNIFIED IDEOGRAPH
    0xE67B, 0x8ADA, //#CJK UNIFIED IDEOGRAPH
    0xE67C, 0x8AEB, //#CJK UNIFIED IDEOGRAPH
    0xE67D, 0x8AF3, //#CJK UNIFIED IDEOGRAPH
    0xE67E, 0x8AE7, //#CJK UNIFIED IDEOGRAPH
    0xE680, 0x8AE4, //#CJK UNIFIED IDEOGRAPH
    0xE681, 0x8AF1, //#CJK UNIFIED IDEOGRAPH
    0xE682, 0x8B14, //#CJK UNIFIED IDEOGRAPH
    0xE683, 0x8AE0, //#CJK UNIFIED IDEOGRAPH
    0xE684, 0x8AE2, //#CJK UNIFIED IDEOGRAPH
    0xE685, 0x8AF7, //#CJK UNIFIED IDEOGRAPH
    0xE686, 0x8ADE, //#CJK UNIFIED IDEOGRAPH
    0xE687, 0x8ADB, //#CJK UNIFIED IDEOGRAPH
    0xE688, 0x8B0C, //#CJK UNIFIED IDEOGRAPH
    0xE689, 0x8B07, //#CJK UNIFIED IDEOGRAPH
    0xE68A, 0x8B1A, //#CJK UNIFIED IDEOGRAPH
    0xE68B, 0x8AE1, //#CJK UNIFIED IDEOGRAPH
    0xE68C, 0x8B16, //#CJK UNIFIED IDEOGRAPH
    0xE68D, 0x8B10, //#CJK UNIFIED IDEOGRAPH
    0xE68E, 0x8B17, //#CJK UNIFIED IDEOGRAPH
    0xE68F, 0x8B20, //#CJK UNIFIED IDEOGRAPH
    0xE690, 0x8B33, //#CJK UNIFIED IDEOGRAPH
    0xE691, 0x97AB, //#CJK UNIFIED IDEOGRAPH
    0xE692, 0x8B26, //#CJK UNIFIED IDEOGRAPH
    0xE693, 0x8B2B, //#CJK UNIFIED IDEOGRAPH
    0xE694, 0x8B3E, //#CJK UNIFIED IDEOGRAPH
    0xE695, 0x8B28, //#CJK UNIFIED IDEOGRAPH
    0xE696, 0x8B41, //#CJK UNIFIED IDEOGRAPH
    0xE697, 0x8B4C, //#CJK UNIFIED IDEOGRAPH
    0xE698, 0x8B4F, //#CJK UNIFIED IDEOGRAPH
    0xE699, 0x8B4E, //#CJK UNIFIED IDEOGRAPH
    0xE69A, 0x8B49, //#CJK UNIFIED IDEOGRAPH
    0xE69B, 0x8B56, //#CJK UNIFIED IDEOGRAPH
    0xE69C, 0x8B5B, //#CJK UNIFIED IDEOGRAPH
    0xE69D, 0x8B5A, //#CJK UNIFIED IDEOGRAPH
    0xE69E, 0x8B6B, //#CJK UNIFIED IDEOGRAPH
    0xE69F, 0x8B5F, //#CJK UNIFIED IDEOGRAPH
    0xE6A0, 0x8B6C, //#CJK UNIFIED IDEOGRAPH
    0xE6A1, 0x8B6F, //#CJK UNIFIED IDEOGRAPH
    0xE6A2, 0x8B74, //#CJK UNIFIED IDEOGRAPH
    0xE6A3, 0x8B7D, //#CJK UNIFIED IDEOGRAPH
    0xE6A4, 0x8B80, //#CJK UNIFIED IDEOGRAPH
    0xE6A5, 0x8B8C, //#CJK UNIFIED IDEOGRAPH
    0xE6A6, 0x8B8E, //#CJK UNIFIED IDEOGRAPH
    0xE6A7, 0x8B92, //#CJK UNIFIED IDEOGRAPH
    0xE6A8, 0x8B93, //#CJK UNIFIED IDEOGRAPH
    0xE6A9, 0x8B96, //#CJK UNIFIED IDEOGRAPH
    0xE6AA, 0x8B99, //#CJK UNIFIED IDEOGRAPH
    0xE6AB, 0x8B9A, //#CJK UNIFIED IDEOGRAPH
    0xE6AC, 0x8C3A, //#CJK UNIFIED IDEOGRAPH
    0xE6AD, 0x8C41, //#CJK UNIFIED IDEOGRAPH
    0xE6AE, 0x8C3F, //#CJK UNIFIED IDEOGRAPH
    0xE6AF, 0x8C48, //#CJK UNIFIED IDEOGRAPH
    0xE6B0, 0x8C4C, //#CJK UNIFIED IDEOGRAPH
    0xE6B1, 0x8C4E, //#CJK UNIFIED IDEOGRAPH
    0xE6B2, 0x8C50, //#CJK UNIFIED IDEOGRAPH
    0xE6B3, 0x8C55, //#CJK UNIFIED IDEOGRAPH
    0xE6B4, 0x8C62, //#CJK UNIFIED IDEOGRAPH
    0xE6B5, 0x8C6C, //#CJK UNIFIED IDEOGRAPH
    0xE6B6, 0x8C78, //#CJK UNIFIED IDEOGRAPH
    0xE6B7, 0x8C7A, //#CJK UNIFIED IDEOGRAPH
    0xE6B8, 0x8C82, //#CJK UNIFIED IDEOGRAPH
    0xE6B9, 0x8C89, //#CJK UNIFIED IDEOGRAPH
    0xE6BA, 0x8C85, //#CJK UNIFIED IDEOGRAPH
    0xE6BB, 0x8C8A, //#CJK UNIFIED IDEOGRAPH
    0xE6BC, 0x8C8D, //#CJK UNIFIED IDEOGRAPH
    0xE6BD, 0x8C8E, //#CJK UNIFIED IDEOGRAPH
    0xE6BE, 0x8C94, //#CJK UNIFIED IDEOGRAPH
    0xE6BF, 0x8C7C, //#CJK UNIFIED IDEOGRAPH
    0xE6C0, 0x8C98, //#CJK UNIFIED IDEOGRAPH
    0xE6C1, 0x621D, //#CJK UNIFIED IDEOGRAPH
    0xE6C2, 0x8CAD, //#CJK UNIFIED IDEOGRAPH
    0xE6C3, 0x8CAA, //#CJK UNIFIED IDEOGRAPH
    0xE6C4, 0x8CBD, //#CJK UNIFIED IDEOGRAPH
    0xE6C5, 0x8CB2, //#CJK UNIFIED IDEOGRAPH
    0xE6C6, 0x8CB3, //#CJK UNIFIED IDEOGRAPH
    0xE6C7, 0x8CAE, //#CJK UNIFIED IDEOGRAPH
    0xE6C8, 0x8CB6, //#CJK UNIFIED IDEOGRAPH
    0xE6C9, 0x8CC8, //#CJK UNIFIED IDEOGRAPH
    0xE6CA, 0x8CC1, //#CJK UNIFIED IDEOGRAPH
    0xE6CB, 0x8CE4, //#CJK UNIFIED IDEOGRAPH
    0xE6CC, 0x8CE3, //#CJK UNIFIED IDEOGRAPH
    0xE6CD, 0x8CDA, //#CJK UNIFIED IDEOGRAPH
    0xE6CE, 0x8CFD, //#CJK UNIFIED IDEOGRAPH
    0xE6CF, 0x8CFA, //#CJK UNIFIED IDEOGRAPH
    0xE6D0, 0x8CFB, //#CJK UNIFIED IDEOGRAPH
    0xE6D1, 0x8D04, //#CJK UNIFIED IDEOGRAPH
    0xE6D2, 0x8D05, //#CJK UNIFIED IDEOGRAPH
    0xE6D3, 0x8D0A, //#CJK UNIFIED IDEOGRAPH
    0xE6D4, 0x8D07, //#CJK UNIFIED IDEOGRAPH
    0xE6D5, 0x8D0F, //#CJK UNIFIED IDEOGRAPH
    0xE6D6, 0x8D0D, //#CJK UNIFIED IDEOGRAPH
    0xE6D7, 0x8D10, //#CJK UNIFIED IDEOGRAPH
    0xE6D8, 0x9F4E, //#CJK UNIFIED IDEOGRAPH
    0xE6D9, 0x8D13, //#CJK UNIFIED IDEOGRAPH
    0xE6DA, 0x8CCD, //#CJK UNIFIED IDEOGRAPH
    0xE6DB, 0x8D14, //#CJK UNIFIED IDEOGRAPH
    0xE6DC, 0x8D16, //#CJK UNIFIED IDEOGRAPH
    0xE6DD, 0x8D67, //#CJK UNIFIED IDEOGRAPH
    0xE6DE, 0x8D6D, //#CJK UNIFIED IDEOGRAPH
    0xE6DF, 0x8D71, //#CJK UNIFIED IDEOGRAPH
    0xE6E0, 0x8D73, //#CJK UNIFIED IDEOGRAPH
    0xE6E1, 0x8D81, //#CJK UNIFIED IDEOGRAPH
    0xE6E2, 0x8D99, //#CJK UNIFIED IDEOGRAPH
    0xE6E3, 0x8DC2, //#CJK UNIFIED IDEOGRAPH
    0xE6E4, 0x8DBE, //#CJK UNIFIED IDEOGRAPH
    0xE6E5, 0x8DBA, //#CJK UNIFIED IDEOGRAPH
    0xE6E6, 0x8DCF, //#CJK UNIFIED IDEOGRAPH
    0xE6E7, 0x8DDA, //#CJK UNIFIED IDEOGRAPH
    0xE6E8, 0x8DD6, //#CJK UNIFIED IDEOGRAPH
    0xE6E9, 0x8DCC, //#CJK UNIFIED IDEOGRAPH
    0xE6EA, 0x8DDB, //#CJK UNIFIED IDEOGRAPH
    0xE6EB, 0x8DCB, //#CJK UNIFIED IDEOGRAPH
    0xE6EC, 0x8DEA, //#CJK UNIFIED IDEOGRAPH
    0xE6ED, 0x8DEB, //#CJK UNIFIED IDEOGRAPH
    0xE6EE, 0x8DDF, //#CJK UNIFIED IDEOGRAPH
    0xE6EF, 0x8DE3, //#CJK UNIFIED IDEOGRAPH
    0xE6F0, 0x8DFC, //#CJK UNIFIED IDEOGRAPH
    0xE6F1, 0x8E08, //#CJK UNIFIED IDEOGRAPH
    0xE6F2, 0x8E09, //#CJK UNIFIED IDEOGRAPH
    0xE6F3, 0x8DFF, //#CJK UNIFIED IDEOGRAPH
    0xE6F4, 0x8E1D, //#CJK UNIFIED IDEOGRAPH
    0xE6F5, 0x8E1E, //#CJK UNIFIED IDEOGRAPH
    0xE6F6, 0x8E10, //#CJK UNIFIED IDEOGRAPH
    0xE6F7, 0x8E1F, //#CJK UNIFIED IDEOGRAPH
    0xE6F8, 0x8E42, //#CJK UNIFIED IDEOGRAPH
    0xE6F9, 0x8E35, //#CJK UNIFIED IDEOGRAPH
    0xE6FA, 0x8E30, //#CJK UNIFIED IDEOGRAPH
    0xE6FB, 0x8E34, //#CJK UNIFIED IDEOGRAPH
    0xE6FC, 0x8E4A, //#CJK UNIFIED IDEOGRAPH
    0xE740, 0x8E47, //#CJK UNIFIED IDEOGRAPH
    0xE741, 0x8E49, //#CJK UNIFIED IDEOGRAPH
    0xE742, 0x8E4C, //#CJK UNIFIED IDEOGRAPH
    0xE743, 0x8E50, //#CJK UNIFIED IDEOGRAPH
    0xE744, 0x8E48, //#CJK UNIFIED IDEOGRAPH
    0xE745, 0x8E59, //#CJK UNIFIED IDEOGRAPH
    0xE746, 0x8E64, //#CJK UNIFIED IDEOGRAPH
    0xE747, 0x8E60, //#CJK UNIFIED IDEOGRAPH
    0xE748, 0x8E2A, //#CJK UNIFIED IDEOGRAPH
    0xE749, 0x8E63, //#CJK UNIFIED IDEOGRAPH
    0xE74A, 0x8E55, //#CJK UNIFIED IDEOGRAPH
    0xE74B, 0x8E76, //#CJK UNIFIED IDEOGRAPH
    0xE74C, 0x8E72, //#CJK UNIFIED IDEOGRAPH
    0xE74D, 0x8E7C, //#CJK UNIFIED IDEOGRAPH
    0xE74E, 0x8E81, //#CJK UNIFIED IDEOGRAPH
    0xE74F, 0x8E87, //#CJK UNIFIED IDEOGRAPH
    0xE750, 0x8E85, //#CJK UNIFIED IDEOGRAPH
    0xE751, 0x8E84, //#CJK UNIFIED IDEOGRAPH
    0xE752, 0x8E8B, //#CJK UNIFIED IDEOGRAPH
    0xE753, 0x8E8A, //#CJK UNIFIED IDEOGRAPH
    0xE754, 0x8E93, //#CJK UNIFIED IDEOGRAPH
    0xE755, 0x8E91, //#CJK UNIFIED IDEOGRAPH
    0xE756, 0x8E94, //#CJK UNIFIED IDEOGRAPH
    0xE757, 0x8E99, //#CJK UNIFIED IDEOGRAPH
    0xE758, 0x8EAA, //#CJK UNIFIED IDEOGRAPH
    0xE759, 0x8EA1, //#CJK UNIFIED IDEOGRAPH
    0xE75A, 0x8EAC, //#CJK UNIFIED IDEOGRAPH
    0xE75B, 0x8EB0, //#CJK UNIFIED IDEOGRAPH
    0xE75C, 0x8EC6, //#CJK UNIFIED IDEOGRAPH
    0xE75D, 0x8EB1, //#CJK UNIFIED IDEOGRAPH
    0xE75E, 0x8EBE, //#CJK UNIFIED IDEOGRAPH
    0xE75F, 0x8EC5, //#CJK UNIFIED IDEOGRAPH
    0xE760, 0x8EC8, //#CJK UNIFIED IDEOGRAPH
    0xE761, 0x8ECB, //#CJK UNIFIED IDEOGRAPH
    0xE762, 0x8EDB, //#CJK UNIFIED IDEOGRAPH
    0xE763, 0x8EE3, //#CJK UNIFIED IDEOGRAPH
    0xE764, 0x8EFC, //#CJK UNIFIED IDEOGRAPH
    0xE765, 0x8EFB, //#CJK UNIFIED IDEOGRAPH
    0xE766, 0x8EEB, //#CJK UNIFIED IDEOGRAPH
    0xE767, 0x8EFE, //#CJK UNIFIED IDEOGRAPH
    0xE768, 0x8F0A, //#CJK UNIFIED IDEOGRAPH
    0xE769, 0x8F05, //#CJK UNIFIED IDEOGRAPH
    0xE76A, 0x8F15, //#CJK UNIFIED IDEOGRAPH
    0xE76B, 0x8F12, //#CJK UNIFIED IDEOGRAPH
    0xE76C, 0x8F19, //#CJK UNIFIED IDEOGRAPH
    0xE76D, 0x8F13, //#CJK UNIFIED IDEOGRAPH
    0xE76E, 0x8F1C, //#CJK UNIFIED IDEOGRAPH
    0xE76F, 0x8F1F, //#CJK UNIFIED IDEOGRAPH
    0xE770, 0x8F1B, //#CJK UNIFIED IDEOGRAPH
    0xE771, 0x8F0C, //#CJK UNIFIED IDEOGRAPH
    0xE772, 0x8F26, //#CJK UNIFIED IDEOGRAPH
    0xE773, 0x8F33, //#CJK UNIFIED IDEOGRAPH
    0xE774, 0x8F3B, //#CJK UNIFIED IDEOGRAPH
    0xE775, 0x8F39, //#CJK UNIFIED IDEOGRAPH
    0xE776, 0x8F45, //#CJK UNIFIED IDEOGRAPH
    0xE777, 0x8F42, //#CJK UNIFIED IDEOGRAPH
    0xE778, 0x8F3E, //#CJK UNIFIED IDEOGRAPH
    0xE779, 0x8F4C, //#CJK UNIFIED IDEOGRAPH
    0xE77A, 0x8F49, //#CJK UNIFIED IDEOGRAPH
    0xE77B, 0x8F46, //#CJK UNIFIED IDEOGRAPH
    0xE77C, 0x8F4E, //#CJK UNIFIED IDEOGRAPH
    0xE77D, 0x8F57, //#CJK UNIFIED IDEOGRAPH
    0xE77E, 0x8F5C, //#CJK UNIFIED IDEOGRAPH
    0xE780, 0x8F62, //#CJK UNIFIED IDEOGRAPH
    0xE781, 0x8F63, //#CJK UNIFIED IDEOGRAPH
    0xE782, 0x8F64, //#CJK UNIFIED IDEOGRAPH
    0xE783, 0x8F9C, //#CJK UNIFIED IDEOGRAPH
    0xE784, 0x8F9F, //#CJK UNIFIED IDEOGRAPH
    0xE785, 0x8FA3, //#CJK UNIFIED IDEOGRAPH
    0xE786, 0x8FAD, //#CJK UNIFIED IDEOGRAPH
    0xE787, 0x8FAF, //#CJK UNIFIED IDEOGRAPH
    0xE788, 0x8FB7, //#CJK UNIFIED IDEOGRAPH
    0xE789, 0x8FDA, //#CJK UNIFIED IDEOGRAPH
    0xE78A, 0x8FE5, //#CJK UNIFIED IDEOGRAPH
    0xE78B, 0x8FE2, //#CJK UNIFIED IDEOGRAPH
    0xE78C, 0x8FEA, //#CJK UNIFIED IDEOGRAPH
    0xE78D, 0x8FEF, //#CJK UNIFIED IDEOGRAPH
    0xE78E, 0x9087, //#CJK UNIFIED IDEOGRAPH
    0xE78F, 0x8FF4, //#CJK UNIFIED IDEOGRAPH
    0xE790, 0x9005, //#CJK UNIFIED IDEOGRAPH
    0xE791, 0x8FF9, //#CJK UNIFIED IDEOGRAPH
    0xE792, 0x8FFA, //#CJK UNIFIED IDEOGRAPH
    0xE793, 0x9011, //#CJK UNIFIED IDEOGRAPH
    0xE794, 0x9015, //#CJK UNIFIED IDEOGRAPH
    0xE795, 0x9021, //#CJK UNIFIED IDEOGRAPH
    0xE796, 0x900D, //#CJK UNIFIED IDEOGRAPH
    0xE797, 0x901E, //#CJK UNIFIED IDEOGRAPH
    0xE798, 0x9016, //#CJK UNIFIED IDEOGRAPH
    0xE799, 0x900B, //#CJK UNIFIED IDEOGRAPH
    0xE79A, 0x9027, //#CJK UNIFIED IDEOGRAPH
    0xE79B, 0x9036, //#CJK UNIFIED IDEOGRAPH
    0xE79C, 0x9035, //#CJK UNIFIED IDEOGRAPH
    0xE79D, 0x9039, //#CJK UNIFIED IDEOGRAPH
    0xE79E, 0x8FF8, //#CJK UNIFIED IDEOGRAPH
    0xE79F, 0x904F, //#CJK UNIFIED IDEOGRAPH
    0xE7A0, 0x9050, //#CJK UNIFIED IDEOGRAPH
    0xE7A1, 0x9051, //#CJK UNIFIED IDEOGRAPH
    0xE7A2, 0x9052, //#CJK UNIFIED IDEOGRAPH
    0xE7A3, 0x900E, //#CJK UNIFIED IDEOGRAPH
    0xE7A4, 0x9049, //#CJK UNIFIED IDEOGRAPH
    0xE7A5, 0x903E, //#CJK UNIFIED IDEOGRAPH
    0xE7A6, 0x9056, //#CJK UNIFIED IDEOGRAPH
    0xE7A7, 0x9058, //#CJK UNIFIED IDEOGRAPH
    0xE7A8, 0x905E, //#CJK UNIFIED IDEOGRAPH
    0xE7A9, 0x9068, //#CJK UNIFIED IDEOGRAPH
    0xE7AA, 0x906F, //#CJK UNIFIED IDEOGRAPH
    0xE7AB, 0x9076, //#CJK UNIFIED IDEOGRAPH
    0xE7AC, 0x96A8, //#CJK UNIFIED IDEOGRAPH
    0xE7AD, 0x9072, //#CJK UNIFIED IDEOGRAPH
    0xE7AE, 0x9082, //#CJK UNIFIED IDEOGRAPH
    0xE7AF, 0x907D, //#CJK UNIFIED IDEOGRAPH
    0xE7B0, 0x9081, //#CJK UNIFIED IDEOGRAPH
    0xE7B1, 0x9080, //#CJK UNIFIED IDEOGRAPH
    0xE7B2, 0x908A, //#CJK UNIFIED IDEOGRAPH
    0xE7B3, 0x9089, //#CJK UNIFIED IDEOGRAPH
    0xE7B4, 0x908F, //#CJK UNIFIED IDEOGRAPH
    0xE7B5, 0x90A8, //#CJK UNIFIED IDEOGRAPH
    0xE7B6, 0x90AF, //#CJK UNIFIED IDEOGRAPH
    0xE7B7, 0x90B1, //#CJK UNIFIED IDEOGRAPH
    0xE7B8, 0x90B5, //#CJK UNIFIED IDEOGRAPH
    0xE7B9, 0x90E2, //#CJK UNIFIED IDEOGRAPH
    0xE7BA, 0x90E4, //#CJK UNIFIED IDEOGRAPH
    0xE7BB, 0x6248, //#CJK UNIFIED IDEOGRAPH
    0xE7BC, 0x90DB, //#CJK UNIFIED IDEOGRAPH
    0xE7BD, 0x9102, //#CJK UNIFIED IDEOGRAPH
    0xE7BE, 0x9112, //#CJK UNIFIED IDEOGRAPH
    0xE7BF, 0x9119, //#CJK UNIFIED IDEOGRAPH
    0xE7C0, 0x9132, //#CJK UNIFIED IDEOGRAPH
    0xE7C1, 0x9130, //#CJK UNIFIED IDEOGRAPH
    0xE7C2, 0x914A, //#CJK UNIFIED IDEOGRAPH
    0xE7C3, 0x9156, //#CJK UNIFIED IDEOGRAPH
    0xE7C4, 0x9158, //#CJK UNIFIED IDEOGRAPH
    0xE7C5, 0x9163, //#CJK UNIFIED IDEOGRAPH
    0xE7C6, 0x9165, //#CJK UNIFIED IDEOGRAPH
    0xE7C7, 0x9169, //#CJK UNIFIED IDEOGRAPH
    0xE7C8, 0x9173, //#CJK UNIFIED IDEOGRAPH
    0xE7C9, 0x9172, //#CJK UNIFIED IDEOGRAPH
    0xE7CA, 0x918B, //#CJK UNIFIED IDEOGRAPH
    0xE7CB, 0x9189, //#CJK UNIFIED IDEOGRAPH
    0xE7CC, 0x9182, //#CJK UNIFIED IDEOGRAPH
    0xE7CD, 0x91A2, //#CJK UNIFIED IDEOGRAPH
    0xE7CE, 0x91AB, //#CJK UNIFIED IDEOGRAPH
    0xE7CF, 0x91AF, //#CJK UNIFIED IDEOGRAPH
    0xE7D0, 0x91AA, //#CJK UNIFIED IDEOGRAPH
    0xE7D1, 0x91B5, //#CJK UNIFIED IDEOGRAPH
    0xE7D2, 0x91B4, //#CJK UNIFIED IDEOGRAPH
    0xE7D3, 0x91BA, //#CJK UNIFIED IDEOGRAPH
    0xE7D4, 0x91C0, //#CJK UNIFIED IDEOGRAPH
    0xE7D5, 0x91C1, //#CJK UNIFIED IDEOGRAPH
    0xE7D6, 0x91C9, //#CJK UNIFIED IDEOGRAPH
    0xE7D7, 0x91CB, //#CJK UNIFIED IDEOGRAPH
    0xE7D8, 0x91D0, //#CJK UNIFIED IDEOGRAPH
    0xE7D9, 0x91D6, //#CJK UNIFIED IDEOGRAPH
    0xE7DA, 0x91DF, //#CJK UNIFIED IDEOGRAPH
    0xE7DB, 0x91E1, //#CJK UNIFIED IDEOGRAPH
    0xE7DC, 0x91DB, //#CJK UNIFIED IDEOGRAPH
    0xE7DD, 0x91FC, //#CJK UNIFIED IDEOGRAPH
    0xE7DE, 0x91F5, //#CJK UNIFIED IDEOGRAPH
    0xE7DF, 0x91F6, //#CJK UNIFIED IDEOGRAPH
    0xE7E0, 0x921E, //#CJK UNIFIED IDEOGRAPH
    0xE7E1, 0x91FF, //#CJK UNIFIED IDEOGRAPH
    0xE7E2, 0x9214, //#CJK UNIFIED IDEOGRAPH
    0xE7E3, 0x922C, //#CJK UNIFIED IDEOGRAPH
    0xE7E4, 0x9215, //#CJK UNIFIED IDEOGRAPH
    0xE7E5, 0x9211, //#CJK UNIFIED IDEOGRAPH
    0xE7E6, 0x925E, //#CJK UNIFIED IDEOGRAPH
    0xE7E7, 0x9257, //#CJK UNIFIED IDEOGRAPH
    0xE7E8, 0x9245, //#CJK UNIFIED IDEOGRAPH
    0xE7E9, 0x9249, //#CJK UNIFIED IDEOGRAPH
    0xE7EA, 0x9264, //#CJK UNIFIED IDEOGRAPH
    0xE7EB, 0x9248, //#CJK UNIFIED IDEOGRAPH
    0xE7EC, 0x9295, //#CJK UNIFIED IDEOGRAPH
    0xE7ED, 0x923F, //#CJK UNIFIED IDEOGRAPH
    0xE7EE, 0x924B, //#CJK UNIFIED IDEOGRAPH
    0xE7EF, 0x9250, //#CJK UNIFIED IDEOGRAPH
    0xE7F0, 0x929C, //#CJK UNIFIED IDEOGRAPH
    0xE7F1, 0x9296, //#CJK UNIFIED IDEOGRAPH
    0xE7F2, 0x9293, //#CJK UNIFIED IDEOGRAPH
    0xE7F3, 0x929B, //#CJK UNIFIED IDEOGRAPH
    0xE7F4, 0x925A, //#CJK UNIFIED IDEOGRAPH
    0xE7F5, 0x92CF, //#CJK UNIFIED IDEOGRAPH
    0xE7F6, 0x92B9, //#CJK UNIFIED IDEOGRAPH
    0xE7F7, 0x92B7, //#CJK UNIFIED IDEOGRAPH
    0xE7F8, 0x92E9, //#CJK UNIFIED IDEOGRAPH
    0xE7F9, 0x930F, //#CJK UNIFIED IDEOGRAPH
    0xE7FA, 0x92FA, //#CJK UNIFIED IDEOGRAPH
    0xE7FB, 0x9344, //#CJK UNIFIED IDEOGRAPH
    0xE7FC, 0x932E, //#CJK UNIFIED IDEOGRAPH
    0xE840, 0x9319, //#CJK UNIFIED IDEOGRAPH
    0xE841, 0x9322, //#CJK UNIFIED IDEOGRAPH
    0xE842, 0x931A, //#CJK UNIFIED IDEOGRAPH
    0xE843, 0x9323, //#CJK UNIFIED IDEOGRAPH
    0xE844, 0x933A, //#CJK UNIFIED IDEOGRAPH
    0xE845, 0x9335, //#CJK UNIFIED IDEOGRAPH
    0xE846, 0x933B, //#CJK UNIFIED IDEOGRAPH
    0xE847, 0x935C, //#CJK UNIFIED IDEOGRAPH
    0xE848, 0x9360, //#CJK UNIFIED IDEOGRAPH
    0xE849, 0x937C, //#CJK UNIFIED IDEOGRAPH
    0xE84A, 0x936E, //#CJK UNIFIED IDEOGRAPH
    0xE84B, 0x9356, //#CJK UNIFIED IDEOGRAPH
    0xE84C, 0x93B0, //#CJK UNIFIED IDEOGRAPH
    0xE84D, 0x93AC, //#CJK UNIFIED IDEOGRAPH
    0xE84E, 0x93AD, //#CJK UNIFIED IDEOGRAPH
    0xE84F, 0x9394, //#CJK UNIFIED IDEOGRAPH
    0xE850, 0x93B9, //#CJK UNIFIED IDEOGRAPH
    0xE851, 0x93D6, //#CJK UNIFIED IDEOGRAPH
    0xE852, 0x93D7, //#CJK UNIFIED IDEOGRAPH
    0xE853, 0x93E8, //#CJK UNIFIED IDEOGRAPH
    0xE854, 0x93E5, //#CJK UNIFIED IDEOGRAPH
    0xE855, 0x93D8, //#CJK UNIFIED IDEOGRAPH
    0xE856, 0x93C3, //#CJK UNIFIED IDEOGRAPH
    0xE857, 0x93DD, //#CJK UNIFIED IDEOGRAPH
    0xE858, 0x93D0, //#CJK UNIFIED IDEOGRAPH
    0xE859, 0x93C8, //#CJK UNIFIED IDEOGRAPH
    0xE85A, 0x93E4, //#CJK UNIFIED IDEOGRAPH
    0xE85B, 0x941A, //#CJK UNIFIED IDEOGRAPH
    0xE85C, 0x9414, //#CJK UNIFIED IDEOGRAPH
    0xE85D, 0x9413, //#CJK UNIFIED IDEOGRAPH
    0xE85E, 0x9403, //#CJK UNIFIED IDEOGRAPH
    0xE85F, 0x9407, //#CJK UNIFIED IDEOGRAPH
    0xE860, 0x9410, //#CJK UNIFIED IDEOGRAPH
    0xE861, 0x9436, //#CJK UNIFIED IDEOGRAPH
    0xE862, 0x942B, //#CJK UNIFIED IDEOGRAPH
    0xE863, 0x9435, //#CJK UNIFIED IDEOGRAPH
    0xE864, 0x9421, //#CJK UNIFIED IDEOGRAPH
    0xE865, 0x943A, //#CJK UNIFIED IDEOGRAPH
    0xE866, 0x9441, //#CJK UNIFIED IDEOGRAPH
    0xE867, 0x9452, //#CJK UNIFIED IDEOGRAPH
    0xE868, 0x9444, //#CJK UNIFIED IDEOGRAPH
    0xE869, 0x945B, //#CJK UNIFIED IDEOGRAPH
    0xE86A, 0x9460, //#CJK UNIFIED IDEOGRAPH
    0xE86B, 0x9462, //#CJK UNIFIED IDEOGRAPH
    0xE86C, 0x945E, //#CJK UNIFIED IDEOGRAPH
    0xE86D, 0x946A, //#CJK UNIFIED IDEOGRAPH
    0xE86E, 0x9229, //#CJK UNIFIED IDEOGRAPH
    0xE86F, 0x9470, //#CJK UNIFIED IDEOGRAPH
    0xE870, 0x9475, //#CJK UNIFIED IDEOGRAPH
    0xE871, 0x9477, //#CJK UNIFIED IDEOGRAPH
    0xE872, 0x947D, //#CJK UNIFIED IDEOGRAPH
    0xE873, 0x945A, //#CJK UNIFIED IDEOGRAPH
    0xE874, 0x947C, //#CJK UNIFIED IDEOGRAPH
    0xE875, 0x947E, //#CJK UNIFIED IDEOGRAPH
    0xE876, 0x9481, //#CJK UNIFIED IDEOGRAPH
    0xE877, 0x947F, //#CJK UNIFIED IDEOGRAPH
    0xE878, 0x9582, //#CJK UNIFIED IDEOGRAPH
    0xE879, 0x9587, //#CJK UNIFIED IDEOGRAPH
    0xE87A, 0x958A, //#CJK UNIFIED IDEOGRAPH
    0xE87B, 0x9594, //#CJK UNIFIED IDEOGRAPH
    0xE87C, 0x9596, //#CJK UNIFIED IDEOGRAPH
    0xE87D, 0x9598, //#CJK UNIFIED IDEOGRAPH
    0xE87E, 0x9599, //#CJK UNIFIED IDEOGRAPH
    0xE880, 0x95A0, //#CJK UNIFIED IDEOGRAPH
    0xE881, 0x95A8, //#CJK UNIFIED IDEOGRAPH
    0xE882, 0x95A7, //#CJK UNIFIED IDEOGRAPH
    0xE883, 0x95AD, //#CJK UNIFIED IDEOGRAPH
    0xE884, 0x95BC, //#CJK UNIFIED IDEOGRAPH
    0xE885, 0x95BB, //#CJK UNIFIED IDEOGRAPH
    0xE886, 0x95B9, //#CJK UNIFIED IDEOGRAPH
    0xE887, 0x95BE, //#CJK UNIFIED IDEOGRAPH
    0xE888, 0x95CA, //#CJK UNIFIED IDEOGRAPH
    0xE889, 0x6FF6, //#CJK UNIFIED IDEOGRAPH
    0xE88A, 0x95C3, //#CJK UNIFIED IDEOGRAPH
    0xE88B, 0x95CD, //#CJK UNIFIED IDEOGRAPH
    0xE88C, 0x95CC, //#CJK UNIFIED IDEOGRAPH
    0xE88D, 0x95D5, //#CJK UNIFIED IDEOGRAPH
    0xE88E, 0x95D4, //#CJK UNIFIED IDEOGRAPH
    0xE88F, 0x95D6, //#CJK UNIFIED IDEOGRAPH
    0xE890, 0x95DC, //#CJK UNIFIED IDEOGRAPH
    0xE891, 0x95E1, //#CJK UNIFIED IDEOGRAPH
    0xE892, 0x95E5, //#CJK UNIFIED IDEOGRAPH
    0xE893, 0x95E2, //#CJK UNIFIED IDEOGRAPH
    0xE894, 0x9621, //#CJK UNIFIED IDEOGRAPH
    0xE895, 0x9628, //#CJK UNIFIED IDEOGRAPH
    0xE896, 0x962E, //#CJK UNIFIED IDEOGRAPH
    0xE897, 0x962F, //#CJK UNIFIED IDEOGRAPH
    0xE898, 0x9642, //#CJK UNIFIED IDEOGRAPH
    0xE899, 0x964C, //#CJK UNIFIED IDEOGRAPH
    0xE89A, 0x964F, //#CJK UNIFIED IDEOGRAPH
    0xE89B, 0x964B, //#CJK UNIFIED IDEOGRAPH
    0xE89C, 0x9677, //#CJK UNIFIED IDEOGRAPH
    0xE89D, 0x965C, //#CJK UNIFIED IDEOGRAPH
    0xE89E, 0x965E, //#CJK UNIFIED IDEOGRAPH
    0xE89F, 0x965D, //#CJK UNIFIED IDEOGRAPH
    0xE8A0, 0x965F, //#CJK UNIFIED IDEOGRAPH
    0xE8A1, 0x9666, //#CJK UNIFIED IDEOGRAPH
    0xE8A2, 0x9672, //#CJK UNIFIED IDEOGRAPH
    0xE8A3, 0x966C, //#CJK UNIFIED IDEOGRAPH
    0xE8A4, 0x968D, //#CJK UNIFIED IDEOGRAPH
    0xE8A5, 0x9698, //#CJK UNIFIED IDEOGRAPH
    0xE8A6, 0x9695, //#CJK UNIFIED IDEOGRAPH
    0xE8A7, 0x9697, //#CJK UNIFIED IDEOGRAPH
    0xE8A8, 0x96AA, //#CJK UNIFIED IDEOGRAPH
    0xE8A9, 0x96A7, //#CJK UNIFIED IDEOGRAPH
    0xE8AA, 0x96B1, //#CJK UNIFIED IDEOGRAPH
    0xE8AB, 0x96B2, //#CJK UNIFIED IDEOGRAPH
    0xE8AC, 0x96B0, //#CJK UNIFIED IDEOGRAPH
    0xE8AD, 0x96B4, //#CJK UNIFIED IDEOGRAPH
    0xE8AE, 0x96B6, //#CJK UNIFIED IDEOGRAPH
    0xE8AF, 0x96B8, //#CJK UNIFIED IDEOGRAPH
    0xE8B0, 0x96B9, //#CJK UNIFIED IDEOGRAPH
    0xE8B1, 0x96CE, //#CJK UNIFIED IDEOGRAPH
    0xE8B2, 0x96CB, //#CJK UNIFIED IDEOGRAPH
    0xE8B3, 0x96C9, //#CJK UNIFIED IDEOGRAPH
    0xE8B4, 0x96CD, //#CJK UNIFIED IDEOGRAPH
    0xE8B5, 0x894D, //#CJK UNIFIED IDEOGRAPH
    0xE8B6, 0x96DC, //#CJK UNIFIED IDEOGRAPH
    0xE8B7, 0x970D, //#CJK UNIFIED IDEOGRAPH
    0xE8B8, 0x96D5, //#CJK UNIFIED IDEOGRAPH
    0xE8B9, 0x96F9, //#CJK UNIFIED IDEOGRAPH
    0xE8BA, 0x9704, //#CJK UNIFIED IDEOGRAPH
    0xE8BB, 0x9706, //#CJK UNIFIED IDEOGRAPH
    0xE8BC, 0x9708, //#CJK UNIFIED IDEOGRAPH
    0xE8BD, 0x9713, //#CJK UNIFIED IDEOGRAPH
    0xE8BE, 0x970E, //#CJK UNIFIED IDEOGRAPH
    0xE8BF, 0x9711, //#CJK UNIFIED IDEOGRAPH
    0xE8C0, 0x970F, //#CJK UNIFIED IDEOGRAPH
    0xE8C1, 0x9716, //#CJK UNIFIED IDEOGRAPH
    0xE8C2, 0x9719, //#CJK UNIFIED IDEOGRAPH
    0xE8C3, 0x9724, //#CJK UNIFIED IDEOGRAPH
    0xE8C4, 0x972A, //#CJK UNIFIED IDEOGRAPH
    0xE8C5, 0x9730, //#CJK UNIFIED IDEOGRAPH
    0xE8C6, 0x9739, //#CJK UNIFIED IDEOGRAPH
    0xE8C7, 0x973D, //#CJK UNIFIED IDEOGRAPH
    0xE8C8, 0x973E, //#CJK UNIFIED IDEOGRAPH
    0xE8C9, 0x9744, //#CJK UNIFIED IDEOGRAPH
    0xE8CA, 0x9746, //#CJK UNIFIED IDEOGRAPH
    0xE8CB, 0x9748, //#CJK UNIFIED IDEOGRAPH
    0xE8CC, 0x9742, //#CJK UNIFIED IDEOGRAPH
    0xE8CD, 0x9749, //#CJK UNIFIED IDEOGRAPH
    0xE8CE, 0x975C, //#CJK UNIFIED IDEOGRAPH
    0xE8CF, 0x9760, //#CJK UNIFIED IDEOGRAPH
    0xE8D0, 0x9764, //#CJK UNIFIED IDEOGRAPH
    0xE8D1, 0x9766, //#CJK UNIFIED IDEOGRAPH
    0xE8D2, 0x9768, //#CJK UNIFIED IDEOGRAPH
    0xE8D3, 0x52D2, //#CJK UNIFIED IDEOGRAPH
    0xE8D4, 0x976B, //#CJK UNIFIED IDEOGRAPH
    0xE8D5, 0x9771, //#CJK UNIFIED IDEOGRAPH
    0xE8D6, 0x9779, //#CJK UNIFIED IDEOGRAPH
    0xE8D7, 0x9785, //#CJK UNIFIED IDEOGRAPH
    0xE8D8, 0x977C, //#CJK UNIFIED IDEOGRAPH
    0xE8D9, 0x9781, //#CJK UNIFIED IDEOGRAPH
    0xE8DA, 0x977A, //#CJK UNIFIED IDEOGRAPH
    0xE8DB, 0x9786, //#CJK UNIFIED IDEOGRAPH
    0xE8DC, 0x978B, //#CJK UNIFIED IDEOGRAPH
    0xE8DD, 0x978F, //#CJK UNIFIED IDEOGRAPH
    0xE8DE, 0x9790, //#CJK UNIFIED IDEOGRAPH
    0xE8DF, 0x979C, //#CJK UNIFIED IDEOGRAPH
    0xE8E0, 0x97A8, //#CJK UNIFIED IDEOGRAPH
    0xE8E1, 0x97A6, //#CJK UNIFIED IDEOGRAPH
    0xE8E2, 0x97A3, //#CJK UNIFIED IDEOGRAPH
    0xE8E3, 0x97B3, //#CJK UNIFIED IDEOGRAPH
    0xE8E4, 0x97B4, //#CJK UNIFIED IDEOGRAPH
    0xE8E5, 0x97C3, //#CJK UNIFIED IDEOGRAPH
    0xE8E6, 0x97C6, //#CJK UNIFIED IDEOGRAPH
    0xE8E7, 0x97C8, //#CJK UNIFIED IDEOGRAPH
    0xE8E8, 0x97CB, //#CJK UNIFIED IDEOGRAPH
    0xE8E9, 0x97DC, //#CJK UNIFIED IDEOGRAPH
    0xE8EA, 0x97ED, //#CJK UNIFIED IDEOGRAPH
    0xE8EB, 0x9F4F, //#CJK UNIFIED IDEOGRAPH
    0xE8EC, 0x97F2, //#CJK UNIFIED IDEOGRAPH
    0xE8ED, 0x7ADF, //#CJK UNIFIED IDEOGRAPH
    0xE8EE, 0x97F6, //#CJK UNIFIED IDEOGRAPH
    0xE8EF, 0x97F5, //#CJK UNIFIED IDEOGRAPH
    0xE8F0, 0x980F, //#CJK UNIFIED IDEOGRAPH
    0xE8F1, 0x980C, //#CJK UNIFIED IDEOGRAPH
    0xE8F2, 0x9838, //#CJK UNIFIED IDEOGRAPH
    0xE8F3, 0x9824, //#CJK UNIFIED IDEOGRAPH
    0xE8F4, 0x9821, //#CJK UNIFIED IDEOGRAPH
    0xE8F5, 0x9837, //#CJK UNIFIED IDEOGRAPH
    0xE8F6, 0x983D, //#CJK UNIFIED IDEOGRAPH
    0xE8F7, 0x9846, //#CJK UNIFIED IDEOGRAPH
    0xE8F8, 0x984F, //#CJK UNIFIED IDEOGRAPH
    0xE8F9, 0x984B, //#CJK UNIFIED IDEOGRAPH
    0xE8FA, 0x986B, //#CJK UNIFIED IDEOGRAPH
    0xE8FB, 0x986F, //#CJK UNIFIED IDEOGRAPH
    0xE8FC, 0x9870, //#CJK UNIFIED IDEOGRAPH
    0xE940, 0x9871, //#CJK UNIFIED IDEOGRAPH
    0xE941, 0x9874, //#CJK UNIFIED IDEOGRAPH
    0xE942, 0x9873, //#CJK UNIFIED IDEOGRAPH
    0xE943, 0x98AA, //#CJK UNIFIED IDEOGRAPH
    0xE944, 0x98AF, //#CJK UNIFIED IDEOGRAPH
    0xE945, 0x98B1, //#CJK UNIFIED IDEOGRAPH
    0xE946, 0x98B6, //#CJK UNIFIED IDEOGRAPH
    0xE947, 0x98C4, //#CJK UNIFIED IDEOGRAPH
    0xE948, 0x98C3, //#CJK UNIFIED IDEOGRAPH
    0xE949, 0x98C6, //#CJK UNIFIED IDEOGRAPH
    0xE94A, 0x98E9, //#CJK UNIFIED IDEOGRAPH
    0xE94B, 0x98EB, //#CJK UNIFIED IDEOGRAPH
    0xE94C, 0x9903, //#CJK UNIFIED IDEOGRAPH
    0xE94D, 0x9909, //#CJK UNIFIED IDEOGRAPH
    0xE94E, 0x9912, //#CJK UNIFIED IDEOGRAPH
    0xE94F, 0x9914, //#CJK UNIFIED IDEOGRAPH
    0xE950, 0x9918, //#CJK UNIFIED IDEOGRAPH
    0xE951, 0x9921, //#CJK UNIFIED IDEOGRAPH
    0xE952, 0x991D, //#CJK UNIFIED IDEOGRAPH
    0xE953, 0x991E, //#CJK UNIFIED IDEOGRAPH
    0xE954, 0x9924, //#CJK UNIFIED IDEOGRAPH
    0xE955, 0x9920, //#CJK UNIFIED IDEOGRAPH
    0xE956, 0x992C, //#CJK UNIFIED IDEOGRAPH
    0xE957, 0x992E, //#CJK UNIFIED IDEOGRAPH
    0xE958, 0x993D, //#CJK UNIFIED IDEOGRAPH
    0xE959, 0x993E, //#CJK UNIFIED IDEOGRAPH
    0xE95A, 0x9942, //#CJK UNIFIED IDEOGRAPH
    0xE95B, 0x9949, //#CJK UNIFIED IDEOGRAPH
    0xE95C, 0x9945, //#CJK UNIFIED IDEOGRAPH
    0xE95D, 0x9950, //#CJK UNIFIED IDEOGRAPH
    0xE95E, 0x994B, //#CJK UNIFIED IDEOGRAPH
    0xE95F, 0x9951, //#CJK UNIFIED IDEOGRAPH
    0xE960, 0x9952, //#CJK UNIFIED IDEOGRAPH
    0xE961, 0x994C, //#CJK UNIFIED IDEOGRAPH
    0xE962, 0x9955, //#CJK UNIFIED IDEOGRAPH
    0xE963, 0x9997, //#CJK UNIFIED IDEOGRAPH
    0xE964, 0x9998, //#CJK UNIFIED IDEOGRAPH
    0xE965, 0x99A5, //#CJK UNIFIED IDEOGRAPH
    0xE966, 0x99AD, //#CJK UNIFIED IDEOGRAPH
    0xE967, 0x99AE, //#CJK UNIFIED IDEOGRAPH
    0xE968, 0x99BC, //#CJK UNIFIED IDEOGRAPH
    0xE969, 0x99DF, //#CJK UNIFIED IDEOGRAPH
    0xE96A, 0x99DB, //#CJK UNIFIED IDEOGRAPH
    0xE96B, 0x99DD, //#CJK UNIFIED IDEOGRAPH
    0xE96C, 0x99D8, //#CJK UNIFIED IDEOGRAPH
    0xE96D, 0x99D1, //#CJK UNIFIED IDEOGRAPH
    0xE96E, 0x99ED, //#CJK UNIFIED IDEOGRAPH
    0xE96F, 0x99EE, //#CJK UNIFIED IDEOGRAPH
    0xE970, 0x99F1, //#CJK UNIFIED IDEOGRAPH
    0xE971, 0x99F2, //#CJK UNIFIED IDEOGRAPH
    0xE972, 0x99FB, //#CJK UNIFIED IDEOGRAPH
    0xE973, 0x99F8, //#CJK UNIFIED IDEOGRAPH
    0xE974, 0x9A01, //#CJK UNIFIED IDEOGRAPH
    0xE975, 0x9A0F, //#CJK UNIFIED IDEOGRAPH
    0xE976, 0x9A05, //#CJK UNIFIED IDEOGRAPH
    0xE977, 0x99E2, //#CJK UNIFIED IDEOGRAPH
    0xE978, 0x9A19, //#CJK UNIFIED IDEOGRAPH
    0xE979, 0x9A2B, //#CJK UNIFIED IDEOGRAPH
    0xE97A, 0x9A37, //#CJK UNIFIED IDEOGRAPH
    0xE97B, 0x9A45, //#CJK UNIFIED IDEOGRAPH
    0xE97C, 0x9A42, //#CJK UNIFIED IDEOGRAPH
    0xE97D, 0x9A40, //#CJK UNIFIED IDEOGRAPH
    0xE97E, 0x9A43, //#CJK UNIFIED IDEOGRAPH
    0xE980, 0x9A3E, //#CJK UNIFIED IDEOGRAPH
    0xE981, 0x9A55, //#CJK UNIFIED IDEOGRAPH
    0xE982, 0x9A4D, //#CJK UNIFIED IDEOGRAPH
    0xE983, 0x9A5B, //#CJK UNIFIED IDEOGRAPH
    0xE984, 0x9A57, //#CJK UNIFIED IDEOGRAPH
    0xE985, 0x9A5F, //#CJK UNIFIED IDEOGRAPH
    0xE986, 0x9A62, //#CJK UNIFIED IDEOGRAPH
    0xE987, 0x9A65, //#CJK UNIFIED IDEOGRAPH
    0xE988, 0x9A64, //#CJK UNIFIED IDEOGRAPH
    0xE989, 0x9A69, //#CJK UNIFIED IDEOGRAPH
    0xE98A, 0x9A6B, //#CJK UNIFIED IDEOGRAPH
    0xE98B, 0x9A6A, //#CJK UNIFIED IDEOGRAPH
    0xE98C, 0x9AAD, //#CJK UNIFIED IDEOGRAPH
    0xE98D, 0x9AB0, //#CJK UNIFIED IDEOGRAPH
    0xE98E, 0x9ABC, //#CJK UNIFIED IDEOGRAPH
    0xE98F, 0x9AC0, //#CJK UNIFIED IDEOGRAPH
    0xE990, 0x9ACF, //#CJK UNIFIED IDEOGRAPH
    0xE991, 0x9AD1, //#CJK UNIFIED IDEOGRAPH
    0xE992, 0x9AD3, //#CJK UNIFIED IDEOGRAPH
    0xE993, 0x9AD4, //#CJK UNIFIED IDEOGRAPH
    0xE994, 0x9ADE, //#CJK UNIFIED IDEOGRAPH
    0xE995, 0x9ADF, //#CJK UNIFIED IDEOGRAPH
    0xE996, 0x9AE2, //#CJK UNIFIED IDEOGRAPH
    0xE997, 0x9AE3, //#CJK UNIFIED IDEOGRAPH
    0xE998, 0x9AE6, //#CJK UNIFIED IDEOGRAPH
    0xE999, 0x9AEF, //#CJK UNIFIED IDEOGRAPH
    0xE99A, 0x9AEB, //#CJK UNIFIED IDEOGRAPH
    0xE99B, 0x9AEE, //#CJK UNIFIED IDEOGRAPH
    0xE99C, 0x9AF4, //#CJK UNIFIED IDEOGRAPH
    0xE99D, 0x9AF1, //#CJK UNIFIED IDEOGRAPH
    0xE99E, 0x9AF7, //#CJK UNIFIED IDEOGRAPH
    0xE99F, 0x9AFB, //#CJK UNIFIED IDEOGRAPH
    0xE9A0, 0x9B06, //#CJK UNIFIED IDEOGRAPH
    0xE9A1, 0x9B18, //#CJK UNIFIED IDEOGRAPH
    0xE9A2, 0x9B1A, //#CJK UNIFIED IDEOGRAPH
    0xE9A3, 0x9B1F, //#CJK UNIFIED IDEOGRAPH
    0xE9A4, 0x9B22, //#CJK UNIFIED IDEOGRAPH
    0xE9A5, 0x9B23, //#CJK UNIFIED IDEOGRAPH
    0xE9A6, 0x9B25, //#CJK UNIFIED IDEOGRAPH
    0xE9A7, 0x9B27, //#CJK UNIFIED IDEOGRAPH
    0xE9A8, 0x9B28, //#CJK UNIFIED IDEOGRAPH
    0xE9A9, 0x9B29, //#CJK UNIFIED IDEOGRAPH
    0xE9AA, 0x9B2A, //#CJK UNIFIED IDEOGRAPH
    0xE9AB, 0x9B2E, //#CJK UNIFIED IDEOGRAPH
    0xE9AC, 0x9B2F, //#CJK UNIFIED IDEOGRAPH
    0xE9AD, 0x9B32, //#CJK UNIFIED IDEOGRAPH
    0xE9AE, 0x9B44, //#CJK UNIFIED IDEOGRAPH
    0xE9AF, 0x9B43, //#CJK UNIFIED IDEOGRAPH
    0xE9B0, 0x9B4F, //#CJK UNIFIED IDEOGRAPH
    0xE9B1, 0x9B4D, //#CJK UNIFIED IDEOGRAPH
    0xE9B2, 0x9B4E, //#CJK UNIFIED IDEOGRAPH
    0xE9B3, 0x9B51, //#CJK UNIFIED IDEOGRAPH
    0xE9B4, 0x9B58, //#CJK UNIFIED IDEOGRAPH
    0xE9B5, 0x9B74, //#CJK UNIFIED IDEOGRAPH
    0xE9B6, 0x9B93, //#CJK UNIFIED IDEOGRAPH
    0xE9B7, 0x9B83, //#CJK UNIFIED IDEOGRAPH
    0xE9B8, 0x9B91, //#CJK UNIFIED IDEOGRAPH
    0xE9B9, 0x9B96, //#CJK UNIFIED IDEOGRAPH
    0xE9BA, 0x9B97, //#CJK UNIFIED IDEOGRAPH
    0xE9BB, 0x9B9F, //#CJK UNIFIED IDEOGRAPH
    0xE9BC, 0x9BA0, //#CJK UNIFIED IDEOGRAPH
    0xE9BD, 0x9BA8, //#CJK UNIFIED IDEOGRAPH
    0xE9BE, 0x9BB4, //#CJK UNIFIED IDEOGRAPH
    0xE9BF, 0x9BC0, //#CJK UNIFIED IDEOGRAPH
    0xE9C0, 0x9BCA, //#CJK UNIFIED IDEOGRAPH
    0xE9C1, 0x9BB9, //#CJK UNIFIED IDEOGRAPH
    0xE9C2, 0x9BC6, //#CJK UNIFIED IDEOGRAPH
    0xE9C3, 0x9BCF, //#CJK UNIFIED IDEOGRAPH
    0xE9C4, 0x9BD1, //#CJK UNIFIED IDEOGRAPH
    0xE9C5, 0x9BD2, //#CJK UNIFIED IDEOGRAPH
    0xE9C6, 0x9BE3, //#CJK UNIFIED IDEOGRAPH
    0xE9C7, 0x9BE2, //#CJK UNIFIED IDEOGRAPH
    0xE9C8, 0x9BE4, //#CJK UNIFIED IDEOGRAPH
    0xE9C9, 0x9BD4, //#CJK UNIFIED IDEOGRAPH
    0xE9CA, 0x9BE1, //#CJK UNIFIED IDEOGRAPH
    0xE9CB, 0x9C3A, //#CJK UNIFIED IDEOGRAPH
    0xE9CC, 0x9BF2, //#CJK UNIFIED IDEOGRAPH
    0xE9CD, 0x9BF1, //#CJK UNIFIED IDEOGRAPH
    0xE9CE, 0x9BF0, //#CJK UNIFIED IDEOGRAPH
    0xE9CF, 0x9C15, //#CJK UNIFIED IDEOGRAPH
    0xE9D0, 0x9C14, //#CJK UNIFIED IDEOGRAPH
    0xE9D1, 0x9C09, //#CJK UNIFIED IDEOGRAPH
    0xE9D2, 0x9C13, //#CJK UNIFIED IDEOGRAPH
    0xE9D3, 0x9C0C, //#CJK UNIFIED IDEOGRAPH
    0xE9D4, 0x9C06, //#CJK UNIFIED IDEOGRAPH
    0xE9D5, 0x9C08, //#CJK UNIFIED IDEOGRAPH
    0xE9D6, 0x9C12, //#CJK UNIFIED IDEOGRAPH
    0xE9D7, 0x9C0A, //#CJK UNIFIED IDEOGRAPH
    0xE9D8, 0x9C04, //#CJK UNIFIED IDEOGRAPH
    0xE9D9, 0x9C2E, //#CJK UNIFIED IDEOGRAPH
    0xE9DA, 0x9C1B, //#CJK UNIFIED IDEOGRAPH
    0xE9DB, 0x9C25, //#CJK UNIFIED IDEOGRAPH
    0xE9DC, 0x9C24, //#CJK UNIFIED IDEOGRAPH
    0xE9DD, 0x9C21, //#CJK UNIFIED IDEOGRAPH
    0xE9DE, 0x9C30, //#CJK UNIFIED IDEOGRAPH
    0xE9DF, 0x9C47, //#CJK UNIFIED IDEOGRAPH
    0xE9E0, 0x9C32, //#CJK UNIFIED IDEOGRAPH
    0xE9E1, 0x9C46, //#CJK UNIFIED IDEOGRAPH
    0xE9E2, 0x9C3E, //#CJK UNIFIED IDEOGRAPH
    0xE9E3, 0x9C5A, //#CJK UNIFIED IDEOGRAPH
    0xE9E4, 0x9C60, //#CJK UNIFIED IDEOGRAPH
    0xE9E5, 0x9C67, //#CJK UNIFIED IDEOGRAPH
    0xE9E6, 0x9C76, //#CJK UNIFIED IDEOGRAPH
    0xE9E7, 0x9C78, //#CJK UNIFIED IDEOGRAPH
    0xE9E8, 0x9CE7, //#CJK UNIFIED IDEOGRAPH
    0xE9E9, 0x9CEC, //#CJK UNIFIED IDEOGRAPH
    0xE9EA, 0x9CF0, //#CJK UNIFIED IDEOGRAPH
    0xE9EB, 0x9D09, //#CJK UNIFIED IDEOGRAPH
    0xE9EC, 0x9D08, //#CJK UNIFIED IDEOGRAPH
    0xE9ED, 0x9CEB, //#CJK UNIFIED IDEOGRAPH
    0xE9EE, 0x9D03, //#CJK UNIFIED IDEOGRAPH
    0xE9EF, 0x9D06, //#CJK UNIFIED IDEOGRAPH
    0xE9F0, 0x9D2A, //#CJK UNIFIED IDEOGRAPH
    0xE9F1, 0x9D26, //#CJK UNIFIED IDEOGRAPH
    0xE9F2, 0x9DAF, //#CJK UNIFIED IDEOGRAPH
    0xE9F3, 0x9D23, //#CJK UNIFIED IDEOGRAPH
    0xE9F4, 0x9D1F, //#CJK UNIFIED IDEOGRAPH
    0xE9F5, 0x9D44, //#CJK UNIFIED IDEOGRAPH
    0xE9F6, 0x9D15, //#CJK UNIFIED IDEOGRAPH
    0xE9F7, 0x9D12, //#CJK UNIFIED IDEOGRAPH
    0xE9F8, 0x9D41, //#CJK UNIFIED IDEOGRAPH
    0xE9F9, 0x9D3F, //#CJK UNIFIED IDEOGRAPH
    0xE9FA, 0x9D3E, //#CJK UNIFIED IDEOGRAPH
    0xE9FB, 0x9D46, //#CJK UNIFIED IDEOGRAPH
    0xE9FC, 0x9D48, //#CJK UNIFIED IDEOGRAPH
    0xEA40, 0x9D5D, //#CJK UNIFIED IDEOGRAPH
    0xEA41, 0x9D5E, //#CJK UNIFIED IDEOGRAPH
    0xEA42, 0x9D64, //#CJK UNIFIED IDEOGRAPH
    0xEA43, 0x9D51, //#CJK UNIFIED IDEOGRAPH
    0xEA44, 0x9D50, //#CJK UNIFIED IDEOGRAPH
    0xEA45, 0x9D59, //#CJK UNIFIED IDEOGRAPH
    0xEA46, 0x9D72, //#CJK UNIFIED IDEOGRAPH
    0xEA47, 0x9D89, //#CJK UNIFIED IDEOGRAPH
    0xEA48, 0x9D87, //#CJK UNIFIED IDEOGRAPH
    0xEA49, 0x9DAB, //#CJK UNIFIED IDEOGRAPH
    0xEA4A, 0x9D6F, //#CJK UNIFIED IDEOGRAPH
    0xEA4B, 0x9D7A, //#CJK UNIFIED IDEOGRAPH
    0xEA4C, 0x9D9A, //#CJK UNIFIED IDEOGRAPH
    0xEA4D, 0x9DA4, //#CJK UNIFIED IDEOGRAPH
    0xEA4E, 0x9DA9, //#CJK UNIFIED IDEOGRAPH
    0xEA4F, 0x9DB2, //#CJK UNIFIED IDEOGRAPH
    0xEA50, 0x9DC4, //#CJK UNIFIED IDEOGRAPH
    0xEA51, 0x9DC1, //#CJK UNIFIED IDEOGRAPH
    0xEA52, 0x9DBB, //#CJK UNIFIED IDEOGRAPH
    0xEA53, 0x9DB8, //#CJK UNIFIED IDEOGRAPH
    0xEA54, 0x9DBA, //#CJK UNIFIED IDEOGRAPH
    0xEA55, 0x9DC6, //#CJK UNIFIED IDEOGRAPH
    0xEA56, 0x9DCF, //#CJK UNIFIED IDEOGRAPH
    0xEA57, 0x9DC2, //#CJK UNIFIED IDEOGRAPH
    0xEA58, 0x9DD9, //#CJK UNIFIED IDEOGRAPH
    0xEA59, 0x9DD3, //#CJK UNIFIED IDEOGRAPH
    0xEA5A, 0x9DF8, //#CJK UNIFIED IDEOGRAPH
    0xEA5B, 0x9DE6, //#CJK UNIFIED IDEOGRAPH
    0xEA5C, 0x9DED, //#CJK UNIFIED IDEOGRAPH
    0xEA5D, 0x9DEF, //#CJK UNIFIED IDEOGRAPH
    0xEA5E, 0x9DFD, //#CJK UNIFIED IDEOGRAPH
    0xEA5F, 0x9E1A, //#CJK UNIFIED IDEOGRAPH
    0xEA60, 0x9E1B, //#CJK UNIFIED IDEOGRAPH
    0xEA61, 0x9E1E, //#CJK UNIFIED IDEOGRAPH
    0xEA62, 0x9E75, //#CJK UNIFIED IDEOGRAPH
    0xEA63, 0x9E79, //#CJK UNIFIED IDEOGRAPH
    0xEA64, 0x9E7D, //#CJK UNIFIED IDEOGRAPH
    0xEA65, 0x9E81, //#CJK UNIFIED IDEOGRAPH
    0xEA66, 0x9E88, //#CJK UNIFIED IDEOGRAPH
    0xEA67, 0x9E8B, //#CJK UNIFIED IDEOGRAPH
    0xEA68, 0x9E8C, //#CJK UNIFIED IDEOGRAPH
    0xEA69, 0x9E92, //#CJK UNIFIED IDEOGRAPH
    0xEA6A, 0x9E95, //#CJK UNIFIED IDEOGRAPH
    0xEA6B, 0x9E91, //#CJK UNIFIED IDEOGRAPH
    0xEA6C, 0x9E9D, //#CJK UNIFIED IDEOGRAPH
    0xEA6D, 0x9EA5, //#CJK UNIFIED IDEOGRAPH
    0xEA6E, 0x9EA9, //#CJK UNIFIED IDEOGRAPH
    0xEA6F, 0x9EB8, //#CJK UNIFIED IDEOGRAPH
    0xEA70, 0x9EAA, //#CJK UNIFIED IDEOGRAPH
    0xEA71, 0x9EAD, //#CJK UNIFIED IDEOGRAPH
    0xEA72, 0x9761, //#CJK UNIFIED IDEOGRAPH
    0xEA73, 0x9ECC, //#CJK UNIFIED IDEOGRAPH
    0xEA74, 0x9ECE, //#CJK UNIFIED IDEOGRAPH
    0xEA75, 0x9ECF, //#CJK UNIFIED IDEOGRAPH
    0xEA76, 0x9ED0, //#CJK UNIFIED IDEOGRAPH
    0xEA77, 0x9ED4, //#CJK UNIFIED IDEOGRAPH
    0xEA78, 0x9EDC, //#CJK UNIFIED IDEOGRAPH
    0xEA79, 0x9EDE, //#CJK UNIFIED IDEOGRAPH
    0xEA7A, 0x9EDD, //#CJK UNIFIED IDEOGRAPH
    0xEA7B, 0x9EE0, //#CJK UNIFIED IDEOGRAPH
    0xEA7C, 0x9EE5, //#CJK UNIFIED IDEOGRAPH
    0xEA7D, 0x9EE8, //#CJK UNIFIED IDEOGRAPH
    0xEA7E, 0x9EEF, //#CJK UNIFIED IDEOGRAPH
    0xEA80, 0x9EF4, //#CJK UNIFIED IDEOGRAPH
    0xEA81, 0x9EF6, //#CJK UNIFIED IDEOGRAPH
    0xEA82, 0x9EF7, //#CJK UNIFIED IDEOGRAPH
    0xEA83, 0x9EF9, //#CJK UNIFIED IDEOGRAPH
    0xEA84, 0x9EFB, //#CJK UNIFIED IDEOGRAPH
    0xEA85, 0x9EFC, //#CJK UNIFIED IDEOGRAPH
    0xEA86, 0x9EFD, //#CJK UNIFIED IDEOGRAPH
    0xEA87, 0x9F07, //#CJK UNIFIED IDEOGRAPH
    0xEA88, 0x9F08, //#CJK UNIFIED IDEOGRAPH
    0xEA89, 0x76B7, //#CJK UNIFIED IDEOGRAPH
    0xEA8A, 0x9F15, //#CJK UNIFIED IDEOGRAPH
    0xEA8B, 0x9F21, //#CJK UNIFIED IDEOGRAPH
    0xEA8C, 0x9F2C, //#CJK UNIFIED IDEOGRAPH
    0xEA8D, 0x9F3E, //#CJK UNIFIED IDEOGRAPH
    0xEA8E, 0x9F4A, //#CJK UNIFIED IDEOGRAPH
    0xEA8F, 0x9F52, //#CJK UNIFIED IDEOGRAPH
    0xEA90, 0x9F54, //#CJK UNIFIED IDEOGRAPH
    0xEA91, 0x9F63, //#CJK UNIFIED IDEOGRAPH
    0xEA92, 0x9F5F, //#CJK UNIFIED IDEOGRAPH
    0xEA93, 0x9F60, //#CJK UNIFIED IDEOGRAPH
    0xEA94, 0x9F61, //#CJK UNIFIED IDEOGRAPH
    0xEA95, 0x9F66, //#CJK UNIFIED IDEOGRAPH
    0xEA96, 0x9F67, //#CJK UNIFIED IDEOGRAPH
    0xEA97, 0x9F6C, //#CJK UNIFIED IDEOGRAPH
    0xEA98, 0x9F6A, //#CJK UNIFIED IDEOGRAPH
    0xEA99, 0x9F77, //#CJK UNIFIED IDEOGRAPH
    0xEA9A, 0x9F72, //#CJK UNIFIED IDEOGRAPH
    0xEA9B, 0x9F76, //#CJK UNIFIED IDEOGRAPH
    0xEA9C, 0x9F95, //#CJK UNIFIED IDEOGRAPH
    0xEA9D, 0x9F9C, //#CJK UNIFIED IDEOGRAPH
    0xEA9E, 0x9FA0, //#CJK UNIFIED IDEOGRAPH
    0xEA9F, 0x582F, //#CJK UNIFIED IDEOGRAPH
    0xEAA0, 0x69C7, //#CJK UNIFIED IDEOGRAPH
    0xEAA1, 0x9059, //#CJK UNIFIED IDEOGRAPH
    0xEAA2, 0x7464, //#CJK UNIFIED IDEOGRAPH
    0xEAA3, 0x51DC, //#CJK UNIFIED IDEOGRAPH
    0xEAA4, 0x7199, //#CJK UNIFIED IDEOGRAPH
    0xED40, 0x7E8A, //#CJK UNIFIED IDEOGRAPH
    0xED41, 0x891C, //#CJK UNIFIED IDEOGRAPH
    0xED42, 0x9348, //#CJK UNIFIED IDEOGRAPH
    0xED43, 0x9288, //#CJK UNIFIED IDEOGRAPH
    0xED44, 0x84DC, //#CJK UNIFIED IDEOGRAPH
    0xED45, 0x4FC9, //#CJK UNIFIED IDEOGRAPH
    0xED46, 0x70BB, //#CJK UNIFIED IDEOGRAPH
    0xED47, 0x6631, //#CJK UNIFIED IDEOGRAPH
    0xED48, 0x68C8, //#CJK UNIFIED IDEOGRAPH
    0xED49, 0x92F9, //#CJK UNIFIED IDEOGRAPH
    0xED4A, 0x66FB, //#CJK UNIFIED IDEOGRAPH
    0xED4B, 0x5F45, //#CJK UNIFIED IDEOGRAPH
    0xED4C, 0x4E28, //#CJK UNIFIED IDEOGRAPH
    0xED4D, 0x4EE1, //#CJK UNIFIED IDEOGRAPH
    0xED4E, 0x4EFC, //#CJK UNIFIED IDEOGRAPH
    0xED4F, 0x4F00, //#CJK UNIFIED IDEOGRAPH
    0xED50, 0x4F03, //#CJK UNIFIED IDEOGRAPH
    0xED51, 0x4F39, //#CJK UNIFIED IDEOGRAPH
    0xED52, 0x4F56, //#CJK UNIFIED IDEOGRAPH
    0xED53, 0x4F92, //#CJK UNIFIED IDEOGRAPH
    0xED54, 0x4F8A, //#CJK UNIFIED IDEOGRAPH
    0xED55, 0x4F9A, //#CJK UNIFIED IDEOGRAPH
    0xED56, 0x4F94, //#CJK UNIFIED IDEOGRAPH
    0xED57, 0x4FCD, //#CJK UNIFIED IDEOGRAPH
    0xED58, 0x5040, //#CJK UNIFIED IDEOGRAPH
    0xED59, 0x5022, //#CJK UNIFIED IDEOGRAPH
    0xED5A, 0x4FFF, //#CJK UNIFIED IDEOGRAPH
    0xED5B, 0x501E, //#CJK UNIFIED IDEOGRAPH
    0xED5C, 0x5046, //#CJK UNIFIED IDEOGRAPH
    0xED5D, 0x5070, //#CJK UNIFIED IDEOGRAPH
    0xED5E, 0x5042, //#CJK UNIFIED IDEOGRAPH
    0xED5F, 0x5094, //#CJK UNIFIED IDEOGRAPH
    0xED60, 0x50F4, //#CJK UNIFIED IDEOGRAPH
    0xED61, 0x50D8, //#CJK UNIFIED IDEOGRAPH
    0xED62, 0x514A, //#CJK UNIFIED IDEOGRAPH
    0xED63, 0x5164, //#CJK UNIFIED IDEOGRAPH
    0xED64, 0x519D, //#CJK UNIFIED IDEOGRAPH
    0xED65, 0x51BE, //#CJK UNIFIED IDEOGRAPH
    0xED66, 0x51EC, //#CJK UNIFIED IDEOGRAPH
    0xED67, 0x5215, //#CJK UNIFIED IDEOGRAPH
    0xED68, 0x529C, //#CJK UNIFIED IDEOGRAPH
    0xED69, 0x52A6, //#CJK UNIFIED IDEOGRAPH
    0xED6A, 0x52C0, //#CJK UNIFIED IDEOGRAPH
    0xED6B, 0x52DB, //#CJK UNIFIED IDEOGRAPH
    0xED6C, 0x5300, //#CJK UNIFIED IDEOGRAPH
    0xED6D, 0x5307, //#CJK UNIFIED IDEOGRAPH
    0xED6E, 0x5324, //#CJK UNIFIED IDEOGRAPH
    0xED6F, 0x5372, //#CJK UNIFIED IDEOGRAPH
    0xED70, 0x5393, //#CJK UNIFIED IDEOGRAPH
    0xED71, 0x53B2, //#CJK UNIFIED IDEOGRAPH
    0xED72, 0x53DD, //#CJK UNIFIED IDEOGRAPH
    0xED73, 0xFA0E, //#CJK COMPATIBILITY IDEOGRAPH
    0xED74, 0x549C, //#CJK UNIFIED IDEOGRAPH
    0xED75, 0x548A, //#CJK UNIFIED IDEOGRAPH
    0xED76, 0x54A9, //#CJK UNIFIED IDEOGRAPH
    0xED77, 0x54FF, //#CJK UNIFIED IDEOGRAPH
    0xED78, 0x5586, //#CJK UNIFIED IDEOGRAPH
    0xED79, 0x5759, //#CJK UNIFIED IDEOGRAPH
    0xED7A, 0x5765, //#CJK UNIFIED IDEOGRAPH
    0xED7B, 0x57AC, //#CJK UNIFIED IDEOGRAPH
    0xED7C, 0x57C8, //#CJK UNIFIED IDEOGRAPH
    0xED7D, 0x57C7, //#CJK UNIFIED IDEOGRAPH
    0xED7E, 0xFA0F, //#CJK COMPATIBILITY IDEOGRAPH
    0xED80, 0xFA10, //#CJK COMPATIBILITY IDEOGRAPH
    0xED81, 0x589E, //#CJK UNIFIED IDEOGRAPH
    0xED82, 0x58B2, //#CJK UNIFIED IDEOGRAPH
    0xED83, 0x590B, //#CJK UNIFIED IDEOGRAPH
    0xED84, 0x5953, //#CJK UNIFIED IDEOGRAPH
    0xED85, 0x595B, //#CJK UNIFIED IDEOGRAPH
    0xED86, 0x595D, //#CJK UNIFIED IDEOGRAPH
    0xED87, 0x5963, //#CJK UNIFIED IDEOGRAPH
    0xED88, 0x59A4, //#CJK UNIFIED IDEOGRAPH
    0xED89, 0x59BA, //#CJK UNIFIED IDEOGRAPH
    0xED8A, 0x5B56, //#CJK UNIFIED IDEOGRAPH
    0xED8B, 0x5BC0, //#CJK UNIFIED IDEOGRAPH
    0xED8C, 0x752F, //#CJK UNIFIED IDEOGRAPH
    0xED8D, 0x5BD8, //#CJK UNIFIED IDEOGRAPH
    0xED8E, 0x5BEC, //#CJK UNIFIED IDEOGRAPH
    0xED8F, 0x5C1E, //#CJK UNIFIED IDEOGRAPH
    0xED90, 0x5CA6, //#CJK UNIFIED IDEOGRAPH
    0xED91, 0x5CBA, //#CJK UNIFIED IDEOGRAPH
    0xED92, 0x5CF5, //#CJK UNIFIED IDEOGRAPH
    0xED93, 0x5D27, //#CJK UNIFIED IDEOGRAPH
    0xED94, 0x5D53, //#CJK UNIFIED IDEOGRAPH
    0xED95, 0xFA11, //#CJK COMPATIBILITY IDEOGRAPH
    0xED96, 0x5D42, //#CJK UNIFIED IDEOGRAPH
    0xED97, 0x5D6D, //#CJK UNIFIED IDEOGRAPH
    0xED98, 0x5DB8, //#CJK UNIFIED IDEOGRAPH
    0xED99, 0x5DB9, //#CJK UNIFIED IDEOGRAPH
    0xED9A, 0x5DD0, //#CJK UNIFIED IDEOGRAPH
    0xED9B, 0x5F21, //#CJK UNIFIED IDEOGRAPH
    0xED9C, 0x5F34, //#CJK UNIFIED IDEOGRAPH
    0xED9D, 0x5F67, //#CJK UNIFIED IDEOGRAPH
    0xED9E, 0x5FB7, //#CJK UNIFIED IDEOGRAPH
    0xED9F, 0x5FDE, //#CJK UNIFIED IDEOGRAPH
    0xEDA0, 0x605D, //#CJK UNIFIED IDEOGRAPH
    0xEDA1, 0x6085, //#CJK UNIFIED IDEOGRAPH
    0xEDA2, 0x608A, //#CJK UNIFIED IDEOGRAPH
    0xEDA3, 0x60DE, //#CJK UNIFIED IDEOGRAPH
    0xEDA4, 0x60D5, //#CJK UNIFIED IDEOGRAPH
    0xEDA5, 0x6120, //#CJK UNIFIED IDEOGRAPH
    0xEDA6, 0x60F2, //#CJK UNIFIED IDEOGRAPH
    0xEDA7, 0x6111, //#CJK UNIFIED IDEOGRAPH
    0xEDA8, 0x6137, //#CJK UNIFIED IDEOGRAPH
    0xEDA9, 0x6130, //#CJK UNIFIED IDEOGRAPH
    0xEDAA, 0x6198, //#CJK UNIFIED IDEOGRAPH
    0xEDAB, 0x6213, //#CJK UNIFIED IDEOGRAPH
    0xEDAC, 0x62A6, //#CJK UNIFIED IDEOGRAPH
    0xEDAD, 0x63F5, //#CJK UNIFIED IDEOGRAPH
    0xEDAE, 0x6460, //#CJK UNIFIED IDEOGRAPH
    0xEDAF, 0x649D, //#CJK UNIFIED IDEOGRAPH
    0xEDB0, 0x64CE, //#CJK UNIFIED IDEOGRAPH
    0xEDB1, 0x654E, //#CJK UNIFIED IDEOGRAPH
    0xEDB2, 0x6600, //#CJK UNIFIED IDEOGRAPH
    0xEDB3, 0x6615, //#CJK UNIFIED IDEOGRAPH
    0xEDB4, 0x663B, //#CJK UNIFIED IDEOGRAPH
    0xEDB5, 0x6609, //#CJK UNIFIED IDEOGRAPH
    0xEDB6, 0x662E, //#CJK UNIFIED IDEOGRAPH
    0xEDB7, 0x661E, //#CJK UNIFIED IDEOGRAPH
    0xEDB8, 0x6624, //#CJK UNIFIED IDEOGRAPH
    0xEDB9, 0x6665, //#CJK UNIFIED IDEOGRAPH
    0xEDBA, 0x6657, //#CJK UNIFIED IDEOGRAPH
    0xEDBB, 0x6659, //#CJK UNIFIED IDEOGRAPH
    0xEDBC, 0xFA12, //#CJK COMPATIBILITY IDEOGRAPH
    0xEDBD, 0x6673, //#CJK UNIFIED IDEOGRAPH
    0xEDBE, 0x6699, //#CJK UNIFIED IDEOGRAPH
    0xEDBF, 0x66A0, //#CJK UNIFIED IDEOGRAPH
    0xEDC0, 0x66B2, //#CJK UNIFIED IDEOGRAPH
    0xEDC1, 0x66BF, //#CJK UNIFIED IDEOGRAPH
    0xEDC2, 0x66FA, //#CJK UNIFIED IDEOGRAPH
    0xEDC3, 0x670E, //#CJK UNIFIED IDEOGRAPH
    0xEDC4, 0xF929, //#CJK COMPATIBILITY IDEOGRAPH
    0xEDC5, 0x6766, //#CJK UNIFIED IDEOGRAPH
    0xEDC6, 0x67BB, //#CJK UNIFIED IDEOGRAPH
    0xEDC7, 0x6852, //#CJK UNIFIED IDEOGRAPH
    0xEDC8, 0x67C0, //#CJK UNIFIED IDEOGRAPH
    0xEDC9, 0x6801, //#CJK UNIFIED IDEOGRAPH
    0xEDCA, 0x6844, //#CJK UNIFIED IDEOGRAPH
    0xEDCB, 0x68CF, //#CJK UNIFIED IDEOGRAPH
    0xEDCC, 0xFA13, //#CJK COMPATIBILITY IDEOGRAPH
    0xEDCD, 0x6968, //#CJK UNIFIED IDEOGRAPH
    0xEDCE, 0xFA14, //#CJK COMPATIBILITY IDEOGRAPH
    0xEDCF, 0x6998, //#CJK UNIFIED IDEOGRAPH
    0xEDD0, 0x69E2, //#CJK UNIFIED IDEOGRAPH
    0xEDD1, 0x6A30, //#CJK UNIFIED IDEOGRAPH
    0xEDD2, 0x6A6B, //#CJK UNIFIED IDEOGRAPH
    0xEDD3, 0x6A46, //#CJK UNIFIED IDEOGRAPH
    0xEDD4, 0x6A73, //#CJK UNIFIED IDEOGRAPH
    0xEDD5, 0x6A7E, //#CJK UNIFIED IDEOGRAPH
    0xEDD6, 0x6AE2, //#CJK UNIFIED IDEOGRAPH
    0xEDD7, 0x6AE4, //#CJK UNIFIED IDEOGRAPH
    0xEDD8, 0x6BD6, //#CJK UNIFIED IDEOGRAPH
    0xEDD9, 0x6C3F, //#CJK UNIFIED IDEOGRAPH
    0xEDDA, 0x6C5C, //#CJK UNIFIED IDEOGRAPH
    0xEDDB, 0x6C86, //#CJK UNIFIED IDEOGRAPH
    0xEDDC, 0x6C6F, //#CJK UNIFIED IDEOGRAPH
    0xEDDD, 0x6CDA, //#CJK UNIFIED IDEOGRAPH
    0xEDDE, 0x6D04, //#CJK UNIFIED IDEOGRAPH
    0xEDDF, 0x6D87, //#CJK UNIFIED IDEOGRAPH
    0xEDE0, 0x6D6F, //#CJK UNIFIED IDEOGRAPH
    0xEDE1, 0x6D96, //#CJK UNIFIED IDEOGRAPH
    0xEDE2, 0x6DAC, //#CJK UNIFIED IDEOGRAPH
    0xEDE3, 0x6DCF, //#CJK UNIFIED IDEOGRAPH
    0xEDE4, 0x6DF8, //#CJK UNIFIED IDEOGRAPH
    0xEDE5, 0x6DF2, //#CJK UNIFIED IDEOGRAPH
    0xEDE6, 0x6DFC, //#CJK UNIFIED IDEOGRAPH
    0xEDE7, 0x6E39, //#CJK UNIFIED IDEOGRAPH
    0xEDE8, 0x6E5C, //#CJK UNIFIED IDEOGRAPH
    0xEDE9, 0x6E27, //#CJK UNIFIED IDEOGRAPH
    0xEDEA, 0x6E3C, //#CJK UNIFIED IDEOGRAPH
    0xEDEB, 0x6EBF, //#CJK UNIFIED IDEOGRAPH
    0xEDEC, 0x6F88, //#CJK UNIFIED IDEOGRAPH
    0xEDED, 0x6FB5, //#CJK UNIFIED IDEOGRAPH
    0xEDEE, 0x6FF5, //#CJK UNIFIED IDEOGRAPH
    0xEDEF, 0x7005, //#CJK UNIFIED IDEOGRAPH
    0xEDF0, 0x7007, //#CJK UNIFIED IDEOGRAPH
    0xEDF1, 0x7028, //#CJK UNIFIED IDEOGRAPH
    0xEDF2, 0x7085, //#CJK UNIFIED IDEOGRAPH
    0xEDF3, 0x70AB, //#CJK UNIFIED IDEOGRAPH
    0xEDF4, 0x710F, //#CJK UNIFIED IDEOGRAPH
    0xEDF5, 0x7104, //#CJK UNIFIED IDEOGRAPH
    0xEDF6, 0x715C, //#CJK UNIFIED IDEOGRAPH
    0xEDF7, 0x7146, //#CJK UNIFIED IDEOGRAPH
    0xEDF8, 0x7147, //#CJK UNIFIED IDEOGRAPH
    0xEDF9, 0xFA15, //#CJK COMPATIBILITY IDEOGRAPH
    0xEDFA, 0x71C1, //#CJK UNIFIED IDEOGRAPH
    0xEDFB, 0x71FE, //#CJK UNIFIED IDEOGRAPH
    0xEDFC, 0x72B1, //#CJK UNIFIED IDEOGRAPH
    0xEE40, 0x72BE, //#CJK UNIFIED IDEOGRAPH
    0xEE41, 0x7324, //#CJK UNIFIED IDEOGRAPH
    0xEE42, 0xFA16, //#CJK COMPATIBILITY IDEOGRAPH
    0xEE43, 0x7377, //#CJK UNIFIED IDEOGRAPH
    0xEE44, 0x73BD, //#CJK UNIFIED IDEOGRAPH
    0xEE45, 0x73C9, //#CJK UNIFIED IDEOGRAPH
    0xEE46, 0x73D6, //#CJK UNIFIED IDEOGRAPH
    0xEE47, 0x73E3, //#CJK UNIFIED IDEOGRAPH
    0xEE48, 0x73D2, //#CJK UNIFIED IDEOGRAPH
    0xEE49, 0x7407, //#CJK UNIFIED IDEOGRAPH
    0xEE4A, 0x73F5, //#CJK UNIFIED IDEOGRAPH
    0xEE4B, 0x7426, //#CJK UNIFIED IDEOGRAPH
    0xEE4C, 0x742A, //#CJK UNIFIED IDEOGRAPH
    0xEE4D, 0x7429, //#CJK UNIFIED IDEOGRAPH
    0xEE4E, 0x742E, //#CJK UNIFIED IDEOGRAPH
    0xEE4F, 0x7462, //#CJK UNIFIED IDEOGRAPH
    0xEE50, 0x7489, //#CJK UNIFIED IDEOGRAPH
    0xEE51, 0x749F, //#CJK UNIFIED IDEOGRAPH
    0xEE52, 0x7501, //#CJK UNIFIED IDEOGRAPH
    0xEE53, 0x756F, //#CJK UNIFIED IDEOGRAPH
    0xEE54, 0x7682, //#CJK UNIFIED IDEOGRAPH
    0xEE55, 0x769C, //#CJK UNIFIED IDEOGRAPH
    0xEE56, 0x769E, //#CJK UNIFIED IDEOGRAPH
    0xEE57, 0x769B, //#CJK UNIFIED IDEOGRAPH
    0xEE58, 0x76A6, //#CJK UNIFIED IDEOGRAPH
    0xEE59, 0xFA17, //#CJK COMPATIBILITY IDEOGRAPH
    0xEE5A, 0x7746, //#CJK UNIFIED IDEOGRAPH
    0xEE5B, 0x52AF, //#CJK UNIFIED IDEOGRAPH
    0xEE5C, 0x7821, //#CJK UNIFIED IDEOGRAPH
    0xEE5D, 0x784E, //#CJK UNIFIED IDEOGRAPH
    0xEE5E, 0x7864, //#CJK UNIFIED IDEOGRAPH
    0xEE5F, 0x787A, //#CJK UNIFIED IDEOGRAPH
    0xEE60, 0x7930, //#CJK UNIFIED IDEOGRAPH
    0xEE61, 0xFA18, //#CJK COMPATIBILITY IDEOGRAPH
    0xEE62, 0xFA19, //#CJK COMPATIBILITY IDEOGRAPH
    0xEE63, 0xFA1A, //#CJK COMPATIBILITY IDEOGRAPH
    0xEE64, 0x7994, //#CJK UNIFIED IDEOGRAPH
    0xEE65, 0xFA1B, //#CJK COMPATIBILITY IDEOGRAPH
    0xEE66, 0x799B, //#CJK UNIFIED IDEOGRAPH
    0xEE67, 0x7AD1, //#CJK UNIFIED IDEOGRAPH
    0xEE68, 0x7AE7, //#CJK UNIFIED IDEOGRAPH
    0xEE69, 0xFA1C, //#CJK COMPATIBILITY IDEOGRAPH
    0xEE6A, 0x7AEB, //#CJK UNIFIED IDEOGRAPH
    0xEE6B, 0x7B9E, //#CJK UNIFIED IDEOGRAPH
    0xEE6C, 0xFA1D, //#CJK COMPATIBILITY IDEOGRAPH
    0xEE6D, 0x7D48, //#CJK UNIFIED IDEOGRAPH
    0xEE6E, 0x7D5C, //#CJK UNIFIED IDEOGRAPH
    0xEE6F, 0x7DB7, //#CJK UNIFIED IDEOGRAPH
    0xEE70, 0x7DA0, //#CJK UNIFIED IDEOGRAPH
    0xEE71, 0x7DD6, //#CJK UNIFIED IDEOGRAPH
    0xEE72, 0x7E52, //#CJK UNIFIED IDEOGRAPH
    0xEE73, 0x7F47, //#CJK UNIFIED IDEOGRAPH
    0xEE74, 0x7FA1, //#CJK UNIFIED IDEOGRAPH
    0xEE75, 0xFA1E, //#CJK COMPATIBILITY IDEOGRAPH
    0xEE76, 0x8301, //#CJK UNIFIED IDEOGRAPH
    0xEE77, 0x8362, //#CJK UNIFIED IDEOGRAPH
    0xEE78, 0x837F, //#CJK UNIFIED IDEOGRAPH
    0xEE79, 0x83C7, //#CJK UNIFIED IDEOGRAPH
    0xEE7A, 0x83F6, //#CJK UNIFIED IDEOGRAPH
    0xEE7B, 0x8448, //#CJK UNIFIED IDEOGRAPH
    0xEE7C, 0x84B4, //#CJK UNIFIED IDEOGRAPH
    0xEE7D, 0x8553, //#CJK UNIFIED IDEOGRAPH
    0xEE7E, 0x8559, //#CJK UNIFIED IDEOGRAPH
    0xEE80, 0x856B, //#CJK UNIFIED IDEOGRAPH
    0xEE81, 0xFA1F, //#CJK COMPATIBILITY IDEOGRAPH
    0xEE82, 0x85B0, //#CJK UNIFIED IDEOGRAPH
    0xEE83, 0xFA20, //#CJK COMPATIBILITY IDEOGRAPH
    0xEE84, 0xFA21, //#CJK COMPATIBILITY IDEOGRAPH
    0xEE85, 0x8807, //#CJK UNIFIED IDEOGRAPH
    0xEE86, 0x88F5, //#CJK UNIFIED IDEOGRAPH
    0xEE87, 0x8A12, //#CJK UNIFIED IDEOGRAPH
    0xEE88, 0x8A37, //#CJK UNIFIED IDEOGRAPH
    0xEE89, 0x8A79, //#CJK UNIFIED IDEOGRAPH
    0xEE8A, 0x8AA7, //#CJK UNIFIED IDEOGRAPH
    0xEE8B, 0x8ABE, //#CJK UNIFIED IDEOGRAPH
    0xEE8C, 0x8ADF, //#CJK UNIFIED IDEOGRAPH
    0xEE8D, 0xFA22, //#CJK COMPATIBILITY IDEOGRAPH
    0xEE8E, 0x8AF6, //#CJK UNIFIED IDEOGRAPH
    0xEE8F, 0x8B53, //#CJK UNIFIED IDEOGRAPH
    0xEE90, 0x8B7F, //#CJK UNIFIED IDEOGRAPH
    0xEE91, 0x8CF0, //#CJK UNIFIED IDEOGRAPH
    0xEE92, 0x8CF4, //#CJK UNIFIED IDEOGRAPH
    0xEE93, 0x8D12, //#CJK UNIFIED IDEOGRAPH
    0xEE94, 0x8D76, //#CJK UNIFIED IDEOGRAPH
    0xEE95, 0xFA23, //#CJK COMPATIBILITY IDEOGRAPH
    0xEE96, 0x8ECF, //#CJK UNIFIED IDEOGRAPH
    0xEE97, 0xFA24, //#CJK COMPATIBILITY IDEOGRAPH
    0xEE98, 0xFA25, //#CJK COMPATIBILITY IDEOGRAPH
    0xEE99, 0x9067, //#CJK UNIFIED IDEOGRAPH
    0xEE9A, 0x90DE, //#CJK UNIFIED IDEOGRAPH
    0xEE9B, 0xFA26, //#CJK COMPATIBILITY IDEOGRAPH
    0xEE9C, 0x9115, //#CJK UNIFIED IDEOGRAPH
    0xEE9D, 0x9127, //#CJK UNIFIED IDEOGRAPH
    0xEE9E, 0x91DA, //#CJK UNIFIED IDEOGRAPH
    0xEE9F, 0x91D7, //#CJK UNIFIED IDEOGRAPH
    0xEEA0, 0x91DE, //#CJK UNIFIED IDEOGRAPH
    0xEEA1, 0x91ED, //#CJK UNIFIED IDEOGRAPH
    0xEEA2, 0x91EE, //#CJK UNIFIED IDEOGRAPH
    0xEEA3, 0x91E4, //#CJK UNIFIED IDEOGRAPH
    0xEEA4, 0x91E5, //#CJK UNIFIED IDEOGRAPH
    0xEEA5, 0x9206, //#CJK UNIFIED IDEOGRAPH
    0xEEA6, 0x9210, //#CJK UNIFIED IDEOGRAPH
    0xEEA7, 0x920A, //#CJK UNIFIED IDEOGRAPH
    0xEEA8, 0x923A, //#CJK UNIFIED IDEOGRAPH
    0xEEA9, 0x9240, //#CJK UNIFIED IDEOGRAPH
    0xEEAA, 0x923C, //#CJK UNIFIED IDEOGRAPH
    0xEEAB, 0x924E, //#CJK UNIFIED IDEOGRAPH
    0xEEAC, 0x9259, //#CJK UNIFIED IDEOGRAPH
    0xEEAD, 0x9251, //#CJK UNIFIED IDEOGRAPH
    0xEEAE, 0x9239, //#CJK UNIFIED IDEOGRAPH
    0xEEAF, 0x9267, //#CJK UNIFIED IDEOGRAPH
    0xEEB0, 0x92A7, //#CJK UNIFIED IDEOGRAPH
    0xEEB1, 0x9277, //#CJK UNIFIED IDEOGRAPH
    0xEEB2, 0x9278, //#CJK UNIFIED IDEOGRAPH
    0xEEB3, 0x92E7, //#CJK UNIFIED IDEOGRAPH
    0xEEB4, 0x92D7, //#CJK UNIFIED IDEOGRAPH
    0xEEB5, 0x92D9, //#CJK UNIFIED IDEOGRAPH
    0xEEB6, 0x92D0, //#CJK UNIFIED IDEOGRAPH
    0xEEB7, 0xFA27, //#CJK COMPATIBILITY IDEOGRAPH
    0xEEB8, 0x92D5, //#CJK UNIFIED IDEOGRAPH
    0xEEB9, 0x92E0, //#CJK UNIFIED IDEOGRAPH
    0xEEBA, 0x92D3, //#CJK UNIFIED IDEOGRAPH
    0xEEBB, 0x9325, //#CJK UNIFIED IDEOGRAPH
    0xEEBC, 0x9321, //#CJK UNIFIED IDEOGRAPH
    0xEEBD, 0x92FB, //#CJK UNIFIED IDEOGRAPH
    0xEEBE, 0xFA28, //#CJK COMPATIBILITY IDEOGRAPH
    0xEEBF, 0x931E, //#CJK UNIFIED IDEOGRAPH
    0xEEC0, 0x92FF, //#CJK UNIFIED IDEOGRAPH
    0xEEC1, 0x931D, //#CJK UNIFIED IDEOGRAPH
    0xEEC2, 0x9302, //#CJK UNIFIED IDEOGRAPH
    0xEEC3, 0x9370, //#CJK UNIFIED IDEOGRAPH
    0xEEC4, 0x9357, //#CJK UNIFIED IDEOGRAPH
    0xEEC5, 0x93A4, //#CJK UNIFIED IDEOGRAPH
    0xEEC6, 0x93C6, //#CJK UNIFIED IDEOGRAPH
    0xEEC7, 0x93DE, //#CJK UNIFIED IDEOGRAPH
    0xEEC8, 0x93F8, //#CJK UNIFIED IDEOGRAPH
    0xEEC9, 0x9431, //#CJK UNIFIED IDEOGRAPH
    0xEECA, 0x9445, //#CJK UNIFIED IDEOGRAPH
    0xEECB, 0x9448, //#CJK UNIFIED IDEOGRAPH
    0xEECC, 0x9592, //#CJK UNIFIED IDEOGRAPH
    0xEECD, 0xF9DC, //#CJK COMPATIBILITY IDEOGRAPH
    0xEECE, 0xFA29, //#CJK COMPATIBILITY IDEOGRAPH
    0xEECF, 0x969D, //#CJK UNIFIED IDEOGRAPH
    0xEED0, 0x96AF, //#CJK UNIFIED IDEOGRAPH
    0xEED1, 0x9733, //#CJK UNIFIED IDEOGRAPH
    0xEED2, 0x973B, //#CJK UNIFIED IDEOGRAPH
    0xEED3, 0x9743, //#CJK UNIFIED IDEOGRAPH
    0xEED4, 0x974D, //#CJK UNIFIED IDEOGRAPH
    0xEED5, 0x974F, //#CJK UNIFIED IDEOGRAPH
    0xEED6, 0x9751, //#CJK UNIFIED IDEOGRAPH
    0xEED7, 0x9755, //#CJK UNIFIED IDEOGRAPH
    0xEED8, 0x9857, //#CJK UNIFIED IDEOGRAPH
    0xEED9, 0x9865, //#CJK UNIFIED IDEOGRAPH
    0xEEDA, 0xFA2A, //#CJK COMPATIBILITY IDEOGRAPH
    0xEEDB, 0xFA2B, //#CJK COMPATIBILITY IDEOGRAPH
    0xEEDC, 0x9927, //#CJK UNIFIED IDEOGRAPH
    0xEEDD, 0xFA2C, //#CJK COMPATIBILITY IDEOGRAPH
    0xEEDE, 0x999E, //#CJK UNIFIED IDEOGRAPH
    0xEEDF, 0x9A4E, //#CJK UNIFIED IDEOGRAPH
    0xEEE0, 0x9AD9, //#CJK UNIFIED IDEOGRAPH
    0xEEE1, 0x9ADC, //#CJK UNIFIED IDEOGRAPH
    0xEEE2, 0x9B75, //#CJK UNIFIED IDEOGRAPH
    0xEEE3, 0x9B72, //#CJK UNIFIED IDEOGRAPH
    0xEEE4, 0x9B8F, //#CJK UNIFIED IDEOGRAPH
    0xEEE5, 0x9BB1, //#CJK UNIFIED IDEOGRAPH
    0xEEE6, 0x9BBB, //#CJK UNIFIED IDEOGRAPH
    0xEEE7, 0x9C00, //#CJK UNIFIED IDEOGRAPH
    0xEEE8, 0x9D70, //#CJK UNIFIED IDEOGRAPH
    0xEEE9, 0x9D6B, //#CJK UNIFIED IDEOGRAPH
    0xEEEA, 0xFA2D, //#CJK COMPATIBILITY IDEOGRAPH
    0xEEEB, 0x9E19, //#CJK UNIFIED IDEOGRAPH
    0xEEEC, 0x9ED1, //#CJK UNIFIED IDEOGRAPH
    0xEEEF, 0x2170, //#SMALL ROMAN NUMERAL ONE
    0xEEF0, 0x2171, //#SMALL ROMAN NUMERAL TWO
    0xEEF1, 0x2172, //#SMALL ROMAN NUMERAL THREE
    0xEEF2, 0x2173, //#SMALL ROMAN NUMERAL FOUR
    0xEEF3, 0x2174, //#SMALL ROMAN NUMERAL FIVE
    0xEEF4, 0x2175, //#SMALL ROMAN NUMERAL SIX
    0xEEF5, 0x2176, //#SMALL ROMAN NUMERAL SEVEN
    0xEEF6, 0x2177, //#SMALL ROMAN NUMERAL EIGHT
    0xEEF7, 0x2178, //#SMALL ROMAN NUMERAL NINE
    0xEEF8, 0x2179, //#SMALL ROMAN NUMERAL TEN
    0xEEF9, 0xFFE2, //#FULLWIDTH NOT SIGN
    0xEEFA, 0xFFE4, //#FULLWIDTH BROKEN BAR
    0xEEFB, 0xFF07, //#FULLWIDTH APOSTROPHE
    0xEEFC, 0xFF02, //#FULLWIDTH QUOTATION MARK
    0xFA40, 0x2170, //#SMALL ROMAN NUMERAL ONE
    0xFA41, 0x2171, //#SMALL ROMAN NUMERAL TWO
    0xFA42, 0x2172, //#SMALL ROMAN NUMERAL THREE
    0xFA43, 0x2173, //#SMALL ROMAN NUMERAL FOUR
    0xFA44, 0x2174, //#SMALL ROMAN NUMERAL FIVE
    0xFA45, 0x2175, //#SMALL ROMAN NUMERAL SIX
    0xFA46, 0x2176, //#SMALL ROMAN NUMERAL SEVEN
    0xFA47, 0x2177, //#SMALL ROMAN NUMERAL EIGHT
    0xFA48, 0x2178, //#SMALL ROMAN NUMERAL NINE
    0xFA49, 0x2179, //#SMALL ROMAN NUMERAL TEN
    0xFA4A, 0x2160, //#ROMAN NUMERAL ONE
    0xFA4B, 0x2161, //#ROMAN NUMERAL TWO
    0xFA4C, 0x2162, //#ROMAN NUMERAL THREE
    0xFA4D, 0x2163, //#ROMAN NUMERAL FOUR
    0xFA4E, 0x2164, //#ROMAN NUMERAL FIVE
    0xFA4F, 0x2165, //#ROMAN NUMERAL SIX
    0xFA50, 0x2166, //#ROMAN NUMERAL SEVEN
    0xFA51, 0x2167, //#ROMAN NUMERAL EIGHT
    0xFA52, 0x2168, //#ROMAN NUMERAL NINE
    0xFA53, 0x2169, //#ROMAN NUMERAL TEN
    0xFA54, 0xFFE2, //#FULLWIDTH NOT SIGN
    0xFA55, 0xFFE4, //#FULLWIDTH BROKEN BAR
    0xFA56, 0xFF07, //#FULLWIDTH APOSTROPHE
    0xFA57, 0xFF02, //#FULLWIDTH QUOTATION MARK
    0xFA58, 0x3231, //#PARENTHESIZED IDEOGRAPH STOCK
    0xFA59, 0x2116, //#NUMERO SIGN
    0xFA5A, 0x2121, //#TELEPHONE SIGN
    0xFA5B, 0x2235, //#BECAUSE
    0xFA5C, 0x7E8A, //#CJK UNIFIED IDEOGRAPH
    0xFA5D, 0x891C, //#CJK UNIFIED IDEOGRAPH
    0xFA5E, 0x9348, //#CJK UNIFIED IDEOGRAPH
    0xFA5F, 0x9288, //#CJK UNIFIED IDEOGRAPH
    0xFA60, 0x84DC, //#CJK UNIFIED IDEOGRAPH
    0xFA61, 0x4FC9, //#CJK UNIFIED IDEOGRAPH
    0xFA62, 0x70BB, //#CJK UNIFIED IDEOGRAPH
    0xFA63, 0x6631, //#CJK UNIFIED IDEOGRAPH
    0xFA64, 0x68C8, //#CJK UNIFIED IDEOGRAPH
    0xFA65, 0x92F9, //#CJK UNIFIED IDEOGRAPH
    0xFA66, 0x66FB, //#CJK UNIFIED IDEOGRAPH
    0xFA67, 0x5F45, //#CJK UNIFIED IDEOGRAPH
    0xFA68, 0x4E28, //#CJK UNIFIED IDEOGRAPH
    0xFA69, 0x4EE1, //#CJK UNIFIED IDEOGRAPH
    0xFA6A, 0x4EFC, //#CJK UNIFIED IDEOGRAPH
    0xFA6B, 0x4F00, //#CJK UNIFIED IDEOGRAPH
    0xFA6C, 0x4F03, //#CJK UNIFIED IDEOGRAPH
    0xFA6D, 0x4F39, //#CJK UNIFIED IDEOGRAPH
    0xFA6E, 0x4F56, //#CJK UNIFIED IDEOGRAPH
    0xFA6F, 0x4F92, //#CJK UNIFIED IDEOGRAPH
    0xFA70, 0x4F8A, //#CJK UNIFIED IDEOGRAPH
    0xFA71, 0x4F9A, //#CJK UNIFIED IDEOGRAPH
    0xFA72, 0x4F94, //#CJK UNIFIED IDEOGRAPH
    0xFA73, 0x4FCD, //#CJK UNIFIED IDEOGRAPH
    0xFA74, 0x5040, //#CJK UNIFIED IDEOGRAPH
    0xFA75, 0x5022, //#CJK UNIFIED IDEOGRAPH
    0xFA76, 0x4FFF, //#CJK UNIFIED IDEOGRAPH
    0xFA77, 0x501E, //#CJK UNIFIED IDEOGRAPH
    0xFA78, 0x5046, //#CJK UNIFIED IDEOGRAPH
    0xFA79, 0x5070, //#CJK UNIFIED IDEOGRAPH
    0xFA7A, 0x5042, //#CJK UNIFIED IDEOGRAPH
    0xFA7B, 0x5094, //#CJK UNIFIED IDEOGRAPH
    0xFA7C, 0x50F4, //#CJK UNIFIED IDEOGRAPH
    0xFA7D, 0x50D8, //#CJK UNIFIED IDEOGRAPH
    0xFA7E, 0x514A, //#CJK UNIFIED IDEOGRAPH
    0xFA80, 0x5164, //#CJK UNIFIED IDEOGRAPH
    0xFA81, 0x519D, //#CJK UNIFIED IDEOGRAPH
    0xFA82, 0x51BE, //#CJK UNIFIED IDEOGRAPH
    0xFA83, 0x51EC, //#CJK UNIFIED IDEOGRAPH
    0xFA84, 0x5215, //#CJK UNIFIED IDEOGRAPH
    0xFA85, 0x529C, //#CJK UNIFIED IDEOGRAPH
    0xFA86, 0x52A6, //#CJK UNIFIED IDEOGRAPH
    0xFA87, 0x52C0, //#CJK UNIFIED IDEOGRAPH
    0xFA88, 0x52DB, //#CJK UNIFIED IDEOGRAPH
    0xFA89, 0x5300, //#CJK UNIFIED IDEOGRAPH
    0xFA8A, 0x5307, //#CJK UNIFIED IDEOGRAPH
    0xFA8B, 0x5324, //#CJK UNIFIED IDEOGRAPH
    0xFA8C, 0x5372, //#CJK UNIFIED IDEOGRAPH
    0xFA8D, 0x5393, //#CJK UNIFIED IDEOGRAPH
    0xFA8E, 0x53B2, //#CJK UNIFIED IDEOGRAPH
    0xFA8F, 0x53DD, //#CJK UNIFIED IDEOGRAPH
    0xFA90, 0xFA0E, //#CJK COMPATIBILITY IDEOGRAPH
    0xFA91, 0x549C, //#CJK UNIFIED IDEOGRAPH
    0xFA92, 0x548A, //#CJK UNIFIED IDEOGRAPH
    0xFA93, 0x54A9, //#CJK UNIFIED IDEOGRAPH
    0xFA94, 0x54FF, //#CJK UNIFIED IDEOGRAPH
    0xFA95, 0x5586, //#CJK UNIFIED IDEOGRAPH
    0xFA96, 0x5759, //#CJK UNIFIED IDEOGRAPH
    0xFA97, 0x5765, //#CJK UNIFIED IDEOGRAPH
    0xFA98, 0x57AC, //#CJK UNIFIED IDEOGRAPH
    0xFA99, 0x57C8, //#CJK UNIFIED IDEOGRAPH
    0xFA9A, 0x57C7, //#CJK UNIFIED IDEOGRAPH
    0xFA9B, 0xFA0F, //#CJK COMPATIBILITY IDEOGRAPH
    0xFA9C, 0xFA10, //#CJK COMPATIBILITY IDEOGRAPH
    0xFA9D, 0x589E, //#CJK UNIFIED IDEOGRAPH
    0xFA9E, 0x58B2, //#CJK UNIFIED IDEOGRAPH
    0xFA9F, 0x590B, //#CJK UNIFIED IDEOGRAPH
    0xFAA0, 0x5953, //#CJK UNIFIED IDEOGRAPH
    0xFAA1, 0x595B, //#CJK UNIFIED IDEOGRAPH
    0xFAA2, 0x595D, //#CJK UNIFIED IDEOGRAPH
    0xFAA3, 0x5963, //#CJK UNIFIED IDEOGRAPH
    0xFAA4, 0x59A4, //#CJK UNIFIED IDEOGRAPH
    0xFAA5, 0x59BA, //#CJK UNIFIED IDEOGRAPH
    0xFAA6, 0x5B56, //#CJK UNIFIED IDEOGRAPH
    0xFAA7, 0x5BC0, //#CJK UNIFIED IDEOGRAPH
    0xFAA8, 0x752F, //#CJK UNIFIED IDEOGRAPH
    0xFAA9, 0x5BD8, //#CJK UNIFIED IDEOGRAPH
    0xFAAA, 0x5BEC, //#CJK UNIFIED IDEOGRAPH
    0xFAAB, 0x5C1E, //#CJK UNIFIED IDEOGRAPH
    0xFAAC, 0x5CA6, //#CJK UNIFIED IDEOGRAPH
    0xFAAD, 0x5CBA, //#CJK UNIFIED IDEOGRAPH
    0xFAAE, 0x5CF5, //#CJK UNIFIED IDEOGRAPH
    0xFAAF, 0x5D27, //#CJK UNIFIED IDEOGRAPH
    0xFAB0, 0x5D53, //#CJK UNIFIED IDEOGRAPH
    0xFAB1, 0xFA11, //#CJK COMPATIBILITY IDEOGRAPH
    0xFAB2, 0x5D42, //#CJK UNIFIED IDEOGRAPH
    0xFAB3, 0x5D6D, //#CJK UNIFIED IDEOGRAPH
    0xFAB4, 0x5DB8, //#CJK UNIFIED IDEOGRAPH
    0xFAB5, 0x5DB9, //#CJK UNIFIED IDEOGRAPH
    0xFAB6, 0x5DD0, //#CJK UNIFIED IDEOGRAPH
    0xFAB7, 0x5F21, //#CJK UNIFIED IDEOGRAPH
    0xFAB8, 0x5F34, //#CJK UNIFIED IDEOGRAPH
    0xFAB9, 0x5F67, //#CJK UNIFIED IDEOGRAPH
    0xFABA, 0x5FB7, //#CJK UNIFIED IDEOGRAPH
    0xFABB, 0x5FDE, //#CJK UNIFIED IDEOGRAPH
    0xFABC, 0x605D, //#CJK UNIFIED IDEOGRAPH
    0xFABD, 0x6085, //#CJK UNIFIED IDEOGRAPH
    0xFABE, 0x608A, //#CJK UNIFIED IDEOGRAPH
    0xFABF, 0x60DE, //#CJK UNIFIED IDEOGRAPH
    0xFAC0, 0x60D5, //#CJK UNIFIED IDEOGRAPH
    0xFAC1, 0x6120, //#CJK UNIFIED IDEOGRAPH
    0xFAC2, 0x60F2, //#CJK UNIFIED IDEOGRAPH
    0xFAC3, 0x6111, //#CJK UNIFIED IDEOGRAPH
    0xFAC4, 0x6137, //#CJK UNIFIED IDEOGRAPH
    0xFAC5, 0x6130, //#CJK UNIFIED IDEOGRAPH
    0xFAC6, 0x6198, //#CJK UNIFIED IDEOGRAPH
    0xFAC7, 0x6213, //#CJK UNIFIED IDEOGRAPH
    0xFAC8, 0x62A6, //#CJK UNIFIED IDEOGRAPH
    0xFAC9, 0x63F5, //#CJK UNIFIED IDEOGRAPH
    0xFACA, 0x6460, //#CJK UNIFIED IDEOGRAPH
    0xFACB, 0x649D, //#CJK UNIFIED IDEOGRAPH
    0xFACC, 0x64CE, //#CJK UNIFIED IDEOGRAPH
    0xFACD, 0x654E, //#CJK UNIFIED IDEOGRAPH
    0xFACE, 0x6600, //#CJK UNIFIED IDEOGRAPH
    0xFACF, 0x6615, //#CJK UNIFIED IDEOGRAPH
    0xFAD0, 0x663B, //#CJK UNIFIED IDEOGRAPH
    0xFAD1, 0x6609, //#CJK UNIFIED IDEOGRAPH
    0xFAD2, 0x662E, //#CJK UNIFIED IDEOGRAPH
    0xFAD3, 0x661E, //#CJK UNIFIED IDEOGRAPH
    0xFAD4, 0x6624, //#CJK UNIFIED IDEOGRAPH
    0xFAD5, 0x6665, //#CJK UNIFIED IDEOGRAPH
    0xFAD6, 0x6657, //#CJK UNIFIED IDEOGRAPH
    0xFAD7, 0x6659, //#CJK UNIFIED IDEOGRAPH
    0xFAD8, 0xFA12, //#CJK COMPATIBILITY IDEOGRAPH
    0xFAD9, 0x6673, //#CJK UNIFIED IDEOGRAPH
    0xFADA, 0x6699, //#CJK UNIFIED IDEOGRAPH
    0xFADB, 0x66A0, //#CJK UNIFIED IDEOGRAPH
    0xFADC, 0x66B2, //#CJK UNIFIED IDEOGRAPH
    0xFADD, 0x66BF, //#CJK UNIFIED IDEOGRAPH
    0xFADE, 0x66FA, //#CJK UNIFIED IDEOGRAPH
    0xFADF, 0x670E, //#CJK UNIFIED IDEOGRAPH
    0xFAE0, 0xF929, //#CJK COMPATIBILITY IDEOGRAPH
    0xFAE1, 0x6766, //#CJK UNIFIED IDEOGRAPH
    0xFAE2, 0x67BB, //#CJK UNIFIED IDEOGRAPH
    0xFAE3, 0x6852, //#CJK UNIFIED IDEOGRAPH
    0xFAE4, 0x67C0, //#CJK UNIFIED IDEOGRAPH
    0xFAE5, 0x6801, //#CJK UNIFIED IDEOGRAPH
    0xFAE6, 0x6844, //#CJK UNIFIED IDEOGRAPH
    0xFAE7, 0x68CF, //#CJK UNIFIED IDEOGRAPH
    0xFAE8, 0xFA13, //#CJK COMPATIBILITY IDEOGRAPH
    0xFAE9, 0x6968, //#CJK UNIFIED IDEOGRAPH
    0xFAEA, 0xFA14, //#CJK COMPATIBILITY IDEOGRAPH
    0xFAEB, 0x6998, //#CJK UNIFIED IDEOGRAPH
    0xFAEC, 0x69E2, //#CJK UNIFIED IDEOGRAPH
    0xFAED, 0x6A30, //#CJK UNIFIED IDEOGRAPH
    0xFAEE, 0x6A6B, //#CJK UNIFIED IDEOGRAPH
    0xFAEF, 0x6A46, //#CJK UNIFIED IDEOGRAPH
    0xFAF0, 0x6A73, //#CJK UNIFIED IDEOGRAPH
    0xFAF1, 0x6A7E, //#CJK UNIFIED IDEOGRAPH
    0xFAF2, 0x6AE2, //#CJK UNIFIED IDEOGRAPH
    0xFAF3, 0x6AE4, //#CJK UNIFIED IDEOGRAPH
    0xFAF4, 0x6BD6, //#CJK UNIFIED IDEOGRAPH
    0xFAF5, 0x6C3F, //#CJK UNIFIED IDEOGRAPH
    0xFAF6, 0x6C5C, //#CJK UNIFIED IDEOGRAPH
    0xFAF7, 0x6C86, //#CJK UNIFIED IDEOGRAPH
    0xFAF8, 0x6C6F, //#CJK UNIFIED IDEOGRAPH
    0xFAF9, 0x6CDA, //#CJK UNIFIED IDEOGRAPH
    0xFAFA, 0x6D04, //#CJK UNIFIED IDEOGRAPH
    0xFAFB, 0x6D87, //#CJK UNIFIED IDEOGRAPH
    0xFAFC, 0x6D6F, //#CJK UNIFIED IDEOGRAPH
    0xFB40, 0x6D96, //#CJK UNIFIED IDEOGRAPH
    0xFB41, 0x6DAC, //#CJK UNIFIED IDEOGRAPH
    0xFB42, 0x6DCF, //#CJK UNIFIED IDEOGRAPH
    0xFB43, 0x6DF8, //#CJK UNIFIED IDEOGRAPH
    0xFB44, 0x6DF2, //#CJK UNIFIED IDEOGRAPH
    0xFB45, 0x6DFC, //#CJK UNIFIED IDEOGRAPH
    0xFB46, 0x6E39, //#CJK UNIFIED IDEOGRAPH
    0xFB47, 0x6E5C, //#CJK UNIFIED IDEOGRAPH
    0xFB48, 0x6E27, //#CJK UNIFIED IDEOGRAPH
    0xFB49, 0x6E3C, //#CJK UNIFIED IDEOGRAPH
    0xFB4A, 0x6EBF, //#CJK UNIFIED IDEOGRAPH
    0xFB4B, 0x6F88, //#CJK UNIFIED IDEOGRAPH
    0xFB4C, 0x6FB5, //#CJK UNIFIED IDEOGRAPH
    0xFB4D, 0x6FF5, //#CJK UNIFIED IDEOGRAPH
    0xFB4E, 0x7005, //#CJK UNIFIED IDEOGRAPH
    0xFB4F, 0x7007, //#CJK UNIFIED IDEOGRAPH
    0xFB50, 0x7028, //#CJK UNIFIED IDEOGRAPH
    0xFB51, 0x7085, //#CJK UNIFIED IDEOGRAPH
    0xFB52, 0x70AB, //#CJK UNIFIED IDEOGRAPH
    0xFB53, 0x710F, //#CJK UNIFIED IDEOGRAPH
    0xFB54, 0x7104, //#CJK UNIFIED IDEOGRAPH
    0xFB55, 0x715C, //#CJK UNIFIED IDEOGRAPH
    0xFB56, 0x7146, //#CJK UNIFIED IDEOGRAPH
    0xFB57, 0x7147, //#CJK UNIFIED IDEOGRAPH
    0xFB58, 0xFA15, //#CJK COMPATIBILITY IDEOGRAPH
    0xFB59, 0x71C1, //#CJK UNIFIED IDEOGRAPH
    0xFB5A, 0x71FE, //#CJK UNIFIED IDEOGRAPH
    0xFB5B, 0x72B1, //#CJK UNIFIED IDEOGRAPH
    0xFB5C, 0x72BE, //#CJK UNIFIED IDEOGRAPH
    0xFB5D, 0x7324, //#CJK UNIFIED IDEOGRAPH
    0xFB5E, 0xFA16, //#CJK COMPATIBILITY IDEOGRAPH
    0xFB5F, 0x7377, //#CJK UNIFIED IDEOGRAPH
    0xFB60, 0x73BD, //#CJK UNIFIED IDEOGRAPH
    0xFB61, 0x73C9, //#CJK UNIFIED IDEOGRAPH
    0xFB62, 0x73D6, //#CJK UNIFIED IDEOGRAPH
    0xFB63, 0x73E3, //#CJK UNIFIED IDEOGRAPH
    0xFB64, 0x73D2, //#CJK UNIFIED IDEOGRAPH
    0xFB65, 0x7407, //#CJK UNIFIED IDEOGRAPH
    0xFB66, 0x73F5, //#CJK UNIFIED IDEOGRAPH
    0xFB67, 0x7426, //#CJK UNIFIED IDEOGRAPH
    0xFB68, 0x742A, //#CJK UNIFIED IDEOGRAPH
    0xFB69, 0x7429, //#CJK UNIFIED IDEOGRAPH
    0xFB6A, 0x742E, //#CJK UNIFIED IDEOGRAPH
    0xFB6B, 0x7462, //#CJK UNIFIED IDEOGRAPH
    0xFB6C, 0x7489, //#CJK UNIFIED IDEOGRAPH
    0xFB6D, 0x749F, //#CJK UNIFIED IDEOGRAPH
    0xFB6E, 0x7501, //#CJK UNIFIED IDEOGRAPH
    0xFB6F, 0x756F, //#CJK UNIFIED IDEOGRAPH
    0xFB70, 0x7682, //#CJK UNIFIED IDEOGRAPH
    0xFB71, 0x769C, //#CJK UNIFIED IDEOGRAPH
    0xFB72, 0x769E, //#CJK UNIFIED IDEOGRAPH
    0xFB73, 0x769B, //#CJK UNIFIED IDEOGRAPH
    0xFB74, 0x76A6, //#CJK UNIFIED IDEOGRAPH
    0xFB75, 0xFA17, //#CJK COMPATIBILITY IDEOGRAPH
    0xFB76, 0x7746, //#CJK UNIFIED IDEOGRAPH
    0xFB77, 0x52AF, //#CJK UNIFIED IDEOGRAPH
    0xFB78, 0x7821, //#CJK UNIFIED IDEOGRAPH
    0xFB79, 0x784E, //#CJK UNIFIED IDEOGRAPH
    0xFB7A, 0x7864, //#CJK UNIFIED IDEOGRAPH
    0xFB7B, 0x787A, //#CJK UNIFIED IDEOGRAPH
    0xFB7C, 0x7930, //#CJK UNIFIED IDEOGRAPH
    0xFB7D, 0xFA18, //#CJK COMPATIBILITY IDEOGRAPH
    0xFB7E, 0xFA19, //#CJK COMPATIBILITY IDEOGRAPH
    0xFB80, 0xFA1A, //#CJK COMPATIBILITY IDEOGRAPH
    0xFB81, 0x7994, //#CJK UNIFIED IDEOGRAPH
    0xFB82, 0xFA1B, //#CJK COMPATIBILITY IDEOGRAPH
    0xFB83, 0x799B, //#CJK UNIFIED IDEOGRAPH
    0xFB84, 0x7AD1, //#CJK UNIFIED IDEOGRAPH
    0xFB85, 0x7AE7, //#CJK UNIFIED IDEOGRAPH
    0xFB86, 0xFA1C, //#CJK COMPATIBILITY IDEOGRAPH
    0xFB87, 0x7AEB, //#CJK UNIFIED IDEOGRAPH
    0xFB88, 0x7B9E, //#CJK UNIFIED IDEOGRAPH
    0xFB89, 0xFA1D, //#CJK COMPATIBILITY IDEOGRAPH
    0xFB8A, 0x7D48, //#CJK UNIFIED IDEOGRAPH
    0xFB8B, 0x7D5C, //#CJK UNIFIED IDEOGRAPH
    0xFB8C, 0x7DB7, //#CJK UNIFIED IDEOGRAPH
    0xFB8D, 0x7DA0, //#CJK UNIFIED IDEOGRAPH
    0xFB8E, 0x7DD6, //#CJK UNIFIED IDEOGRAPH
    0xFB8F, 0x7E52, //#CJK UNIFIED IDEOGRAPH
    0xFB90, 0x7F47, //#CJK UNIFIED IDEOGRAPH
    0xFB91, 0x7FA1, //#CJK UNIFIED IDEOGRAPH
    0xFB92, 0xFA1E, //#CJK COMPATIBILITY IDEOGRAPH
    0xFB93, 0x8301, //#CJK UNIFIED IDEOGRAPH
    0xFB94, 0x8362, //#CJK UNIFIED IDEOGRAPH
    0xFB95, 0x837F, //#CJK UNIFIED IDEOGRAPH
    0xFB96, 0x83C7, //#CJK UNIFIED IDEOGRAPH
    0xFB97, 0x83F6, //#CJK UNIFIED IDEOGRAPH
    0xFB98, 0x8448, //#CJK UNIFIED IDEOGRAPH
    0xFB99, 0x84B4, //#CJK UNIFIED IDEOGRAPH
    0xFB9A, 0x8553, //#CJK UNIFIED IDEOGRAPH
    0xFB9B, 0x8559, //#CJK UNIFIED IDEOGRAPH
    0xFB9C, 0x856B, //#CJK UNIFIED IDEOGRAPH
    0xFB9D, 0xFA1F, //#CJK COMPATIBILITY IDEOGRAPH
    0xFB9E, 0x85B0, //#CJK UNIFIED IDEOGRAPH
    0xFB9F, 0xFA20, //#CJK COMPATIBILITY IDEOGRAPH
    0xFBA0, 0xFA21, //#CJK COMPATIBILITY IDEOGRAPH
    0xFBA1, 0x8807, //#CJK UNIFIED IDEOGRAPH
    0xFBA2, 0x88F5, //#CJK UNIFIED IDEOGRAPH
    0xFBA3, 0x8A12, //#CJK UNIFIED IDEOGRAPH
    0xFBA4, 0x8A37, //#CJK UNIFIED IDEOGRAPH
    0xFBA5, 0x8A79, //#CJK UNIFIED IDEOGRAPH
    0xFBA6, 0x8AA7, //#CJK UNIFIED IDEOGRAPH
    0xFBA7, 0x8ABE, //#CJK UNIFIED IDEOGRAPH
    0xFBA8, 0x8ADF, //#CJK UNIFIED IDEOGRAPH
    0xFBA9, 0xFA22, //#CJK COMPATIBILITY IDEOGRAPH
    0xFBAA, 0x8AF6, //#CJK UNIFIED IDEOGRAPH
    0xFBAB, 0x8B53, //#CJK UNIFIED IDEOGRAPH
    0xFBAC, 0x8B7F, //#CJK UNIFIED IDEOGRAPH
    0xFBAD, 0x8CF0, //#CJK UNIFIED IDEOGRAPH
    0xFBAE, 0x8CF4, //#CJK UNIFIED IDEOGRAPH
    0xFBAF, 0x8D12, //#CJK UNIFIED IDEOGRAPH
    0xFBB0, 0x8D76, //#CJK UNIFIED IDEOGRAPH
    0xFBB1, 0xFA23, //#CJK COMPATIBILITY IDEOGRAPH
    0xFBB2, 0x8ECF, //#CJK UNIFIED IDEOGRAPH
    0xFBB3, 0xFA24, //#CJK COMPATIBILITY IDEOGRAPH
    0xFBB4, 0xFA25, //#CJK COMPATIBILITY IDEOGRAPH
    0xFBB5, 0x9067, //#CJK UNIFIED IDEOGRAPH
    0xFBB6, 0x90DE, //#CJK UNIFIED IDEOGRAPH
    0xFBB7, 0xFA26, //#CJK COMPATIBILITY IDEOGRAPH
    0xFBB8, 0x9115, //#CJK UNIFIED IDEOGRAPH
    0xFBB9, 0x9127, //#CJK UNIFIED IDEOGRAPH
    0xFBBA, 0x91DA, //#CJK UNIFIED IDEOGRAPH
    0xFBBB, 0x91D7, //#CJK UNIFIED IDEOGRAPH
    0xFBBC, 0x91DE, //#CJK UNIFIED IDEOGRAPH
    0xFBBD, 0x91ED, //#CJK UNIFIED IDEOGRAPH
    0xFBBE, 0x91EE, //#CJK UNIFIED IDEOGRAPH
    0xFBBF, 0x91E4, //#CJK UNIFIED IDEOGRAPH
    0xFBC0, 0x91E5, //#CJK UNIFIED IDEOGRAPH
    0xFBC1, 0x9206, //#CJK UNIFIED IDEOGRAPH
    0xFBC2, 0x9210, //#CJK UNIFIED IDEOGRAPH
    0xFBC3, 0x920A, //#CJK UNIFIED IDEOGRAPH
    0xFBC4, 0x923A, //#CJK UNIFIED IDEOGRAPH
    0xFBC5, 0x9240, //#CJK UNIFIED IDEOGRAPH
    0xFBC6, 0x923C, //#CJK UNIFIED IDEOGRAPH
    0xFBC7, 0x924E, //#CJK UNIFIED IDEOGRAPH
    0xFBC8, 0x9259, //#CJK UNIFIED IDEOGRAPH
    0xFBC9, 0x9251, //#CJK UNIFIED IDEOGRAPH
    0xFBCA, 0x9239, //#CJK UNIFIED IDEOGRAPH
    0xFBCB, 0x9267, //#CJK UNIFIED IDEOGRAPH
    0xFBCC, 0x92A7, //#CJK UNIFIED IDEOGRAPH
    0xFBCD, 0x9277, //#CJK UNIFIED IDEOGRAPH
    0xFBCE, 0x9278, //#CJK UNIFIED IDEOGRAPH
    0xFBCF, 0x92E7, //#CJK UNIFIED IDEOGRAPH
    0xFBD0, 0x92D7, //#CJK UNIFIED IDEOGRAPH
    0xFBD1, 0x92D9, //#CJK UNIFIED IDEOGRAPH
    0xFBD2, 0x92D0, //#CJK UNIFIED IDEOGRAPH
    0xFBD3, 0xFA27, //#CJK COMPATIBILITY IDEOGRAPH
    0xFBD4, 0x92D5, //#CJK UNIFIED IDEOGRAPH
    0xFBD5, 0x92E0, //#CJK UNIFIED IDEOGRAPH
    0xFBD6, 0x92D3, //#CJK UNIFIED IDEOGRAPH
    0xFBD7, 0x9325, //#CJK UNIFIED IDEOGRAPH
    0xFBD8, 0x9321, //#CJK UNIFIED IDEOGRAPH
    0xFBD9, 0x92FB, //#CJK UNIFIED IDEOGRAPH
    0xFBDA, 0xFA28, //#CJK COMPATIBILITY IDEOGRAPH
    0xFBDB, 0x931E, //#CJK UNIFIED IDEOGRAPH
    0xFBDC, 0x92FF, //#CJK UNIFIED IDEOGRAPH
    0xFBDD, 0x931D, //#CJK UNIFIED IDEOGRAPH
    0xFBDE, 0x9302, //#CJK UNIFIED IDEOGRAPH
    0xFBDF, 0x9370, //#CJK UNIFIED IDEOGRAPH
    0xFBE0, 0x9357, //#CJK UNIFIED IDEOGRAPH
    0xFBE1, 0x93A4, //#CJK UNIFIED IDEOGRAPH
    0xFBE2, 0x93C6, //#CJK UNIFIED IDEOGRAPH
    0xFBE3, 0x93DE, //#CJK UNIFIED IDEOGRAPH
    0xFBE4, 0x93F8, //#CJK UNIFIED IDEOGRAPH
    0xFBE5, 0x9431, //#CJK UNIFIED IDEOGRAPH
    0xFBE6, 0x9445, //#CJK UNIFIED IDEOGRAPH
    0xFBE7, 0x9448, //#CJK UNIFIED IDEOGRAPH
    0xFBE8, 0x9592, //#CJK UNIFIED IDEOGRAPH
    0xFBE9, 0xF9DC, //#CJK COMPATIBILITY IDEOGRAPH
    0xFBEA, 0xFA29, //#CJK COMPATIBILITY IDEOGRAPH
    0xFBEB, 0x969D, //#CJK UNIFIED IDEOGRAPH
    0xFBEC, 0x96AF, //#CJK UNIFIED IDEOGRAPH
    0xFBED, 0x9733, //#CJK UNIFIED IDEOGRAPH
    0xFBEE, 0x973B, //#CJK UNIFIED IDEOGRAPH
    0xFBEF, 0x9743, //#CJK UNIFIED IDEOGRAPH
    0xFBF0, 0x974D, //#CJK UNIFIED IDEOGRAPH
    0xFBF1, 0x974F, //#CJK UNIFIED IDEOGRAPH
    0xFBF2, 0x9751, //#CJK UNIFIED IDEOGRAPH
    0xFBF3, 0x9755, //#CJK UNIFIED IDEOGRAPH
    0xFBF4, 0x9857, //#CJK UNIFIED IDEOGRAPH
    0xFBF5, 0x9865, //#CJK UNIFIED IDEOGRAPH
    0xFBF6, 0xFA2A, //#CJK COMPATIBILITY IDEOGRAPH
    0xFBF7, 0xFA2B, //#CJK COMPATIBILITY IDEOGRAPH
    0xFBF8, 0x9927, //#CJK UNIFIED IDEOGRAPH
    0xFBF9, 0xFA2C, //#CJK COMPATIBILITY IDEOGRAPH
    0xFBFA, 0x999E, //#CJK UNIFIED IDEOGRAPH
    0xFBFB, 0x9A4E, //#CJK UNIFIED IDEOGRAPH
    0xFBFC, 0x9AD9, //#CJK UNIFIED IDEOGRAPH
    0xFC40, 0x9ADC, //#CJK UNIFIED IDEOGRAPH
    0xFC41, 0x9B75, //#CJK UNIFIED IDEOGRAPH
    0xFC42, 0x9B72, //#CJK UNIFIED IDEOGRAPH
    0xFC43, 0x9B8F, //#CJK UNIFIED IDEOGRAPH
    0xFC44, 0x9BB1, //#CJK UNIFIED IDEOGRAPH
    0xFC45, 0x9BBB, //#CJK UNIFIED IDEOGRAPH
    0xFC46, 0x9C00, //#CJK UNIFIED IDEOGRAPH
    0xFC47, 0x9D70, //#CJK UNIFIED IDEOGRAPH
    0xFC48, 0x9D6B, //#CJK UNIFIED IDEOGRAPH
    0xFC49, 0xFA2D, //#CJK COMPATIBILITY IDEOGRAPH
    0xFC4A, 0x9E19, //#CJK UNIFIED IDEOGRAPH
    0xFC4B, 0x9ED1, //#CJK UNIFIED IDEOGRAPH

    0x00,   0x0000  //#NULL  (Table Stopper)
};

/*
 *  IN  uni_str  : unicode str (terminate 0x0000)
 *  OUT sjis_str : sjis str 
 */
char *uni2sjis(unsigned short *uni_str, char *sjis_str)
{
    unsigned short *uni = (unsigned short *)uni_str;
    int iu = 0;          /* uni_str index */
    int tbl_index = 0;   /* tbl index     */
    int is = 0;
    int found = 0;
    char *pt;

    strcpy(sjis_str, "");

    while (uni[iu]) {
	found = 0;
	tbl_index = 0;
	//printf("uni[%d]=0x%04x\n", iu, uni[iu]);

	/* [tbl_index]:SJIS   [tbl_index+1]:UNI */
	while (sjis_uni_tbl[tbl_index]) {
	    if (sjis_uni_tbl[tbl_index + 1] == uni[iu]) {
                found = 1;
		//printf("found : %d\n", tbl_index);
		break;
	    }
	    tbl_index += 2;
	}

	if (found) {
	    pt = (char *)&sjis_uni_tbl[tbl_index];   /* SJIS */
	    if (sjis_uni_tbl[tbl_index] & 0xff00) {
		/* 2bytes */
		sjis_str[is++] = pt[1];
		sjis_str[is++] = pt[0];
	    } else {
		/* 1bytes */
		sjis_str[is++] = pt[0];
	    }
	    sjis_str[is] = '\0';
	} else {
            sjis_str[is++] = '?';
	    sjis_str[is] = '\0';
	}
        ++iu;
    }
    return sjis_str;
}


/*
 *  NULL terminate strncpy 
 *    OUT dest 
 *    IN  src
 *    IN  size
 */
char *STRNCPY(char *dest, char *src, int size)
{
    strncpy(dest, src, size);
    dest[size] = '\0';
    return dest;
}


#define READ_SIZE (1024)
#define DUMP_DATA_FILE 1

#ifdef DUMP_DATA_FILE
void DumpData(int fp, uint64 start_pos, int isize)
#else
void DumpData(unsigned char *buf, uint64 start_pos, int isize)
#endif
{
    int c, xx, addr = 0;
    int f=0, f2=0;
    int kflag = 0;
    int fflag = 0;            /* -f flush */
    int rev   = 0;            /* 1:dpの出力結果を元のファイルに戻す */
    int size  = 0;
    int work;
    int pos   = 0;            /* for read  */
    int opos  = 0;            /* for write */
    unsigned char obuf[1024]; /* for write */
    char asc[17];
    int count = 0;            /* dump len  */

    uint   *val = (uint *)&start_pos; /* uint64->uint32 x 2 */
    uint64 wk64;                      /* work 64bit value   */
    uint   *wk64val = (uint *)&wk64;  /* uint64->uint32 x 2 */

#ifdef DUMP_DATA_FILE
    unsigned char buf[READ_SIZE+1];
#endif

    printf("Location: +0       +4       +8       +C       /0123456789ABCDEF\n");

#ifdef DUMP_DATA_FILE
    if (start_pos >= 0) {
        if (lseek64(fp, start_pos, SEEK_SET) < 0) {
	    fprintf(stderr, "fseek Error. pos.h=0x%08x pos.l=0x%08x errno=%d\n", val[1], val[0], errno); /* for little endian */
	}
    }
    /***** (original getc(first)) *****/
    size = read(fp, buf, READ_SIZE);
    pos  = 0;
    if (size <= 0) {
        c = EOF;
    } else {
        c = buf[pos++];
	++count;
    }
    /**********************************/
#else
    /* dummy read */
    if (isize < READ_SIZE) {
        size = isize;
    } else {
        size = READ_SIZE;
    }
    if (isize == 0) {
        c = EOF;
    } else {
        c = buf[pos++];
	++count;
    }
#endif

    opos = 0;
    while(c != EOF && count < isize) {
	xx = 0;
	strcpy(asc,"                ");
	/* printf("%08x: ",addr); */
	// printf("%08x: ",addr + start_pos);
	wk64 = start_pos + addr;
	printf("%02x%08x: ",wk64val[1], wk64val[0]);
	f = 0;
	while(c != EOF && xx < 16) {
	    if (fflag) {
	        printf("%02x",c);          /* これが実行時間の60% */
	        fflush(stdout);            /* 2.2倍くらい遅くなる */
	    } else {
	        work = c >> 4;
	        obuf[opos++] = ((work > 9)?work-10+'a':work+'0');
	        work = c & 0x0f;
	        obuf[opos++] = ((work > 9)?work-10+'a':work+'0');
	    }

	    if (c < 32) {
		if (f) {
		    asc[xx] = c;
		    if (c == 10 || c == 13) {
			asc[xx] = '.'; /* 暫定 */
		    }
		} else {
		    asc[xx] = '.';
		}
		f = 0;
	    } else if (c >= 127 ) {
		if (kflag) {
		    if (f) {
			asc[xx] = c;
			f = 0;
		    } else {
			asc[xx] = c;
			f = 1;
		    }
		} else {
		    asc[xx] = '.';
		}
	    } else {
		asc[xx] = c;
		f = 0;
	    }
	    if (xx == 0 && f2) {
		asc[xx] = '.';
		f = 0;
		f2 = 0;
	    }
	    if ((xx % 4) == 3) {
	        if (fflag) {
	            printf(" ");
		} else {
	            obuf[opos++] = ' ';
		}
	    }
	    ++xx;
	    ++addr;

            /***** (original getc(next))  10%up *****/
	    if (pos >= size) {
#ifdef DUMP_DATA_FILE
                size = read(fp, buf, READ_SIZE);
                pos  = 0;
                if (size <= 0) {
                    c = EOF;
                } else {
                    c = buf[pos++];
	            ++count;
                }
#else
                /* dummy read */
                if (isize < pos + READ_SIZE) {
                    size = isize - pos;
                }
                if (isize == 0) {
                    c = EOF;
                } else {
                    c = buf[pos++];
	            ++count;
                }
#endif
	    } else {
                c = buf[pos++];
	        ++count;
	    }
	    /****************************************/

	    if (f == 1 && xx >= 16) {
		asc[xx++] = c;
		f = 0;
		f2 = 1;
	    }
	}
	if (!fflag) {
	    obuf[opos] = '\0';
	    printf("%s", obuf);
	    opos = 0;
	}

	while (xx <16) {
		printf("  ");
		if ((xx % 4) == 3) printf(" ");
		++xx;
	}
	asc[xx]='\0';

	printf("/%16s \n",asc);
    }
}

/*
 *  GetValue from buffer
 *
 *  IN  buf : value address 
 *  IN  size: value size
 *  OUT ret : value
 */
unsigned int GetValue(unsigned char *buf, int size)
{
    unsigned short shortval = 0;
    unsigned int   intval = 0;

    switch (size) {
      case 1:
	intval = (unsigned int)buf[0];
	break;
      case 2:
	memcpy(&shortval, buf, 2);
	intval = (unsigned int)shortval;
	break;
      case 4:
	memcpy(&intval, buf, 4);
	break;
      default:
	printf("GetValue: unsupported size %d\n", size);
	break;
    }
    return intval;
}

/*
 *  Get date and time from FAT time format
 *
 *  IN  date2     : date data (2bytes)
 *  IN  time2     : time data (2bytes)
 *
 *  DATE
 *    7bit : year (1980が0)
 *    4bit : month
 *    5bit : day
 *
 *  TIME
 *    5bit : hour
 *    6bit : min
 *    5bit : sec  (2sec unit)
 */
void GetDateTime(char *date2, char *time2, datetime_t *dt)
{
    ushort sdate;
    ushort stime;

    memcpy(&sdate, date2, sizeof(short));
    memcpy(&stime, time2, sizeof(short));

    dt->year  = (sdate >> 9) + 1980;
    dt->month = (sdate >> 5) & 0xf;
    dt->day   = sdate & 0x1f;

    dt->hour  = stime >> 11;
    dt->min   = (stime >> 5) & 0x3f;
    dt->sec   = (stime & 0x1f) * 2;
}

/*
 *  List directory entry
 *
 *  IN  fp        : file discriptor
 *  IN  bpb       : bpb pointor
 *  IN  dir_clust : clustor no. of dir entry
 *  IN  out_size  : clustor size
 */
void ListDirEnt(int fp, bpb_t *bpb, uint dir_clust, int out_size)
{
    int    count = 0;
    uchar  fnames[16];    /* short filename */
    uchar  fnamel[2048];  /* long  filename */

    ushort uni13[64];     /* unicode work   */
    ushort uni[1024];     /* unicode str    */
    char   sjis_wk[1024]; /* sjis    work   */
    char   sjis[1024];    /* sjis    str    */
    char   datebuf[64];   /* mod date time  V0.14-A */
    datetime_t dt;        /* date/time      V0.14-A */
    int    dircnt = 0;    /* dirent count   V0.15-A */

    uchar  attr[16];
    uchar  *pt;
    int    size;
    fat_dirent_t fdirent;  /* one entry     */
    fat_dirent_t *dirents; /* direntrys     V0.15-A */
    uint64 dir_pos;
    uint   next_clust;

    uint64 wk64;                      /* work 64bit value   */
    uint   *wk64val = (uint *)&wk64;  /* uint64->uint32 x 2 */

    fnames[0] = '\0';
    fnamel[0] = '\0';
    memset(uni13, 0, sizeof(uni13));
    memset(uni  , 0, sizeof(uni));

    /* V0.15-A start */
    dirents = (fat_dirent_t *)malloc(out_size);
    if (dirents == NULL) {
	printf("ListDirEnt: malloc failed errno=%d", errno);
	exit(1);
    }
    /* V0.15-A end   */

#if 0
    dir_pos = (bpb->fat_start_sect
               + bpb->num_of_fat * bpb->fat_sectors
               + (dir_clust-2) * bpb->sects_per_clust) * bpb->sector_size;
#else
    dir_pos = CLUST2POS(bpb, dir_clust);
#endif

    //printf("Pos : 0x%08x\n", dir_pos);
    lseek64(fp, dir_pos, SEEK_SET);

    /* V0.15-A start */
    /* win32ではセクタサイズ倍数で読まないといけないので、まとめて読むようにする */
    size = read(fp, dirents, out_size);
    if (size != out_size) {
        printf("Error: read size = %d/%d\n", size, out_size);
	if (dirents) free(dirents);
        exit(1);
    }
    /* V0.15-A end   */

    dircnt = 0;  /* dirent count */
    while (1) {

#if 0  /* V0.15-C start */
	size = read(fp, &fdirent, sizeof(fat_dirent_t));
        if (size != sizeof(fat_dirent_t)) {
	    printf("Warning: read size = %d\n", size);
	    break;
	}
#else
	memcpy(&fdirent, &dirents[dircnt], sizeof(fat_dirent_t));
	size = sizeof(fat_dirent_t);
#endif  /* V0.15-C end   */

        if (fdirent.u.dshort.filename[0] == 0x00) {
	    count += size;
	    if (count >= out_size) {
	        break;
	    }
	    ++dircnt;  /* V0.15-A */
	    continue;
	}

	/* analyze ent */
        if (fdirent.u.dlong.attr == 0x0f)                      /* +0b */
             //|| GetValue(fdirent.u.dshort.fcl_low, 2) == 0)    /* +0c */
	{
	    /* long name entry */
	    memcpy(&uni13[0],  fdirent.u.dlong.uni_fname1, 10);
	    memcpy(&uni13[5],  fdirent.u.dlong.uni_fname2, 12);
	    memcpy(&uni13[11], fdirent.u.dlong.uni_fname3,  4);
	    strcpy(sjis_wk, fnamel);  /* backup              */
	    uni2sjis(uni13, fnamel);  /* convert uni to sjis */
	    dprintf("### convert [%s]\n", fnamel);
	    strcat(fnamel, sjis_wk);  /* add to front        */
	} else {
	    /* short entry */
	    /* get attr */
	    attr[0] = '\0';
	    if (fdirent.u.dshort.attr & ENT_ATTR_DIR) {
	        strcat(attr, "D");
	    }
	    if (fdirent.u.dshort.attr & ENT_ATTR_FILE) {
	        strcat(attr, "F");
	    }
	    if (fdirent.u.dshort.attr & ENT_ATTR_VOLLABEL) {
	        strcat(attr, "V");
	    }
	    if (fdirent.u.dshort.attr & ENT_ATTR_SYSFILE) {
	        strcat(attr, "S");
	    }
	    if (fdirent.u.dshort.attr & ENT_ATTR_HIDDEN) {
	        strcat(attr, "H");
	    }
	    if (fdirent.u.dshort.attr & ENT_ATTR_READONLY) {
	        strcat(attr, "R");
	    }

	    STRNCPY(fnames, fdirent.u.dshort.filename, 8);
	    pt = &fnames[7];
	    while (*pt == ' ') --pt;
	    if (fdirent.u.dshort.suffix[0] != ' ') {
	        *(++pt) = '.';
	    }
	    ++pt;
	    STRNCPY(pt, fdirent.u.dshort.suffix, 3);
	    pt = &fnames[strlen(fnames)-1];
	    while (*pt == ' ') --pt;
	    *(++pt) = '\0';

	    if (fnames[0] == 0xe5) {
		/* deleted entry */
	        strcat(attr, "X");
		/* 1st byte, copy form long filenmae */
		fnames[0] = fnamel[0]?toupper(fnamel[0]):'?';
	    }
	    next_clust = (GetValue(fdirent.u.dshort.fcl_high, 2) << 16) +
                          GetValue(fdirent.u.dshort.fcl_low,  2);
	    if ((next_clust & 0xffff0000) == 0xffff0000) {
		next_clust &= 0xffff;
            }

	    wk64 = dir_pos + count;
	    /* V0.14-A start */
	    datebuf[0] = '\0';
	    if (lf) {
		GetDateTime(fdirent.u.dshort.mdate, fdirent.u.dshort.mtime, &dt);
		sprintf(datebuf, " %04d/%02d/%02d %02d:%02d:%02d",
			dt.year, dt.month, dt.day, dt.hour, dt.min, dt.sec);
	    }
	    /* V0.14-A end   */
            printf("%02x%08x[%-4s]: %-12s/%-16s H:%04x L:%04x Clst:%d Sz:%d%s\n", 
                   wk64val[1], wk64val[0],
                   attr, fnames, fnamel, 
                   GetValue(fdirent.u.dshort.fcl_high, 2),
                   GetValue(fdirent.u.dshort.fcl_low,  2),
                   next_clust,
                   GetValue(fdirent.u.dshort.file_size, 4),
		   datebuf);  /* V0.14-A */

	    /* clear */
	    fnames[0] = '\0';
	    fnamel[0] = '\0';
	}

	count += size;
	if (count >= out_size) {
	    break;
	}
        ++dircnt;  /* V0.15-A */
    }
    if (dirents) free(dirents);
}

void SetBPB(bpb32_t *bpb32, bpb_t *bpb)
{
    bpb16_t *bpb16 = (bpb16_t *)bpb32;

    /* deside FAT Type */
    if (!strncmp(bpb16->filesys_type, "FAT1", 4)) {
	/* FAT12/16 */
	bpb->fat_type = atoi(&bpb16->filesys_type[3]);
    } else if (!strncmp(bpb32->filesys_type, "FAT32", 5)) {
	/* FAT32 */
	bpb->fat_type = atoi(&bpb32->filesys_type[3]);
    } else if (!strncmp(bpb16->oem_label, "NTFS", 4)){
	/* NTFS */
	bpb->fat_type = 0;
    } else {
	char buf[128];
	unsigned char *pt = (unsigned char *)bpb32;

	strncpy(buf, bpb32->filesys_type, 5);
	buf[5] = '\0';
        printf("filesys_type[%s] %02x%02x\n", buf, pt[0], pt[1]);

	strncpy(buf, bpb16->oem_label, 8);
	buf[8] = '\0';
        printf("oem_label[%s]\n", buf);

        printf("Unknown Filesystem\n");
	return;
    }

    bpb->sector_size     = GetValue(bpb32->s_sector_size, 2);
    bpb->sects_per_clust = bpb32->sects_per_clust;
    bpb->clustor_size    = bpb32->sects_per_clust * bpb->sector_size;
    if (bpb->fat_type) {
	/* common value */
        bpb->fat_start_sect  = GetValue(bpb32->s_reserve_sect, 2);
        bpb->fat_start_pos   = bpb->fat_start_sect * bpb->sector_size;
        bpb->num_of_fat      = bpb32->num_of_fat;
        bpb->secret_sects    = GetValue(bpb32->i_secret_sects, 4);
        bpb->secret_size     = bpb->secret_sects * bpb->sector_size;

	/* FAT12/16 value */
        bpb->max_root_dirent = GetValue(bpb32->s_max_root_dirent,2); /* FAT16 */
        bpb->total_sectors   = GetValue(bpb32->s_total_sectors, 2);  /* FAT16 */
        bpb->fat_sectors     = GetValue(bpb32->s_fat_sectors, 2);    /* FAT16 */
    }

    if (bpb->fat_type == 0) {
	return;  /* NTFS */
    }

    if (bpb->total_sectors == 0) {
        bpb->total_sectors = GetValue(bpb32->i_total_sectors, 4);
    }
    if (bpb->fat_sectors == 0) {
        bpb->fat_sectors = GetValue(bpb32->i_sect_per_fat, 4);
    }

    if (bpb->fat_type == 32) {
	/* FAT32 value */
        //bpb->sect_per_fat   = GetValue(bpb32->i_sect_per_fat, 4);
        bpb->filesys_ver    = GetValue(bpb32->s_filesys_ver, 2);
        bpb->root_clust     = GetValue(bpb32->i_root_clust, 4);
        bpb->fsinfo_sect    = GetValue(bpb32->s_fsinfo_sect, 2);
        bpb->boot_sect_copy = GetValue(bpb32->s_boot_sect_copy, 2);

    } else {
	/* After FAT */
        bpb->root_clust     = 2;
        //bpb->root_dir_pos   = (bpb->fat_start_sect
        //                        + bpb->num_of_fat*bpb->fat_sectors)
        //                        * bpb->sector_size;
    }
    bpb->root_dir_pos   = (bpb->fat_start_sect
		             + bpb->num_of_fat*bpb->fat_sectors)
		             * bpb->sector_size
                           + (bpb->root_clust-2) * bpb->sects_per_clust
                                                 * bpb->sector_size;
    dprintf("Root Dir Pos: 0x%08x\n", bpb->root_dir_pos);
}

void ReadBPB(int fp, bpb_t *bpb)
{
    //bpb32_t work;
    char    work[512];       /* sector size */
    bpb32_t *bpb32 = (bpb32_t *)work;
    bpb16_t *bpb16 = (bpb16_t *)work;
    int     i;
    uint64  curaddr;
    FSINFO  fsinfo;

    char    wk[256];
    //int     fat_type = 0;  /* 12/16/32/0(NTFS) */

    memset(work, 0, sizeof(work));

    /* read BPB */
    /* win32ではセクタサイズの倍数で読まないとInvalid Argとなる */
    if (read(fp, work, sizeof(work)) < 0) {
	perror("read BPB");
	exit(1);
    }
    //if (vf) DumpData(fp, 0, 256);

    /* set BPB */
    memset(bpb, 0, sizeof(bpb_t));
    SetBPB((bpb32_t *)work, bpb);

    if (bpb->fat_type != 0) {
        printf("BPB FAT%d size:%d (0x%02x)\n", bpb->fat_type,
               (bpb->fat_type==32)?sizeof(bpb32_t):sizeof(bpb16_t),
               (bpb->fat_type==32)?sizeof(bpb32_t):sizeof(bpb16_t));
    } else {
        printf("BPB NTFS\n");
    }
    printf("----------------------------------\n");
    printf("+03 OEM Label      : [%s]\n", STRNCPY(wk, bpb32->oem_label, 8));
    printf("+0b Sector Size    : [%d]\n", GetValue(bpb32->s_sector_size, 2));
    printf("+0d Sectors/Clustor: [%d] Clustor Size:%d\n",
        bpb32->sects_per_clust,
	bpb->clustor_size);
    if (bpb->fat_type) {
        printf("+0e FAT Start Sect : [%d] +0x%04x\n",
               GetValue(bpb32->s_reserve_sect, 2),
               GetValue(bpb32->s_reserve_sect, 2)
	        * GetValue(bpb32->s_sector_size, 2));
        printf("+10 Num of FAT     : [%d]\n", bpb32->num_of_fat);
        printf("+11 Max Root Dirent: [%d] (FAT12/16)\n",
               GetValue(bpb32->s_max_root_dirent,2));
        printf("+13 Total Sectors1 : [%d] (FAT12/16)\n",
               GetValue(bpb32->s_total_sectors, 2));
    }

    printf("+15 Media Desc     : [0x%02x] %s\n", bpb32->media_desc,
           (bpb32->media_desc==0xf8)?"HDD":"other");

    if (bpb->fat_type) {
        printf("+16 FAT Sectors    : [%d] 0x%04x (FAT12/16)\n",
               GetValue(bpb32->s_fat_sectors, 2),
               GetValue(bpb32->s_fat_sectors, 2)
	        * GetValue(bpb32->s_sector_size, 2));
    }
    printf("+18 Sector/Track   : [%d]\n", GetValue(bpb32->s_sect_per_track, 2));
    printf("+1a Num of Head    : [%d]\n", GetValue(bpb32->s_num_of_head, 2));
    printf("+1c Secret Sectors : [%d] 0x%04x\n",
           GetValue(bpb32->i_secret_sects, 4),
           bpb->secret_size);

    if (bpb->fat_type == 0) {
	return;  /* NTFS */
    }

    printf("+20 Total Sectors2 : [%u] %dMB (FAT32)\n",
           GetValue(bpb32->i_total_sectors, 4),
           GetValue(bpb32->i_total_sectors, 4)/1024
	    /(1024/GetValue(bpb32->s_sector_size, 2)));

    if (bpb->fat_type != 32) {
	/* FAT12/16 */
        printf("\n");
        printf("+24 Phys Drive Num : [0x%02x] %s\n", bpb16->pdrive_num,
           (bpb16->pdrive_num==0x80)?"HDD":"other");
        printf("+26 Boot Signature : [0x%02x] %s\n", bpb16->boot_sign,
           (bpb16->boot_sign==0x29)?"Exist Vol Serial":"No Vol Serial");
        printf("+27 Volume Serial  : [%d]\n",
           GetValue(bpb16->i_vol_serial, 4));
        printf("+2b Volume Label   : [%s]\n",
           STRNCPY(wk, bpb16->vol_label, 11));
        printf("+36 Filesystem Type: [%s]\n",
           STRNCPY(wk, bpb16->filesys_type, 8));
    } else {
	/* FAT32 */
        printf("--- FAT32 data start ---\n");
        printf("+24 Sectors/FAT    : [%d] 0x%08x\n",
           GetValue(bpb32->i_sect_per_fat, 4),
           GetValue(bpb32->i_sect_per_fat, 4)
	    * GetValue(bpb32->s_sector_size, 2));
        printf("+28 Media Desc Flag: [0x%04x]\n",
           GetValue(bpb32->s_media_desc, 2));
        printf("+2a Filesystem Ver : [%d]\n",
           GetValue(bpb32->s_filesys_ver, 2));
        printf("+2c RootDir Clust  : [%d] +0x%08x  FAT Addr:0x%08x\n",
           GetValue(bpb32->i_root_clust, 4),
           GetValue(bpb32->i_root_clust, 4)
	     * bpb32->sects_per_clust
	     * GetValue(bpb32->s_sector_size, 2),
	     bpb->fat_start_pos);
        printf("+30 FSINFO Sector  : [%d] +0x%08x\n",
           GetValue(bpb32->s_fsinfo_sect, 2),
           GetValue(bpb32->s_fsinfo_sect, 2)
	     * GetValue(bpb32->s_sector_size, 2));
        printf("+32 Boot Copy Sect : [%d] +0x%08x\n",
           GetValue(bpb32->s_boot_sect_copy, 2),
           GetValue(bpb32->s_boot_sect_copy, 2)
	     * GetValue(bpb32->s_sector_size, 2));
        printf("--- FAT32 data end ---\n");
        printf("+40 Phys Drive Num : [0x%02x] %s\n", bpb32->pdrive_num,
           (bpb32->pdrive_num==0x80)?"HDD":"other");
        printf("+42 Boot Signature : [0x%02x] %s\n", bpb32->boot_sign,
           (bpb32->boot_sign==0x29)?"Exist Vol Serial":"No Vol Serial");
        printf("+43 Volume Serial  : [%d]\n",
           GetValue(bpb32->i_vol_serial, 4));
        printf("+47 Volume Label   : [%s]\n",
           STRNCPY(wk, bpb32->vol_label, 11));
        printf("+52 Filesystem Type: [%s]\n",
           STRNCPY(wk, bpb32->filesys_type, 8));
        printf("--- Etc --------------\n");
        printf("Clustor 0 addr     : 0x%08x (%d)\n",
           (uint)CLUST2POS(bpb, 0), (uint)CLUST2POS(bpb, 0));
    }

    /* read FSINFO */
    if (bpb->fsinfo_sect) {
	curaddr = ((uint64)bpb->fsinfo_sect) * bpb->sector_size;
	lseek64(fp, curaddr, SEEK_SET);
        read(fp, &fsinfo, sizeof(FSINFO));
	printf("\n--- FSINFO ---\n");
	printf("+%04x Free Clustors    : 0x%08x (%d) %uMB\n",
	    (uint)curaddr + offsetof(FSINFO, i_free_clusts),
	    GetValue(fsinfo.i_free_clusts, 4),
	    GetValue(fsinfo.i_free_clusts, 4), 
	    GetValue(fsinfo.i_free_clusts, 4)*(bpb->clustor_size/1024)/1024);
	printf("+%04x Last mod Clustor : 0x%08x\n",
	    GetValue(fsinfo.i_last_mod_clust, 4));
    }

    /* disp map */
    if (mapf) {
	printf("--- MAP ---\n");
	printf("+%08x +----------------+ (+%08x)\n",
		0, bpb->secret_size);
	printf("          |BPB             |\n");
	printf("          +----------------+\n");

	/* FAT area start */
	for (i = 0; i < bpb->num_of_fat; i++) {
	    curaddr = (bpb->fat_start_sect + i*bpb->fat_sectors)
		        *bpb->sector_size;
	    printf("+%08x +----------------+ (+%08x)\n",
		(uint)curaddr, (uint)curaddr + bpb->secret_size);
	    printf("          |FAT%-2d(%08x) |\n", i+1,
	        bpb->fat_sectors*bpb->sector_size);
	}
	curaddr = (bpb->fat_start_sect + i*bpb->fat_sectors)*bpb->sector_size;
        printf("+%08x +----------------+ (+%08x) <- clustor 2\n",
		(uint)curaddr, (uint)curaddr + bpb->secret_size);
	/* FAT area end   */

	if (bpb->fat_type == 32) {
	    //curaddr = bpb->root_clust*bpb->sects_per_clust*bpb->sector_size;
	    curaddr = bpb->root_dir_pos;
	    printf("          |        :       |\n");
	    printf("+%08x +----------------+ (+%08x) <- clustor %d\n",
		(uint)curaddr, (uint)curaddr + bpb->secret_size,
		bpb->root_clust);
	    printf("          |ROOTdir         |\n");
            printf("          +----------------+\n");
	} else {
	    printf("          |ROOTdir(%06x) |\n",
		bpb->max_root_dirent*32);

	    curaddr += bpb->max_root_dirent*32;
	    printf("+%08x +----------------+ (+%08x)\n",
		(uint)curaddr, (uint)curaddr + bpb->secret_size);
	}

        printf("          |Data Area       |\n");
        printf("          +----------------+\n");
    }

#if 0
    if (dumpf) {
	/* dump dirent */
	printf("--- Root DirEnt Area Dump ---\n");
        DumpData(fp, bpb->root_dir_pos, 32*10);
    }

    /* List Root DirEnt */
    if (dirf) {
	printf("--- DirEnt ---\n");
        ListDirEnt(fp, bpb, bpb->root_clust, 
                   bpb->max_root_dirent?
		     (bpb->max_root_dirent * 32): /* FAT12/16         */
		     bpb->clustor_size);          /* FAT32: 1 clustor */
    }
#endif
}

/*
 *  get next clustor number (support only FAT16/32)
 *
 *   IN  fp    : infile
 *   IN  bpb   : bpb
 *   IN  clust : clustor number
 *   OUT ret   : >=0: target clustor's FAT value  -1: error
 */
int GetFatValue(int fp, bpb_t *bpb, uint clust)
{
    off64_t fatpos    = 0;
    int     fatpage   = 0;
    int     valuepos  = 0;
    int     size;

    ushort  *fat16val = NULL;
    uint    *fat32val = NULL;

    if (bpb->fat_type != 16 && bpb->fat_type != 32) {
        printf("GetFileValue: Unsupported FAT Type %d\n",bpb->fat_type);
	return -1;
    }

    fatpage = (clust * (bpb->fat_type/8)) / PAGE_SIZE;
    fatpos  = ((off64_t)bpb->fat_start_pos) + fatpage * PAGE_SIZE;
    if (fatpage != cur_fatpage) {
	/* cache miss! */
	printf("GetFatValue: fatpage=%d fat read addr=0x%08x\n",
		fatpage, (uint)fatpos);
        if (lseek64(fp, fatpos, SEEK_SET)<0){
	    printf("GetFileValue: lseek64 error. errno=%d\n",errno);
	    return -1;
        } 
	/* read FAT */
        if ((size = read(fp, fatdata, sizeof(fatdata))) < sizeof(fatdata)) {
	    printf("GetFileValue: read error. errno=%d size=%d/%d\n",
		    errno, size, sizeof(fatdata));
	    return -1;
	}
	cur_fatpage = fatpage;
    }
    valuepos = (clust * (bpb->fat_type/8)) % PAGE_SIZE;
    if (bpb->fat_type == 32) {
	fat32val = (uint *)&fatdata[valuepos];
        dprintf("GetFileValue: valuepos=%d fat32val=%d\n",valuepos, *fat32val);
	return (int)*fat32val;
    } else {
	fat16val = (ushort *)&fatdata[valuepos];
	return (int)*fat16val;
    }
}

/*
 *  get next clustor number
 *
 *   IN  fp    : infile
 *   IN  bpb   : bpb
 *   IN  clust : current clustor number
 *   OUT ret   : >0: next clustor number 0:EOF -1: error
 */
int GetNextClust(int fp, bpb_t *bpb, uint clust)
{
    int     fatval;

    fatval = GetFatValue(fp, bpb, clust);
    if (fatval < 0) {
	return -1;  /* error */
    } else if (fatval > 0) {
	/* exist next value */
	return fatval;
    }

    /* fatval == 0 (maybe deleted data) */
    /* get next */
    ++clust;
    /* search next zeor position */
    while (fatval = GetFatValue(fp, bpb, clust)) {
	if (fatval < 0) {
	    break;
	}
	++clust;
    }
    return clust;
}

/*
 *  read 1 clustor
 *
 *   IN  fp   : infile
 *   IN  bpb  : bpb
 *   IN  clust: clustor number
 *   OUT buf  : clustor data
 *   OUT ret  : 0: success  -1: error
 */
int ReadClustor(int fp, bpb_t *bpb, uint clust, char *buf)
{
    off64_t curaddr = CLUST2POS(bpb, clust);
    int     size;

    if (lseek64(fp, curaddr, SEEK_SET) < 0) {
	printf("ReadClustor: lseek64 error. errno=%d\n", errno);
	return -1;
    } 
    if ((size = read(fp, buf, bpb->clustor_size)) < bpb->clustor_size) {
	int *iaddr = (int *)&curaddr;
	printf("ReadClustor: read error. errno=%d size=%d/%d cur_addr=0x%04x %08x\n",
		errno, size, bpb->clustor_size, iaddr[1], iaddr[0]);
	return -1;
    }
    return 0;
}

/*
 *  RecoverFile
 *
 *   IN  fp    : infile
 *   IN  bpb   : bpb
 *   IN  clust : 1st clustor number of recover file
 *   IN  fsize : recover filesize
 *   IN  ofname: output filename 
 *   OUT ret   : 0: success  -1: error
 */
int RecoverFile(int fp, bpb_t *bpb, uint clust, uint fsize, char *ofname)
{
    uint fat_addr;
    char *buf;
    FILE *ofp = NULL;
    int  clustsize = bpb->clustor_size;
    int  ret;
    int  result = 0;
    uint cur_clust = clust;
    uint sizesv    = fsize;

    if ((ofp = fopen(ofname, "wb")) == NULL) {
        perror(ofname);
	return -1;
    } 

    buf = (char *)malloc(bpb->clustor_size);

    if (vf == 0) printf("recover clustor %d ...\n", cur_clust);
    while (1) {
	dprintf("recover clustor %d ...\n", cur_clust);
	ret = ReadClustor(fp, bpb, cur_clust, buf);
        if (ret) {
	    printf("RecoverFile: ReadClustor error.\n");
	    result = -1;
	    break;
	}

	dprintf("RevoerFile: write...\n", cur_clust);
	ret = fwrite(buf, 1, (fsize >= clustsize)?clustsize:fsize, ofp);
	fsize -= ret;
	if (fsize <= 0) break;  /* end */

	cur_clust = GetNextClust(fp, bpb, cur_clust);
        if (cur_clust < 0) {
	    result = -1;
	    break;
	} else if (cur_clust == 0) {
	    /* EOF */
	    break;
	}
    }

    if (ofp) {
	fclose(ofp);
    }

    if (result == 0) {
	printf("clustor %d data output to %s size=%d\n", clust, ofname, sizesv);
    }

    if (buf) free(buf);
    return result;
}

/* V0.13-A start */
/* Search Directory Clustor */
void SearchDirEnt(int fp, bpb_t *bpb)
{
    uint  max_clustor;    /* num of clustor */
    uint  clust = 0;
    int   dir_cnt = 0;
    char  *buf = NULL;
    char  *isDeleted;
    off64_t max_addr;
    off64_t clust2_addr;  /* clustor #2 addr */
    dirent_short_t *sdirent;

    max_addr    = ((off64_t)bpb->total_sectors)*bpb->sector_size;
    clust2_addr = (((off64_t)bpb->fat_start_sect)
	            + bpb->num_of_fat * bpb->fat_sectors)
	              * bpb->sector_size;

    max_clustor = (uint)((max_addr - clust2_addr)/bpb->clustor_size);
    printf("## max_clustor = %u\n", max_clustor);

    buf = (char *)malloc(bpb->clustor_size);
    sdirent = (dirent_short_t *)buf;

    for (clust = 2; clust < max_clustor + 1; clust++) {
	if (ReadClustor(fp, bpb, clust, buf) < 0) {
            printf("ReadClustor Error. clust=%u max_clustor=%u\n", 
		    clust, max_clustor);
	    break;
	}
	/* check dir ent */
	if (buf[0] == '.' && buf[0x20] == '.' && buf[0x21] == '.'
	      && (sdirent[0].attr & ENT_ATTR_DIR)
	      && (sdirent[1].attr & ENT_ATTR_DIR)) {

	    ++dir_cnt;
	    isDeleted = "";
            if (GetFatValue(fp, bpb, clust) == 0) {
		isDeleted = "(Deleted) ";
	    }
	    printf("--- #%d Clustor %d DirEnt %s---\n",
		    dir_cnt, clust, isDeleted);
            ListDirEnt(fp, bpb, clust, bpb->clustor_size);
	}
    }

    if (buf) free(buf);
}
/* V0.13-A end   */

void usage()
{
    char *dev_name = "<dev_name>";  /* ex. /dev/hda1 */

#ifdef _WIN32
    dev_name = "<drv_name>";        /* ex. c:        */
#endif
    printf("hdinfo ver %s\n", VERSION);
    printf("usage: hdinfo [-m] [-d] [-dump] [-c <clust> [-o <ofile> -s <osize>]] %s\n", dev_name);
    printf("usage: hdinfo [-ds] %s\n", dev_name);
    printf("       -m    : disp map\n");
    printf("       -d    : disp dirent\n");
    printf("       -ds   : dirent search\n");
    printf("       -dump : disp dirent dump\n");
    printf("       -c    : clustor dump\n");
    printf("       -o    : output to file   (recovery)\n");
    printf("       -s    : output file size (recovery)\n");
    exit(1);
}

int main(int a, char *b[])
{
    int   fp;
    bpb_t bpb;              /* BPB common */
    char  *devname = NULL;  /* device name (ex. /dev/hda1) */
    int   i;
    char  *ofname = NULL;   /* -o output file name */
    uint  ofsize = 0;       /* -s output file size */
#ifdef _WIN32
    char  devname_wk[256];  /* device name for win32 */
#endif

    // printf("bpb32_t=%d bpb16_t=%d bpb_t=%d\n", sizeof(bpb32_t), sizeof(bpb16_t), sizeof(bpb_t));

    for (i = 1; i < a; i++) {
	if (!strncmp(b[i], "-m", 2)) {
	    mapf = 1;
	    continue;
	}
	if (!strcmp(b[i], "-d")) {
	    dirf = 1;
	    continue;
	}
	if (!strcmp(b[i], "-ds")) {   /* V0.13-A */
	    dsf = 1;
	    continue;
	}
	if (!strcmp(b[i], "-dump")) {
	    dumpf = 1;
	    continue;
	}
	if (i+1 < a && !strcmp(b[i], "-c")) {
	    clustf = atoi(b[++i]);
	    continue;
	}
	if (i+1 < a && !strcmp(b[i], "-o")) {
	    ofname = b[++i];
	    continue;
	}
	if (i+1 < a && !strcmp(b[i], "-s")) {
	    ofsize = atoi(b[++i]);
	    continue;
	}
	if (!strcmp(b[i], "-l")) {  /* V0.14-A */
	    lf = 1;
	    continue;
	}
	if (!strcmp(b[i], "-v")) {
	    ++vf;
	    continue;
	}
	if (!strcmp(b[i], "-h")) {
	    usage();
	}
	devname = b[i];
    }

    /* check arg */
    if (devname == NULL) {
        usage();
    }
    if ((ofname && (clustf == 0 || ofsize == 0)) ||
	(ofname == NULL && ofsize)) {
        usage();
    }

#ifdef _WIN32
    if (strlen(devname) != 2 || devname[1] != ':') {
	usage();
    }
    sprintf(devname_wk, "\\\\.\\%s", devname);
    devname = devname_wk;  /* "\\\\.\\x:" */
#endif

    /* open device */
    if ((fp = open64(devname, O_RDONLY)) < 0) {
	perror(devname);
	exit(1);
    }

    ReadBPB(fp, &bpb);

    /* check -o */
    if (clustf && ofname) {
        RecoverFile(fp, &bpb, clustf, ofsize, ofname);
	if (fp) close(fp);
	exit(0);
    }

    if (clustf) {
	if (dirf) {
	    printf("--- Clustor %d DirEnt ---\n", clustf);
	    ListDirEnt(fp, &bpb, clustf, bpb.clustor_size);
	} else {
	    printf("--- Clustor %d Area Dump ---\n", clustf);
            DumpData(fp, CLUST2POS(&bpb, clustf), bpb.clustor_size);
        }
    } else {
        if (dumpf) {
	    /* dump dirent */
	    printf("--- Root DirEnt Area Dump (clust:%d) ---\n", bpb.root_clust);
            DumpData(fp, bpb.root_dir_pos, 32*10);
        }

        /* List Root DirEnt */
        if (dirf || dsf) { /* V0.13-C */
	    printf("--- Root DirEnt (clust:%d) ---\n", bpb.root_clust);
            ListDirEnt(fp, &bpb, bpb.root_clust, 
                       bpb.max_root_dirent?
		         (bpb.max_root_dirent * 32): /* FAT12/16         */
		         bpb.clustor_size);          /* FAT32: 1 clustor */
        }

        /* V0.13-A start */
	if (dsf) {
	    /* Search Directory Clustor */
	    SearchDirEnt(fp, &bpb);
	}
        /* V0.13-A end   */
    }

    if (fp) close(fp);
    return 0;
}

/* vim:ts=8:sw=4:
 */

