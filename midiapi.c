/*
 *  MIDI API Sample
 *
 *    10/01/03 V0.10 by oga.
 *
 *    参考: http://www.deqnotes.net/midi/winapi_midiprog/winapi_midiprog.pdf
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

	/* MIDI出力デバイス数を取得*/
	num = midiOutGetNumDevs();
	printf("## Number of output devices: %d\n", num);
	/* 各デバイスID に対してfor ループ*/
	for (devid=0; devid<num; devid++) {
		/* デバイスの情報をoutCaps に格納*/
		res = midiOutGetDevCaps(devid, &outCaps, sizeof(outCaps));
		/* midiOutGetDevCaps の戻り値が成功でない(=失敗) なら次のループへ*/
		if (res != MMSYSERR_NOERROR) { continue; }
		/* デバイスID とそのデバイス名を表示*/
		printf("ID=%d: %s\n", devid, outCaps.szPname);
	}

	/* MIDI入力デバイス数を取得*/
	num = midiInGetNumDevs();
	printf("## Number of input devices: %d\n", num);
	/* 各デバイスID に対してfor ループ*/
	for (devid=0; devid<num; devid++) {
		/* デバイスの情報をoutCaps に格納*/
		res = midiInGetDevCaps(devid, &inCaps, sizeof(inCaps));
		/* midiOutGetDevCaps の戻り値が成功でない(=失敗) なら次のループへ*/
		if (res != MMSYSERR_NOERROR) { continue; }
		/* デバイスID とそのデバイス名を表示*/
		printf("ID=%d: %s\n", devid, inCaps.szPname);
	}
	return 0;
}
