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
    int mm;                      // �� : 0����
    int dd;                      // �� : 0����
    int hour;                    // �� : 0:��G���g��
    int min;                     // �� : 0:��G���g��
    unsigned long sch_flag;      // �X�P�W���[���t���O
    char snd_file[1024];         // �T�E���h�t�@�C��
    int done;                    // �Đ��ς݃t���O
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


/* �X�P�W���[���̃N���A */
void ClearSched()
{
    memset(sched, 0, sizeof(sched));
}

/* �X�P�W���[���s���\���̂Ɋi�[ */
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


/* �X�P�W���[���̓ǂݍ��� */
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
 *   �X�P�W���[���̃`�F�b�N
 *   
 *   IN  : �Ȃ�
 *   OUT : snd_file : �X�P�W���[�������ɂȂ����ꍇ�A���t����t�@�C������Ԃ�
 *   OUT : return   : 0 : �X�P�W���[�������łȂ�
 *                    1 : �X�P�W���[�������ɂɂȂ���
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

    // �X�P�W���[���`�F�b�N 
    if (oldHH != tm->tm_hour || oldMM != tm->tm_min) {
        // �O��Ǝ������ς������ 
        schedp = sched;
        for (i = 0; i<= last_index; i++) {
	    // ��������v���Ă��邩? 
	    if (schedp[i].hour == tm->tm_hour &&
	        schedp[i].min  == tm->tm_min) {
		// ������v
		if (schedp[i].mm > 0 && schedp[i].dd > 0) {
		    // ���t�w�肠�� 
		    if (schedp[i].mm == tm->tm_mon+1 &&
                        schedp[i].dd == tm->tm_mday) {
			play_flag = 1;  // �Đ�
			break;
                    }
		    else {
		        //printf( "syst = %d/%d", tm->tm_mon+1, tm->tm_mday);
		    }
		} else {
		    // ���t�w��Ȃ� 
		    if (schedp[i].sch_flag & SCH_FLG_EVERYDAY) {
		        // �����w��
			play_flag = 1;  // �Đ�
			break;
		    } else {
		        // �j����v�̃`�F�b�N 
		        if (schedp[i].sch_flag & 
			    (SCH_FLG_EVERY_SUN << tm->tm_wday)) {
			    play_flag = 1;  // �Đ�
			    break;
			}
		    }
		}
            }
	}
	if (play_flag) {
	    /* ���t�t�@�C�����̃R�s�[ */
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

/* MP3�t�@�C���̍Đ� */
void PlayMp3(char *snd_file)
{
    char command[2048];

    sprintf(command, "mpg123 %s", snd_file);
    system(command);
}

/* WAV�t�@�C���̍Đ� */
void PlayWav(char *snd_file)
{
    char command[2048];

    sprintf(command, "play %s", snd_file);
    system(command);
}

/* �T�E���h�t�@�C���̍Đ� */
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
	    // �Đ� 
            Play(snd_file);
            if (vf) printf("---\n\n");
        }
        sleep(1);
    }
}


