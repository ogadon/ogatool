/*
 *  .WAV cutter
 *
 *    05/10/18 V0.10 support blank cut by oga.
 *    05/10/28 V0.11 support -ex
 *    05/11/05 V0.20 support -no (normalize)
 *    05/11/13 V0.21 perf up 29.51sec=>7.93sec (41.9MB wav, K6/175MHz)
 *    05/12/25 V0.22 normalize 110% (test)
 *    06/02/01 V0.23 support -noX normalize 1X0%
 *    07/03/11 V0.24 support -rv reduce voice part
 *    07/10/07 V0.25 support -vol volume
 *    07/11/18 V0.26 fix riff_len bug
 *    07/12/02 V0.27 support -m (merge wavs)
 *    11/01/29 V0.28 fix display bug
 *    12/03/19 V0.29 fix -ex degrade  (調査中 ex. -ex 00:05-21:50) 
 *    21/05/02 V0.30 support -cs cut silent part
 *    21/05/03 V0.31 fix LIST chunk bug
 *    21/05/08 V0.32 support 64bit Linux
 *
 *
 *    お勧め: wavcut -s -lv 20 -ln 15
 */

#if 0
    typical wav format
    ---- RIFF Header ----
    riff_id   : [RIFF]
    riff_len  : 0x0017d93c (1562940)
    ---- Chunk Header ----
    chunk_id  : [fmt ]
    chunk_len : 0x00000010 (16)
    ---- fmt chunk (len=16) ----
    FormatTag      : 0x0001     (1) MS PCM
    Channels       : 0x0002     (2) Stereo
    SamplesPerSec  : 0x0000ac44 (44100)
    AvgBytesPerSec : 0x0002b110 (176400)
    BlockAlign     : 0x0004     (4)
    BitsPerSample  : 0x0010     (16 bits)
    ---- Chunk Header ----
    chunk_id  : [data]
    chunk_len : 0x0017d918 (1562904)
#endif

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define strcasecmp stricmp
#define strncasecmp strnicmp
#endif /* _WIN32 */

#define VER      "0.32"
#define dprintf if (vf) printf
#define dprintf2 if (vf >= 2) printf
#define sgn(x) (x==0)?0:((x>0)?1:-1)

int  vf     = 0;    /* -v verbose                                     */
int  df     = 0;    /* -d disp all data value                         */
int  sf     = 0;    /* -s split wav data                              */
int  th_val = 60;   /* -lv threshold value (blank level) default:6.0% */
int  bl_sec = 10;   /* -ln time to recognize blank       default:1.0s */
char in_fname[1024]; /* in wav filename (without suffix)              */
int  *start_dsec;   /* start dsecs */
int  *end_dsec;     /* end   dsecs */
int  ntment = 0;    /* -ex num of time data                           */
int  nf     = 0;    /* -no normalize                                  */
int  volf   = 0;    /* -vol volume                            V0.25-A */
int  rvf    = 0;    /* -rv reduce voice V0.24-A                       */
int  mf     = 0;    /* -m  merge wavfiles                     V0.27-A */
int  csf    = 0;    /* -cs cut silent part                    V0.30-A */
int  peak_level = 0;  /* peak level for -no                           */
int  max_level  = 0;  /* max  level for -no                           */
/* long pre_datlen = 0;  heders total length to wav data              */
int  pre_datlen = 0;  /* heders total length to wav data      V0.32-C */

int  gBytePerSample = 0; /* Byte / Sample                             */
int  gChannels      = 0; /* Num of Channel                            */
int  gUnit          = 0; /* Byte / Sample * nChannel                  */


/* data formats */
struct riff_hdr {
    char          id[4];           /* RIFF id   ("RIFF")        */
    unsigned int  len;             /* length            V0.32-C */
    char          wave_id[4];      /* data type ("WAVE")        */
};

struct chunk_hdr {
    char          id[4];           /* chunk id  ("fmt "|"data"...) */
    unsigned int  len;             /* chunk length         V0.32-C */
};

/* chunk type fmt  1 */
struct ck_fmt {
    unsigned short wFormatTag;        /* Format category       */
#define WAVE_FORMAT_UNKNOWN	0x0000  /* unknown                          */
#define WAVE_FORMAT_PCM		0x0001  /* Microsoft PCM                    */
#define WAVE_FORMAT_ADPCM	0x0002  /* Microsoft ADPCM                  */
#define WAVE_FORMAT_ALAW   	0x0006  /* A-Law                            */
#define WAVE_FORMAT_MULAW   	0x0007  /* mu-Law                           */
#define WAVE_FORMAT_IDADPCM   	0x0011  /* IMA/DVI ADPCM                    */
#define WAVE_FORMAT_TRUESPEECH 	0x0022  /* TrueSpeech                       */
#define WAVE_FORMAT_GSM        	0x0031  /* TrueSpeech                       */
#define IBM_FORMAT_MULAW	0x0101  /* IBM mu-law format                */
#define IBM_FORMAT_ALAW		0x0102  /* IBM a-law format                 */
#define IBM_FORMAT_ADPCM	0x0103  /* IBM AVC Adaptive Diff PCM format */
    unsigned short wChannels;         /* Number of channels    */
    unsigned int   dwSamplesPerSec;   /* Sampling rate         V0.32-C */
    unsigned int   dwAvgBytesPerSec;  /* For buffer estimation V0.32-C */
    unsigned short wBlockAlign;       /* Data block size       */
    unsigned short wBitsPerSample;    /* Sample size (for WAVE_FORMAT_PCM */
    unsigned short unknown;           /* Unknown data                     */
    char dummy[256];                  /* dummy area for over run */
};

/* chunk type fact 2 */
struct ck_fact {
    int  unknown;		/* unknown */
};

/* chunk type data 3 */
/* any length data */

/*
 *  Dump Data
 *
 *   IN  buf : dump buffer
 *       len : dump length
 */
void DumpData(unsigned char *buf, int len)
{
    int i;

    for (i=0; i<len; i++){
        printf(" %02x",buf[i]);
    }
    printf("\n");
}

/*
 *  Read fmt chunk header block
 *
 *   IN  fp
 *   IN  cklen : chunk data len
 *   OUT ckfmt : fmt chunk data
 *   OUT ret   :  0 success
 *               -1 error
 */
