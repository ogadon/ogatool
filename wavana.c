/*
 *  .WAV file analyzeer
 *
 *  2001/01/14 V0.10 by oga
 *  2005/10/16 V0.11 support max level, and -d option
 *  2005/12/06 V0.21 perfup
 *
 */
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

int  vf     = 0;  /* -v verbose             */
int  df     = 0;  /* -d disp all data value */

/* data formats */
struct riff_hdr {
    char          id[4];           /* RIFF id   ("RIFF")        */
    unsigned long len;             /* length                    */
    char          wave_id[4];      /* data type ("WAVE")        */
};

struct chunk_hdr {
    char          id[4];           /* chunk id  ("fmt "|"data"...) */
    unsigned long len;             /* chunk length                 */
};

/* chunk type fmt  1 */
struct ck_fmt {
    unsigned short wFormatTag;        /* Format category       */
#define WAVE_FORMAT_PCM		0x0001  /* Microsoft PCM                    */
#define FORMAT_MULAW   		0x0101  /* IBM mu-law format                */
#define IBM_FORMAT_ALAW		0x0102  /* IBM a-law format                 */
#define IBM_FORMAT_ADPCM	0x0103  /* IBM AVC Adaptive Diff PCM format */
    unsigned short wChannels;         /* Number of channels    */
    unsigned long  dwSamplesPerSec;   /* Sampling rate         */
    unsigned long  dwAvgBytesPerSec;  /* For buffer estimation */
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
int ReadChunkHeader(FILE *fp, char *ckid, int *cklen)
{
    struct chunk_hdr chunkhdr;

    if (vf) printf("chunk_hdr size = %d\n", sizeof(struct chunk_hdr));
    if (fread(&chunkhdr, 1, sizeof(struct chunk_hdr), fp) < sizeof(struct chunk_hdr)) {
        /* EOF or Error or Format Error */
        return -1;
    }
    printf("---- Chunk Header ----\n");
    strncpy(ckid, chunkhdr.id, 4);
    ckid[4] = '\0';
    *cklen  = chunkhdr.len;
    printf("chunk_id  : [%s]\n", ckid);
    printf("chunk_len : 0x%08x (%d)\n", *cklen, *cklen);

    return 0;
}

/*
 *  Analyze WAV header block
 *
 *   IN  fp
 *   OUT ret :  0 success
 *             -1 error
 */
int ReadWavHeader(FILE *fp)
{
    char wk[4096];
    int  len;

    struct riff_hdr riffhdr;

    /* read riff header */
    if (vf) printf("riffhdr size = %d\n", sizeof(riffhdr));
    fread(&riffhdr, sizeof(riffhdr), 1,  fp);	/* get header */
    strncpy(wk, riffhdr.id, 4);
    wk[4] = '\0';
    if (strcmp(wk, "RIFF")) {
        /* not WAV file */
        printf("Error : Not RIFF [%s]\n",wk);
        return -1;
    }

    printf("---- RIFF Header ----\n");
    printf("riff_id   : [%s]\n", wk);
    printf("riff_len  : 0x%08x (%d)\n", riffhdr.len, riffhdr.len);

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
 *  Wav Analyze
 *
 *   IN  fp : input file pointer
 *
 */

int WavAnalyze(FILE *fp)
{
    int  i;
    int  j;                     /*                 V0.21-A */
    int  cklen = 0;             /* chunk len               */
    char ckid[256];             /* chunk id                */
    unsigned char rbuf[4096];   /* buf for fread() V0.21-A */
    unsigned char *buf = rbuf;  /* dummy buffer    V0.21-C */
    char buf_fmt[4096];
    char buf_fact[4096];
    int  maxval[16];     /* Max Power       16ch */
    int  maxvalcnt[16];  /* Max Power count 16ch */

    struct ck_fmt  *ckfmt  = (struct ck_fmt *)buf_fmt;
    struct ck_fact *ckfact = (struct ck_fact *)buf_fact;

    /* initialize */
    for (i = 0; i<16; i++) {
        maxval[i]    = 0;
        maxvalcnt[i] = 0;
    }

    printf("\n# Analyze WAV file.  by oga.\n");

    if (ReadWavHeader(fp)) {
        printf("This is not WAV(RIFF) format file.\n");
        exit(1);
    }

    while (!ReadChunkHeader(fp, ckid, &cklen)) {
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
                    //printf("<0x%02x>", buf[0]);
                    printf("<%02x>", buf[0]);
		} else {
                    printf("%c", buf[0]);
		}
	    }
            printf("]\n");
        } else if (!strncmp(ckid, "data", 4)) {
            /* PCM Data */
            int total = 0;
            int rlen  = 0;  /* read len             V0.21-A */
            int len   = 0;  /* dummy read len       V0.21-C */
            int unit  = (ckfmt->wBitsPerSample/8) * ckfmt->wChannels;
            int val;
            short *buf16 = (short *)buf;

            while ((rlen = fread(rbuf, 1, unit*50, fp)) > 0) {    /* V0.21-C */
	      for (j = 0; j < rlen; j+=unit) {                    /* V0.21-A */
	        len = unit;                                       /* V0.21-A */
	        buf = &rbuf[j];                                   /* V0.21-A */
		buf16 = (short *)buf;                             /* V0.21-A */
                if (df) {
                    printf("%08x: ", total);
                    DumpData(buf, len);
                }

                for (i = 0; i < ckfmt->wChannels; i++) {
                    if (ckfmt->wBitsPerSample == 8) {
                        /* 1 byte */
                        val = buf[i*(ckfmt->wBitsPerSample/8)] - 0x80;
                    } else {
                        /* 2 byte wBitsPerSample == 16 */
                        /* val = buf16[i*(ckfmt->wBitsPerSample/8)]; */
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
                total += len;

                if (total >= cklen) {
                    break;
                }
              } /* 1 bufferd data loop  V0.21-A */
            } /* data chunk loop */
            printf("---- Other Info ----\n");
	    /* ch0:Left ch1:Right */
	    printf("Max Power : ");
            for (i = 0; i < ckfmt->wChannels; i++) {
	        printf("ch%d:%d/%d (%d times)  ", i+1,
	                maxval[i], 1 << (ckfmt->wBitsPerSample - 1),
			maxvalcnt[i]);
            }
	    printf("\n");
	    printf("Time      : %d:%04.2f\n", cklen/ckfmt->dwAvgBytesPerSec/60,
	            ((float)(cklen-(cklen/ckfmt->dwAvgBytesPerSec/60)*ckfmt->dwAvgBytesPerSec*60)/ckfmt->dwAvgBytesPerSec));
        } else {
	    /* unknown chank */
	    fseek(fp, cklen, SEEK_CUR);
	}
    }

    printf("WAV file Analyze end.\n");
    return 0;
}

void usage()
{
    printf("usage: wavana [-d] [<wav_file>]\n");
    printf("       -d : display data\n");
    exit(1);
}

int main(int a, char *b[])
{
    int  i;
    char *filename = NULL;
    char buf[4096];
    FILE *fp;

    for (i = 1; i<a; i++) {
        if (!strncmp(b[i],"-v",2)) {
            vf = 1;
            continue;
        }
        if (!strncmp(b[i],"-d",2)) {
            df = 1;
            continue;
        }
        if (!strncmp(b[i],"-h",2)) {
	    usage();
        }
        filename = b[i];
    }
    if (filename) {
	if (!(fp = fopen(filename,"rb"))) {
		perror("fopen");
		exit(1);
	}
    } else {
        fp = stdin;
    }

    WavAnalyze(fp);

    fclose(fp);
}
