// Wave format and multimedia I/O types used by the DirectSound code.
#ifndef D2WEB_MMSYSTEM_H
#define D2WEB_MMSYSTEM_H
#include <windows.h>

#define WAVE_FORMAT_PCM 1
#pragma pack(push, 1)
typedef struct tWAVEFORMATEX {
    WORD wFormatTag; WORD nChannels; DWORD nSamplesPerSec; DWORD nAvgBytesPerSec; WORD nBlockAlign; WORD wBitsPerSample; WORD cbSize;
} WAVEFORMATEX, *LPWAVEFORMATEX, *PWAVEFORMATEX;
typedef struct waveformat_tag { WORD wFormatTag; WORD nChannels; DWORD nSamplesPerSec; DWORD nAvgBytesPerSec; WORD nBlockAlign; } WAVEFORMAT;
typedef struct pcmwaveformat_tag { WAVEFORMAT wf; WORD wBitsPerSample; } PCMWAVEFORMAT;
#pragma pack(pop)
typedef const WAVEFORMATEX *LPCWAVEFORMATEX;

typedef struct _MMCKINFO { FOURCC ckid; DWORD cksize; FOURCC fccType; DWORD dwDataOffset; DWORD dwFlags; } MMCKINFO, *LPMMCKINFO;
#define mmioFOURCC(a, b, c, d) ((DWORD)(BYTE)(a) | ((DWORD)(BYTE)(b) << 8) | ((DWORD)(BYTE)(c) << 16) | ((DWORD)(BYTE)(d) << 24))
#define FOURCC_RIFF mmioFOURCC('R', 'I', 'F', 'F')
#endif
