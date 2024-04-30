#include <windows.h>
#include <Mmsystem.h>
#include <stdio.h>

void play1(char *media_file)
{
    int  play_flag = SND_SYNC;

    printf("playing by sndPlaySound()...\n");
    sndPlaySound(media_file, play_flag);
    
}

void play2(char *media_file)
{
    //int  play_flag = SND_APPLICATION | SND_SYNC;
    int  play_flag = SND_SYNC;

    printf("playing by PlaySound()...\n");
    PlaySound(media_file,
              NULL, 
	      SND_FILENAME | play_flag);
}

// ### play3 start
#define OUTPUT_BUFFER_NUM   3       //再生ギャップを起こさないために2以上にする
#define OUTPUT_BUFFER_SIZE  0x8000  //処理が間に合わないならば大きくする
#define dprintf printf
LPWAVEHDR  g_OutputBuffer[OUTPUT_BUFFER_NUM];
HWAVEOUT   g_hwo;
FILE       *g_fp;
int        g_nBuffUseNum;
int        g_nBuffSetPos;
int        g_playing  = 0;
int        g_rsize    = 0;
int        g_total    = 0;


//再生バッファ確保
BOOL AllocOutputBuffer()
{
    int i;
    for(i = 0 ; i < OUTPUT_BUFFER_NUM ; i++){
        g_OutputBuffer[i]
            = (LPWAVEHDR) HeapAlloc(GetProcessHeap(), 
                                    HEAP_ZERO_MEMORY, 
                                    sizeof(WAVEHDR));
        if(g_OutputBuffer[i]){
            g_OutputBuffer[i]->lpData
                = (LPSTR) HeapAlloc(GetProcessHeap(), 
		                    HEAP_ZERO_MEMORY, 
				    OUTPUT_BUFFER_SIZE);
            if(g_OutputBuffer[i]->lpData) {
                g_OutputBuffer[i]->dwBufferLength = OUTPUT_BUFFER_SIZE;
	    }
        }
    }
    g_nBuffSetPos = g_nBuffUseNum = 0;
    return TRUE;
}

//再生バッファ開放
void FreeOutputBuffer()
{
    int i;
    for(i = 0 ; i < OUTPUT_BUFFER_NUM ; i++){
        if(g_OutputBuffer[i]){
            if(g_OutputBuffer[i]->lpData) {
                HeapFree(GetProcessHeap(), 0, g_OutputBuffer[i]->lpData);
	    }
            HeapFree(GetProcessHeap(), 0, g_OutputBuffer[i]);
            g_OutputBuffer[i] = NULL;
        }
    }
}

//データをバッファに読みこんで再生デバイスに送信
void    FillBuffer(HWAVEOUT hwo, FILE *fp)
{
    MMRESULT    mmRes;
    WAVEHDR*    pHdr;
    int         size;

    while(g_nBuffUseNum < OUTPUT_BUFFER_NUM) {
        pHdr = g_OutputBuffer[g_nBuffSetPos];

	dprintf("DEBUG: read wav to #%d buff\n", g_nBuffSetPos);

        size = fread(pHdr->lpData, 1, OUTPUT_BUFFER_SIZE, fp);
        if (size == 0) {
	    dprintf("DEBUG: no more data. g_nBuffUseNum = %d\n", g_nBuffUseNum);
	    break;
        }

        pHdr->dwBufferLength = size;

	g_rsize += size;
	dprintf("DEBUG: read size %d   %3d%% (%d/%d)KB\n", size,
	                               g_rsize/(g_total/100),
				       g_rsize/1024,
				       g_total/1024);

        mmRes = waveOutPrepareHeader(hwo, pHdr, sizeof(WAVEHDR));
        if(mmRes != MMSYSERR_NOERROR){
            dprintf("Error in FillBuffer\n");
            break;
        }
        mmRes = waveOutWrite(hwo, pHdr, sizeof(WAVEHDR));
        if(mmRes != MMSYSERR_NOERROR){
            dprintf("Error in FillBuffer\n");
            break;
        }
	g_playing = 1;

        g_nBuffSetPos = (g_nBuffSetPos + 1) % OUTPUT_BUFFER_NUM;
        g_nBuffUseNum++;
    }

    return;
}


