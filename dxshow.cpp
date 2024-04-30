#include <windows.h>

#define DWORD_PTR DWORD*
#define LONG_PTR  LONG*

#include <dshow.h>

#define MOVIE_FILE "V:\\www\\media\\letsgyo.mpg"

void main(int a, char *b[])
{
    int  i;
    char *fname = MOVIE_FILE;
    WCHAR wPath[MAX_PATH];

    for (i = 1; i<a; i++) {
        if (!strcmp(b[i], "-h")) {
	    printf("usage: dxshow <filename>\n");
	    exit(1);
	}
	fname = b[i];
    }

    MultiByteToWideChar(CP_ACP, 0, fname, -1, wPath, MAX_PATH);

    IGraphBuilder *pGraph;
    IMediaControl *pMediaControl;
    IMediaEvent   *pEvent;

    CoInitialize(NULL);
    
    // �t�B���^�O���t�}�l�[�W�����쐬���A�C���^�[�t�F�C�X���N�G������B
    CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, 
                        IID_IGraphBuilder, (void **)&pGraph);
    pGraph->QueryInterface(IID_IMediaControl, (void **)&pMediaControl);
    pGraph->QueryInterface(IID_IMediaEvent, (void **)&pEvent);

    // �O���t���쐬�B�d�v: �g�p�V�X�e���̃t�@�C��������ɕύX���邱�ƁB
    //pGraph->RenderFile(L"C:\\Hello_World.avi", NULL);
    pGraph->RenderFile(wPath, NULL);

    // �O���t�̎��s�B
    pMediaControl->Run();

    // �I����҂B 
    long evCode;
    pEvent->WaitForCompletion(INFINITE, &evCode);

    // �N���[�� �A�b�v�B
    pMediaControl->Release();
    pEvent->Release();
    pGraph->Release();
    CoUninitialize();
}


