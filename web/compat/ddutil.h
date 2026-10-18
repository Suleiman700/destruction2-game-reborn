// Bitmap/palette helpers used by the game (implemented in compat/ddutil.cpp).
#ifndef D2WEB_DDUTIL_H
#define D2WEB_DDUTIL_H
#include <ddraw.h>
#ifdef __cplusplus
extern "C" {
#endif
IDirectDrawPalette *DDLoadPalette(IDirectDraw7 *pdd, LPCSTR szBitmap);
IDirectDrawSurface7 *DDLoadBitmap(IDirectDraw7 *pdd, LPCSTR szBitmap, int dx, int dy);
HRESULT DDReLoadBitmap(IDirectDrawSurface7 *pdds, LPCSTR szBitmap);
HRESULT DDCopyBitmap(IDirectDrawSurface7 *pdds, HBITMAP hbm, int x, int y, int dx, int dy);
DWORD DDColorMatch(IDirectDrawSurface7 *pdds, COLORREF rgb);
HRESULT DDSetColorKey(IDirectDrawSurface7 *pdds, COLORREF rgb);
#ifdef __cplusplus
}
#endif
#endif
