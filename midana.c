/*
 *  MIDI file analyzeer
 *
 *  1999/01/02 V0.10 by oga
 *  2002/07/14 V0.10.1 support windwos
 *  2003/08/10 V0.11 add ff04
 *  2007/01/14 V0.12 support win Play
 *  2007/02/05 V0.13 support -p
 *  2008/09/15 V0.14 support -o 
 *  2009/03/07 V0.15 fix sum bug
 *  2010/10/03 V0.16 add ff00 ff20 ff21 ff54 ff7f SysEx(f7), fix wait time
 *  2010/10/11 V0.17 support multi track play 
 *  2011/09/09 V0.18 support -t(fix tempo) and -s(play start time)
 *  2014/06/08 V0.19 add Control Change(0xBn) sub command (02,04,08,44-58,5c,5e-61)
 *  2014/07/06 V0.20 change dump method, fix 0xff str length prob.
 *  2014/07/06 V0.21 fix tempo change prob.
 *
 */

/*
 *  +-------------+
 *  |"MThd"       | 4bytes  (Header Chank)
 *  +-------------+
 *  |<size>       | 4bytes
 *  +-------------+
 *  |<body>       | SMF Format(0,1,2)
 *  |             | Num of Track
 *  |             | Delta Time
 *  +-------------+
 *  |"MTrk"       | 4bytes  (Track Chank1)
 *  +-------------+
 *  |<size>       | 4bytes
 *  +-------------+
 *  |<body>       |
 *  |             |
 *  +-------------+
 *  |"MTrk"       | 4bytes  (Track Chank2)
 *  +-------------+
 *  |<size>       | 4bytes
 *  +-------------+
 *  |<body>       |
 *  |             |
 *
 *  データは基本bigendian (但し、bend(Ex mm ll)のみlittleendian)
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef _WIN32
#include <winsock.h>
#include <mmsystem.h>
#define WIN_MIDIOUT 1
#define	usleep(x)	Sleep((x)/1000)
#endif

#define VER      "0.21"
#define dprintf  if (vf) printf
#define dprintf2 if (vf >= 2) printf

#ifdef _WIN32
typedef unsigned int u_int;
#endif

/* V0.14-A start */
typedef struct _mtck_t {
    char  mtck_key[4];   /* "MThd"/"MTrk" Chank Key          */
    int   size;          /* "MThd"/"MTrk" Chank Header size  */
    short format;        /* "MThd" format                    */
    short ntrack;        /* "MThd" num of track              */
    short dtime;         /* "MThd" DeltaTime                 */
} mtck_t;

/* V0.21-A start */
typedef struct _tempotbl_t {
    u_int delta;         /* total delta                      */
    int   tempo;         /* tempo                            */
    u_int ms;            /* total ms                         */
} tempotbl_t;
/* V0.21-A end   */


#define SIZE_MTHD  14
#define SIZE_MTRK   8

#define KEY_MTHD   "MThd"
#define KEY_MTRK   "MTrk"
#define MAX_CHLIST 32       /* max channel list */
#define MAX_TRACK  50       /* max track        */
#define MAX_TEMPO  1000     /* max tempo tbl          V0.21-A */
#define DEF_TEMPO  500000   /* default tempo (MM=120) V0.21-A */

/* V0.14-A end   */
#define EOD        (0xffffffff)
#define TICK       20       /* ms */

#define MS2DELTA(ms)	    (((ms) * hd_dtime)/(tempo/1000))
#define MS2DELTA2(ms, tmp)  (((ms) * hd_dtime)/((tmp)/1000))     /* V0.21-A */
#define MS2DELTA_ORG(ms)    (((ms) * hd_dtime)/(orgtempo/1000))  /* V0.18-A */

int  vf     = 0;        /* -v verbose            */
int  dismid = 0;	/* Command disp 8n 9n An */
int  playf  = 0;	/* -p play midi  V0.13-A */
int  trkno  = 0;	/* Track No.             */
int  of     = 0;        /* -o track out           V0.14-A */
int  cf     = 0;        /* -c channel play        V0.17-A */
int  nch    = 0;	/* num of -o track        V0.14-A */
int  sttime = 0;	/* -s starttime           V0.18-A */
char ofname[1024];      /* -o output file name    V0.14-A */
FILE *ofp   = NULL;     /* -o output file pointer V0.14-A */
int  chlist[MAX_CHLIST];/* channel list           V0.14-A */
u_int *playdat[MAX_TRACK]; /* play data 50trk     V0.17-A */

tempotbl_t tempodat[MAX_TEMPO]; /* tempo data        V0.21-A */
int        temponum = 0;        /* num of tempo data V0.21-A */


/* V0.13-A start */
int  hd_format = 0;	/* MTHeader Format       */
int  hd_track  = 0;	/* MTHeader Track        */
int  hd_dtime  = 0;	/* MTHeader DeltaTime    */
/* V0.13-A end   */

/* tempo is global value */
int  tempo    = 500000;    /* Tempoのデフォルト値μs(4分♪=120:0.5sec) V0.13-A V0.16-M */ 
int  fixtempo = 0;         /* tempo固定再生時のTempo値                         V0.18-A */ 
int  orgtempo = 500000;    /* 本来のTempo値                                    V0.18-A */ 


/* Key Name */
char *note[128] = {
      "C0 ","C0#","D0 ","D0#","E0 ","F0 ","F0#","G0 ","G0#","A0 ","A0#","B0 ",
      "C1 ","C1#","D1 ","D1#","E1 ","F1 ","F1#","G1 ","G1#","A1 ","A1#","B1 ",
      "C2 ","C2#","D2 ","D2#","E2 ","F2 ","F2#","G2 ","G2#","A2 ","A2#","B2 ",
      "C3 ","C3#","D3 ","D3#","E3 ","F3 ","F3#","G3 ","G3#","A3 ","A3#","B3 ",
      "C4 ","C4#","D4 ","D4#","E4 ","F4 ","F4#","G4 ","G4#","A4 ","A4#","B4 ",
      "C5 ","C5#","D5 ","D5#","E5 ","F5 ","F5#","G5 ","G5#","A5 ","A5#","B5 ",
      "C6 ","C6#","D6 ","D6#","E6 ","F6 ","F6#","G6 ","G6#","A6 ","A6#","B6 ",
      "C7 ","C7#","D7 ","D7#","E7 ","F7 ","F7#","G7 ","G7#","A7 ","A7#","B7 ",
      "C8 ","C8#","D8 ","D8#","E8 ","F8 ","F8#","G8 ","G8#","A8 ","A8#","B8 ",
      "C9 ","C9#","D9 ","D9#","E9 ","F9 ","F9#","G9 ","G9#","A9 ","A9#","B9 ",
      "Ca ","Ca#","Da ","Da#","Ea ","Fa ","Fa#","Ga " };

