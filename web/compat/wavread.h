// WAV reader used by the sound engine (implemented in compat/wavread.cpp).
#ifndef D2WEB_WAVREAD_H
#define D2WEB_WAVREAD_H
#include <windows.h>
#include <mmsystem.h>

class CWaveSoundRead {
public:
    WAVEFORMATEX *m_pwfx;  // format of the wave
    HMMIO m_hmmioIn;       // open file
    MMCKINFO m_ckIn;       // 'data' chunk; cksize counts down the bytes left to read
    MMCKINFO m_ckInRiff;   // RIFF chunk; cksize is the RIFF size
    CWaveSoundRead();
    ~CWaveSoundRead();
    HRESULT Open(CHAR *strFilename);
    HRESULT Reset();
    HRESULT Read(UINT nSizeToRead, BYTE *pbData, UINT *pnSizeRead);
    HRESULT Close();
};
#endif
