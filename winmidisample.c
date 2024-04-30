#include <windows.h>
#include <stdio.h>

#pragma comment (lib, "winmm.lib")

struct EVENT {
    BYTE   state;   // �X�e�[�^�X�o�C�g
    BYTE   data1;   // ���f�[�^�o�C�g
    BYTE   data2;   // ���f�[�^�o�C�g
    BYTE   type;    // �^�C�v
    int    nData;   // �f�[�^��
    LPBYTE lpData;  // �ϒ��f�[�^
    DWORD  dwDelta; // �f���^�^�C��

    struct EVENT *lpNext; // ���̃C�x���g�ւ̃|�C���^
};
typedef struct EVENT EVENT;
typedef struct EVENT *LPEVENT;

static WORD     wTime       = 0;
static BOOL     bPlayThread = FALSE;
static DWORD    dwTempo     = 0;
static HANDLE   hheap       = NULL;
static HANDLE   hthread     = NULL;
static LPEVENT  lpHeader    = NULL;
static HMIDIOUT hmo         = NULL;

static void   Destroy(void);
static BOOL   LoadFile(HANDLE);
static void   Reverse(LPVOID, int);
static BOOL   OpenDialog(HWND, LPTSTR);
static void   ReadDelta(HANDLE, LPDWORD);
static BOOL   ReadTrack(HANDLE, LPEVENT *);
static double DeltaToMilliSecond(DWORD);
static LPVOID Alloc(int);
static DWORD WINAPI ThreadProc(LPVOID);
static LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);

FILE *fp = NULL;