/* Program Name (SC-55) */
char *prog_name[128] = {
/*  1*/ "Piano1",
        "Piano2",
        "Piano3",
        "Honky-tonk",
        "E.Piano1",
        "E.Piano2",
        "Harpsichord",
        "Clav.",
        "Celesta",
/* 10*/ "Glockenspiel",
        "MusicBox(Orgel)",
        "Vibraphone",
        "Marimba",
        "Xylophone",
        "Tubular-bell",
        "Santur",
        "Organ1",
        "Organ2",
        "Organ3",
/* 20*/ "ChurchOrg.1",
        "ReedOrgan",
        "AccordionFr",
        "Harmonica",
        "Bandneon",
        "Nylon-str.Gt",
        "Steel-str.Gt",
        "JazzGt.",
        "CleanGt.",
        "MutedGt.",
/* 30*/ "OverdriveGt",
        "DistortionGt",
        "Gt.Harmonics",
        "AcousticBs.",
        "FingerdBs.",
        "PickedBs.",
        "FratlessBs.",
        "SlapBass1",
        "SlapBass2",
        "SynthBass1",
/* 40*/ "SynthBass2",
        "Violin",
        "Viola",
        "Cello",
        "Contrabass",
        "TremoloStr",
        "PizzicatoStr",
        "Harp",
        "Timpani",
        "Strings",
/* 50*/ "SlowStrings",
        "Syn.Strings1",
        "Syn.Strings2",
        "ChoirAahs",
        "VoiceOohs",
        "SynVox",
        "OrchestraHit",
        "Trumpet",
        "Trombone",
        "Tuba",
/* 60*/ "MutedTrumpet",
        "FrenchHorn",
        "Brass1",
        "SynthBrass1",
        "SynthBrass2",
        "SopranoSax",
        "AltoSax",
        "TenorSax",
        "BaritoneSax",
        "Oboe",
/* 70*/ "EnglishHorn",
        "Bassoon",
        "Clarinet",
        "Piccolo",
        "Flute",
        "Recorder",
        "PanFlute",
        "BottleBlow",
        "Shakuhachi",
        "Whistle",
/* 80*/ "Ocarina",
        "SquareWave",
        "SawWave",
        "Syn.Calliope",
        "ChifferLead",
        "Charang",
        "SoloVox",
        "5thSawWave",
        "Bass&Lead",
        "Fantasia",
/* 90*/ "WarmPad",
        "Polysynth",
        "SpaceVoice",
        "BowedGlass",
        "MetalPad",
        "HaloPad",
        "SweepPad",
        "IceRain",
        "Soundtrack",
        "Crystal",
/*100*/ "Atmosphere",
        "Brightness",
        "Goblin",
        "EchoDrops",
        "StarTheme",
        "Sitar",
        "Banjo",
        "Shamisen",
        "Koto",
        "Kalimba",
/*110*/ "BagPipe",
        "Fiddle",
        "Shanai",
        "TinkleBell",
        "Agogo",
        "SteelDrums",
        "Woodblock",
        "Taiko",
        "Melo.Tom1",
        "SynthDrum",
/*120*/ "ReverseCym.",
        "Gt.FretNoise",
        "BreathNoise",
        "Seashore",
        "Bird",
        "Telephone1",
        "Helicopter",
        "Applause",
/*128*/ "GunShot",
};


#ifdef WIN_MIDIOUT
/* for Windows Only start V0.12 */
/*
 *   playdat
 *      +--+--+--+--+
 *      |XX|D2|D1|ST|
 *      +--+--+--+--+
 * 
 *      XX : 0xff is Meta Event
 */
#define MIDIMSG(stat , data1 , data2) (DWORD)(stat | (data1 << 8) | (data2 << 16))

typedef struct _midiinst {
    HMIDIOUT hmo;
    int      playing;
} MidiInst;

MidiInst minst;

void CALLBACK WinMidiOutCB(HMIDIOUT hmo, UINT wMsg, DWORD dwinst, DWORD param1, DWORD param2)
{
    MidiInst *pminst = (MidiInst *)dwinst;
    dprintf("Info: Call WinMidiOutCB() param1=%d param2=%d\n", param1, param2);
    if (param1 == MM_MOM_DONE) {
        dprintf("Info: WinMidiOutCB() MM_MOM_DONE\n");
	pminst->playing = 0;
    }
}

void WinMidiOpen(MidiInst *pminst)
{
    int st;
    memset(pminst, 0, sizeof(MidiInst));
    st = midiOutOpen(&pminst->hmo,         /* &HMIDIOUT            */
                     MIDI_MAPPER ,         /* Midi DeviceID (Auto) */
                     (DWORD)WinMidiOutCB,  /* Callback Func        */
		     (DWORD)pminst,        /* Callback Instance    */
                     CALLBACK_FUNCTION);   /* Func is Callback     */
    if (st != MMSYSERR_NOERROR) {
	printf("Error: midiOutOpen() return=%d\n", st);
	pminst->hmo = NULL;
    }
}

void WinMidiClose(MidiInst *pminst)
{
    MMRESULT st = 0;

    /* 出力デバイス開放 */
    st = midiOutReset(pminst->hmo);
    if (st != MMSYSERR_NOERROR) {
	printf("Error: midiOutReset() return=%d\n", st);
    }


    while (1) {
        /* 出力デバイスClose */
        st = midiOutClose(pminst->hmo);
	if (st != MIDIERR_STILLPLAYING) {
	    break;
	}
        /* まだ再生中 */
	Sleep(200);
	dprintf("Info: Wait Midi Play ...\n");
    }

    if (st != MMSYSERR_NOERROR) {
	printf("Error: midiOutClose() return=%d\n", st);
    }
}

void WinMidiShortMsg(MidiInst *pminst, DWORD dwMsg)
{
    MMRESULT st = 0;

    dprintf2("Info: midiOutShortMsg(0x%08x)\n", dwMsg);
    st = midiOutShortMsg(pminst->hmo, dwMsg);
    if (st == MMSYSERR_NOERROR) {
        pminst->playing = 1;
    } else {
	printf("Error: midiOutShortMsg() return=%d\n", st);
    }
}

/* for Windows Only end   V0.12 */
#endif /* WIN_MIDIOUT */

/*
 *  pow2
 *
 *   IN  bs   : base value
 *   IN  pow  : power
 */
int pow2(bs, pow)
int bs;
int pow;
{
    int i;
    int value = 1;

    for (i = 0; i<pow; i++) {
        value *= bs;
    }
    return value;
}

/*
 *  Dump Data
 *
 *   IN  buf : dump buffer
 *       len : dump length
 */
void DumpData(buf,len)
unsigned char *buf;
int  len;
{
    int i;

    printf("\n+0 +1 +2 +3 +4 +5 +6 +7 +8 +9 +a +b +c +d +e +f");  /* V0.20-A */
    for (i=0; i<len; i++){
	/* V0.20-C start */
	if (i % 16 == 0) {
	    printf("\n%02x",buf[i]);
	} else {
            printf(" %02x",buf[i]);
	}
	/* V0.20-C end   */
    }
    printf("\n");
}

/*
 *  Put Text Data
 *
 *   IN  buf : dump buffer
 *       len : dump length
 */
void PutTextData(buf,len)
unsigned char *buf;
int  len;
{
    int i;

    for (i=0; i<len; i++){
        putchar(buf[i]);
    }
    putchar('\n');
}

/*
 *  Get delta time  (or get 0xff command's string length)
 *
 *   IN  buf     : pointer of delta time area
 *   IN  codebuf : if not NULL, dump delta time byte
 *                 "00   |" "00 00|" "00 00 00|"
 *   OUT size    : length of delta time area
 *   OUT ret     : delta time (tick)
 *
 *   DeltaTime: MSB  continuation flag 
 *              7bit time data
 */
int GetDeltaTime(unsigned char *buf, char *codebuf, int *size)
{
    int i = 0;
    int delta = 0;
    char work[128];

    if (size) *size = 0;
    if (codebuf) strcpy(codebuf, "");

    while (1) {
	if (size) ++(*size);
        if (codebuf) {
	    /* バイトデータ出力用 */
	    sprintf(work, "%s%02x", (*size>1)?" ":"", buf[i]); /* V0.16-C */
	    strcat(codebuf, work);
	}
        delta |= (buf[i] & 0x7f);
	/* 0x80ビットが立っていたら続く */
        if ((buf[i] & 0x80) == 0) break;
	delta <<= 7;  /* shift 7bit */
	++i;
    }
    /* V0.16-A start */
    if (*size <= 1) {
	strcat(codebuf, "   ");
    }
    strcat(codebuf, "|");
    return delta;
    /* V0.16-A end   */
}