int ReadChunkFmt(FILE *fp, int len, struct ck_fmt *ckfmt)
{
    if (len > sizeof(struct ck_fmt)) {
        printf("Error: ReadChunkFmt: Length Too long. (%d)\n",len);
        return -1;
    }

    /* read wav format chunk */
    if (vf) printf("ck_fmt size = %d\n", sizeof(struct ck_fmt));
    fread(ckfmt, 1, len, fp);	/* get fmt data */
    printf("---- fmt chunk (len=%d) ----\n", len);
    printf("FormatTag      : 0x%04x     (%d) %s\n",ckfmt->wFormatTag, ckfmt->wFormatTag, 
            (ckfmt->wFormatTag == WAVE_FORMAT_PCM)?"MS PCM":"Other PCM");
    printf("Channels       : 0x%04x     (%d) %s\n",ckfmt->wChannels, ckfmt->wChannels,
            (ckfmt->wChannels == 1)?"Mono":"Stereo");
    printf("SamplesPerSec  : 0x%08x (%d)\n",ckfmt->dwSamplesPerSec, ckfmt->dwSamplesPerSec);
    printf("AvgBytesPerSec : 0x%08x (%d)\n",ckfmt->dwAvgBytesPerSec, ckfmt->dwAvgBytesPerSec);
    printf("BlockAlign     : 0x%04x     (%d)\n",ckfmt->wBlockAlign, ckfmt->wBlockAlign);
    printf("BitsPerSample  : 0x%04x     (%d bits)\n",ckfmt->wBitsPerSample, ckfmt->wBitsPerSample);
    if (len > 16) {
        printf("unknown        : 0x%04x     (%d)\n",ckfmt->unknown, ckfmt->unknown);
    }
    return 0;
}

/*
 *  Read fact chunk header block
 *
 *   IN  fp
 *   IN  cklen  : chunk data len
 *   OUT ckfact : fact chunk data
 *   OUT ret    :  0 success
 *                -1 error
 */
int ReadChunkFact(FILE *fp, int len, struct ck_fact *ckfact)
{
    if (len > sizeof(struct ck_fact)) {
        printf("Error: ReadChunkFact: Length Too long. (%d)\n",len);
        return -1;
    }

    /* read wav format chunk */
    if (vf) printf("ck_fact size = %d\n", sizeof(struct ck_fact));
    fread(ckfact, 1, len, fp);	/* get fact data */
    printf("---- fact chunk (len=%d) ----\n", len);
    printf("unknown        : 0x%08x (%d)\n",ckfact->unknown, 
                                            ckfact->unknown);
    return 0;
}

/*
 *  Analyze chunk header block
 *
 *   IN  fp
 *   OUT ckid  : chunk id
 *   OUT cklen : chunk data len
 *   OUT ret   :  0 success
 *               -1 EOF or error
 */
int ReadChunkHeader(FILE *fp, char *ckid, int *cklen, struct chunk_hdr *chunkhdr)
{
    if (vf) printf("chunk_hdr size = %d\n", sizeof(struct chunk_hdr));
    if (fread(chunkhdr, 1, sizeof(struct chunk_hdr), fp) < sizeof(struct chunk_hdr)) {
        /* EOF or Error or Format Error */
        return -1;
    }
    printf("---- Chunk Header ----\n");
    strncpy(ckid, chunkhdr->id, 4);
    ckid[4] = '\0';
    *cklen  = chunkhdr->len;
    printf("chunk_id  : [%s]\n", ckid);
    printf("chunk_len : 0x%08x (%d)\n", *cklen, *cklen);

    return 0;
}

/*
 *  Analyze WAV(RIFF) header block
 *
 *   IN  fp
 *   OUT ret :  0 success
 *             -1 error
 */
int ReadWavHeader(FILE *fp, struct riff_hdr *riffhdr)
{
    char wk[4096];
    int  len;

    /* read riff header */
    if (vf) printf("riffhdr size = %d\n", sizeof(struct riff_hdr));
    fread(riffhdr, sizeof(struct riff_hdr), 1,  fp);	/* get header */
    strncpy(wk, riffhdr->id, 4);
    wk[4] = '\0';
    if (strcmp(wk, "RIFF")) {
        /* not WAV file */
        printf("Error : Not RIFF [%s]\n",wk);
        return -1;
    }

    printf("---- RIFF Header ----\n");
    printf("riff_id   : [%s]\n", wk);
    printf("riff_len  : 0x%08x (%d)\n", riffhdr->len, riffhdr->len);

    return 0;
}

/*
 *  Analyze WAV header block
 *
 *   IN  fp
 *   OUT ret :  0 success
 *             -1 error
 */
int ReadWavData(FILE *fp)
{
    char wk[4096];
    int  len;
    int  i, c;

    for (i=0; i<10; i++) {
        len = fread(wk, 1, 16, fp);
        DumpData(wk, len);
    }
    return 0;
}

/*
 *  Write wav headers
 *
 *    note: not write fact,LIST chunk
 */
int WriteWavHeaders(FILE *wfp,
                    struct riff_hdr *rhdr,
		    struct chunk_hdr *cnkhdr, int cnkcnt,
		    struct ck_fmt *cnkfmt,
		    struct ck_fact *cnkfct)
{
    int i;
    char buf[16];

    dprintf("## write RIFF header ...\n");

    fwrite(rhdr, sizeof(struct riff_hdr), 1, wfp);

    for (i = 0; i<cnkcnt; i++) {
        if (strncmp(cnkhdr[i].id, "fact", 4) && strncmp(cnkhdr[i].id, "LIST", 4)) { /* V0.31-C */
	    /* write excpt fact,LIST chunk (fmt, data...) */
            fwrite(&cnkhdr[i], sizeof(struct chunk_hdr), 1, wfp); /* chunk header */
	    if (!strncmp(cnkhdr[i].id, "fmt ", 4)) {
		fwrite(cnkfmt, cnkhdr[i].len, 1, wfp); /* fmt body   */
	    }

            /* verbose log */
	    strncpy(buf, cnkhdr[i].id, 4);
	    buf[4] = '\0';
	    dprintf("## write %s header ...\n", buf);
	}
    }
    return 0;
}

/*
 *  Change wav headers
 *
 */
int ChangeWavHeaders(FILE *wfp,
                    struct riff_hdr *rhdr,
		    struct chunk_hdr *cnkhdr, int cnkcnt,
		    struct ck_fmt *cnkfmt,
		    struct ck_fact *cnkfct,  /* no write */
		    int    data_size)
{
    int i;
    int fmt_len = 0;
    char buf[16];

    printf("## change RIFF header ...\n");

    /* Seek to head */
    fseek(wfp, 0, SEEK_SET);

    /* get fmt length & set data size */
    for (i = 0; i<cnkcnt; i++) {
        if (!strncmp(cnkhdr[i].id, "fmt ", 4)) {
	    fmt_len = cnkhdr[i].len;
	} else if (!strncmp(cnkhdr[i].id, "data", 4)) {
	    cnkhdr[i].len = data_size;
	}
    }

    /* set riffhdr length */
    rhdr->len = data_size                     /* data size */
                + sizeof(struct chunk_hdr)    /* data chunk header */
		+ fmt_len                     /* fmt  size */
		+ sizeof(struct chunk_hdr)    /* fmt  chunk header */
		+ 4;                          /* riff_hdr."WAVE" size V0.26-A */

    WriteWavHeaders(wfp, rhdr, cnkhdr, cnkcnt, cnkfmt, cnkfct);

    return 0;
}

/*
 *  Wav Analyze
 *
 *   IN  fp : input file pointer
 *
 *  [global]
 *   OUT in_fname   : input file name (without ".wav")
 *   OUT pre_datlen : Header Length
 *
 */

