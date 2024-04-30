/*
 *  MIDI API Sample
 *
 *    10/01/03 V0.10 by oga.
 *
 *    �Q�l: http://www.deqnotes.net/midi/winapi_midiprog/winapi_midiprog.pdf
 */

#include <stdio.h>
#include <windows.h>
#include <mmsystem.h>

int main(int a, char *b)
{
	MIDIOUTCAPS outCaps;
	MIDIINCAPS  inCaps;
	MMRESULT res;
	UINT num, devid;

	/* MIDI�o�̓f�o�C�X�����擾*/
	num = midiOutGetNumDevs();
	printf("## Number of output devices: %d\n", num);
	/* �e�f�o�C�XID �ɑ΂���for ���[�v*/
	for (devid=0; devid<num; devid++) {
		/* �f�o�C�X�̏���outCaps �Ɋi�[*/
		res = midiOutGetDevCaps(devid, &outCaps, sizeof(outCaps));
		/* midiOutGetDevCaps �̖߂�l�������łȂ�(=���s) �Ȃ玟�̃��[�v��*/
		if (res != MMSYSERR_NOERROR) { continue; }
		/* �f�o�C�XID �Ƃ��̃f�o�C�X����\��*/
		printf("ID=%d: %s\n", devid, outCaps.szPname);
	}

	/* MIDI���̓f�o�C�X�����擾*/
	num = midiInGetNumDevs();
	printf("## Number of input devices: %d\n", num);
	/* �e�f�o�C�XID �ɑ΂���for ���[�v*/
	for (devid=0; devid<num; devid++) {
		/* �f�o�C�X�̏���outCaps �Ɋi�[*/
		res = midiInGetDevCaps(devid, &inCaps, sizeof(inCaps));
		/* midiOutGetDevCaps �̖߂�l�������łȂ�(=���s) �Ȃ玟�̃��[�v��*/
		if (res != MMSYSERR_NOERROR) { continue; }
		/* �f�o�C�XID �Ƃ��̃f�o�C�X����\��*/
		printf("ID=%d: %s\n", devid, inCaps.szPname);
	}
	return 0;
}