/*
 *  Analyze MThd block
 *
 *   IN  fp
 *       buf : MThd block
 *   OUT ret :  0 success
 *             -1 error
 */
int GetMidiHeader(fp,buf)
FILE *fp;
char *buf;
{
    char *wk   = buf;              /* V0.14-C */
    int  *size = (int *)wk;
    int  len;
    short *short_data;
    mtck_t *pmthd = (mtck_t *)wk;  /* V0.14-A */

    /* V0.14-C start */
    /* printf("sizeof(mtck_t) = %d\n", sizeof(mtck_t)); */
    fread(pmthd, 1, SIZE_MTHD, fp);  /* read "MThd" Header area */
    if (strncmp(pmthd->mtck_key, KEY_MTHD, strlen(KEY_MTHD))) {
        /* not MIDI file */
        printf("Error : Not MThd [%s]\n",wk);
        return -1;
    }
    len = htonl(pmthd->size);   
    if (len > 6) {
        printf("Warning : MThd len is [%s]\n", wk, len);
	fread(&wk[SIZE_MTHD], 1, len - 6, fp);  /* 残りを空読み */
    }
    printf("MThd: len = %d: ", len);
    DumpData(&pmthd->format, len);

    hd_format = htons(pmthd->format);
    hd_track  = htons(pmthd->ntrack);
    hd_dtime  = htons(pmthd->dtime);
    /* V0.14-C end   */

    printf("MThd: SMF Format=%d NumOfTrack=%d DeltaTime=%d(=1/4)\n", hd_format, hd_track, hd_dtime);

    if (of) {
	printf("output to %s\n", ofname);
	if ((ofp = fopen(ofname, "wb")) == NULL) {
	    perror(ofname);
	}
	pmthd->ntrack = ntohs((short)nch);
	if (ofp) fwrite(pmthd, 1, SIZE_MTHD, ofp);
    }

    return 0;
}

/* V0.13-A start */
/*
 *  Delta2MilliSec(int delta, int tempo)
 *    DeltaTimeをmsecに変換する
 * 
 *  IN : delta   変換対象のDeltaTime
 *  OUT: tempo   μsec/四分音符 (SMFでの値)
 *
 *  D: SMFヘッダでのDelta値(hd_dtime)
 *  T: μsec/四分音符 (SMFでの値)
 *  MM(BPM) = 60x1000000/tempo
 *  DeltaTimeに対する待ち時間
 *
 *  <Delta>x60x1000   <Delta>x60x1000   <Delta>
 *  --------------- = --------------- = --------
 *       MMxD         60x1000000/TxD    1000/TxD
 *
 *
 */
int Delta2MilliSec(int delta, int tempo)
{
    int mm =  60 * 1000000/tempo;                  /* V0.16-A */
    //return (delta * ((double)tempo / 1000) ) / hd_dtime;
    //return ((delta * 60 * 1000) / mm / hd_dtime);   /* V0.16-C */
    return ((delta * (60 * 1000 / mm)) / hd_dtime);   /* V0.19-C */
}
/* V0.13-A end   */

/* V0.14-A start */
/*
 *  Check Track No
 *
 *   IN  track: Current Track No
 *   OUT ret  : 1: 出力対象
 *              0: 出力対象外
 */
int CheckTrkNo(int track)
{
    int i;
    int outputf = 0;

    for (i = 0; i<nch; i++) {
	if (track == chlist[i]) {
	    outputf = 1;
	    break;
	}
    }
    return outputf;
}
/* V0.14-A end   */

/* V0.21-A start */
/*
 *  Add tempo data (sort)
 *
 *    IN  delta_sum : total delta time
 *    IN  tempo     : tempo
 *    OUT dempodat  : tempo data list
 *    OUT demponum  : num of tempodat entry
 *
 *    tempodat[]
 *         +--+--+--+--+--+--+--+--+--+--+--+--+
 *    [0]  | DeltaTime | tempo val | milli sec |
 *         +--+--+--+--+--+--+--+--+--+--+--+--+
 *    [1]  | DeltaTime | tempo val | milli sec |
 *         +--+--+--+--+--+--+--+--+--+--+--+--+
 */
void AddTempoDat(u_int delta_sum, int tempo)
{
	int i;

	if (vf) printf("AddTempoDat: delta=%d, tempo=%d\n", delta_sum, tempo);

	if (temponum > 0 && tempodat[temponum-1].delta == delta_sum) {
		/* 前回と同時刻にTempo変更があった場合は後が有効 */
		tempodat[temponum].tempo = tempo;
	} else {
		/* 新規エントリ追加 (通常はこちら) */
		/* ToDo: 複数トラックにSet Tempoが存在する場合、昇順に挿入が必要 */
		tempodat[temponum].delta = delta_sum;
		tempodat[temponum].tempo = tempo;
		++temponum;
	}

	/* millisec 再計算 */
	for (i = 0; i < temponum; i++) {
		if (i == 0) {
			if (tempodat[i].delta) {
				tempodat[i].ms = Delta2MilliSec(tempodat[i].delta, DEF_TEMPO);
			} else {
				tempodat[i].ms = 0;
			}
			continue;
		}
		tempodat[i].ms = tempodat[i-1].ms 
		   + Delta2MilliSec(tempodat[i].delta-tempodat[i-1].delta, tempodat[i-1].tempo);
	}
}
/* V0.21-A end   */

/*
 *  Analyze MTrk data block
 *
 *   [DeltaTime][MidiMessage]
 *
 *   IN  buf : Track data
 *       len : Track data len
 */