int WavAnalyze(FILE *fp)
{
    int  i, j;
    int  cklen = 0;      /* chunk len */
    char ckid[256];      /* chunk id  */
    unsigned char rbuf[4096];   /* buf for fread() V0.21-A */
    unsigned char *buf = rbuf;  /* dummy buffer    V0.21-C */
    char buf_fmt[4096];
    char buf_fact[4096];
    char fname[1024];
    int  maxval[16];     /* Max Power       16ch */
    int  maxvalcnt[16];  /* Max Power count 16ch */
    int  lowcont   = 0;  /* low level continue count */
    int  contf     = 0;  /* blank continue flag      */
    int  outf      = 0;  /* output flag for -ex      */
    int  reopen    = 0;  /* reopen flag for -ex (end=next start) */
    int  curdsec   = 0;  /* current dsec             */

    struct riff_hdr riffhdr;        /* 01 */
    struct chunk_hdr chunkhdr[10];  /* 02 */
    int    chunk_cnt = 0;
    struct ck_fmt  *ckfmt  = (struct ck_fmt *)buf_fmt;   /* 03 */
    struct ck_fact *ckfact = (struct ck_fact *)buf_fact; /* 04 */

    /* initialize */
    for (i = 0; i<16; i++) {
        maxval[i]    = 0;
        maxvalcnt[i] = 0;
    }

    printf("\n# Analyze WAV file.  by oga.\n");

    if (ReadWavHeader(fp, &riffhdr)) {
        printf("This is not WAV(RIFF) format file.\n");
        exit(1);
    }

    while (!ReadChunkHeader(fp, ckid, &cklen, &chunkhdr[chunk_cnt++])) {
        if (!strncmp(ckid, "fmt", 3)) {
            ReadChunkFmt(fp, cklen, (struct ck_fmt *)buf_fmt);
        } else if (!strncmp(ckid, "fact", 4)) {
            ReadChunkFact(fp, cklen, (struct ck_fact *)buf_fact);
        } else if (!strncmp(ckid, "LIST", 4)) {
            printf("---- LIST chunk (len=%d) ----\n", cklen);
            printf("LIST:[");
	    for (i = 0; i<cklen; i++) {
                if (fread(buf, 1, 1, fp) <= 0) {
		    break;
		}
		if (buf[0] < 0x20) {
                    printf("<0x%02x>", buf[0]);
		} else {
                    printf("%c", buf[0]);
		}
	    }
            printf("]\n");
        } else if (!strncmp(ckid, "data", 4)) {
            /* PCM Data */
            int total = 0;  /* total data chunk size */
            int rlen  = 0;  /* read len             V0.21-A */
            int len   = 0;  /* dummy read len       V0.21-C */
            int val;
            short *buf16 = (short *)buf;

            int file_cnt  = 0;  /* split file counter       */
            int data_size = 0;  /* data chank size          */
	    FILE *wfp = NULL;   /* write wav fp             */

	    gBytePerSample = ckfmt->wBitsPerSample/8;
	    gChannels      = ckfmt->wChannels;
	    gUnit          = gBytePerSample * gChannels;

	    pre_datlen = ftell(fp); /* get current file pointer */
	    printf("Headers Len: %d\n", pre_datlen); 

            printf("---- Cut Info ----\n");
	    printf("CutLevel: %d (%.1f%%)\n", 
	         ((1 << (ckfmt->wBitsPerSample - 1)) * th_val/1000),
		 ((float)th_val)/10);
	    printf("BlankSec: %.1fsec\n", ((float)bl_sec)/10);

	    outf = 0;
	    curdsec = total/(ckfmt->dwAvgBytesPerSec/10);
	    for (i = 0; i<ntment; i++) {
	        if (start_dsec[i] <= curdsec && curdsec < end_dsec[i]){
		    outf = 1;
		}
	    }

	    if (sf || csf || (ntment && outf)) {  /* V0.30-A */
                /* ### Open Output File */
	        sprintf(fname, "%s_%03d.wav", in_fname, file_cnt++);
	        printf("## Writing1 %s ...\n", fname);
	        wfp = fopen(fname, "wb");
	        if (wfp == NULL) {
	            perror(fname);
		    exit(1);
	        }
                /* ### WriteWavHeaders */
	        WriteWavHeaders(wfp,
	                    &riffhdr,              /* 01 riff header        */
	                    chunkhdr, chunk_cnt,   /* 02 each chunk headers */
			    ckfmt,                 /* 03 fmt data           */
			    ckfact);               /* 04: no write          */
	    }

	    data_size = 0;

            while ((rlen = fread(rbuf, 1, gUnit*50, fp)) > 0) {   /* V0.21-C */
	      for (j = 0; j < rlen; j+=gUnit) {                   /* V0.21-A */
	        len = gUnit;                                      /* V0.21-A */
	        buf = &rbuf[j];                                   /* V0.21-A */
                buf16 = (short *)buf;                             /* V0.21-A */

		if (wfp) {
		    if (csf && contf) {  /* V0.30-A */
			/* -cs指定でblank中は書き込みスキップ */
		    } else {
		    	fwrite(buf, 1, gUnit, wfp);
		    	data_size += len;   /* data chunk size for write */
		    }
		}

                if (df) {
                    printf("%08x: ", total);
                    DumpData(buf, len);
                }

                /* current time */
	        if (df) {
		    printf("%02d:%05.2f ", total/ckfmt->dwAvgBytesPerSec/60,
	            ((float)(total-(total/ckfmt->dwAvgBytesPerSec/60)*ckfmt->dwAvgBytesPerSec*60)/ckfmt->dwAvgBytesPerSec));
		}

                for (i = 0; i < gChannels; i++) {
                    if (gBytePerSample == 1) {
                        /* 1 byte */
                        val = buf[i * gBytePerSample] - 0x80;
                    } else {
                        /* 2 byte gBytePerSample == 2 */
                        /* val = buf16[i * gBytePerSample]; */
                        val = buf16[i];
                    }
                    if (df) printf("ch%d:%6d  ", i+1, val);

		    if (maxval[i] < abs(val)) {
		        maxval[i] = abs(val);   /* new max  */
			maxvalcnt[i] = 1;  /* reset    */
		    } else if (maxval[i] == abs(val)) {
		        ++maxvalcnt[i];    /* count up */
	            }
                }
                if (df) printf("\n");

                /* sound blank check for -s */
                if (abs(val) < 
		    ((1 << (ckfmt->wBitsPerSample - 1)) * th_val/1000)) {
		    /* blank level */
		    //printf("%8d: th_val...%d\n", total,  
		    //     ((1 << (ckfmt->wBitsPerSample - 1)) * th_val/1000));
		    ++lowcont;
		    if (contf == 0 && lowcont > (ckfmt->dwSamplesPerSec*bl_sec)/10) {
		        /* 1sec continue... maybe blank area */
	                printf("Blank Time Start: %02d:%05.2f\n", 
			        total/ckfmt->dwAvgBytesPerSec/60,
	                        ((float)(total-
				         (total/ckfmt->dwAvgBytesPerSec/60)
					 *ckfmt->dwAvgBytesPerSec*60)
					 /ckfmt->dwAvgBytesPerSec));
                        contf = 1;

                        if (sf && wfp) {
                            /* ### Change Each Header Size */
	                    ChangeWavHeaders(wfp,
	                        &riffhdr,            /* 01 riff header        */
	                        chunkhdr, chunk_cnt, /* 02 each chunk headers */
			        ckfmt,               /* 03 fmt data           */
			        ckfact,              /* 04: no write          */
			        data_size);          /* "data" block size     */
	                    printf("##1 %s (%02d:%05.2f)\n", fname,
			        data_size/ckfmt->dwAvgBytesPerSec/60,
	                        ((float)(data_size-
				         (data_size/ckfmt->dwAvgBytesPerSec/60)
					 *ckfmt->dwAvgBytesPerSec*60)
					 /ckfmt->dwAvgBytesPerSec));
                            /* ### Close Wav File */
			    if (wfp) fclose(wfp);
			    wfp = NULL;
			}
		    }

		} else {
		    /* no blank level */
		    lowcont = 0;
		    if (contf) {
		        contf = 0;
	                printf("Blank Time End  : %02d:%05.2f\n", 
			        total/ckfmt->dwAvgBytesPerSec/60,
	                        ((float)(total-
				         (total/ckfmt->dwAvgBytesPerSec/60)
				         *ckfmt->dwAvgBytesPerSec*60)
					 /ckfmt->dwAvgBytesPerSec));

			if (csf == 0) {      /* V0.30-A */
                            data_size = 0;   /* reset data chunk size */
			}
			if (sf) {
	                    sprintf(fname, "%s_%03d.wav", in_fname, file_cnt++);
	                    printf("## Writing2 %s ...\n", fname);
                            /* ### Open Next Wav File */
	                    wfp = fopen(fname, "wb");
	                    if (wfp == NULL) {
	                        perror(fname);
		                exit(1);
	                    }
                            /* ### WriteWavHeaders */
	                    WriteWavHeaders(wfp,
	                        &riffhdr,            /* 01 riff header        */
	                        chunkhdr, chunk_cnt, /* 02 each chunk headers */
			        ckfmt,               /* 03 fmt data           */
			        ckfact);             /* 04: no write          */
		        }
		    }
		}

		/* for -ex */
		if (ntment) {
		    outf = 0;
		    reopen = 0;
		    curdsec = total/(ckfmt->dwAvgBytesPerSec/10);
		    for (i = 0; i<ntment; i++) {
		        if (start_dsec[i] <= curdsec && curdsec < end_dsec[i]){
			    outf = 1;
		            if (curdsec == end_dsec[i]) {
			        reopen = 1;
			    }
			}
		    }
		    if (outf == 0 || reopen == 1) {
			if (wfp) {
                            /* ### Change Each Header Size */
	                    ChangeWavHeaders(wfp,
	                        &riffhdr,            /* 01 riff header        */
	                        chunkhdr, chunk_cnt, /* 02 each chunk headers */
			        ckfmt,               /* 03 fmt data           */
			        ckfact,              /* 04: no write          */
			        data_size);          /* "data" block size     */
	                    printf("##2 %s (%02d:%05.2f)\n", fname,
			        data_size/ckfmt->dwAvgBytesPerSec/60,
	                        ((float)(data_size-
				         (data_size/ckfmt->dwAvgBytesPerSec/60)
					 *ckfmt->dwAvgBytesPerSec*60)
					 /ckfmt->dwAvgBytesPerSec));
                            /* ### Close Wav File */
			    if (wfp) fclose(wfp);
			    wfp = NULL;
                            data_size = 0;   /* reset data chunk size */
			}
		    }
		    if (outf) {
			if (wfp == NULL) {
                            data_size = 0;   /* reset data chunk size */
	                    sprintf(fname, "%s_%03d.wav", in_fname, file_cnt++);
	                    printf("## Writing3 %s ...\n", fname);
                            /* ### Open Wav File */
	                    wfp = fopen(fname, "wb");
	                    if (wfp == NULL) {
	                        perror(fname);
		                exit(1);
	                    }
                            /* ### WriteWavHeaders */
	                    WriteWavHeaders(wfp,
	                        &riffhdr,            /* 01 riff header        */
	                        chunkhdr, chunk_cnt, /* 02 each chunk headers */
			        ckfmt,               /* 03 fmt data           */
			        ckfact);             /* 04: no write          */
		        }
		    }
		}

                total += len;       /* total read size           */

                if (total >= cklen) {
                    break;
                }
	      } /* 1 bufferd data loop  V0.21-A */
            } /* data chunk loop */

	    if (contf) {
	        contf = 0;
                printf("Blank Time End  : %02d:%05.2f\n", 
		        total/ckfmt->dwAvgBytesPerSec/60,
                        ((float)(total-
			         (total/ckfmt->dwAvgBytesPerSec/60)
			         *ckfmt->dwAvgBytesPerSec*60)
				 /ckfmt->dwAvgBytesPerSec));
	    }

            if (wfp) {
                /* ### Change Each Header Size */
	        ChangeWavHeaders(wfp,
	                &riffhdr,            /* 01 riff header        */
	                chunkhdr, chunk_cnt, /* 02 each chunk headers */
			ckfmt,               /* 03 fmt data           */
			ckfact,              /* 04: no write          */
			data_size);          /* "data" block size     */
                printf("##3 %s (%02d:%05.2f)\n", fname, 
		       data_size/ckfmt->dwAvgBytesPerSec/60,
	               ((float)(data_size-
	                (data_size/ckfmt->dwAvgBytesPerSec/60)
			 *ckfmt->dwAvgBytesPerSec*60)
			 /ckfmt->dwAvgBytesPerSec));
                /* ### Close Wav File */
		if (wfp) fclose(wfp);
		wfp = NULL;
	    }

            printf("---- Other Info ----\n");
	    /* ch0:Left ch1:Right */
	    printf("Max Power : ");
            for (i = 0; i < gChannels; i++) {
	        printf("ch%d:%d/%d (%d times)  ", i+1,
	                maxval[i], 1 << (ckfmt->wBitsPerSample - 1),
			maxvalcnt[i]);
                if (peak_level < maxval[i]) {
		    peak_level = maxval[i];
		}
            }
	    max_level = 1 << (ckfmt->wBitsPerSample - 1);
	    printf("\n");
	    //printf("Time      : %.2f sec\n", 
	    //        ((float)cklen)/ckfmt->dwAvgBytesPerSec);
	    printf("Time      : %02d:%05.2f\n", cklen/ckfmt->dwAvgBytesPerSec/60,
	            ((float)(cklen-(cklen/ckfmt->dwAvgBytesPerSec/60)*ckfmt->dwAvgBytesPerSec*60)/ckfmt->dwAvgBytesPerSec));
        }
    }

    printf("WAV file Analyze end.\n");
    return 0;
}