// コールバック関数  
void CALLBACK waveOutCallBack(HWAVEOUT hwo, 
                              UINT uMsg,
			      DWORD dwInstance, 
			      DWORD dwParam1, 
			      DWORD dwParam2)
{
    switch(uMsg) {
      case WOM_OPEN:
        // デバイスOpen
	dprintf("WOM_OPEN event!\n");
        break;

      case WOM_CLOSE:
        // デバイスClose
	dprintf("WOM_CLOSE event!\n");
        break;

      case WOM_DONE:
        // データブロック再生終了
	{
	    int i;
	    LPWAVEHDR pwh = (LPWAVEHDR)dwParam1;
	    dprintf("WOM_DONE event!\n");

            //使用中バッファ数を一つ開放
            g_nBuffUseNum--;

	    FillBuffer(hwo, g_fp);

	    dprintf("WOM_DONE event! (g_nBuffUseNum:%d)\n", g_nBuffUseNum);
	    if (g_nBuffUseNum == 0){
	        // 再生終了 
                // データブロックの非準備状態へ
	        dprintf("Play done!\n");
		g_playing = 0;
	    }
	}
        break;
    }
}

/* data formats */
typedef struct riff_hdr {
    char          id[4];           /* RIFF id   ("RIFF")        */
    unsigned long len;             /* length                    */
    char          wave_id[4];      /* data type ("WAVE")        */
} riff_hdr_t;

typedef struct chunk_hdr {
    char          id[4];           /* chunk id  ("fmt "|"data"...) */
    unsigned long len;             /* chunk length                 */
} chunk_hdr_t;

/* chunk type fmt  1 */
typedef struct ck_fmt {
    unsigned short wFormatTag;        /* Format category       */
    unsigned short wChannels;         /* Number of channels    */
    unsigned long  dwSamplesPerSec;   /* Sampling rate         */
    unsigned long  dwAvgBytesPerSec;  /* For buffer estimation */
    unsigned short wBlockAlign;       /* Data block size       */
    unsigned short wBitsPerSample;    /* Sample size (for WAVE_FORMAT_PCM */
    unsigned short unknown;           /* Unknown data                     */
    char dummy[256];                  /* dummy area for over run */
} ck_fmt_t;

/* chunk type fact 2 */
typedef struct ck_fact {
    int  unknown;               /* unknown */
} ck_fact_t;

int GetWaveFormat(FILE *fp, WAVEFORMATEX *pwfx)
{
    riff_hdr_t  riffhdr;
    chunk_hdr_t chunkhdr;
    char        buf_fmt[4096];
    char        buf_fact[4096];
    int         size;

    ck_fmt_t  *ckfmt  = (ck_fmt_t *)buf_fmt;
    ck_fact_t *ckfact = (ck_fact_t *)buf_fact;

    // Read WAV Header
    fread(&riffhdr, sizeof(riffhdr), 1, fp);
    if (memcmp(riffhdr.id, "RIFF", 4)) {
        dprintf("not RIFF format\n");
	return -1;
    }

    // Read Chunk Header
    while (1) {
        size = fread(&chunkhdr, 1, sizeof(chunkhdr), fp);
	if (size <= 0) {
            dprintf("Fmt Chunk Not Found Error\n");
	    return -1;
	}
	if (!strncmp(chunkhdr.id, "fmt", 3)) {
	    // WAVフォーマットチャンク(fmt)発見 
            // Read Fmt Chunk Header
            size = fread(ckfmt, 1, chunkhdr.len, fp);
            if (size != chunkhdr.len) {
                dprintf("Fmt Chunk Read Error\n");
        	return -1;
            }

            // Stereo 16bit 44kHz
            //wfx.wFormatTag      = WAVE_FORMAT_PCM;
            //wfx.nChannels       = 2;
            //wfx.nSamplesPerSec  = 44100;
            //wfx.wBitsPerSample  = 16;
            //wfx.nBlockAlign     = wfx.nChannels * wfx.wBitsPerSample/8;
            //wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
            //wfx.cbSize          = 0;

            pwfx->wFormatTag      = ckfmt->wFormatTag;
            pwfx->nChannels       = ckfmt->wChannels;
            pwfx->nSamplesPerSec  = ckfmt->dwSamplesPerSec;
            pwfx->wBitsPerSample  = ckfmt->wBitsPerSample;
            pwfx->nBlockAlign     = ckfmt->wBlockAlign;
            pwfx->nAvgBytesPerSec = ckfmt->dwAvgBytesPerSec;
            pwfx->cbSize          = 0;

            dprintf("---- fmt chunk (len=%d) ----\n", chunkhdr.len);
            dprintf("FormatTag      : 0x%04x     (%d) %s\n",ckfmt->wFormatTag, 
                                                   ckfmt->wFormatTag,
                    (ckfmt->wFormatTag == WAVE_FORMAT_PCM)?"MS PCM":"Other PCM");
            dprintf("Channels       : 0x%04x     (%d) %s\n",ckfmt->wChannels, 
                                                   ckfmt->wChannels,
                    (ckfmt->wChannels == 1)?"Mono":"Stereo");
            dprintf("SamplesPerSec  : 0x%08x (%d)\n",ckfmt->dwSamplesPerSec, 
                                            ckfmt->dwSamplesPerSec);
            dprintf("AvgBytesPerSec : 0x%08x (%d)\n",ckfmt->dwAvgBytesPerSec, 
                                            ckfmt->dwAvgBytesPerSec);
            dprintf("BlockAlign     : 0x%04x     (%d)\n",ckfmt->wBlockAlign, 
                                                ckfmt->wBlockAlign);
            dprintf("BitsPerSample  : 0x%04x     (%d bits)\n",ckfmt->wBitsPerSample, 
                                                     ckfmt->wBitsPerSample);

	} else if (!strncmp(chunkhdr.id, "fact", 4)) {
	    // FACTチャンク(fact)は空読み 
            fread(ckfact, chunkhdr.len, 1, fp);
	} else if (!strncmp(chunkhdr.id, "data", 4)) {
	    // DATAチャンク(data)発見
	    // DATAブロックの先頭位置に位置付けてこの関数は終了 
	    dprintf("data chunk found. len = %d\n", chunkhdr.len);
	    g_total = chunkhdr.len;
	    break;
	}
    };

    return 0;
}

