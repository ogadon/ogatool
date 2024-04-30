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
    
    // フィルタグラフマネージャを作成し、インターフェイスをクエリする。
    CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, 
                        IID_IGraphBuilder, (void **)&pGraph);
    pGraph->QueryInterface(IID_IMediaControl, (void **)&pMediaControl);
    pGraph->QueryInterface(IID_IMediaEvent, (void **)&pEvent);

    // グラフを作成。重要: 使用システムのファイル文字列に変更すること。
    //pGraph->RenderFile(L"C:\\Hello_World.avi", NULL);
    pGraph->RenderFile(wPath, NULL);

    // グラフの実行。
    pMediaControl->Run();

    // 終了を待つ。 
    long evCode;
    pEvent->WaitForCompletion(INFINITE, &evCode);

    // クリーン アップ。
    pMediaControl->Release();
    pEvent->Release();
    pGraph->Release();
    CoUninitialize();
}


