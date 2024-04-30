/* 
 *  irctrl.c
 *    �p�������C���^�t�F�[�X�𗘗p�����ԊO���f�[�^R/W
 *
 *    (��)PC�̃v�����^��I/O�A�h���X��PORT_PRT_BASE�����킹�邱��
 *        PC�̃v�����^��BIOS�ݒ�ł͑o�����ʐM�ɐݒ肷�邱�� 
 *
 *    04/01/04 V0.10 by oga.
 *    04/06/14 V0.20 by oga.
 */

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

/* I/O Ports */
#define PORT_PRT_BASE 0x378               /* lp0:0x3bc lp1:0x378 lp2:0x278 */
#define PORT_PRT_DATA (PORT_PRT_BASE+0)   /* Data R/W                      */
#define   BIT_DATA_IRREAD  0x01           /*   Bit0: Input  IR Data        */
#define   BIT_DATA_IRWRITE 0x02           /*   Bit1: Output IR Data        */
#define PORT_PRT_STAT (PORT_PRT_BASE+1)   /* Status (Read Only)            */
#define   BIT_STAT_SLCT    0x10           /*   Bit4: SLCT(Not Use)         */
#define PORT_PRT_CTRL (PORT_PRT_BASE+2)   /* Control(Write Only)           */
#define   BIT_CTRL_DIRECT  0x20           /*   Bit5: Direction 0:OUT 1:IN  */

#define WAIT_TIME 00000  /* 20ms */
/* #define WAIT_TIME 200000  /* 200ms */
/* #define WAIT_SPIN 1000000 /* 6.5ms(loop(K6-2/366MHz)) */
#define WAIT_SPIN 5000   /*  5ms */

/* �Ƃ肠���� �i�[�f�[�^�͉��f�[�^�� */
#define IR_DATA_OFF '0'
#define IR_DATA_ON  '1'

/* globals */
int  vf = 0;   /* verbose */

/*
 *  tv_diff(tv1, tv2, tv3)
 *
 *  IN 
 *   tv1, tv2 : time value
 *
 *  OUT 
 *   tv3      : tv1 - tv2
 *
 */
void tv_diff(tv1, tv2, tv3)
struct timeval *tv1,*tv2,*tv3;
{
    if (tv1->tv_usec < tv2->tv_usec) {
        tv3->tv_sec  = tv1->tv_sec  - tv2->tv_sec - 1;
        tv3->tv_usec = tv1->tv_usec + 1000000 - tv2->tv_usec;
    } else {
        tv3->tv_sec  = tv1->tv_sec  - tv2->tv_sec;
        tv3->tv_usec = tv1->tv_usec - tv2->tv_usec;
    }
}

/*
 *  SpinSleep
 *    �w���s�����X�s�����đ҂�(1/100�b�����������𑜓x�ő҂������ꍇ)
 *
 *   usec : micro seconds
 */
void SpinSleep(int usec)
{
    int i;
    struct timeval  tv, tv2, tv3;
    struct timezone tz;

    gettimeofday(&tv, &tz);
    while (1) {
        gettimeofday(&tv2, &tz);
	tv_diff(&tv2, &tv, &tv3);
	if (tv3.tv_sec*1000000+tv3.tv_usec >= usec) {
	    break;
	}
    }
}

/*
 *  IrRead
 *
 *    OUT data : Infrared Read Data
 *    IN  size : data buffer size
 *    OUT ret  : Read Data Size
 */
int IrRead(char *data, int size)
{
    int i = 0;
    int val;
    int off_len = 0;  /* IR_DATA_OFF length */
    int last_point = 0;

    outb(BIT_CTRL_DIRECT, PORT_PRT_CTRL);   /* Bit5:1 ... Input */

    /* Wait IR Data */
    printf("Waiting IR Data...\n");
    fflush(stdout);
    while (1) {
        usleep(WAIT_TIME);   /* DEBUG */
        val = inb(PORT_PRT_DATA);
        if (vf) printf("IrRead1: INB[%d] : %d\n", i++, val);   /* DEBUG */
	if (!(val & BIT_DATA_IRREAD)) {
	    data[0] = IR_DATA_ON;
	    break;
	}
    }
    SpinSleep(WAIT_TIME);

    printf("Reading ...\n");
    fflush(stdout);
    
    for (i=1; i<size; i++) {
	//if (off_len > 500000/WAIT_TIME) {
	if (off_len > 50000) {
            /* 500ms�ȏ�f�[�^���Ȃ��ꍇ�I�� */
	    break;
	}
        val = inb(PORT_PRT_DATA);
        if (vf) printf("IrRead2: val=%d\n", val);
	if (!(val & BIT_DATA_IRREAD)) { /* Bit0(D0)�œ��� */
	    data[i] = IR_DATA_ON;
            off_len = 0;
	    last_point = i;  /* �Ō��1�ɂȂ����ꏊ */
	} else {
	    data[i] = IR_DATA_OFF;
	    ++off_len;
	}
        SpinSleep(WAIT_TIME);
    }
    return (last_point+1);
}

