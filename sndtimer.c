/*
 *  sndtimer
 *
 *    2003/06/17 V0.10 by oga.
 *
 *  Schedule file   $HOME/.sndschedule  , /etc/sndschedule
 *  Format          yy,mm,hour,min,flag,wav_path
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>

typedef struct _sch_t {
    int mm;                      // 月 : 0あり
    int dd;                      // 日 : 0あり
    int hour;                    // 時 : 0:空エントリ
    int min;                     // 時 : 0:空エントリ
    unsigned long sch_flag;      // スケジュールフラグ
    char snd_file[1024];         // サウンドファイル
    int done;                    // 再生済みフラグ
} sch_t;

// sch_flag value definition
#define SCH_FLG_TODAY       0x00000001   //   1
#define SCH_FLG_EVERYDAY    0x00000002   //   2
#define SCH_FLG_EVERY_SUN   0x00000004   //   4
#define SCH_FLG_EVERY_MON   0x00000008   //   8
#define SCH_FLG_EVERY_TUE   0x00000010   //  16
#define SCH_FLG_EVERY_WED   0x00000020   //  32
#define SCH_FLG_EVERY_THU   0x00000040   //  64
#define SCH_FLG_EVERY_FRI   0x00000080   // 128
#define SCH_FLG_EVERY_SAT   0x00000100   // 256

#define NUM_SCHED 50


/* globals */
sch_t sched[NUM_SCHED+1];
int   last_index = 0;
int   oldHH = -1;
int   oldMM = -1;
int   vf    = 0;


/* スケジュールのクリア */
void ClearSched()
{
    memset(sched, 0, sizeof(sched));
}

/* スケジュール行を構造体に格納 */
void ConvertToSched(char *buf, sch_t *schedp)
{
    // format
    // yy,mm,hour,min,flag,wav_path
    
    char *pt;

    pt = strtok(buf, ",");
    if (pt) schedp->mm = atoi(pt);

    pt = strtok(NULL, ",");
    if (pt) schedp->dd = atoi(pt);

    pt = strtok(NULL, ",");
    if (pt) schedp->hour = atoi(pt);

    pt = strtok(NULL, ",");
    if (pt) schedp->min = atoi(pt);

    pt = strtok(NULL, ",");
    if (pt) schedp->sch_flag = strtoul(pt,(char **)NULL,0);

    pt = strtok(NULL, ",");
    if (pt) strcpy(schedp->snd_file, pt);
}


/* スケジュールの読み込み */
int ReadSchedule()
{
    FILE *fp;
    char buf[4096];
    int  i = 0;
    char *home;
    char sched_file[1024];
    struct stat stbuf;

    if (vf >= 2) printf("ReadSchedule()\n");
    if (vf) printf("------------------------------------------------\n", buf);

    last_index = -1;
    memset(sched, 0, sizeof(sched));

    home = getenv("HOME");
    if (home) {
        /* user spec schedule */
	sprintf(sched_file, "%s/.sndschedule", home);
    } else {
        /* default schedule */
        strcpy(sched_file, "/etc/sndschedule");
    }

    if (stat(sched_file, &stbuf)) {
        if (vf) printf("no %s file\n", sched_file);
        strcpy(sched_file, "/etc/sndschedule");
    }

    if ((fp = fopen(sched_file, "r")) == NULL) {
        printf("no %s file\n", sched_file);
        return -1;
    }
    while (fgets(buf, sizeof(buf), fp) && i < NUM_SCHED) {
        if (buf[strlen(buf)-1] == 0x0a) {
	    buf[strlen(buf)-1] = '\0';
	}
        if (buf[strlen(buf)-1] == 0x0d) {
	    buf[strlen(buf)-1] = '\0';
	}

        if (buf[0] == '#' || strlen(buf) == 0) continue;
	if (vf) printf("%s\n", buf);
	ConvertToSched(buf, &sched[i]);
	if (strlen(sched[i].snd_file) == 0) continue;
	++i;
    }
    fclose(fp);
    last_index = i - 1;
    if (vf) printf("------------------------------------------------\n", buf);

    return 0;
}

