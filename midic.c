/*
 *  midic: midi compiler
 *
 *   11/10/01 V0.10 by oga.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* script format */
/*
-------------------------
# comment
SmfFormat=1                             (è»ó™â¬)
# 480: 1/4
# 240: 1/8
# 120: 1/16
DeltaTime=480                           (è»ó™â¬)

[Track1]
TrackName=Track1                        (è»ó™â¬: "")
# 00ff03lldddddd...
Copyright=Copyright (C)2011 oga. Labo.  (è»ó™â¬: Ç»Çµ)
# 00ff02lldddddd...
Tempo=120                               (è»ó™â¬: 120)
# 00ff5103tttttt
Beat=4/4                                (è»ó™â¬: 4/4)  {4/4|3/4|6/8|2/2}
# 00ff5804 0402 1808  

[Track2]
TrackName=Track2
ChannelPrefix=0
# 00ff2101nn
<Delta>:9xnnvv  904364   (Note On ) G5
<Delta>:8xnn00  804300   (Note Off)
MML=t120o4l4cdef#gabc+d+e+f#+
-------------------------
 */

/* globals */
int  vf = 0;           /* verbose */

/* for header */
int   HeadLen    = 6;
short SmfFormat  = 1;
short NumOfTrack = 0;
short DeltaTime  = 480; /* 1/4 time */

/* for track */
int  Tempo         = 120; /* MM=120 : 500000us */
int  ChannelPrefix = 0;   /* Channel Prefix    */
int  Beat1         = 4;   /* Beat1/Beat2       */
int  Beat2         = 4;   /* Beat1/Beat2       */

#define MAX_TRACK   16
#define ALLOC_SIZE  (500*1024)   /* 500KB/track */

char *TrackData[MAX_TRACK];
int  TrackLen[MAX_TRACK];

/* for Midi Header */
#define ID_HEAD        "MThd"

/* for Midi Track  */
#define ID_TRACK       "MTrk"

#define KEY_SMFFMT     "SmfFormat="
#define KEY_DELTA      "DeltaTime="

#define SECT_TRACK     "[Track"
#define KEY_TRACKN     "TrackName="
#define KEY_COPYR      "Copyright="
#define KEY_TEMPO      "Tempo="
#define KEY_BEAT       "Beat="
#define KEY_CHPREFIX   "ChannelPrefix="

/* for Midi Command */
#define CMD_EndOfTrack "ff2f00"

/*
 *  output syntax error & msg
 *
 *   IN : buf  : error script line data
 *        msg  : error message
 */
void SyntaxErr(char *buf, char *msg)
{
	printf("Syntax Error: [%s]\n", buf);
	if (msg != NULL && strlen(msg)) {
		printf("%s\n", msg);
	}
}

/*
 *  read script file
 *
 *   IN : fp   : script file pointer
 */
