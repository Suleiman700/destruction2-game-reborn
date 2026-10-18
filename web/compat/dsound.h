// DirectSound subset backed by a software mixer that feeds Web Audio.
#ifndef D2WEB_DSOUND_H
#define D2WEB_DSOUND_H
#include <windows.h>
#include <mmsystem.h>

#define DS_OK S_OK
#define DSERR_BUFFERLOST MAKE_HRESULT(1, 0x878, 150)
#define DSERR_OUTOFMEMORY E_OUTOFMEMORY
#define DSERR_INVALIDPARAM E_INVALIDARG
#define DSSCL_NORMAL 1
#define DSSCL_PRIORITY 2
#define DSSCL_EXCLUSIVE 3
#define DSBCAPS_PRIMARYBUFFER 0x00000001
#define DSBCAPS_STATIC 0x00000002
#define DSBCAPS_CTRLFREQUENCY 0x00000020
#define DSBCAPS_CTRLPAN 0x00000040
#define DSBCAPS_CTRLVOLUME 0x00000080
#define DSBCAPS_CTRLPOSITIONNOTIFY 0x00000100
#define DSBCAPS_GETCURRENTPOSITION2 0x00010000
#define DSBPLAY_LOOPING 0x00000001
#define DSBSTATUS_PLAYING 0x00000001
#define DSBSTATUS_BUFFERLOST 0x00000002
#define DSBSTATUS_LOOPING 0x00000004
#define DSBVOLUME_MIN (-10000)
#define DSBVOLUME_MAX 0
#define DSBPAN_LEFT (-10000)
#define DSBPAN_CENTER 0
#define DSBPAN_RIGHT 10000

typedef struct _DSBUFFERDESC { DWORD dwSize, dwFlags, dwBufferBytes, dwReserved; LPWAVEFORMATEX lpwfxFormat; } DSBUFFERDESC, *LPDSBUFFERDESC;
typedef const DSBUFFERDESC *LPCDSBUFFERDESC;
typedef struct _DSCAPS { DWORD dwSize, dwFlags, dwMinSecondarySampleRate, dwMaxSecondarySampleRate, dwPrimaryBuffers,
    dwMaxHwMixingAllBuffers, dwMaxHwMixingStaticBuffers, dwMaxHwMixingStreamingBuffers; } DSCAPS, *LPDSCAPS;

struct IDirectSoundBuffer : public IUnknown {
    BOOL primary;
    BYTE *data;
    DWORD size;
    WAVEFORMATEX fmt;
    double pos;       // play cursor in frames (fractional for resampling)
    BOOL playing, looping;
    LONG volume, pan;
    ULONG refs;
    IDirectSoundBuffer *nextBuffer;
    IDirectSoundBuffer(LPCDSBUFFERDESC desc);
    ~IDirectSoundBuffer();
    ULONG Release() override;
    HRESULT SetFormat(LPCWAVEFORMATEX);
    HRESULT Play(DWORD, DWORD, DWORD flags);
    HRESULT Stop();
    HRESULT SetVolume(LONG);
    HRESULT SetPan(LONG);
    HRESULT GetVolume(LPLONG);
    HRESULT GetPan(LPLONG);
    HRESULT SetFrequency(DWORD);
    HRESULT GetStatus(LPDWORD);
    HRESULT SetCurrentPosition(DWORD);
    HRESULT GetCurrentPosition(LPDWORD play, LPDWORD write);
    HRESULT Lock(DWORD off, DWORD bytes, LPVOID *p1, LPDWORD n1, LPVOID *p2, LPDWORD n2, DWORD flags);
    HRESULT Unlock(LPVOID, DWORD, LPVOID, DWORD);
    HRESULT Restore();
};
struct IDirectSound : public IUnknown {
    ULONG Release() override;
    HRESULT SetCooperativeLevel(HWND, DWORD);
    HRESULT CreateSoundBuffer(LPCDSBUFFERDESC, IDirectSoundBuffer **, LPUNKNOWN);
    HRESULT GetCaps(LPDSCAPS);
};
typedef IDirectSound *LPDIRECTSOUND;
typedef IDirectSoundBuffer *LPDIRECTSOUNDBUFFER;
HRESULT DirectSoundCreate(LPGUID, LPDIRECTSOUND *, LPUNKNOWN);
#endif
