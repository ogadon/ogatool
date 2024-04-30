/*
 *  Joystick�e�X�g (DirectInput)
 *
 *    08/08/17 V0.10 by oga.
 */
#include <windows.h>
#include <stdio.h>
#include "dinput.h"

#define RELEASE(x) {if(x) { x->Release(); x = NULL;}}

#define JOY_BUFSIZE	50


LPDIRECTINPUT pDInput;
LPDIRECTINPUTDEVICE2 pDInputDevice;
int joy_ready = 0;

/*
 *  	�W���C�X�e�B�b�N���擾
 */
BOOL CALLBACK GetJoystickCallback(LPDIDEVICEINSTANCE lpddi,LPVOID pvRef)
{
	HRESULT ret;
	LPDIRECTINPUTDEVICE pDev;

	printf("Start GetJoystickCallback.\n");
	// �W���C�X�e�B�b�N�p�f�o�C�X�I�u�W�F�N�g�̍쐬
	ret = pDInput->CreateDevice(lpddi->guidInstance, &pDev, NULL);
	if(ret != DI_OK){
		printf("End   GetJoystickCallback. (CONTINUE)\n");
		return DIENUM_CONTINUE;
	}

	pDev->QueryInterface(IID_IDirectInputDevice2, (LPVOID *)&pDInputDevice);

	printf("End   GetJoystickCallback. (STOP)\n");
	return DIENUM_STOP;
}

/*
 *	Direct Input ������
 */
int InitDInput(void)
{
	HRESULT ret;

	//ret = DirectInputCreate(hInstApp,DIRECTINPUT_VERSION,&pDInput,NULL);
	ret = DirectInputCreate(GetModuleHandle(NULL), DIRECTINPUT_VERSION, &pDInput,NULL);
	if(ret != DI_OK){
		printf("DirectInputCreate Failed\n");
		return FALSE;
	}

	// �W���C�X�e�B�b�N��T��
	pDInputDevice = NULL;
	pDInput->EnumDevices(DIDEVTYPE_JOYSTICK,(LPDIENUMDEVICESCALLBACK)GetJoystickCallback,NULL,DIEDFL_ATTACHEDONLY);
	if(pDInputDevice == NULL){
		printf("EnumDevices Failed. (no available joystick)\n");
		RELEASE(pDInput);
		return TRUE;
	}
	
	// �f�[�^�t�H�[�}�b�g��ݒ�
	ret = pDInputDevice->SetDataFormat(&c_dfDIJoystick);
	if(ret != DI_OK){
		printf("SetDataFormat Failed\n");
		RELEASE(pDInputDevice);
		RELEASE(pDInput);
		return FALSE;
	}

#if 0
	// ���[�h��ݒ�
	//ret = pDInputDevice->SetCooperativeLevel(hwnd,DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);
	ret = pDInputDevice->SetCooperativeLevel(hwnd,DISCL_NONEXCLUSIVE  | DISCL_BACKGROUND);

	if(ret != DI_OK){
		printf("SetCooperativeLevel Failed\n");
		RELEASE(pDInputDevice);
		RELEASE(pDInput);
		return FALSE;
	}
#endif

	DIPROPRANGE diprg; 
 
	// ���̒l�͈̔͂�ݒ�
	diprg.diph.dwSize	= sizeof(diprg); 
	diprg.diph.dwHeaderSize	= sizeof(diprg.diph); 
	diprg.diph.dwObj	= DIJOFS_X; 
	diprg.diph.dwHow	= DIPH_BYOFFSET; 
	diprg.lMin	= -1000; 
	diprg.lMax	= +1000; 
 	ret = pDInputDevice->SetProperty(DIPROP_RANGE, &diprg.diph);
	if(ret != DI_OK){
		printf("SetProperty(DIPROP_RANGE X) Failed\n");
	}

	diprg.diph.dwObj	= DIJOFS_Y; 
 	ret = pDInputDevice->SetProperty(DIPROP_RANGE, &diprg.diph);
	if(ret != DI_OK){
		printf("SetProperty(DIPROP_RANGE Y) Failed\n");
	}

	// ���̓o�b�t�@�̎w�� (��肱�ڂ��h�~�̂��߃o�b�t�@���g�p����ꍇ)
	DIPROPDWORD diwd;

	diwd.diph.dwSize = sizeof(diwd);
	diwd.diph.dwHeaderSize = sizeof(diwd.diph);
	diwd.diph.dwObj  = 0;
	diwd.diph.dwHow  = DIPH_DEVICE;
	diwd.dwData      = JOY_BUFSIZE;        // �o�b�t�@�T�C�Y
 	ret = pDInputDevice->SetProperty(DIPROP_BUFFERSIZE, &diwd.diph);
	if(ret != DI_OK){
		printf("SetProperty(DIPROP_BUFFERSIZE) Failed\n");
	}

	// �A�N�Z�X�����擾
	ret = pDInputDevice->Acquire();
	if(ret != DI_OK){
		printf("Acquire Failed\n");
		RELEASE(pDInputDevice);
		RELEASE(pDInput);
		return FALSE;
	}

	joy_ready = 1;
	return TRUE;
}


void ReleaseDInput()
{
	//if (pDInputDevice) pDInputDevice->Unacquire();
	RELEASE(pDInputDevice);
	RELEASE(pDInput);
}

int main(int a, char *b[])
{
	DIJOYSTATE dijs;
	int        ret;
	int        i;
	DWORD      itemno;
	DIDEVICEOBJECTDATA diobjdat[JOY_BUFSIZE];

	InitDInput();

	while (1) {
		// ���݂̏�Ԃ�����ꍇ
		pDInputDevice->Poll();
		ret = pDInputDevice->GetDeviceState(sizeof(DIJOYSTATE), &dijs);
		if (ret == DI_OK) {
			printf("dijs.lX:%d  lY:%d  lRx:%d  lRy:%d  button:%d %d %d %d\n",
					dijs.lX, dijs.lY,
					dijs.lRx, dijs.lRy,
					dijs.rgbButtons[0], dijs.rgbButtons[1],
					dijs.rgbButtons[2], dijs.rgbButtons[3]);
			if (dijs.rgbButtons[3]) {
				printf("button4: quit!!\n");
				break;
			}
		}

		// �o�b�t�@������ꍇ
		itemno = JOY_BUFSIZE;
		ret = pDInputDevice->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), 
				            &diobjdat[0],   // �i�[��o�b�t�@
							&itemno,        // I:�p�ӂ����o�b�t�@�� / O:�擾���ꂽ�o�b�t�@��
							0);
		if (ret == DI_OK) {
			printf("itemno:%d\n", itemno);
			for (i = 0; i < itemno; i++) {
				if (diobjdat[i].dwOfs == DIJOFS_BUTTON0) {
					printf("button0: %s %d\n", diobjdat[i].dwData?"ON":"OFF", diobjdat[i].dwData);
				} else if (diobjdat[i].dwOfs == DIJOFS_BUTTON1) {
					printf("button1: %s %d\n", diobjdat[i].dwData?"ON":"OFF", diobjdat[i].dwData);
				} else if (diobjdat[i].dwOfs == DIJOFS_BUTTON2) {
					printf("button2: %s %d\n", diobjdat[i].dwData?"ON":"OFF", diobjdat[i].dwData);
				} else if (diobjdat[i].dwOfs == DIJOFS_BUTTON3) {
					printf("button3: %s %d\n", diobjdat[i].dwData?"ON":"OFF", diobjdat[i].dwData);
				}
			}
		}
		Sleep(500);
	}

	ReleaseDInput();
	return 0;
}


/* vim:ts=4:sw=4:
 */