/*
 *  IrWrite
 *
 *    IN  data : Infrared Output Data
 *    IN  size : Data Size
 */
void IrWrite(char *data, int size)
{
    int i;

    outb(0x00, PORT_PRT_CTRL);   /* Bit5:0 ... Output */

    for (i=0; i<size; i++) {
        if (data[i] == IR_DATA_ON) {
            outb(            0x00, PORT_PRT_DATA);
	} else {
            outb(BIT_DATA_IRWRITE, PORT_PRT_DATA);  /* Bit1(D1)�ŏo�� */
	}
        SpinSleep(WAIT_TIME);
    }

    /* Ir Off */
    outb(BIT_DATA_IRWRITE, PORT_PRT_DATA);  /* Bit1(D1)�ŏo�� */
}

/*
 *  ReadIRData
 *    �t�@�C������IR�f�[�^���擾
 *
 *  IN  fname : filename
 *  IN  data  : read data buffer
 *  IN  size  : data buffer size
 *  OUT data  : read data buffer
 *  OUT ret   : read data size
 *
 */
int ReadIRData(char *fname, char *data, int size)
{
    FILE *fp;
    struct stat stbuf;
    int  size2 = size;
    int  ret = 0;

    if (stat(fname, &stbuf) < 0) {
        perror(fname);
        exit(1);
    }
    if (stbuf.st_size < size) {
        size2 = stbuf.st_size;
    }

    if ((fp = fopen(fname,"rb")) == 0) {
        perror(fname);
        exit(1);
    }
    ret = fread(data, 1, size2, fp);
    fclose(fp);

    return ret;
}

/*
 *  WriteIRData
 *    �t�@�C����IR�f�[�^���i�[
 *
 *  IN  fname : filename
 *  OUT data  : read data buffer
 *  OUT size  : data buffer size
 *
 */
void WriteIRData(char *fname, char *data, int size)
{
    FILE *fp;
    struct stat stbuf;

    if ((fp = fopen(fname,"wb")) == 0) {
        perror(fname);
        exit(1);
    }
    fwrite(data, 1, size, fp);
    fclose(fp);

    printf("Write %s completed.\n", fname);
}

int main(int a, char* b[])
{
    int  val = 0;
    //char data[200];  /* 0.02x200 = 4sec */
    char data[2000000];  /* 0.02x200 = 4sec */
    int  size;
    char buf[4096];   /* dummy buf */

    char *fname = NULL;
    int  i;

    /* arg check */
    for (i = 1; i<a; i++) {
	if (!strncmp(b[i],"-v",2)) {
	    vf = 1;
	    continue;
	}
	if (!strncmp(b[i],"-h",2)) {
	    printf("usage: irctrl [in_ir_data_file]\n");
	    exit(1);
	}
	fname = b[i];
    }

    /* ���� ioperm(from, num, turn_on)
     *   from    : �A�N�Z�X�������|�[�g�̍ŏ��̃A�h���X (0x000�`0x3ff)
     *   num     : from���炢���̘A���A�h���X�̃A�N�Z�X�����������w��
     *   turn_on : true:1 = �A�N�Z�X������, false:0 = �A�N�Z�X���֎~
     *
     *   <��>
     *     ioperm(0x300, 5, 1) �́A0x300 ���� 0x304 (���v�� 5 �̃|�[�g)
     *     �ɑ΂��鋖���w�肵�A�Ō�̃p�����[�^�ł́A���̃|�[�g�ɑ΂���
     *     �A�N�Z�X����^����̂��֎~����̂���_���l
     *     (true:1 = �A�N�Z�X��������, false:0 = �A�N�Z�X���֎~����)�Ŏw��
     */
    if (ioperm(PORT_PRT_BASE, 3, 1) < 0) {
        perror("ioperm");
	exit(1);
    }

    memset(data, 0, sizeof(data));

    outb(0x00, PORT_PRT_CTRL);   /* Bit5:0 ... Output */
    outb(0x00, PORT_PRT_DATA);


    /* debug 04/06/13 */
    strcpy(buf, "0101010");
    IrWrite(buf, strlen(buf));

    if (fname != NULL) {
        /* �t�@�C���w�肪����ꍇ�̓t�@�C������f�[�^�擾 */
        if ((size = ReadIRData(fname, data, sizeof(data))) < 0) {
	    printf("IR Data Read Error\n");
	    exit(1);
	}
    } else {
        /* �t�@�C���w�肪�Ȃ��ꍇ�̓f�o�C�X����f�[�^�擾 */
        printf("IR Read start.\n");
        fflush(stdout);

        size = IrRead(data, sizeof(data)-1);  /* �ԊO������ */

        printf("IR Read end. size:%d\n", size);
        fflush(stdout);
    }


    while (1) {
        printf("IR Out start.  hit any key! [q:quit]\n");
        fflush(stdout);

        scanf("%s", buf);
        if (buf[0] == 'q' || buf[0] == 'e') {
	    /* "quit" or "exit" */
            break;
        }
        if (buf[0] == 'd') {
	    /* "dump" : dump data */
            printf("## dump Ir Data...\n");
            for (i = 0; i<size; i++) {
                printf("%c", data[i]);
            }                
            printf("\n");
            continue;
        }
        printf("PiPiPi...\n");
        fflush(stdout);

	IrWrite(data, size);  /* �ԊO���o�� */

        printf("IR Out end. size:%d\n", size);
        fflush(stdout);
    }

    printf("data=%s\n", data);

    WriteIRData("/tmp/irdata.ir", data, size);

    /* ��n�� */
    if (ioperm(PORT_PRT_BASE, 3, 0) < 0) {
        perror("ioperm");
	exit(1);
    }
    return 0;
}