int WINAPI WinMain(HINSTANCE hinst, HINSTANCE hinstPrev, LPSTR lpszCmdLine, int nCmdShow)
{
    TCHAR      szAppName[] = TEXT("MIDI�Đ��T���v��(�t�H�[�}�b�g0)");
    MSG        msg;
    HWND       hwnd;
    WNDCLASSEX wc = {0};

    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.hCursor       = (HCURSOR)LoadImage(NULL, MAKEINTRESOURCE(IDC_ARROW), IMAGE_CURSOR, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
    wc.hInstance     = hinst;
    wc.lpfnWndProc   = WindowProc;
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = szAppName;

    if (!RegisterClassEx(&wc))
        return 0;

    hwnd = CreateWindowEx(0, szAppName, szAppName, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hinst, NULL);
    if (hwnd == NULL)
        return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {

    case WM_CREATE: {
        MMRESULT mr;

        mr = midiOutOpen(&hmo, MIDIMAPPER, 0, 0, CALLBACK_NULL);

        return mr == MMSYSERR_NOERROR ? 0 : -1;
    }
    
    case WM_LBUTTONDOWN: {
        TCHAR szFile[MAX_PATH];

        if (OpenDialog(hwnd, szFile)) {
            DWORD  dwThreadId;
            HANDLE hfile;

            Destroy();
            
            hheap = HeapCreate(0, 4096, 0);
            if (hheap == NULL)
                return 0;

            hfile = CreateFile(szFile, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hfile == INVALID_HANDLE_VALUE) {
                MessageBox(NULL, TEXT("�t�@�C���̃I�[�v���Ɏ��s���܂����B"), NULL, MB_ICONWARNING);
                return 0;
            }

            if (!LoadFile(hfile)) {
                MessageBox(NULL, TEXT("MIDI�t�@�C���̓ǂݍ��݂Ɏ��s���܂����B"), NULL, MB_ICONWARNING);
                CloseHandle(hfile);
                return 0;
            }
            
            CloseHandle(hfile);

            dwTempo     = 500000; // �e���|�̃f�t�H���g�l
            bPlayThread = TRUE;
            
            hthread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadProc, NULL, 0, &dwThreadId);
        }

        return 0;
    }

    case WM_DESTROY:
        if (hmo != NULL) {
            Destroy();
            midiOutClose(hmo);
	    if (fp) fclose(fp);  /* oga */
	    fp = NULL;           /* oga */
        }
        
        PostQuitMessage(0);

        return 0;

    default:
        break;

    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

static DWORD WINAPI ThreadProc(LPVOID lpParameter)
{
    MIDIHDR mh      = {0};
    LPEVENT lpEvent = NULL;

    if (fp == NULL) {
        fp = fopen("winmidisample.log", "w");
    }
    
    while (bPlayThread) {
        if (lpEvent == NULL)
            lpEvent = lpHeader;
        
        Sleep((DWORD)DeltaToMilliSecond(lpEvent->dwDelta));
        
        if (lpEvent->state == 0xFF) { // ���^�C�x���g
            if (lpEvent->type == 0x51) // �Z�b�g�e���|
                dwTempo = (DWORD)(lpEvent->lpData[2] | (lpEvent->lpData[1] << 8) | (lpEvent->lpData[0] << 16));
        }
        else if (lpEvent->state == 0xF0) { // SysEx�C�x���g
            mh.lpData          = (LPSTR)lpEvent->lpData;
            mh.dwFlags         = 0;
            mh.dwBufferLength  = lpEvent->nData;
            mh.dwBytesRecorded = lpEvent->nData;
        
            midiOutPrepareHeader(hmo, &mh, sizeof(MIDIHDR));
            midiOutLongMsg(hmo, &mh, sizeof(MIDIHDR));
	    if (fp) fprintf(fp, "Long : %02x %02x %02x type=%02x BufferLen=%d\n",
	        lpEvent->state,
	        lpEvent->data1,
	        lpEvent->data2,
	        lpEvent->type,
	        lpEvent->nData);  /* oga */

            while ((mh.dwFlags & MHDR_DONE) == 0);

            midiOutUnprepareHeader(hmo, &mh, sizeof(MIDIHDR));
        }
        else { // MIDI�C�x���g
            DWORD dwMsg = (DWORD)(lpEvent->state | (lpEvent->data1 << 8) | (lpEvent->data2 << 16));
            
            midiOutShortMsg(hmo, dwMsg);
	    if (fp) fprintf(fp, "Short: %08x\n", dwMsg);
        }

        lpEvent = lpEvent->lpNext;
    }

    return 0;
}

static BOOL LoadFile(HANDLE hfile)
{
    int     i;
    WORD    wTrack;
    WORD    wFormat;
    DWORD   dwMagic;
    DWORD   dwResult;
    DWORD   dwDataLen;
    LPEVENT *lpaEvent; // �e�g���b�N���̍ŏ��̃C�x���g���w���|�C���^�z��

    ReadFile(hfile, &dwMagic, sizeof(DWORD), &dwResult, NULL);
    if (dwMagic != *(LPDWORD)"MThd")
        return FALSE;
    
    ReadFile(hfile, &dwDataLen, sizeof(DWORD), &dwResult, NULL);
    Reverse(&dwDataLen, sizeof(DWORD));
    if (dwDataLen != 6)
        return FALSE;
    
    ReadFile(hfile, &wFormat, sizeof(WORD), &dwResult, NULL);
    Reverse(&wFormat, sizeof(WORD));
    
    ReadFile(hfile, &wTrack, sizeof(WORD), &dwResult, NULL);
    Reverse(&wTrack, sizeof(WORD));
    
    ReadFile(hfile, &wTime, sizeof(WORD), &dwResult, NULL);
    Reverse(&wTime, sizeof(WORD));
    
    if (wFormat != 0) {
        MessageBox(NULL, TEXT("�t�H�[�}�b�g0�łȂ���΍Đ��ł��܂���B"), NULL, MB_ICONWARNING);
        return FALSE;
    }

    lpaEvent = (LPEVENT *)Alloc(sizeof(DWORD) * wTrack);
    
    for (i = 0; i < wTrack; i++) {
        if (!ReadTrack(hfile, &lpaEvent[i])) {
            MessageBox(NULL, TEXT("�s���ȃg���b�N�ł��B"), NULL, MB_ICONWARNING);
            return FALSE;
        }
    }
    
    lpHeader = lpaEvent[0];
    
    return TRUE;
}

static void Destroy(void)
{
    if (hthread != NULL) {
        bPlayThread = FALSE;

        WaitForSingleObject(hthread, INFINITE); // �X���b�h���I������܂őҋ@
        hthread = NULL;

        midiOutReset(hmo); // �Đ����̉�������
    }
    
    if (hheap != NULL) {
        HeapDestroy(hheap); // �X���b�h���I�����Ă���
        hheap = NULL;
    }
}

static BOOL ReadTrack(HANDLE hfile, LPEVENT *lplpEvent)
{
    BYTE    statePrev = 0; // �O�̃C�x���g�̃X�e�[�^�X�o�C�g
    DWORD   dwLen;
    DWORD   dwMagic;
    DWORD   dwResult;
    LPEVENT lpEvent;
    
    ReadFile(hfile, &dwMagic, sizeof(DWORD), &dwResult, NULL);
    if (dwMagic != *(LPDWORD)"MTrk")
        return FALSE;
    
    ReadFile(hfile, &dwLen, sizeof(DWORD), &dwResult, NULL);
    Reverse(&dwLen, sizeof(DWORD));

    lpEvent = (LPEVENT)Alloc(sizeof(EVENT)); // �ŏ��̃C�x���g�̃��������m��

    *lplpEvent = lpEvent; // *lplpEvent�͏�ɍŏ��̃C�x���g���w��
    
    for (;;) {
        ReadDelta(hfile, &lpEvent->dwDelta); // �f���^�^�C����ǂݍ���
        
        ReadFile(hfile, &lpEvent->state, 1, &dwResult, NULL); // �X�e�[�^�X�o�C�g��ǂݍ���
        if (!(lpEvent->state & 0x80)) { // �����j���O�X�e�[�^�X��
            lpEvent->state = statePrev; // ��O�̃C�x���g�̃X�e�[�^�X�o�C�g����
            SetFilePointer(hfile, -1, NULL, FILE_CURRENT); // �t�@�C���|�C���^����߂�
        }
        
        switch (lpEvent->state & 0xF0) { // �X�e�[�^�X�o�C�g����ɂǂ̃C�x���g������

        case 0x80:
        case 0x90:
        case 0xA0:
        case 0xB0:
        case 0xE0:
            ReadFile(hfile, &lpEvent->data1, 1, &dwResult, NULL);
            ReadFile(hfile, &lpEvent->data2, 1, &dwResult, NULL);
            break;
        case 0xC0:
        case 0xD0:
            ReadFile(hfile, &lpEvent->data1, 1, &dwResult, NULL);
            lpEvent->data2 = 0;
            break;
        
        case 0xF0:
            if (lpEvent->state == 0xF0) { // SysEx�C�x���g
                ReadFile(hfile, &lpEvent->nData, 1, &dwResult, NULL);

                lpEvent->lpData    = (LPBYTE)Alloc(lpEvent->nData + 1); // �擪��0xF0���܂߂�
                lpEvent->lpData[0] = lpEvent->state; // �ϒ��f�[�^�̐擪��0xF0
                ReadFile(hfile, (lpEvent->lpData + 1), lpEvent->nData, &dwResult, NULL);
            
                lpEvent->nData++;
            }
            else if (lpEvent->state == 0xFF) { // ���^�C�x���g
                DWORD dw;
                DWORD tmp;
                
                ReadFile(hfile, &lpEvent->type, 1, &dwResult, NULL); // type�̎擾

                dw = (DWORD)-1;

                switch (lpEvent->type) {

                case 0x00: dw = 2; break;
                case 0x01:
                case 0x02:
                case 0x03:
                case 0x04:
                case 0x05:
                case 0x06:
                case 0x07: break;
                case 0x20: dw = 1; break; 
                case 0x21: dw = 1; break; 
                case 0x2F: dw = 0; break; // �G���h�I�u�g���b�N
                case 0x51: dw = 3; break; // �Z�b�g�e���|
                case 0x54: dw = 5; break;
                case 0x58: dw = 4; break;
                case 0x59: dw = 2; break;
                case 0x7F: break;

                default:
                    MessageBox(NULL, TEXT("���݂��Ȃ����^�C�x���g�ł��B"), NULL, MB_ICONWARNING);
                    return FALSE;

                }
                
                tmp = dw;

                if (dw != -1) { // �f�[�^���͌Œ肩
                    ReadDelta(hfile, &dw);
                    if (dw != tmp) {
                        MessageBox(NULL, TEXT("�Œ蒷���^�C�x���g�̃f�[�^�����s���ł��B"), NULL, MB_ICONWARNING);
                        return FALSE;
                    }
                }
                else 
                    ReadDelta(hfile, &dw); // �C�ӂ̃f�[�^�����擾

                lpEvent->nData  = dw;
                lpEvent->lpData = (LPBYTE)Alloc(lpEvent->nData);
                ReadFile(hfile, lpEvent->lpData, lpEvent->nData, &dwResult, NULL); // �f�[�^�̎擾
                
                if (lpEvent->type == 0x2F) // �g���b�N�̏I�[
                    return TRUE;
            }
            else
                ;

            break;

        default:
            MessageBox(NULL, TEXT("�X�e�[�^�X�o�C�g�s��"), NULL, MB_ICONWARNING);
            return FALSE;

        }
        
        statePrev = lpEvent->state;
        
        lpEvent->lpNext = (LPEVENT)Alloc(sizeof(EVENT)); // ���̃C�x���g�̂��߂Ƀ��������m��
        lpEvent         = lpEvent->lpNext;
        if (lpEvent == NULL)
            break;
    }

    return FALSE;
}

static BOOL OpenDialog(HWND hwnd, LPTSTR lpszFile)
{
    OPENFILENAME ofn = {0};

    lpszFile[0] = '\0'; // �v������

    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.nMaxFile    = MAX_PATH;
    ofn.hwndOwner   = hwnd;
    ofn.lpstrFile   = lpszFile;
    ofn.lpstrTitle  = TEXT("MIDI�t�@�C���ǂݍ���");
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.lpstrFilter = TEXT("MIDI File (*.mid)\0*.mid\0\0");
    
    if (!GetOpenFileName(&ofn))
        return FALSE;

    return TRUE;
}

static void ReadDelta(HANDLE hfile, LPDWORD lpdwDelta)
{
    int   i;
    BYTE  tmp;
    DWORD dwResult;
    
    *lpdwDelta = 0;

    if (fp) fprintf(fp, "Delta: ");  /* oga */
    for (i = 0; i < sizeof(DWORD); i++) {
        ReadFile(hfile, &tmp, 1, &dwResult, NULL);
	if (fp) fprintf(fp, "%02x ", tmp);  /* oga */

        *lpdwDelta = ( (*lpdwDelta) << 7 ) | (tmp & 0x7F);

        if (!(tmp & 0x80)) // MSB�������Ă��Ȃ��Ȃ�΁A���̃o�C�g�̓f���^�^�C���ł͂Ȃ��̂Ŕ�����
            break;
    }
    if (fp) fprintf(fp, "\n");  /* oga */
}

static void Reverse(LPVOID lpData, int nSize)
{
    int    i;
    BYTE   tmp;
    LPBYTE lp     = (LPBYTE)lpData;
    LPBYTE lpTail = lp + nSize - 1;
    
    for (i = 0; i < nSize / 2; i++) {
        tmp = *lp;
        *lp = *lpTail;
        *lpTail = tmp;

        lp++;
        lpTail--;
    }
}

static double DeltaToMilliSecond(DWORD dwDelta)
{
    return (dwDelta * ((double)dwTempo / 1000) ) / wTime;
}

static LPVOID Alloc(int nSize)
{
    return HeapAlloc(hheap, HEAP_ZERO_MEMORY, nSize);
}