/*
 *   スケジュールのチェック
 *   
 *   IN  : なし
 *   OUT : snd_file : スケジュール時刻になった場合、演奏するファイル名を返す
 *   OUT : return   : 0 : スケジュール時刻でない
 *                    1 : スケジュール時刻にになった
 */
int CheckSchedule(char *snd_file)
{
    char       buf[128];
    int        i;
    int        play_flag = 0;

    sch_t *schedp;

    time_t    tt;
    struct tm *tm;

    if (vf >= 2) printf("ChedkSchedule()\n");

    tt = time(0);
    tm = localtime(&tt);

    // スケジュールチェック 
    if (oldHH != tm->tm_hour || oldMM != tm->tm_min) {
        // 前回と時分が変わったら 
        schedp = sched;
        for (i = 0; i<= last_index; i++) {
	    // 時刻が一致しているか? 
	    if (schedp[i].hour == tm->tm_hour &&
	        schedp[i].min  == tm->tm_min) {
		// 時刻一致
		if (schedp[i].mm > 0 && schedp[i].dd > 0) {
		    // 日付指定あり 
		    if (schedp[i].mm == tm->tm_mon+1 &&
                        schedp[i].dd == tm->tm_mday) {
			play_flag = 1;  // 再生
			break;
                    }
		    else {
		        //printf( "syst = %d/%d", tm->tm_mon+1, tm->tm_mday);
		    }
		} else {
		    // 日付指定なし 
		    if (schedp[i].sch_flag & SCH_FLG_EVERYDAY) {
		        // 毎日指定
			play_flag = 1;  // 再生
			break;
		    } else {
		        // 曜日一致のチェック 
		        if (schedp[i].sch_flag & 
			    (SCH_FLG_EVERY_SUN << tm->tm_wday)) {
			    play_flag = 1;  // 再生
			    break;
			}
		    }
		}
            }
	}
	if (play_flag) {
	    /* 演奏ファイル名のコピー */
	    strcpy(snd_file, schedp[i].snd_file);
	    if (vf) {
	        printf("#### %02d:%02d play %s\n", 
	            tm->tm_hour, tm->tm_min, snd_file);
            }
	}
    }
    oldHH = tm->tm_hour;
    oldMM = tm->tm_min;

    if (vf) {
        strftime(buf, sizeof(buf), "%Y/%m/%d(%a) %H:%M:%S", tm);
	printf("%cM%s\n", 27, buf);
    }

    return play_flag;
}

/* MP3ファイルの再生 */
void PlayMp3(char *snd_file)
{
    char command[2048];

    sprintf(command, "mpg123 %s", snd_file);
    system(command);
}

/* WAVファイルの再生 */
void PlayWav(char *snd_file)
{
    char command[2048];

    sprintf(command, "play %s", snd_file);
    system(command);
}

/* サウンドファイルの再生 */
void Play(char *snd_file) 
{
    if (strlen(snd_file) > 4 && !strcasecmp(&snd_file[strlen(snd_file)-4], ".mp3")) {
        PlayMp3(snd_file);
    } else {
        PlayWav(snd_file);
    }
}

void SigHup(int sig)
{
    if (vf) printf("reload schedule file..\n");
    ClearSched();
    ReadSchedule();
    if (vf) printf("\n");
    signal(SIGHUP, SigHup);
}


int main(int a, char *b[])
{

    char snd_file[1024];
    int  i;

    for (i=1; i<a; i++) {
        if (!strcmp(b[i], "-h")) {
	    printf("usage: sndtimer [-v]\n");
	    printf("       config file : $HOME/.sndschedule , /etc/sndschedule\n");
	    exit(1);
	}
        if (!strcmp(b[i], "-v")) {
	    ++vf;
	    continue;
	}
    }

    signal(SIGHUP, SigHup);

    ClearSched();
    ReadSchedule();

    if (vf) printf("\n");
    while (1) {
        if (CheckSchedule(snd_file)) {
	    // 再生 
            Play(snd_file);
            if (vf) printf("---\n\n");
        }
        sleep(1);
    }
}


