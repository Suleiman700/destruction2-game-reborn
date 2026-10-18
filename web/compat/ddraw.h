// DirectDraw 7 subset emulated in software: 8-bit palettized surfaces, colour-keyed blits,
// GDI device contexts and a flipping primary surface presented to an HTML canvas.
#ifndef D2WEB_DDRAW_H
#define D2WEB_DDRAW_H

#include <windows.h>

#define DD_OK S_OK
#define DD_FALSE S_FALSE
#define _FACDD 0x876
#define MAKE_DDHRESULT(code) MAKE_HRESULT(1, _FACDD, code)
#define DDERR_GENERIC E_FAIL
#define DDERR_INVALIDPARAMS E_INVALIDARG
#define DDERR_OUTOFMEMORY E_OUTOFMEMORY
#define DDERR_SURFACELOST MAKE_DDHRESULT(450)
#define DDERR_WASSTILLDRAWING MAKE_DDHRESULT(540)
#define DDERR_NOEXCLUSIVEMODE MAKE_DDHRESULT(225)
#define DDERR_EXCLUSIVEMODEALREADYSET MAKE_DDHRESULT(581)
#define DDERR_WRONGMODE MAKE_DDHRESULT(587)
#define DDERR_NOTFOUND MAKE_DDHRESULT(255)

#define DDSCL_FULLSCREEN 0x00000001
#define DDSCL_NORMAL 0x00000008
#define DDSCL_EXCLUSIVE 0x00000010

#define DDSD_CAPS 0x00000001
#define DDSD_HEIGHT 0x00000002
#define DDSD_WIDTH 0x00000004
#define DDSD_PITCH 0x00000008
#define DDSD_BACKBUFFERCOUNT 0x00000020
#define DDSD_CKSRCBLT 0x00010000
#define DDSD_PIXELFORMAT 0x00001000

#define DDSCAPS_BACKBUFFER 0x00000004
#define DDSCAPS_COMPLEX 0x00000008
#define DDSCAPS_FLIP 0x00000010
#define DDSCAPS_FRONTBUFFER 0x00000020
#define DDSCAPS_OFFSCREENPLAIN 0x00000040
#define DDSCAPS_PRIMARYSURFACE 0x00000200
#define DDSCAPS_SYSTEMMEMORY 0x00000800
#define DDSCAPS_VIDEOMEMORY 0x00004000

#define DDBLT_COLORFILL 0x00000400
#define DDBLT_KEYSRC 0x00008000
#define DDBLT_WAIT 0x01000000
#define DDBLT_DDFX 0x00000800
#define DDBLT_ROTATIONANGLE 0x00010000
#define DDBLTFAST_NOCOLORKEY 0x00000000
#define DDBLTFAST_SRCCOLORKEY 0x00000001
#define DDBLTFAST_DESTCOLORKEY 0x00000002
#define DDBLTFAST_WAIT 0x00000010
#define DDBLTFX_MIRRORLEFTRIGHT 0x00000002
#define DDBLTFX_MIRRORUPDOWN 0x00000004
#define DDBLTFX_ROTATE90 0x00000010
#define DDBLTFX_ROTATE180 0x00000008
#define DDBLTFX_ROTATE270 0x00000020
#define DDFXCAPS_BLTROTATION90 0x00000080

#define DDLOCK_SURFACEMEMORYPTR 0x00000000
#define DDLOCK_WAIT 0x00000001
#define DDLOCK_READONLY 0x00000010
#define DDLOCK_WRITEONLY 0x00000020
#define DDLOCK_NOSYSLOCK 0x00000800

#define DDFLIP_WAIT 0x00000001
#define DDGFS_CANFLIP 0x00000001
#define DDGFS_ISFLIPDONE 0x00000002

#define DDCKEY_SRCBLT 0x00000008
#define DDPCAPS_8BIT 0x00000004
#define DDPCAPS_ALLOW256 0x00000040
#define DDPF_PALETTEINDEXED8 0x00000020