int ReadMidiScript(FILE *fp)
{
	char buf[2048];
	char msg[256];
	char Copyright[4096];
	char TrackName[4096];
	char CmdStr[4096];
	char *cmd;
	int  CurTrack  = -1;    /* current track number  */
	int  PrevTrack = -1;    /* previous track number */

	while (fgets(buf, sizeof(buf), fp)) {
		/* delete CR/LF */
		if (buf[strlen(buf)-1] == 0x0a) {
			buf[strlen(buf)-1] = '\0';
		}
		if (buf[strlen(buf)-1] == 0x0d) {
			buf[strlen(buf)-1] = '\0';
		}
		if (vf > 1) printf("DEBUG: ReadMidiScript2: buf=[%s]\n", buf);
		if (strlen(buf) == 0) continue;   /* blank line */
		if (buf[0] == '#')    continue;   /* comment    */

		/* Common Info */
		if (!strncmp(buf, KEY_SMFFMT, strlen(KEY_SMFFMT))) {
			SmfFormat = atoi(&buf[strlen(KEY_SMFFMT)]);
			if (vf) printf("SmfFormat=%d\n", SmfFormat);
			continue;
		}
		if (!strncmp(buf, KEY_DELTA, strlen(KEY_DELTA))) {
			DeltaTime = atoi(&buf[strlen(KEY_DELTA)]);
			if (vf) printf("DeltaTime=%d\n", DeltaTime);
			continue;
		}

		/* Track */
		if (!strncmp(buf, SECT_TRACK, strlen(SECT_TRACK))) {
			PrevTrack = CurTrack;
			CurTrack = atoi(&buf[strlen(SECT_TRACK)]) - 1;
			if (CurTrack < 0 || CurTrack >= MAX_TRACK) {
				sprintf(msg, "The track number must be %d or less.", MAX_TRACK);
				SyntaxErr(buf, msg);
				return -1;
			}

			if (TrackData[CurTrack]) {
				SyntaxErr(buf, "Duplicate Track");
				return -1;
			}
			TrackData[CurTrack] = (char *)malloc(ALLOC_SIZE);
			memset(TrackData[CurTrack], 0, ALLOC_SIZE);
			++NumOfTrack;
			if (PrevTrack >= 0) {
				// add EOT
				AddMidiCommand(TrackData[PrevTrack], &TrackLen[PrevTrack], 0, CMD_EndOfTrack);
			}
			if (vf) printf("## Track : %d\n", CurTrack);
			continue;
		}

		if (CurTrack >= 0) {
			/* Track Name (str) */
			if (!strncmp(buf, KEY_TRACKN, strlen(KEY_TRACKN))) {
				strcpy(TrackName, &buf[strlen(KEY_TRACKN)]);
				if (vf) printf("TrackName=[%s]\n", TrackName);
				if (strlen(TrackName) > 255) {
					SyntaxErr(buf, "Track name too long.");
					TrackName[256] = '\0';
				}
				AddMidiCmdStr(TrackData[CurTrack], &TrackLen[CurTrack], "ff03", TrackName);
				continue;
			}

			/* Copyright (str) */
			if (!strncmp(buf, KEY_COPYR, strlen(KEY_COPYR))) {
				strcpy(Copyright, &buf[strlen(KEY_COPYR)]);
				if (vf) printf("Copyright=[%s]\n", Copyright);
				if (strlen(Copyright) > 255) {
					SyntaxErr(buf, "Copyright too long.");
					Copyright[256] = '\0';
				}
				AddMidiCmdStr(TrackData[CurTrack], &TrackLen[CurTrack], "ff02", Copyright);
				continue;
			}

			/* Tempo */
			if (!strncmp(buf, KEY_TEMPO, strlen(KEY_TEMPO))) {
				Tempo = atoi(&buf[strlen(KEY_TEMPO)]);
				if (vf) printf("Tempo=%d\n", Tempo);
				sprintf(CmdStr, "ff5103%06x", 60000000/Tempo);  /* ff5103<ms> */
				AddMidiCommand(TrackData[CurTrack], &TrackLen[CurTrack], 0, CmdStr);
				continue;
			}

			/* Channel Prefix */
			if (!strncmp(buf, KEY_CHPREFIX, strlen(KEY_CHPREFIX))) {
				ChannelPrefix = atoi(&buf[strlen(KEY_CHPREFIX)]);
				if (vf) printf("ChannelPrefix=%d\n", ChannelPrefix);
				sprintf(CmdStr, "ff2101%02x", ChannelPrefix);  /* ff2101<chprefix> */
				AddMidiCommand(TrackData[CurTrack], &TrackLen[CurTrack], 0, CmdStr);
				continue;
			}

			/* Beat */
			if (!strncmp(buf, KEY_BEAT, strlen(KEY_BEAT))) {
				char *pt = strchr(&buf[strlen(KEY_BEAT)], '/');
				if (pt == NULL) {
					sprintf(msg, "ex.)%s4/4\n", KEY_BEAT);
					SyntaxErr(buf, msg);
				} else {
					int k, Beat2val = 0;

					Beat1 = atoi(&buf[strlen(KEY_BEAT)]);
					++pt;  /* skip '/' */
					Beat2 = atoi(pt);
					if (vf) printf("Beat=%d/%d\n", Beat1, Beat2);
					for (k = 0; k < 30; k++) {
						if ((Beat2 >> k) & 1) {
							Beat2val = k;
							break;
						}
					}
					sprintf(CmdStr, "ff5804%02x%02x1808", Beat1, Beat2val); /*ff5804xxyy1808*/
					AddMidiCommand(TrackData[CurTrack], &TrackLen[CurTrack], 0, CmdStr);
				}
				continue;
			}

			/* midi command          */
			/* <delta>:9xnnvv        */
			if (cmd = strchr(buf, ':')) {
				char *delta_pt = &buf[0];
				int  delta;

				while(isspace(delta_pt[0])) ++delta_pt;
				if (isdigit(delta_pt[0])) {
					++cmd;   /* skip ":" */
					delta = atoi(delta_pt);
					AddMidiCommand(TrackData[CurTrack], &TrackLen[CurTrack], delta, cmd);
				} else {
					SyntaxErr(buf, "no delta len");
				}
				continue;
			}
		}
		if (CurTrack < 0) {
			SyntaxErr(buf, "No Track Section.");
		} else {
			SyntaxErr(buf, "unknown command");
		}
	}
	if (CurTrack >= 0) {
		AddMidiCommand(TrackData[CurTrack], &TrackLen[CurTrack], 0, CMD_EndOfTrack);
	}
	return 0;
}