void AnalyzeTrack(buf,len)
unsigned char *buf;
int len;
{ 
    int  pt = 0;
    int  cmd1,cmd2,stat,ch,cmd3,nlen;
    char nbuf[128];
    char codebuf[128];
    char wk[16];
    int  i;
    int  first = 1;
    int  size;             /* lenght area size             */ 
    int  delta = 0;        /* DeltaTime            V0.13-A */
    int  delta_sum = 0;    /* DeltaTime Total      V0.15-A */ 
    int  msec_sum  = 0;    /* DeltaTime(msec)Total V0.19-A */ 
    int  next_mes  = 0;    /* 次の小節のDelta値    V0.15-A */ 
    int  measure   = 1;    /* 現在の小節番号       V0.15-A */ 
    int  start_note = 1;   /* 最初の音が出たか     V0.15-A V0.19-C */ 
    int  msec;             /* wait msec            V0.13-A */ 
    int  val;              /* work value           V0.16-A */ 

    memset(wk,     0, sizeof(wk));
    memset(nbuf,   0, sizeof(nbuf));
    memset(nbuf, ' ', 64);
    while (pt < len) {
	dprintf2("<%d>", pt); fflush(stdout);
#if 1  /* V0.13 */
	size = 0;

	/* 先頭には必ずデルタタイムがある V0.16-A */
	delta = GetDeltaTime(&buf[pt], codebuf, &size);   /* pt 更新 */
	pt += size;
	msec = Delta2MilliSec(delta, tempo);
	if (vf) printf("# Delta: %5d (%4d msec)\n", delta, msec); /* DEBUG   */

	/* V0.15-A start */
	if ((buf[pt] >> 4) == 9) {  /* 9n */
	    start_note = 1;		/* 最初のNote Onがあった所からカウントする */
	}
	if (start_note) {
	    delta_sum += delta;
	    msec_sum  += msec;             /* V0.19-A      */
	    while (delta_sum >= next_mes) {
		if (dismid) printf("# Measure %d\n", measure);
		next_mes += hd_dtime * 4;  /* 一小節分先へ */
		++measure;
	    }
	}
	if (dismid) printf("%s", codebuf);
	/* V0.15-A end */
#else
        if (buf[pt] == 0x00) {
            ++pt;
            continue;
        }
#endif
	dprintf2("<%d>", pt); fflush(stdout);
        if (buf[pt] == 0xff) {
            /* 
             *  sub command block  (Meta Event)
             * 
             *    ffxxLLDDDDDDD...
             */
            cmd1 = buf[pt];             /* 0xff           */
            cmd2 = buf[pt+1];		/* sub command    */
            nlen = buf[pt+2];           /* command len    */
            pt += 3;		        /* skip ffxxLL    */

            nlen = GetDeltaTime(&buf[pt-1], codebuf, &size); /* get datalen V0.20-A */
            pt += (size-1);                              /* skip ffxx<LL..> V0.20-A */
            /* printf("\nlen area size=%d, strlen=%d\n", size, nlen); */

            strcpy(&codebuf[strlen(codebuf)-1], "    :");                /* V0.20-A */
            printf("%02x %02x %s ",cmd1, cmd2, codebuf);                 /* V0.20-C */

	    switch (cmd2) {
              case 0x00: /* ff00 02 ssss  Sequence Number V0.16-A */
	        /* シーケンス番号(format 2 only) len:2 */
	        val = buf[pt]*256 + buf[pt+1]*256; /* sequence num */
	        printf("%02x %02x : ", buf[pt], buf[pt+1]);
	        printf("SUB(00):Sequence Number : %d\n", val);
		break;
              case 0x01: /* ff01 Comment Text */
                /* テキスト */
                printf("SUB(01):Text(%d) : ", nlen);
		PutTextData(&buf[pt], nlen);
		break;
              case 0x02: /* ff02 Copyright */
                /* 著作権表示 */
                printf("SUB(02):Copyright(%d) : ", nlen);
		PutTextData(&buf[pt], nlen);
		break;
              case 0x03: /* ff03 Track name */
                /* シーケンス名(format0 or format1.1stTrack) */
                /* トラック名(format 2) */
                printf("SUB(03):Sequence/Track Name(%d) : ", nlen);
		PutTextData(&buf[pt], nlen);
		break;
              case 0x04: /* ff04 Instrument name */
                /* 楽器名 */
                printf("SUB(04):Instrument Name(%d) : ", nlen);
		PutTextData(&buf[pt], nlen);
		break;
              case 0x05: /* ff05 Song Lyrics */
                /* 歌詞 */
                printf("SUB(05):Lyrics(%d) : ", nlen);
		PutTextData(&buf[pt], nlen);
		break;
              case 0x06: /* ff06 Marker */
                /* リハーサル記号やセクション名のような、 シーケンスの  
                 * その時点の名称を記述する。   
                 * ("Introduction" とか "A", "B" など)  
                 */
                printf("SUB(06):Marker(%d) : ", nlen);
		PutTextData(&buf[pt], nlen);
		break;
              case 0x07: /* ff07 Queue Point */
                /* キューポイント: 曲データ中、このメタイベントの
                   挿入されている位置で、 その曲以外の進行を記述する
                   のに用いる。 (曲中の進行の記述には、マーカーを用いる) 
                 */
                printf("SUB(07):Queue Point(%d) : ", nlen);
		PutTextData(&buf[pt], nlen);
		break;
              case 0x20: /* ff20 01 cc  MIDI channel prefix V0.16-A */
                /* MIDIチャンネルプリフィックス(len:1) */
                val = buf[pt]; /* prefix */
                printf("SUB(20):MIDI Channel Prefix : %d\n", val);
		break;
              case 0x21: /* ff21 01 pp  Ouput port (defact std.) V0.16-A */
		/* 出力ポート指定(正式なSMFでは未定義) (len:1) */
	        val = buf[pt]; /* port */
	        printf("SUB(20):MIDI Channel Prefix : %d\n", val);
		break;
              case 0x2f: /* ff2f 00 End of track */
		/* トラック終端(len:0) */
	        printf("SUB(2f):End of Track\n");
		break;
              case 0x51: /* ff51 03 tttttt  Set Tempo */
	        tempo = buf[pt]*256*256 + buf[pt+1]*256 + buf[pt+2]; /* tempo */
	        printf("%02x %02x %02x : ", buf[pt], buf[pt+1], buf[pt+2]); /* V0.15-A */
	        printf("SUB(51):Set Tempo : %d (MM=%d)\n", tempo, 60 * 1000000/tempo);
		break;
	      case 0x54: /* ff54 05 hh mm ss fr ff  SMPTE Offset(len:5) V0.16-A */
		/* SMPTE オフセット(トラックデータの演奏開始時刻) */
	        val = buf[pt]; /* port */
	        printf("SUB(54):SMPTE Offset %02d:%02d:%02d %d %d\n", 
	               buf[pt],buf[pt+1],buf[pt+2],buf[pt+3],buf[pt+4]);
		break;
	      case 0x58: /* ff58 04 nnddccbb  beat(len:4) */
                /* 拍子記号 */
                /*  拍子記号は、4つの数字で表現される。 
                    nnとddは、そのまま拍子記号の分子と分母を表す。 
                    ただし分母は2のマイナス乗で表現する。 
                    つまり2=4分音符、3=8分音符、4=16分音符・・・となる。
                    ccは、メトロノーム1拍あたりのMIDIクロック数を表している。 
                    24MIDIクロックで4分音符を表す(後述)ため、 
                    例えば4分音符ごとにメトロノームを鳴らす場合、 cc=0x18 (=24)となる。
                    bbは、MIDI4分音符(24MIDIクロック)の中に入る32分音符の
                    数を表す。 大抵bb=8となる。
                    例:
                    4/4拍子だと、FF 58 04 04 02 18 08
                    3/4拍子だと、FF 58 04 03 02 18 08
                    6/8拍子だと、FF 58 04 06 03 18 08
                    いずれも、cc=0x18(=24)、bb=8の場合。 
                    (余程のことがない限り変更する必要はないと思いますが) 
	        */
	        printf("SUB(58):Time Signature (Beat) : %d/%d  click(4th=24):%d  4th=%d(x32th)\n",
		       buf[pt], pow2(2, buf[pt+1]), buf[pt+2], buf[pt+3]); /* V0.16-C */
		break;
              case 0x59: /* ff59 02 sf mi  Key Signature */
		/* 調の設定 #, b の数 mi: 0:長調, 1:短調 */
	        printf("SUB(59):Key Signature: %s x %d %s\n",
		       (buf[pt] < 128)?"#":"b", 
		       (buf[pt] < 128)?buf[pt]:256-buf[pt],
                       (buf[pt+1] == 0)?"Major":"Minor");
		break;
              case 0x7f: /* ff7f Sequencer specific meta data */
                /* シーケンサー特定メタイベント 
                   特定のシーケンサーのための特別な要求のためこの
                   イベントタイプを用いる。 
                   データバイトの最初の1バイトはメーカーID 
                   */
                printf("SUB(7f):Maker Specific Data: Maker ID(%d) ", buf[pt]);
                DumpData(&buf[pt+1],nlen-1);	/* DDDD,LL */
		break;
              default: /* unknown sub cmd */
                printf("SUB:Unknown(%d) :", nlen);
                DumpData(&buf[pt],nlen);	/* DDDD,LL */
		break;
	    }
            pt += nlen;			/* skip len & data area */
            first = 1;
        } else {
            /* MIDI data */
#if 0
            if (first) {
                printf("ch1 ch2 ch3 ch4 ch5 ch6 ch7 ch8 ch9 c10 c11 c12 c13 c14 c15 c16\n");
                first = 0;
            }
#endif
	    if ((buf[pt] & 0x80) == 0x80) {
		/* status byte である */
                cmd1 = buf[pt];             /* status byte    */
	    } else {
		/* status byte でなかった場合、RunningStatusであるため
		 * 前回のstatusが継続する V0.13-C 
		 */
		--pt;
		dprintf("Run:");
	    }
            stat = cmd1 >> 4;           /* status         */
            ch   = cmd1 & 0xf;          /* channel        */
            cmd2 = buf[pt+1];		/* note no byte   */
            cmd3 = buf[pt+2];		/* velocity byte  */
            /* nlen = buf[pt+3]; */     /* note length    */

            switch(stat) {
	      case 8:	/* 8n kk vv  Note Off Data:2byte */
	        pt += 3;
                strncpy(&nbuf[ch*4]," x ",3);
                sprintf(codebuf, "%02x %02x %02x ",cmd1, cmd2, cmd3);
		//nlen = GetDeltaTime(&buf[pt], codebuf, &size);   /* pt 更新 */
                if (dismid) {
                    //printf("%-15s: Note Off     (len:%d)\n", codebuf, delta);
                    printf("%-15s: (wait:%4d) Note Off\n", codebuf, delta);
                }
                //pt += size;
                break;

	      case 9:	/* 9n kk vv  Note On Data:2byte */
	        pt += 3;
		if (cmd2 > 0x7f) {
                    strncpy(&nbuf[ch*4], "???", 3);
		} else {
                    strncpy(&nbuf[ch*4],note[cmd2],3);
		}
                sprintf(codebuf, "%02x %02x %02x ",cmd1, cmd2, cmd3);
		//nlen = GetDeltaTime(&buf[pt], codebuf, &size);   /* pt 更新 */
                if (dismid) {
		    if (cmd2 > 0x7f) {
                        sprintf(wk,"?? (0x%02x)", cmd2);
		    } else {
                        strncpy(wk,note[cmd2],3);
		    }
                    //printf("%-15s: Note On  %s (len:%d)\n", codebuf, wk, delta);
		    /* Velocity 0: Note Off */
                    printf("%-15s: (wait:%4d) Note %s %s\n",
                           codebuf, delta, (cmd3 == 0)?"Off":"On", wk);  /* V0.16-C */
                }
                //pt += size;
                break;

	      case 10:	/* An kk vv (Polyphonic After Touch)  Data: 2byte */
	        pt += 3;
                strncpy(&nbuf[ch*4+1],"x",1);
                sprintf(codebuf, "%02x %02x %02x ",cmd1, cmd2, cmd3);
		//nlen = GetDeltaTime(&buf[pt], codebuf, &size);   /* pt 更新 */
                if (dismid) {
                    strncpy(wk,note[cmd2],3);
                    printf("%-15s: Poryphonic Key Pressure %s\n",
                    					codebuf, wk, delta);
                }
                //pt += size;
                break;

	      case 12:	/* Cn pp (Program Change)  Data:1byte */
                printf("%02x %02x          : Program Change (Prog.%d) %s\n",
		       cmd1, cmd2, cmd2+1, 
		       (cmd2 < 128)?prog_name[cmd2]:"");
                //if (cmd3 == 0xff) {  /* V0.13-D */
                    pt += 2;
                //} else {
                //    pt += 3;
                //}
                break;

	      case 13:	/* Dn vv (Channel After Touch)  Data:1byte */
                printf("%02x %02x          : Channel Pressure\n",cmd1,cmd2);
                //if (cmd3 == 0xff) {  /* V0.13-D */
                    pt += 2;
                //} else {
                //    pt += 3;
                //}
                break;

	      case 14:	/* En mm ll (Pitch Wheel)   Data:2byte */
                printf("%02x %02x %02x       : Pitch Bend Change (bend value:%5d %5d)\n",
                        cmd1,cmd2,cmd3,
                        (cmd3*256+cmd2), (cmd3*256+cmd2)-0x4000);  /* V0.16-C */
                //if (nlen == 0xff) {  /* V0.13-D */
                    pt += 3;
                //} else {
                //    pt += 4;
                //}
                break;

              case 15:	/* Fx ..... */
                if (cmd1 == 0xfe) {
                    printf("%02x             : Active Sensing (every 250msec)\n",cmd1);
                    ++pt;                 /* V0.15-C V0.16-M */
                } else if (cmd1 == 0xf0) {
                    printf("%02x %02x          : System Exclusive message(f0) :",cmd1,cmd2);
		    pt += 2;
                    while (buf[pt] != 0xf7 && buf[pt] != 0xff) {
                        printf(" %02x",buf[pt++]);
                    }
                    printf(" %02x\n",buf[pt++]); /* 0xf7 end of exclusive */
                } else if (cmd1 == 0xf7) {
                    printf("%02x %02x          : System Exclusive message(f7) :",cmd1,cmd2);
		    pt += 2;
                    DumpData(&buf[pt], cmd2);    /* DDDD,LL */
		    pt += cmd2;                  /* V0.17-C */
                }
                break;

              case 11:	/* Bn cc vv (Control Change) */
                printf("%02x %02x %02x       : ",cmd1,cmd2,cmd3);
                switch (cmd2) {
                  case 0x00:
                    printf("Bank Select (MSB) 0x%02x\n", cmd3);
                    break;
                  case 0x20:
                    printf("Bank Select (LSB) 0x%02x\n", cmd3);
                    break;
                  case 0x01:
                    printf("Modulation Depth:%d\n", cmd3);
                    break;
                  case 0x02:   /* V0.19-A */
                    printf("Breath Controller:%d\n", cmd3);
                    break;
                  case 0x04:   /* V0.19-A */
                    printf("Foot Controller:%d\n", cmd3);
                    break;
                  case 0x05:
                    printf("Portament Time %d\n", cmd3);
                    break;
                  case 0x06:
                    printf("Data Entry (MSB) 0x%02x\n", cmd3);
                    break;
                  case 0x26:
                    printf("Data Entry (LSB) 0x%02x\n", cmd3);
                    break;
                  case 0x07:
                    printf("Volume %d%% (%d/127)\n",(cmd3*100/127),cmd3);
                    break;
                  case 0x08:   /* V0.19-A */
                    printf("Barance:%d\n", cmd3);
                    break;
                  case 0x0a:
                    printf("Panpot %d (left:0 - right:127)\n", cmd3);
                    break;
                  case 0x0b:
                    printf("Expression %d\n", cmd3);
                    break;
                  case 0x40:
                    printf("Hold1(Sustain) val:%d\n", cmd3);
                    break;
                  case 0x41:
                    printf("Portament val:%d\n", cmd3);
                    break;
                  case 0x42:
                    printf("Sostenuto val:%d\n", cmd3);
                    break;
                  case 0x43:
                    printf("Soft val:%d\n", cmd3);
                    break;
                  case 0x44:   /* V0.19-A */
                    printf("Legato Footswitch:%d\n", cmd3);
                    break;
                  case 0x45:   /* V0.19-A */
                    printf("Hold2 (Freeze):%d\n", cmd3);
                    break;
                  case 0x46:   /* V0.19-A */
                    printf("Sound Control1  (def:Sound Variation):%d\n", cmd3);
                    break;
                  case 0x47:   /* V0.19-A */
                    printf("Sound Control2  (def:Timbre/Harmonic Intensity):%d\n", cmd3);
                    break;
                  case 0x48:   /* V0.19-A */
                    printf("Sound Control3  (def:Release Time):%d\n", cmd3);
                    break;
                  case 0x49:   /* V0.19-A */
                    printf("Sound Control4  (def:Attack Time):%d\n", cmd3);
                    break;
                  case 0x4a:   /* V0.19-A */
                    printf("Sound Control5  (def:Brightness):%d\n", cmd3);
                    break;
                  case 0x4b:   /* V0.19-A */
                    printf("Sound Control6  (def:Decay Time):%d\n", cmd3);
                    break;
                  case 0x4c:   /* V0.19-A */
                    printf("Sound Control7  (def:Vibrato Rate):%d\n", cmd3);
                    break;
                  case 0x4d:   /* V0.19-A */
                    printf("Sound Control8  (def:Vibrato Depth):%d\n", cmd3);
                    break;
                  case 0x4e:   /* V0.19-A */
                    printf("Sound Control9  (def:Vibrato Delay):%d\n", cmd3);
                    break;
                  case 0x4f:   /* V0.19-A */
                    printf("Sound Control10 (not defined):%d\n", cmd3);
                    break;
                  case 0x50:   /* V0.19-A */
                    printf("General Purpose Control5:%d\n", cmd3);
                    break;
                  case 0x51:   /* V0.19-A */
                    printf("General Purpose Control6:%d\n", cmd3);
                    break;
                  case 0x52:   /* V0.19-A */
                    printf("General Purpose Control7:%d\n", cmd3);
                    break;
                  case 0x53:   /* V0.19-A */
                    printf("General Purpose Control8:%d\n", cmd3);
                    break;
                  case 0x54:   /* V0.19-A */
                    printf("Portamento Contorl:%d\n", cmd3);
                    break;
                  case 0x58:   /* V0.19-A */
                    printf("High Resolution Velocity Prefix:%d\n", cmd3);
                    break;
                  case 0x5b:
                    printf("Effect1 (revarb send level) level:%d\n", cmd3);
                    break;
                  case 0x5c:   /* V0.19-A */
                    printf("Effect2 (tremolo depth)     Depth:%d\n", cmd3);
                    break;
                  case 0x5d:
                    printf("Effect3 (choras send level) level:%d\n", cmd3);
                    break;
                  case 0x5e:   /* V0.19-A */
                    printf("Effect4 (celeste depth)     Depth:%d\n", cmd3);
                    break;
                  case 0x5f:   /* V0.19-A */
                    printf("Effect5 (phaser depth)      Depth:%d\n", cmd3);
                    break;
                  case 0x60:   /* V0.19-A */
                    printf("Data Increment");
                    break;
                  case 0x61:   /* V0.19-A */
                    printf("Data Decremetn");
                    break;
                  case 0x63:
                    printf("NRPN MSB\n");
                    break;
                  case 0x62:
                    printf("NRPN LSB\n");
                    break;
                  case 0x65:
                    printf("PRN MSB\n");
                    break;
                  case 0x64:
                    printf("PRN LSB\n");
                    break;
                  case 0x78:
                    printf("All Sound Off\n");       /* V0.19-C */
                    break;
                  case 0x79:
                    printf("Reset All Controler\n");
                    break;
                  case 0x7b:
                    printf("All Notes Off\n");
                    break;
                  case 0x7c:
                    printf("OMNI OFF\n");
                    break;
                  case 0x7d:
                    printf("OMNI ON\n");
                    break;
                  case 0x7e:
                    printf("MONO Mode\n");
                    break;
                  case 0x7f:
                    printf("POLY Mode\n");
                    break;
                  default:
                    printf("Unknown Control Change.\n");
                    break;
                }
                //if (nlen == 0xff) {  /* ← XXXX 必要? V0.13-D */
                    pt += 3;
                //} else {
                //    pt += 4;
                //}
                break;

              default:
                if (cmd1 & 0x80) {  /* V0.13-C */
                    printf("unknown status byte 0x%02x\n",cmd1);
                    pt += 1;
                }
                break;
            }

#ifdef WIN_MIDIOUT
	    if (playf) {
	        if (msec) Sleep(msec); /* V0.16-M */
	        WinMidiShortMsg(&minst, MIDIMSG(cmd1, cmd2, cmd3));
	        if (vf && (dismid && msec)) printf("Delta: %-7s (%d) : wait %d msec\n", codebuf, delta, msec);
	    }
#endif

        }
    }
    if (vf) printf("# Total Delta: %d (%d msec) %d\n", delta_sum, msec_sum, Delta2MilliSec(delta_sum, tempo));
} 