/*
 *  WavNormalize
 *
 *  IN  infp    : input wav file fp
 *  IN  peek_lv : peek level
 *  IN  max_lv  : max level
 *
 *  [global]
 *  IN  in_fname   : input file name (without ".wav")
 *  IN  pre_datlen : headers length
 *
 *  OUT "<in_fname>.normalize.wav"
 *
 */
void WavNormalize(FILE *infp, int peak_lv, int max_lv)
{
    char  ofname[1024];
    char  buf[1024];
    short *buf16 = (short *)buf;
    FILE  *ofp;
    int   size, rsize;
    int   rtotal = 0;
    int   val, newval;
    int   len;
    int   i;

    printf("Normalize: Level x %.2f (%d%%)\n", ((float)max_lv)/peak_lv, 100+10*(nf-1));
    if (volf) {
	/* change volume */
        sprintf(ofname, "%s.%d%%.wav", in_fname, volf);
    } else {
	/* normalize */
        sprintf(ofname, "%s.normalize.wav", in_fname);
    }
    
    if (pre_datlen > sizeof(buf)) {
        printf("Error: WavNormalize: headers size too big!\n");
	exit(1);
    }

    ofp = fopen(ofname, "wb");
    if (ofp == NULL) {
       perror(ofname);
       return;
    }

    /* copy headers */
    size = fread(buf, 1, pre_datlen, infp);
    if (pre_datlen != size) {
        printf("Error: WavNormalize: read size too short!\n");
	exit(1);
    }
    fwrite(buf, 1, size, ofp);

    /* normalize */
    while ((len = fread(buf, 1, gUnit, infp)) > 0)  {

        for (i = 0; i < gChannels; i++) {
            if (gBytePerSample == 1) {
                /* 1 byte */
                /*val = buf[i * gBytePerSample] - 0x80; */
                val = buf[i * gBytePerSample];
                newval = (val * max_lv) / peak_lv;

		/* check overflow V0.23 */
                if (nf > 1 && abs(newval) >= max_lv) {
                    buf[i * gBytePerSample] = sgn(newval) * (max_lv-1); /* V0.23 */
                } else {
                    buf[i * gBytePerSample] = newval;
                }
		if (vf) printf("%d => %d\n", val, buf[i * gBytePerSample]);
            } else {
                /* 2 byte gBytePerSample == 2 */
                /* val = buf16[i * gBytePerSample]; */
                val = buf16[i];
                newval = (val * max_lv) / peak_lv;

		/* check overflow V0.23 */
                if (nf > 1 && abs(newval) >= max_lv) {
                    buf16[i] = sgn(newval) * (max_lv-1); /* V0.23 */
                } else {
                    buf16[i] = newval;
                }
		if (vf) printf("%d => %d\n", val, buf16[i]);
            }
        }

	if (ofp) {
	    fwrite(buf, 1, gUnit, ofp);
	}
    } /* data chunk loop */

    fclose(ofp);
}