/*
 *  AddDelta()
 *
 *  IN    : delta    : delta value
 *  IN/OUT: trackdat : track data
 *          len      : tracklen
 *
 *  DeltaTime bytes: MSB is continuation flag
 *                   7bit time data
 */
int AddDelta(unsigned char *trackdat, int *len, int delta)
{
	char work_delta[4];
	int  i;
	int  cnt = 0;

	/* delta to bytes */
	if (delta == 0) {
		work_delta[0] = delta;
		++cnt;
	} else {
		while (delta) {
			work_delta[cnt] = delta & 0x7f;
			if (vf > 1) printf("AddDelta: work_delta[%d] = 0x%02x\n", cnt, work_delta[cnt]);
			delta = delta >> 7;
			++cnt;
		}
	}

	for (i = 0; i<cnt; i++) {
		trackdat[*len + i] = work_delta[(cnt-1)-i];
		if (i < cnt-1) {
			trackdat[*len + i] |= 0x80;  /* set continuation flag */
		}
		if (vf > 1) printf("AddDelta: trackdat[%d] = 0x%02x\n", *len+i, trackdat[*len+i]);
	}
	*len += cnt;

	return 0;
}

/*
 *  AddMidiCommand()
 *
 *  IN    : delta    : delta time
 *        : cmd      : midi command (hexadecimal ascii string)
 *  IN/OUT: trackdat : track data
 *          len      : tracklen
 *
 */
int AddMidiCommand(unsigned char *trackdat, int *len, int delta, char *cmd)
{
	char work[128];

	if (vf > 1) printf("AddMidiCommand: cmd=%s [%c] %d\n", cmd, *cmd, !isspace(*cmd));
	/* add delta */
	AddDelta(trackdat, len, delta);

	/* add midi command */
	strcpy(work, "0x");
	while ((*cmd != '\0') && (!isspace(*cmd)) && (*cmd != '#')) {
		strncpy(&work[2], cmd, 2);
		work[4] = '\0';
		trackdat[*len] = (unsigned char)strtoul(work,(char **)NULL,0);
		if (vf > 1) printf("AddMidiCommand: %s\n", work);
		++(*len);
		++cmd;
		++cmd;
	}
	if (vf > 1) printf("AddMidiCommand: len = %d\n", *len);

	return 0;
}

/*
 *  AddMidiCmdStr()  
 *     add MIDI command & string
 *
 *  IN    : delta    : always zero
 *        : cmd      : midi command (hexadecimal ascii string)
 *        : str      : string 
 *  IN/OUT: trackdat : track data
 *          len      : tracklen
 *
 */
int AddMidiCmdStr(unsigned char *trackdat, int *len, char *cmd, char *str)
{
	char CmdStr[4096];

	if (strlen(str) > 255) {
		return -1;
	}

	/* add command */
	sprintf(CmdStr, "%s%02x", cmd, strlen(str));
	AddMidiCommand(trackdat, len, 0, CmdStr);

	/* add string */
	strcpy(&trackdat[*len], str);
	*len += strlen(str);

	return 0;
}

/*
 *  write midi data
 *
 *   IN : fp   : output file pointer
 */