/*
 *  Read MTrk data block and midi data
 *
 *   [DeltaTime][MidiMessage]
 *
 *   IN  buf   : Track data
 *   IN  len   : Track data len
 *   IN  trknm : Track number
 *   OUT playdat[trknm][] : play data [DeltaTime][MidiMessage]
 *
 *        +0          +4
 *       +--+--+--+--+--+--+--+--+
 *       | DeltaTime |XX|D2|D1|ST|
 *       +--+--+--+--+--+--+--+--+
 */
void ReadTrack(buf, len, trknm)
unsigned char *buf;
int len;
int trknm;
{ 
    int  pt = 0;
    int  cmd1,cmd2,stat,ch,cmd3,nlen;
    char nbuf[128];
    char codebuf[128];
    char wk[16];
    int  i;
    int  first = 1;
    int  size;
    int  delta = 0;        /* DeltaTime                    */
    int  delta_sum = 0;    /* DeltaTime Total              */ 
    int  next_mes  = 0;    /* 次の小節のDelta値            */ 
    int  measure   = 1;    /* 現在の小節番号               */ 
    int  start_note = 0;   /* 最初の音が出たか             */ 
    int  msec;             /* wait msec                    */ 
    int  msec_total = 0;   /* wait msec total              */ 
    int  val;              /* work value                   */ 
    int  idx = 0;          /* midi data pointer            */
    int  shortmsg;         /* midi short msg               */

    memset(wk,     0, sizeof(wk));
    memset(nbuf,   0, sizeof(nbuf));
    memset(nbuf, ' ', 64);
    while (pt < len) {
	size = 0;
	shortmsg = 1;

	/* 先頭には必ずデルタタイムがある V0.16-A */
	delta = GetDeltaTime(&buf[pt], codebuf, &size);   /* pt 更新 */
	pt += size;
	msec = Delta2MilliSec(delta, tempo);
	msec_total += msec;
	delta_sum  += delta;

#if 0 /* 小節区切り */
	if ((buf[pt] >> 4) == 9) {  /* 9n */
	    start_note = 1;		/* 最初のNote Onがあった所からカウントする */
	}
	if (start_note) {
	    //delta_sum += delta;
	    while (delta_sum >= next_mes) {
		if (dismid) printf("# Measure %d\n", measure);
		next_mes += hd_dtime * 4;  /* 一小節分先へ */
		++measure;
	    }
	}
#endif

        if (buf[pt] == 0xff) {
            /* Meta Event  ffxxLLDDDDDDD...  */
            cmd1 = buf[pt];             /* 0xff           */
            cmd2 = buf[pt+1];		/* sub command    */
            nlen = buf[pt+2];           /* command len    */
            pt += 3;		        /* skip ffxxLL    */

            nlen = GetDeltaTime(&buf[pt-1], codebuf, &size);  /* get strlen V0.20-A */
            pt += (size-1);                              /* skip ffxx<LL..> V0.20-A */

	    switch(cmd2) {
              case 0x51: /* ff51 03 tttttt  Set Tempo */
	        tempo = buf[pt]*256*256 + buf[pt+1]*256 + buf[pt+2]; /* tempo */
	        printf("%02x %02x %02x : ", buf[pt], buf[pt+1], buf[pt+2]); /* V0.15-A */
	        printf("SUB(51):Set Tempo : %d (MM=%d)\n", tempo, 60 * 1000000/tempo);

	        // テンポだけは再生時に必要なためMetaだがデータ格納する  
	        playdat[trknm][idx*2]   = delta_sum;     /* Delta Time 累計で格納 */
	        playdat[trknm][idx*2+1] = (tempo | 0xff000000); /* FFtttttt */
	        ++idx;
	        AddTempoDat(delta_sum, tempo);                           /* V0.21-A */
	        break;
              default: /* other event */
	        break;
	    }
            pt += nlen;			/* skip len & data area */
            first = 1;
        } else {
            /* MIDI data */
#if 0
            if (first) {
                printf("ch1 ch2 ch3 ch4 ch5 ch6 ch7 ch8 ch9 c10 c11 c12 c13 c14 c15 c16\n");
                first = 0;
            }
#endif
	    if ((buf[pt] & 0x80) == 0x80) {
		/* status byte である */
                cmd1 = buf[pt];             /* status byte    */
	    } else {
		/* status byte でなかった場合、RunningStatusであるため
		 * 前回のstatusが継続する V0.13-C 
		 */
		--pt;
		//dprintf("Run:");
	    }
            stat = cmd1 >> 4;           /* status         */
            ch   = cmd1 & 0xf;          /* channel        */
            cmd2 = buf[pt+1];		/* note no byte   */
            cmd3 = buf[pt+2];		/* velocity byte  */

            switch(stat) {
	      case 8:	/* 8n kk vv  Note Off Data:2byte */
	      case 10:	/* An kk vv (Polyphonic After Touch)  Data: 2byte */
              case 11:	/* Bn cc vv (Control Change) */
	      case 14:	/* En mm ll (Pitch Wheel)   Data:2byte */
	        pt += 3;
                break;

	      case 12:	/* Cn pp (Program Change)  Data:1byte */
	      case 13:	/* Dn vv (Channel After Touch)  Data:1byte */
                pt += 2;
		cmd3 = 0;
                break;

	      case 9:	/* 9n kk vv  Note On Data:2byte */
	        pt += 3;
	        /* note = note[cmd2]    */
	        /* Note On but Velocity 0 then Note Off */
                break;

              case 15:	/* Fx ..... */
                if (cmd1 == 0xfe) {         /* Active Sensing (every 250msec) */
                    ++pt;
                } else if (cmd1 == 0xf0) {  /* System Exclusive message(f0)   */
		    pt += 2;
                    while (buf[pt] != 0xf7 && buf[pt] != 0xff) {
                        ++pt;
                    }
                    ++pt;                   /* fix bug V0.19-A                */
                } else if (cmd1 == 0xf7) {  /* System Exclusive message(f7)   */
		    pt += 2;
		    pt += cmd2;
                }
		shortmsg = 0;  /* no midi short msg */
                break;

              default:
                if (cmd1 & 0x80) {  /* V0.13-C */
                    /* unknown status byte */
                    pt += 1;
                }
		shortmsg = 0;  /* no midi short msg */
                break;
            }
#ifdef WIN_MIDIOUT
	    if (shortmsg) {
	        //playdat[trknm][idx*2]   = msec_total;  /* msで格納すると誤差でずれる */
	        playdat[trknm][idx*2]   = delta_sum;     /* Delta Time 累計で格納 */
	        playdat[trknm][idx*2+1] = MIDIMSG(cmd1, cmd2, cmd3);
	        ++idx;
	    }
#endif
        }
    }
    playdat[trknm][idx*2]   = EOD;
}