/* V0.24-A start */
/*
 *  ReduceVoice
 *
 *  IN  infp    : input wav file fp
 *
 *  [global]
 *  IN  in_fname   : input file name (without ".wav")
 *  IN  pre_datlen : headers length
 *
 *  OUT "<in_fname>.novoice.wav"
 *
 */
void ReduceVoice(FILE *infp)
{
    char  ofname[1024];
    char  buf[1024];
    short *buf16 = (short *)buf;
    FILE  *ofp;
    int   size, rsize;
    int   rtotal = 0;
    int   val, newval;
    int   len;
    int   i;

    sprintf(ofname, "%s.novoice.wav", in_fname);

    printf("reduce voice data output to %s ...\n", ofname);
    
    if (pre_datlen > sizeof(buf)) {
        printf("Error: ReduceVoice: headers size too big!\n");
	exit(1);
    }
    if (gChannels < 2) {
        printf("Error: ReduceVoice: not support mono data\n");
	exit(1);
    }

    ofp = fopen(ofname, "wb");
    if (ofp == NULL) {
       perror(ofname);
       return;
    }

    /* copy headers */
    size = fread(buf, 1, pre_datlen, infp);
    if (pre_datlen != size) {
        printf("Error: ReduceVoice: read size too short!\n");
	exit(1);
    }
    fwrite(buf, 1, size, ofp);

    /* reduce voice */
    while ((len = fread(buf, 1, gUnit, infp)) > 0)  {
            if (gBytePerSample == 1) {
                /* 1 byte */
                /*val = buf[i * gBytePerSample] - 0x80; */
		printf("unsupport reduce voice for 1byte/sample.\n");
#if 0
                val = buf[i * gBytePerSample];

		/* check overflow V0.23 */
                if (nf > 1 && abs(newval) >= max_lv) {
                    buf[i * gBytePerSample] = sgn(newval) * (max_lv-1); /* V0.23 */
                } else {
                    buf[i * gBytePerSample] = newval;
                }
		if (vf) printf("%d => %d\n", val, buf[i * gBytePerSample]);
#endif
            } else {
                /* 2 byte gBytePerSample == 2 */
                /* val = buf16[i * gBytePerSample]; */
		if (buf16[0] * buf16[1] > 0) {
		    if (buf16[0] > 0) {
			/* plus */
			if (buf16[0] > buf16[1]) {
			    buf16[0] = buf16[0] - buf16[1];
			    buf16[1] = 0;
			} else {
			    buf16[0] = 0;
			    buf16[1] = buf16[1] - buf16[0];
		        }
		    } else {
			/* minus */
			if (buf16[0] > buf16[1]) {
			    buf16[1] = buf16[1] - buf16[0];
			    buf16[0] = 0;
			} else {
			    buf16[1] = 0;
			    buf16[0] = buf16[0] - buf16[1];
		        }
		    }
		}
            }

	if (ofp) {
	    fwrite(buf, 1, gUnit, ofp);
	}
    } /* data chunk loop */

    fclose(ofp);
}
/* V0.24-A start */

/* V0.27-A start */
/*
 *  MergeWavs
 *
 *  IN  files    : input wav files
 *
 *  OUT "<in_file1>.merge.wav"
 *
 */