typedef struct _DDCOLORKEY { DWORD dwColorSpaceLowValue, dwColorSpaceHighValue; } DDCOLORKEY, *LPDDCOLORKEY;
typedef struct _DDSCAPS2 { DWORD dwCaps, dwCaps2, dwCaps3, dwCaps4; } DDSCAPS2, *LPDDSCAPS2;
typedef struct _DDPIXELFORMAT {
    DWORD dwSize, dwFlags, dwFourCC, dwRGBBitCount, dwRBitMask, dwGBitMask, dwBBitMask, dwRGBAlphaBitMask;
} DDPIXELFORMAT;
typedef struct _DDSURFACEDESC2 {
    DWORD dwSize, dwFlags, dwHeight, dwWidth;
    LONG lPitch;
    DWORD dwBackBufferCount, dwMipMapCount, dwAlphaBitDepth, dwReserved;
    LPVOID lpSurface;
    DDCOLORKEY ddckCKDestOverlay, ddckCKDestBlt, ddckCKSrcOverlay, ddckCKSrcBlt;
    DDPIXELFORMAT ddpfPixelFormat;
    DDSCAPS2 ddsCaps;
    DWORD dwTextureStage;
} DDSURFACEDESC2, *LPDDSURFACEDESC2;
typedef struct _DDBLTFX {
    DWORD dwSize, dwDDFX, dwROP, dwDDROP, dwRotationAngle;
    DWORD dwFillColor;
    DDCOLORKEY ddckDestColorkey, ddckSrcColorkey;
} DDBLTFX, *LPDDBLTFX;
typedef struct _DDCAPS { DWORD dwSize, dwCaps, dwCaps2, dwCKeyCaps, dwFXCaps, dwFXAlphaCaps, dwPalCaps; } DDCAPS, *LPDDCAPS;

struct IDirectDrawPalette : public IUnknown {
    PALETTEENTRY entries[256];
    ULONG refs;
    IDirectDrawPalette();
    ULONG AddRef() override;
    ULONG Release() override;
    HRESULT GetEntries(DWORD flags, DWORD start, DWORD count, LPPALETTEENTRY out);
    HRESULT SetEntries(DWORD flags, DWORD start, DWORD count, LPPALETTEENTRY in);
};

struct IDirectDrawSurface7 : public IUnknown {
    int width, height, pitch;
    BYTE *pixels;
    BOOL hasColorKey;
    BYTE colorKey;
    BOOL isPrimary;
    IDirectDrawSurface7 *backBuffer;  // primary only
    IDirectDrawPalette *palette;      // primary only
    ULONG refs;
    HDC dc;

    IDirectDrawSurface7(int w, int h);
    ~IDirectDrawSurface7();
    ULONG AddRef() override;
    ULONG Release() override;
    HRESULT Blt(LPRECT dst, IDirectDrawSurface7 *src, LPRECT srcRect, DWORD flags, LPDDBLTFX fx);
    HRESULT BltFast(DWORD x, DWORD y, IDirectDrawSurface7 *src, LPRECT srcRect, DWORD trans);
    HRESULT Lock(LPRECT rect, LPDDSURFACEDESC2 desc, DWORD flags, HANDLE ev);
    HRESULT Unlock(LPRECT rect);
    HRESULT GetDC(HDC *out);
    HRESULT ReleaseDC(HDC hdc);
    HRESULT Flip(IDirectDrawSurface7 *target, DWORD flags);
    HRESULT GetFlipStatus(DWORD flags);
    HRESULT SetPalette(IDirectDrawPalette *pal);
    HRESULT GetPalette(IDirectDrawPalette **pal);
    HRESULT SetColorKey(DWORD flags, LPDDCOLORKEY key);
    HRESULT Restore();
    HRESULT IsLost();
    HRESULT GetSurfaceDesc(LPDDSURFACEDESC2 desc);
    HRESULT GetAttachedSurface(LPDDSCAPS2 caps, IDirectDrawSurface7 **out);
};

struct IDirectDraw7 : public IUnknown {
    ULONG Release() override;
    HRESULT SetCooperativeLevel(HWND, DWORD);
    HRESULT SetDisplayMode(DWORD w, DWORD h, DWORD bpp, DWORD refresh, DWORD flags);
    HRESULT RestoreDisplayMode();
    HRESULT CreateSurface(LPDDSURFACEDESC2 desc, IDirectDrawSurface7 **out, IUnknown *outer);
    HRESULT CreatePalette(DWORD flags, LPPALETTEENTRY entries, IDirectDrawPalette **out, IUnknown *outer);
    HRESULT TestCooperativeLevel();
    HRESULT GetCaps(LPDDCAPS hw, LPDDCAPS hel);
    HRESULT RestoreAllSurfaces();
    HRESULT WaitForVerticalBlank(DWORD, HANDLE);
};

typedef IDirectDraw7 *LPDIRECTDRAW7, *LPDIRECTDRAW;
typedef IDirectDrawSurface7 *LPDIRECTDRAWSURFACE7, *LPDIRECTDRAWSURFACE;
typedef IDirectDrawPalette *LPDIRECTDRAWPALETTE;
typedef IDirectDrawSurface7 IDirectDrawSurface;
static const GUID IID_IDirectDraw7 = {0x15e65ec0, 0x3b9c, 0x11d2, {0xb9, 0x2f, 0x00, 0x60, 0x97, 0x97, 0xea, 0x5b}};
HRESULT DirectDrawCreateEx(GUID *, LPVOID *out, REFIID, IUnknown *);

#endif