/*
 *  Analyze MTrk block
 *
 *   IN  fp
 *       buf (no use)
 *   OUT ret :  0 success
 *             -1 error
 */
int GetMidiRk(fp,buf)
FILE *fp;
char *buf;
{
    unsigned char wk[4096];
    unsigned char name[512];
    int  *size = (int *)wk;
    int  len;
    int  loc = 0;
    int  ret;
    long pos;
    unsigned char *track;

    mtck_t *pmtrk = (mtck_t *)wk;  /* V0.14-A */

    /* V0.14-C start */
    strcpy(wk, "");

    ret = fread(pmtrk, 1, SIZE_MTRK, fp);  /* read "MTrk" Chank Header area */
    if (ret == 0) {
	/* EOF */
	return -1;
    }
    if (strncmp(pmtrk->mtck_key, KEY_MTRK, strlen(KEY_MTRK))) {
        /* not MIDI file */
        printf("Error : Not MTrk [%s]\n",wk);
        return -1;
    }
    /* 出力対象トラックならば出力(MTrkのチャンクヘッダ部) */
    if (ofp && CheckTrkNo(trkno+1)) fwrite(pmtrk, 1, SIZE_MTRK, ofp);

    len = htonl(pmtrk->size);   
    /* V0.14-C end   */

    printf("\nMTrk: len = %d\n",len );  /* V0.13-A */

    track = (unsigned char *)malloc(len);
    if (!track) {
        printf("GetMidiRk: malloc error. errno=%d\n",errno);
        return -1;
    }
    pos = ftell(fp);
    fread(track,1,len,fp);	/* get data body */
    printf("# Track %d Start.  Pos:%d Len:%d\n", trkno+1, pos, len);
    if (vf > 1) {
        printf("MTrk:%4d:",len);
        DumpData(track,len);
    }
    if (playf) {
	playdat[trkno] = (int *)malloc(sizeof(u_int)*len);   /* delta + midi dat */
	memset(&playdat[trkno][0], 0, sizeof(u_int)*len);
        ReadTrack(track, len, trkno);
    } else {
        AnalyzeTrack(track,len);
    }

    /* 出力対象トラックならば出力(MTrkのデータ部) */
    if (ofp && CheckTrkNo(trkno+1)) fwrite(track, 1, len, ofp);

    printf("# Track %d End.\n",trkno+1);
    ++trkno;

    free(track);

    return 0;
}