void MergeWavs(char **files)
{
    char  ofname[1024];
    FILE  *ofp, *fp;
#if 0
    char  buf[1024];
    short *buf16 = (short *)buf;
    int   size, rsize;
    int   rtotal = 0;
    int   val, newval;
    int   len;
#endif
    int   i;
    int   first = 1;
    int   fcnt = 0;
    char  *pt;

    int  cklen = 0;      /* chunk len */
    char ckid[256];      /* chunk id  */
    char   buf_fmt[4096];
    char   buf_fact[4096];
    struct riff_hdr riffhdr;        /* 01 */
    struct chunk_hdr chunkhdr[10];  /* 02 */
    int    chunk_cnt = 0;
    struct ck_fmt  *ckfmt  = (struct ck_fmt *)buf_fmt;   /* 03 */
    struct ck_fact *ckfact = (struct ck_fact *)buf_fact; /* 04 */

#if 0  /* 作成中 */
    strcpy(ofname, files[0]);
    pt = strrchr(ofname, '.');
    if (pt) {
	*pt = '\0';   /* delete suffix */
    }

    strcat(ofname, ".merge.wav");
    if (!(ofp = fopen(ofname, "wb"))) {
        perror(ofname);
        exit(1);
    }

    while (files[fcnt]) {
	if (!(fp = fopen(files[fcnt], "rb"))) {
	    perror(files[fcnt]);
	    exit(1);
	}

	if (ReadWavHeader(fp, &riffhdr)) {
	    printf("This is not WAV(RIFF) format file.\n");
	    exit(1);
	}

	while (!ReadChunkHeader(fp, ckid, &cklen, &chunkhdr[chunk_cnt++])) {
	    if (!strncmp(ckid, "fmt", 3)) {
		ReadChunkFmt(fp, cklen, (struct ck_fmt *)buf_fmt);
	    } else if (!strncmp(ckid, "fact", 4)) {
		ReadChunkFact(fp, cklen, (struct ck_fact *)buf_fact);
	    } else if (!strncmp(ckid, "LIST", 4)) {
		printf("---- LIST chunk (len=%d) ----\n", cklen);
		printf("LIST:[");
		for (i = 0; i<cklen; i++) {
		    if (fread(buf, 1, 1, fp) <= 0) {
			break;
		    }
		    if (buf[0] < 0x20) {
			printf("<0x%02x>", buf[0]);
		    } else {
			printf("%c", buf[0]);
		    }
		}
		printf("]\n");
	    } else if (!strncmp(ckid, "data", 4)) {
		/* PCM Data */
		int total = 0;  /* total data chunk size */
		int rlen  = 0;  /* read len             V0.21-A */
		int len   = 0;  /* dummy read len       V0.21-C */
		int val;
		short *buf16 = (short *)buf;

		int file_cnt  = 0;  /* split file counter       */
		int data_size = 0;  /* data chank size          */
		FILE *wfp = NULL;   /* write wav fp             */

		gBytePerSample = ckfmt->wBitsPerSample/8;
		gChannels      = ckfmt->wChannels;
		gUnit          = gBytePerSample * gChannels;

		pre_datlen = ftell(fp); /* get current file pointer */
		printf("Headers Len: %d\n", pre_datlen); 

		printf("---- Cut Info ----\n");
		printf("CutLevel: %d (%.1f%%)\n", 
		     ((1 << (ckfmt->wBitsPerSample - 1)) * th_val/1000),
		     ((float)th_val)/10);
		printf("BlankSec: %.1fsec\n", ((float)bl_sec)/10);

		outf = 0;
		curdsec = total/(ckfmt->dwAvgBytesPerSec/10);
		for (i = 0; i<ntment; i++) {
		    if (start_dsec[i] <= curdsec && curdsec < end_dsec[i]){
			outf = 1;
		    }
		}

		if (sf || (ntment && outf)) {
		    /* ### Open Output File */
		    sprintf(fname, "%s_%03d.wav", in_fname, file_cnt++);
		    printf("## Writing4 %s ...\n", fname);
		    wfp = fopen(fname, "wb");
		    if (wfp == NULL) {
			perror(fname);
			exit(1);
		    }
		    /* ### WriteWavHeaders */
		    WriteWavHeaders(wfp,
				&riffhdr,              /* 01 riff header        */
				chunkhdr, chunk_cnt,   /* 02 each chunk headers */
				ckfmt,                 /* 03 fmt data           */
				ckfact);               /* 04: no write          */
		}

		data_size = 0;

		while ((rlen = fread(rbuf, 1, gUnit*50, fp)) > 0) {   /* V0.21-C */
		  for (j = 0; j < rlen; j+=gUnit) {                   /* V0.21-A */
		    len = gUnit;                                      /* V0.21-A */
		    buf = &rbuf[j];                                   /* V0.21-A */
		    buf16 = (short *)buf;                             /* V0.21-A */

		    if (wfp) {
			fwrite(buf, 1, gUnit, wfp);
			data_size += len;   /* data chunk size for write */
		    }

		    if (df) {
			printf("%08x: ", total);
			DumpData(buf, len);
		    }

		    /* current time */
		    if (df) {
			printf("%02d:%05.2f ", total/ckfmt->dwAvgBytesPerSec/60,
			((float)(total-(total/ckfmt->dwAvgBytesPerSec/60)*ckfmt->dwAvgBytesPerSec*60)/ckfmt->dwAvgBytesPerSec));
		    }

		    for (i = 0; i < gChannels; i++) {
			if (gBytePerSample == 1) {
			    /* 1 byte */
			    val = buf[i * gBytePerSample] - 0x80;
			} else {
			    /* 2 byte gBytePerSample == 2 */
			    /* val = buf16[i * gBytePerSample]; */
			    val = buf16[i];
			}
			if (df) printf("ch%d:%6d  ", i+1, val);

			if (maxval[i] < abs(val)) {
			    maxval[i] = abs(val);   /* new max  */
			    maxvalcnt[i] = 1;  /* reset    */
			} else if (maxval[i] == abs(val)) {
			    ++maxvalcnt[i];    /* count up */
			}
		    }
		    if (df) printf("\n");

		    /* sound blank check for -s */
		    if (abs(val) < 
			((1 << (ckfmt->wBitsPerSample - 1)) * th_val/1000)) {
			/* blank level */
			//printf("%8d: th_val...%d\n", total,  
			//     ((1 << (ckfmt->wBitsPerSample - 1)) * th_val/1000));
			++lowcont;
			if (contf == 0 && lowcont > (ckfmt->dwSamplesPerSec*bl_sec)/10) {
			    /* 1sec continue... maybe blank area */
			    printf("Blank Time Start: %02d:%05.2f\n", 
				    total/ckfmt->dwAvgBytesPerSec/60,
				    ((float)(total-
					     (total/ckfmt->dwAvgBytesPerSec/60)
					     *ckfmt->dwAvgBytesPerSec*60)
					     /ckfmt->dwAvgBytesPerSec));
			    contf = 1;

			    if (sf && wfp) {
				/* ### Change Each Header Size */
				ChangeWavHeaders(wfp,
				    &riffhdr,            /* 01 riff header        */
				    chunkhdr, chunk_cnt, /* 02 each chunk headers */
				    ckfmt,               /* 03 fmt data           */
				    ckfact,              /* 04: no write          */
				    data_size);          /* "data" block size     */
				printf("##4 %s (%02d:%05.2f)\n", fname,
				    data_size/ckfmt->dwAvgBytesPerSec/60,
				    ((float)(data_size-
					     (data_size/ckfmt->dwAvgBytesPerSec/60)
					     *ckfmt->dwAvgBytesPerSec*60)
					     /ckfmt->dwAvgBytesPerSec));
				/* ### Close Wav File */
				if (wfp) fclose(wfp);
				wfp = NULL;
			    }
			}

		    } else {
			/* no blank level */
			lowcont = 0;
			if (contf) {
			    contf = 0;
			    printf("Blank Time End  : %02d:%05.2f\n", 
				    total/ckfmt->dwAvgBytesPerSec/60,
				    ((float)(total-
					     (total/ckfmt->dwAvgBytesPerSec/60)
					     *ckfmt->dwAvgBytesPerSec*60)
					     /ckfmt->dwAvgBytesPerSec));

			    data_size = 0;   /* reset data chunk size */
			    if (sf) {
				sprintf(fname, "%s_%03d.wav", in_fname, file_cnt++);
				printf("## Writing5 %s ...\n", fname);
				/* ### Open Next Wav File */
				wfp = fopen(fname, "wb");
				if (wfp == NULL) {
				    perror(fname);
				    exit(1);
				}
				/* ### WriteWavHeaders */
				WriteWavHeaders(wfp,
				    &riffhdr,            /* 01 riff header        */
				    chunkhdr, chunk_cnt, /* 02 each chunk headers */
				    ckfmt,               /* 03 fmt data           */
				    ckfact);             /* 04: no write          */
			    }
			}
		    }

		    /* for -ex */
		    if (ntment) {
			outf = 0;
			reopen = 0;
			curdsec = total/(ckfmt->dwAvgBytesPerSec/10);
			for (i = 0; i<ntment; i++) {
			    if (start_dsec[i] <= curdsec && curdsec < end_dsec[i]){
				outf = 1;
				if (curdsec == end_dsec[i]) {
				    reopen = 1;
				}
			    }
			}
			if (outf == 0 || reopen == 1) {
			    if (wfp) {
				/* ### Change Each Header Size */
				ChangeWavHeaders(wfp,
				    &riffhdr,            /* 01 riff header        */
				    chunkhdr, chunk_cnt, /* 02 each chunk headers */
				    ckfmt,               /* 03 fmt data           */
				    ckfact,              /* 04: no write          */
				    data_size);          /* "data" block size     */
				printf("##5 %s (%02d:%05.2f)\n", fname,
				    data_size/ckfmt->dwAvgBytesPerSec/60,
				    ((float)(data_size-
					     (data_size/ckfmt->dwAvgBytesPerSec/60)
					     *ckfmt->dwAvgBytesPerSec*60)
					     /ckfmt->dwAvgBytesPerSec));
				/* ### Close Wav File */
				if (wfp) fclose(wfp);
				wfp = NULL;
				data_size = 0;   /* reset data chunk size */
			    }
			}
			if (outf) {
			    if (wfp == NULL) {
				data_size = 0;   /* reset data chunk size */
				sprintf(fname, "%s_%03d.wav", in_fname, file_cnt++);
				printf("## Writing6 %s ...\n", fname);
				/* ### Open Wav File */
				wfp = fopen(fname, "wb");
				if (wfp == NULL) {
				    perror(fname);
				    exit(1);
				}
				/* ### WriteWavHeaders */
				WriteWavHeaders(wfp,
				    &riffhdr,            /* 01 riff header        */
				    chunkhdr, chunk_cnt, /* 02 each chunk headers */
				    ckfmt,               /* 03 fmt data           */
				    ckfact);             /* 04: no write          */
			    }
			}
		    }

		    total += len;       /* total read size           */

		    if (total >= cklen) {
			break;
		    }
		  } /* 1 bufferd data loop  V0.21-A */
		} /* data chunk loop */

		if (contf) {
		    contf = 0;
		    printf("Blank Time End  : %02d:%05.2f\n", 
			    total/ckfmt->dwAvgBytesPerSec/60,
			    ((float)(total-
				     (total/ckfmt->dwAvgBytesPerSec/60)
				     *ckfmt->dwAvgBytesPerSec*60)
				     /ckfmt->dwAvgBytesPerSec));
		}

		if (wfp) {
		    /* ### Change Each Header Size */
		    ChangeWavHeaders(wfp,
			    &riffhdr,            /* 01 riff header        */
			    chunkhdr, chunk_cnt, /* 02 each chunk headers */
			    ckfmt,               /* 03 fmt data           */
			    ckfact,              /* 04: no write          */
			    data_size);          /* "data" block size     */
		    printf("##6 %s (%02d:%05.2f)\n", fname, 
			   data_size/ckfmt->dwAvgBytesPerSec/60,
			   ((float)(data_size-
			    (data_size/ckfmt->dwAvgBytesPerSec/60)
			     *ckfmt->dwAvgBytesPerSec*60)
			     /ckfmt->dwAvgBytesPerSec));
		    /* ### Close Wav File */
		    if (wfp) fclose(wfp);
		    wfp = NULL;
		}

		printf("---- Other Info ----\n");
		/* ch0:Left ch1:Right */
		printf("Max Power : ");
		for (i = 0; i < gChannels; i++) {
		    printf("ch%d:%d/%d (%d times)  ", i+1,
			    maxval[i], 1 << (ckfmt->wBitsPerSample - 1),
			    maxvalcnt[i]);
		    if (peak_level < maxval[i]) {
			peak_level = maxval[i];
		    }
		}
		max_level = 1 << (ckfmt->wBitsPerSample - 1);
		printf("\n");
		//printf("Time      : %.2f sec\n", 
		//        ((float)cklen)/ckfmt->dwAvgBytesPerSec);
		printf("Time      : %02d:%05.2f\n", cklen/ckfmt->dwAvgBytesPerSec/60,
			((float)(cklen-(cklen/ckfmt->dwAvgBytesPerSec/60)*ckfmt->dwAvgBytesPerSec*60)/ckfmt->dwAvgBytesPerSec));
	    }
	}
    }
#endif  /* 作成中 */
}
/* V0.24-A start */