int WriteMidi(FILE *fp)
{
	int   i;
	int   worki;
	short works;
	int   pos  = 0;
	int   size = 0;

	/* header */
	fwrite(ID_HEAD, 1, strlen(ID_HEAD), fp);

	/* convert to big endian and write*/
	worki = htonl(HeadLen);
	fwrite(&worki, 1, sizeof(worki), fp);
	works = htons(SmfFormat);
	fwrite(&works, 1, sizeof(works), fp);
	works = htons(NumOfTrack);
	fwrite(&works, 1, sizeof(works), fp);
	works = htons(DeltaTime);
	fwrite(&works, 1, sizeof(works), fp);

	/* track */
	for (i = 0; i < MAX_TRACK; i++) {
		if (TrackData[i]) {
			fwrite(ID_TRACK, 1, strlen(ID_TRACK), fp);
			worki = htonl(TrackLen[i]);
			fwrite(&worki, 1, sizeof(worki), fp);
			if (vf) printf("Write Track%d len=%d\n", i+1, TrackLen[i]);
			pos = 0;
			while (pos < TrackLen[i]) {
				size = TrackLen[i] - pos;
				if (size > 4096) size = 4096;
				size = fwrite(&TrackData[i][pos], 1, size, fp);
				if (size < 0) {
					perror("fwrite");
					return -1;
				}
				pos += size;
			}
		}
	}
	return 0;
}

/*
 *   Disp Script Sample
 */
void DispScriptSample()
{
	printf("#SmfFormat=1\n");
	printf("#DeltaTime=480      # 480 = 1/4\n");

	printf("[Track1]\n");
	printf("#TrackName=Track1\n");
	printf("#Copyright=Copyright (C)2011 xxx.\n");
	printf("#Beat=4/4  # {4/4 | 3/4 | 6/8 | 2/2}\n");
	printf("#Tempo=120\n");

	printf("#[Track2]\n");
	printf("#TrackName=Track2\n");
	printf("#ChannelPrefix=0\n");
	printf("<DeltaLen>:9xnnvv  #904364   (Note On ) G5\n");
	printf("<DeltaLen>:9xnn00  #904300   (Note Off)\n");
	printf("<DeltaLen>:8xnnvv  #804300   (Note Off)\n");
	printf("<DeltaLen>:Cxnn    #c072     Program Change (SteelDrum)\n");
	printf("# DeltaLen ... 1920:1/1  960:1/2  480:1/4  240:1/8  120:1/16\n");
}

void usage()
{
	printf("usage: midic <script> [-o <output.mid>]\n");
	printf("usage: midic [-sample]\n");
	printf("       -sample: output sample script\n");
	exit(1);
}

int main(int a, char *b[])
{
	int  i;
	char *filename = NULL;
	char ofname[2048];
	FILE *ifp;
	FILE *ofp;


	strcpy(ofname, "");
	for (i = 1; i<a; i++) {
		if (!strcmp(b[i],"-h")) {
			usage();
		}
		if (!strcmp(b[i],"-v")) {
			++vf;
			continue;
		}
		if (!strcmp(b[i],"-sample")) {
			DispScriptSample();
			exit(0);
		}
		if (i+1 < a && !strcmp(b[i],"-o")) {
			strcpy(ofname, b[++i]);
			continue;
		}
   		filename = b[i];
	}

	if (filename) {
		if (!(ifp = fopen(filename, "r"))) {
			perror(filename);
			exit(1);
		}
	} else {
		ifp = stdin;
	}

	if (filename == NULL && strlen(ofname) == 0) {
		usage();
	}
	if (strlen(ofname) == 0) {
		strcpy(ofname, filename);
		strcat(ofname, ".mid");
	}
	printf("output to %s\n", ofname);

	memset(TrackData, 0, sizeof(TrackData));
	memset(TrackLen,  0, sizeof(TrackLen));

	// read script
	ReadMidiScript(ifp);
	fclose(ifp);

	// ouput midi
	if (NumOfTrack == 0) {
		printf("Error: no track.\n");
		exit(1);
	}
	if (!(ofp = fopen(ofname, "w"))) {
		perror(ofname);
		exit(1);
	}
	WriteMidi(ofp);
	fclose(ofp);

	return 0;
}


/* vim:ts=4:sw=4:
 */