/* V0.21-A start */
/*  Ms2Delta()
 *    milli second to delta time
 *
 *  IN  ms  : milli second
 *  OUT ret : delta time
 *
 */
u_int Ms2Delta(u_int ms)
{
	u_int wkdelta = 0;
	int   i;

	if (temponum == 0) {
		return MS2DELTA(ms);
	}

	for (i = 0; i < temponum; i++) {
		if (ms < tempodat[i].ms) break;
	}
	if (i == 0 || tempodat[i-1].tempo == 0) {
		return MS2DELTA2(ms, DEF_TEMPO);
	} 
	return tempodat[i-1].delta + MS2DELTA2(ms-tempodat[i-1].ms, tempodat[i-1].tempo);
}

/*  Delta2Ms()
 *    delta to milli second 
 *
 *  IN  delta: delta time
 *  OUT ret  : milli second
 *
 */
u_int Delta2Ms(u_int delta)
{
	u_int wkdelta = 0;
	int   i;

	if (temponum == 0) {
		return Delta2MilliSec(delta, DEF_TEMPO);
	}

	for (i = 0; i < temponum; i++) {
		if (delta < tempodat[i].delta) break;
	}
	if (i == 0 || tempodat[i-1].tempo == 0) {
		return Delta2MilliSec(delta, DEF_TEMPO);
	} 
	return tempodat[i-1].ms + Delta2MilliSec(delta-tempodat[i-1].delta, tempodat[i-1].tempo);
}
/* V0.21-A end   */

/*
 *  PlayData()
 *
 *    ReadTrack()で読み込んだデータを再生する  
 *
 */