/*
 *   TimeStr2Sec()
 *
 *   IN  str : mm:ss.n
 *   OUT ret : dsec  (1/10sec)
 *             -1 : invalid time format
 */
int TimeStr2Sec(char *str)
{
    char *pt;
    char wk[1024];
    int mm, ss, ds;
    int  rval = 0;

    mm = ss = ds = 0;
    
    /* get mm */
    pt = strchr(str, ':');
    if (pt) {
        mm = atoi(str);
	++pt;
    } else {
        pt = str;
    }

    /* get ss */
    ss = atoi(pt);

    /* get ds */
    pt = strchr(str, '.');
    if (pt) {
	++pt;
	ds = *pt - '0';
	if (ds < 0 || ds > 9) {
	    printf("invalid time value [%s]\n", str);
	    return -1;
	}
    }

    rval = (mm*60+ss)*10 + ds;
    dprintf2("dsec = %d\n", rval);

    return rval;
}

/*
 *   GetTimes()
 *
 *   IN  time_exp   : mm:ss.n-mm:ss.n[,mm:ss.n-mm:ss.n,...]
 *   OUT st_dsec[]  : start dsec (1/10sec)
 *   OUT ed_dsec[]  : end   dsec (1/10sec)
 *   OUT ret        : num of time (0 : format error)
 *
 */