#if 0
// �Q�l Linux HOWTO IO-Port-Programming/useful-ports.html
//
// ��BASE+0�̃|�[�g(�f�[�^�|�[�g)�́A�|�[�g�̃f�[�^�M���𐧌䂵�܂��B
//   D0 �` D7 �̓r�b�g 0 �` 7 �ɑΉ����܂��B
//   ��� : 0 = low (0V)�A1 = high (5V))�B
//     ���̃|�[�g�ɑ΂��郉�C�g�̓f�[�^�����b�`���ĊO���̃s���ɏo�͂��܂��B
//     �W���܂��͊g�����C�g���[�h�̏ꍇ�ɂ́A���[�h����ƁA�Ō�Ƀ��C�g
//     ���ꂽ�f�[�^���ǂݏo����܂��B
//     �g�����[�h���[�h�̏ꍇ�ɂ́A�O���f�o�C�X����̃f�[�^���ǂݏo����܂��B
//
// ��BASE+1�̃|�[�g(�X�e�[�^�X�|�[�g) �̓��[�h�I�����[�ŁA�ȉ��̂悤�ȓ��͐M���̃X�e�[�^�X��Ԃ��܂�: 
//   Bit 0 �� 1 �͗\���
//   Bit 2 IRQ �X�e�[�^�X (�O���s���̒l�ł͂Ȃ��B�ǂ������ӂ��ɓ��삷�邩�m��Ȃ�)
//   Bit 3 ERROR (1=high)
//   Bit 4 SLCT (1=high)
//   Bit 5 PE (1=high)
//   Bit 6 ACK (1=high)
//   Bit 7 -BUSY (0=high)
//
// ��BASE+2�̃|�[�g(����|�[�g)�̓��C�g�I�����[ (���[�h�����ꍇ�A�Ō�Ƀ��C�g�����f�[�^��Ԃ��B)�ŁA�ȉ��̃X�e�[�^�X�M���𐧌䂵�܂�: 
//   Bit 0 -STROBE (0=high)
//   Bit 1 -AUTO_FD_XT (0=high)
//   Bit 2 INIT (1=high)
//   Bit 3 -SLCT_IN (0=high)
//   Bit 4 '1' �ɂ���ƁA�p�������|�[�g�̊��荞�݂�������(ACK �� low -> high �̑J�ڂŊ��荞�ݔ����B)
//   Bit 5 �g�����[�h�ł̕���(0 = �o��, 1 = ����) �𐧌䂷��B ��
//         ���̃r�b�g�͊��S�Ƀ��C�g�I�����[�ŁA���[�h���ɂ͕s��̒l���Ԃ�B
//   Bit 6 �� 7 �͗\���
//
// ���s���ڑ� (25 �s���� D-sub ���X�R�l�N�^) (i=input, o=output): 
//     1io -STROBE,
//   ��2io D0, 3io D1, 4io D2, 5io D3, 6io D4, 7io D5, 8io D6, 9io D7,
//     10i ACK, 11i -BUSY, 12i PE, 13i SLCT, 14o -AUTO_FD_XT,
//     15i ERROR, 16o INIT, 17o -SLCT_IN, 
//   ��18-25 Ground
#endif /* 0 */

/* vim:ts=8:sw=8:
 */