void PlayData()
{
#ifdef _WIN32
    int   trk;
    int   done;
    int   measure  = 0;     /* 小節の切れ目のDelta Time */
    int   skipping = 0;     /* 1: 再生スキップ中        */
    int   pos[MAX_TRACK];
    u_int tick = 0;
    u_int wk;
    u_int ch;             /* channel */

    memset(pos, 0, sizeof(pos));
    tempo = 500000;    /* Tempoのデフォルト値μs(4分♪=120:0.5sec) */ 
    orgtempo = tempo;  /* 本来のテンポ値                           */

    //WinMidiOpen(&minst);
 
    if (sttime) skipping = 1; /* V0.18-A */

    while(1) {
	done = 0;
	if (measure * hd_dtime * 4 <= Ms2Delta(tick)) {         /* V0.21-C */
	    if (vf && !skipping) printf("--- %4d ---------------------------------------------------------------\n", measure + 1);
	    ++measure;
	}
	for (trk = 0; trk < trkno; trk++) {
	    /* DeltaTime 相当の時間にきたら次のイベントを再生 */
	    while (playdat[trk][pos[trk]*2] <= Ms2Delta(tick))  /* delta sum V0.21-C */
	    {
		/* V0.18-A start */
		if (sttime) {
		    /* printf("XXX dat_delta:%d \n"); */
		    if (playdat[trk][pos[trk]*2] < Ms2Delta(sttime*1000)) {
		        /* skipping = 1; */
		    } else {
		        skipping = 0;
		    }
		}
		/* V0.18-A end   */

		wk = playdat[trk][pos[trk]*2+1];                /* midi data */
		ch = wk & 0xf;
		if (vf) {
		    u_int st = wk & 0xff;
		    u_int kk = (wk & 0xff00)   >> 8;
		    u_int vv = (wk & 0xff0000) >> 16;
		    int   noteon = 0;

		    if (((st & 0xf0) == 0x90) && vv != 0) {
			noteon = 1;
		    }
		    if (cf == 0 || cf && CheckTrkNo(ch+1)) {   /* -cの場合指定chのみ表示 */
			if (!skipping ) {
		            printf("Cur:%7dms Play Tick:%7d Trk:%2d Pos:%5d Ch:%2d Dat:%02x %02x %02x  %s\n",
                                   tick, playdat[trk][pos[trk]*2],
			           trk, pos[trk], ch+1, 
			           st, kk, vv, noteon?note[kk]:"");
		    	}
		    }
		}

		if (wk >> 24 == 0xff) {
		    /* メタイベント (Set Tempo) */
		    tempo = wk & 0xffffff;
		    orgtempo = tempo;         /* 本来のテンポを覚えておく V0.18-A */
		    if (playf && fixtempo) {  /* V0.18-C */
	        	printf("SUB(51):Set Tempo : %d (MM=%d) ... Ignored. assume %d (MM=%d).\n", 
				tempo, 60 * 1000000/tempo,
				fixtempo, 60 * 1000000/fixtempo);
		    	tempo = fixtempo;
		    } else {
	                printf("Set Tempo : %d (MM=%d)\n", tempo, 60 * 1000000/tempo);
		    }
		} else {
		    /* メタイベント以外は再生 */
		    if (cf == 0 || cf && CheckTrkNo(ch+1)) {
			/* -cの場合指定chのみ発音 */
			if (!skipping || ((wk & 0xf0) == 0xc0)) {
			    /* skip中以外 または、Prog Changeは再生 */
	                    WinMidiShortMsg(&minst, wk);
			}
		    }
		}
		++pos[trk];
	    }
	    if (playdat[trk][pos[trk]*2] == EOD) ++done;
	}
	if (done >= trkno) break;
	if (!skipping) usleep(TICK * 1000);
	tick += TICK;
    }
    usleep(1000*1000);  /* いきなり終わらないよう1秒待つ */
    //WinMidiClose(&minst);
#endif /* _WIN32 */
}

void usage()
{
    printf("midana Ver %s\n", VER);
    printf("usage: midana [-a] [-o <trk1>,<trk2>,..]\n");
    printf("              [-p [-c <ch1>,<ch2>,..] [-t <tempo>] [-s [mm:]ss]]\n");
    printf("              [<midi_filename>]\n");
    printf("       -a : disp all midi command\n");
    printf("       -p : play (for win only)\n");
    printf("       -o : output spec track(1-) data\n");
    printf("       -c : play specified channel(1-) data\n");
    printf("       -t : play specified tempo\n");
    printf("       -s : play start from specified time\n");
    exit(1);
}

int main(a,b)
int a;
char *b[];
{
    int  i;
    char *filename = NULL;
    char buf[4096];
    char *pt;                /* work           V0.14-A */
    char *chstr = NULL;      /* channel string V0.14-A */
    FILE *fp;

    strcpy(ofname, "");
    memset(playdat, 0, sizeof(playdat));

    for (i = 1; i<a; i++) {
        if (!strncmp(b[i],"-v",2)) {
            ++vf;
            continue;
        }
        if (!strncmp(b[i],"-a",2)) {
            dismid = 1;
            continue;
        }
        if (!strncmp(b[i],"-p",2)) {  /* V0.13-A */
            playf = 1;
            continue;
        }
        if (i+1 < a && !strncmp(b[i], "-o", 2)) {  /* V0.14-A */
            chstr = b[++i];
	    of = 1;
            continue;
        }
        if (i+1 < a && !strncmp(b[i], "-c", 2)) {  /* V0.17-A */
            chstr = b[++i];
	    cf = 1;
            continue;
        }
        if (i+1 < a && !strncmp(b[i], "-s", 2)) {  /* V0.18-A */
	    ++i;
	    if (pt = strchr(b[i], ':')) {
		++pt;
                sttime = atoi(b[i]) * 60 + atoi(pt);    /* mm:ss */
	    } else {
                sttime = atoi(b[i]);                    /* ss    */
	    }
            continue;
        }
        if (i+1 < a && !strncmp(b[i], "-t", 2)) {  /* V0.18-A */
            fixtempo = 1000000*60/atoi(b[++i]);
	    tempo = fixtempo;
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

#ifdef WIN_MIDIOUT
    if (playf) {
    	WinMidiOpen(&minst);
    }
#endif

    printf("\n# Analyze MIDI file.  by oga.\n");

    /* V0.14-A V0.17-C start */
    if (of || cf) {
	pt = chstr;
	do {
	    if (*pt == ',') ++pt;
	    chlist[nch++] = atoi(pt);
	    printf("track %d\n", chlist[nch-1]);
	} while ((pt = strchr(pt, ',')) && nch < MAX_CHLIST);
    }
    if (of) {
	if (filename) {
	    strcpy(ofname, filename);
	} else {
	    strcpy(ofname, "stdin.mid");
	}
	pt = strrchr(ofname, '.');
	if (pt) {
	    *pt = '\0';
	    strcat(ofname, "_");
	    strcat(ofname, chstr);
	    strcat(ofname, ".mid");
	}
    }
    /* V0.14-A start */

    if (GetMidiHeader(fp,buf)) {
        printf("This is not MIDI format file.\n");
        exit(1);
    }

    while (!GetMidiRk(fp,buf)) ;

    if (fp && fp != stdin) fclose(fp);
    if (ofp) fclose(ofp);

    printf("MIDI file Analyze end.\n");

#ifdef WIN_MIDIOUT
    if (playf) {
        PlayData();
        WinMidiClose(&minst);

	for (i = 0; i<MAX_TRACK; i++) {
	    if (playdat[i]) free(playdat[i]);
	}
    }
#endif




    return 0;
}

/* vim:ts=8:sw=4:
 */