int GetTimes(char *time_exp, int **st_dsec, int **en_dsec)
{
    int n_time = 0;
    int i;
    int err = 0;
    char buf[4096];
    char *pt, *pt2;
    char wkst[1024];
    char wken[1024];

    strncpy(buf, time_exp, sizeof(buf));
    buf[sizeof(buf)-1] = '\0';

    ++n_time;
    for (i = 0; i<strlen(buf); i++) {
        if (buf[i] == ',') {
	    ++n_time;
	}
    }
    if (n_time) {
        *st_dsec = malloc(sizeof(int)*n_time);
        *en_dsec = malloc(sizeof(int)*n_time);
	if (*st_dsec == NULL || *en_dsec == NULL) {
	    err = 1;
	} else {
	    memset(*st_dsec, 0, sizeof(int)*n_time);
	    memset(*en_dsec, 0, sizeof(int)*n_time);
	}
    }

    i = 0;
    pt = strtok(buf, ",");
    while (err == 0 && pt) {
        pt2 = strchr(pt, '-');
        if (pt2 == NULL) {
	    err = 1;
	    break;
	}
	*pt2 = '\0';
	++pt2;
	strcpy(wkst, pt);  /* copy start time */
	strcpy(wken, pt2); /* copy end   time */
	(*st_dsec)[i] = TimeStr2Sec(wkst);
	(*en_dsec)[i] = TimeStr2Sec(wken);
	if ((*st_dsec)[i] < 0 || (*en_dsec)[i] < 0) {
	    err = 1;
	    break;
	}
	dprintf("## st = %d  en = %d\n", (*st_dsec)[i], (*en_dsec)[i]);
        ++i;
        pt = strtok(NULL, ",");
    }

    if (err) {
        n_time = 0;
    }
    return n_time;
}

void usage()
{
    printf("wavcut Ver %s\n", VER);
    printf("usage: wavcut [-d]\n");
    printf("              { [{-s|-cs}] [-lv <cut_level(%d)>] [-ln <blank_len>(%d)>]\n", th_val, bl_sec);
    printf("               | -ex mm:ss-mm:ss[,mm:ss-mm:ss,...]\n");
    printf("               | {-no | -vol <%%>}\n");
    printf("               | -rv }\n");
    printf("              [<wav_file>]\n");
    printf("       -d  : display data\n");
    printf("       -s  : split wav data\n");
    printf("       -cs : cut silent part\n");
    printf("       -lv : blank level. default:%d (x0.1%%)\n", th_val);
    printf("       -ln : time to recognize blank. default:%d (x0.1sec)\n", bl_sec);
    printf("       -ex : extract wav part\n");
    printf("       -no : normalize wav data\n");
    printf("       -vol: set volume\n");
    printf("       -rv : reduce voice part\n");
    printf("usage: wavcut -m <wav_file> <wav_file> ...\n");   /* V0.27-A */
    printf("       -m  : merge wav files (not available)\n"); /* V0.27-A */
    exit(1);
}

int main(int a, char *b[])
{
    int  i;
    char buf[4096];
    char *filename = NULL;
    FILE *fp;
    char **files;            /* files for -m            V0.27-A */
    int  fcnt = 0;           /* file count              V0.27-A */

    files = (char **)malloc((a+1) * sizeof(char *)); /* V0.27-A */
    memset(files, 0, (a+1) * sizeof(char *));        /* V0.27-A */

    for (i = 1; i<a; i++) {
        if (!strcmp(b[i],"-v")) {
            ++vf;
            continue;
        }
        if (!strcmp(b[i],"-d")) {
            df = 1;    /* Display Wav Data */
            continue;
        }
        if (!strcmp(b[i],"-rv")) {
            rvf = 1;    /* Reduce Voice */
            continue;
        }
        if (!strcmp(b[i],"-s")) {
	    if (ntment || csf) {      /* V0.30-C */
	        usage();
	    }
            sf = 1;    /* Split Wav Data */
            continue;
        }

	/* V0.30-A start */
        if (!strcmp(b[i],"-cs")) {
	    if (ntment || sf) {
	        usage();
	    }
            csf = 1;   /* Cut Silent Part */
            continue;
        }
	/* V0.30-A end   */

        if (!strcmp(b[i],"-m")) {
            mf = 1;    /* Merge Wav Files */
            continue;
        }
        if (i+1 < a && !strncmp(b[i],"-lv",3)) {
            th_val = atoi(b[++i]);  /* Blank Level */
            continue;
        }
        if (i+1 < a && !strncmp(b[i],"-ln",3)) {
            bl_sec = atoi(b[++i]);  /* Blank Sec */
            continue;
        }
        if (!strncmp(b[i],"-no",3)) {
            /* nf = 1;              /* normalize */
            nf = 1 + atoi(&b[i][3]);  /* -no[X] normalize +1X0% (secret opt) */
            continue;
        }

	/* V0.25-A start */
        if (i+1 < a && !strcmp(b[i],"-vol")) {
            volf = atoi(b[++i]);    /* set Volume */
            continue;
        }
	/* V0.25-A end   */

        if (i+1 < a && !strncmp(b[i],"-ex",3)) {
	    if (sf || csf) {       /* V0.30-C */
	        usage();
	    }
            ntment = GetTimes(b[++i], &start_dsec, &end_dsec); /* Extract wav */
	    if (ntment <= 0) {
	        usage();
	    }

            continue;
        }
        if (!strncmp(b[i],"-h",2)) {
	    usage();
        }
        filename = b[i];
	files[fcnt++];
    }

    if (volf && nf) {
        usage();
    }

    /* V0.27-A start */
    if (mf && (volf || nf || sf || csf || ntment || rvf)) {   /* V0.30-C */
        usage();
    }

    if (mf) {
	/* -m: merage wav files */
	MergeWavs(files);
	return 0;
    }
    /* V0.27-A end   */

    if (filename) {
	if (!(fp = fopen(filename,"rb"))) {
		perror(filename);
		exit(1);
	}
        strcpy(in_fname, filename);
	if (!strcasecmp(&in_fname[strlen(in_fname)-4], ".wav")) {
	    in_fname[strlen(in_fname)-4] = '\0';  /* delete .wav */
	}
    } else {
        if (nf) {
	    printf("Error: -no option not support stdin data.\n");
	    exit(1);
	}
        fp = stdin;
        strcpy(in_fname, "stdin");
    }

    WavAnalyze(fp);

    if (nf) {
	fseek(fp, 0, SEEK_SET);  /* seek to top */
	if (nf > 1) {
	    /* peak_level = (peak_level*10)/11;  /* 110% V0.22 */

            /* nf = 1 : 100% */
            /* nf = 2 : 110% */
            /* nf = 3 : 120% */
            /* nf = 4 : 130% */
            /* nf =10 : 190% */
	    peak_level = (peak_level*10)/(10+(nf-1));  /* 1[nf-1]0% V0.23 */
	}
	WavNormalize(fp, peak_level, max_level);
    }
    /* V0.25-A start */
    else if (volf) {
	fseek(fp, 0, SEEK_SET);  /* seek to top */
	WavNormalize(fp, 100, volf);  /* volf/100 */
    }
    /* V0.25-A end   */
    /* V0.24-A start */
    else if (rvf) {
	fseek(fp, 0, SEEK_SET);  /* seek to top */
	ReduceVoice(fp);
    }
    /* V0.24-A end   */

    fclose(fp);

    return 0;
}

/* vim:ts=8:sw=8:
 */