void play3(char *media_file)
{
    MMRESULT     mmRes;
    WAVEFORMATEX wfx;
    int          size;
    int          i;

    // Stereo 16bit 44kHz
    //wfx.wFormatTag      = WAVE_FORMAT_PCM;
    //wfx.nChannels       = 2;
    //wfx.nSamplesPerSec  = 44100;
    //wfx.wBitsPerSample  = 16;
    //wfx.nBlockAlign     = wfx.nChannels * wfx.wBitsPerSample/8;
    //wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    //wfx.cbSize          = 0;

    // open wav
    if ((g_fp = fopen(media_file, "rb")) == NULL) {
        perror(media_file);
        return;
    }

    // get wav format
    if (GetWaveFormat(g_fp, &wfx) < 0) {
        dprintf("Wave Format Error. %s\n", media_file);
        return;
    }

    mmRes = waveOutOpen(&g_hwo,
                        WAVE_MAPPER,
		        &wfx,
		        (DWORD)(LPVOID)waveOutCallBack,
		        0,
		        CALLBACK_FUNCTION);
    if (mmRes != MMSYSERR_NOERROR) {
        dprintf("waveOutOpen Error %d\n", mmRes);
        exit(1);
    }

    AllocOutputBuffer();

    // wav読み込み & デバイス書き込み要求 
    FillBuffer(g_hwo, g_fp);

    // すべて再生されるまで待つ  
    while (g_playing) {
        Sleep(500);
    }

    // データブロックの非準備状態へ
    for(i = 0 ; i < OUTPUT_BUFFER_NUM ; i++) {
        waveOutUnprepareHeader(g_hwo, 
                               g_OutputBuffer[i], 
                               sizeof(WAVEHDR));
    }

    //再生デバイスをクローズ
    dprintf("DEBUG: waveOutClose()\n");
    waveOutClose(g_hwo);

    //再生バッファを開放
    dprintf("DEBUG: FreeOutputBuffer()\n");
    FreeOutputBuffer();

    //WAVファイルクローズ 
    dprintf("DEBUG: fclose()\n");
    fclose(g_fp);
}
// ### play3 end

int main(int a, char *b[])
{
    int  i;
    int  play_type = 3;
    char *media_file = "I:\\WINNT\\Media\\ringin.wav";

    for (i=1; i<a; i++) {
        if (!strcmp(b[i], "-h")) {
            printf("usage: play [{-1|-2|<-3>}] [media_file]\n");
	    printf("    -1 : sndPlaySound()\n");
	    printf("    -2 : PlaySound()\n");
	    printf("    -3 : waveOutWrite()  (default)\n");
	    exit(1);
        }
        if (!strcmp(b[i], "-1")) {
	    play_type = 1;
	    continue;
	}
        if (!strcmp(b[i], "-2")) {
	    play_type = 2;
	    continue;
	}
        if (!strcmp(b[i], "-3")) {
	    play_type = 3;
	    continue;
	}
        media_file = b[i];
    }

    switch (play_type) {
      case 1:
        play1(media_file);
	break;
      case 2:
        play2(media_file);
	break;
      case 3:
        play3(media_file);
	break;
    }

    return 0;
}
