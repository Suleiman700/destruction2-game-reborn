// Replacement for the DirectX SDK ddutil.cpp. Bitmaps are read from the extracted D2.exe resources
// (/game/Bitmaps/<NAME>.bmp). Pixel colours are matched to the current system (primary) palette the
// way GDI's StretchBlt does on an 8-bit display.
#include <windows.h>
#include <ddraw.h>
#include "ddutil.h"
#include "runtime.h"

extern "C" IDirectDrawPalette *DDLoadPalette(IDirectDraw7 *pdd, LPCSTR szBitmap) {
    IDirectDrawPalette *ddpal = NULL;
    PALETTEENTRY ape[256];
    // Build a 332 palette as the default (same as the SDK sample).
    for (int i = 0; i < 256; i++) {
        ape[i].peRed = (BYTE)(((i >> 5) & 0x07) * 255 / 7);
        ape[i].peGreen = (BYTE)(((i >> 2) & 0x07) * 255 / 7);
        ape[i].peBlue = (BYTE)(((i >> 0) & 0x03) * 255 / 3);
        ape[i].peFlags = 0;
    }
    const D2Bitmap *bmp = szBitmap ? d2_find_bitmap(szBitmap) : NULL;
    if (bmp) {
        for (int i = 0; i < bmp->paletteCount; i++) {
            ape[i].peRed = bmp->palette[i].rgbRed;
            ape[i].peGreen = bmp->palette[i].rgbGreen;
            ape[i].peBlue = bmp->palette[i].rgbBlue;
            ape[i].peFlags = 0;
        }
    }
    pdd->CreatePalette(DDPCAPS_8BIT, ape, &ddpal, NULL);
    return ddpal;
}

extern "C" HRESULT DDCopyBitmap(IDirectDrawSurface7 *pdds, HBITMAP hbm, int x, int y, int dx, int dy) {
    const D2Bitmap *bmp = (const D2Bitmap *)hbm;
    if (!bmp || !pdds) return E_FAIL;
    d2_blit_bitmap_to_surface(bmp, pdds);
    return DD_OK;
}

extern "C" IDirectDrawSurface7 *DDLoadBitmap(IDirectDraw7 *pdd, LPCSTR szBitmap, int dx, int dy) {
    const D2Bitmap *bmp = d2_find_bitmap(szBitmap);
    if (!bmp) return NULL;
    DDSURFACEDESC2 ddsd;
    ZeroMemory(&ddsd, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
    ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
    ddsd.dwWidth = bmp->width;
    ddsd.dwHeight = bmp->height;
    IDirectDrawSurface7 *pdds;
    if (pdd->CreateSurface(&ddsd, &pdds, NULL) != DD_OK) return NULL;
    d2_blit_bitmap_to_surface(bmp, pdds);
    return pdds;
}

extern "C" HRESULT DDReLoadBitmap(IDirectDrawSurface7 *pdds, LPCSTR szBitmap) {
    const D2Bitmap *bmp = d2_find_bitmap(szBitmap);
    if (!bmp) {
        OutputDebugString("handle is null\n");
        return E_FAIL;
    }
    d2_blit_bitmap_to_surface(bmp, pdds);
    return DD_OK;
}

extern "C" DWORD DDColorMatch(IDirectDrawSurface7 *pdds, COLORREF rgb) {
    if (rgb == CLR_INVALID) return pdds && pdds->pixels ? pdds->pixels[0] : 0;
    return d2_match_color(rgb);
}

extern "C" HRESULT DDSetColorKey(IDirectDrawSurface7 *pdds, COLORREF rgb) {
    if (!pdds) return E_FAIL;
    DDCOLORKEY ddck;
    ddck.dwColorSpaceLowValue = DDColorMatch(pdds, rgb);
    ddck.dwColorSpaceHighValue = ddck.dwColorSpaceLowValue;
    return pdds->SetColorKey(DDCKEY_SRCBLT, &ddck);
}
