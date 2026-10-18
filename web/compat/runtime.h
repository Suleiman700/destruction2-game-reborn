// Internal interfaces shared by the web runtime translation units.
#ifndef D2WEB_RUNTIME_H
#define D2WEB_RUNTIME_H
#include <windows.h>
#include <ddraw.h>

struct D2Bitmap {
    int width, height, bpp;
    int paletteCount;
    RGBQUAD palette[256];
    BYTE *pixels;  // top-down; 1 byte per pixel (8 bpp) or 3 bytes BGR (24 bpp)
};

// Bitmap resources extracted from D2.exe, looked up case-insensitively like FindResource.
const D2Bitmap *d2_find_bitmap(const char *name);
// GDI-style colour matching against the current system palette (the primary surface's palette).
BYTE d2_match_color(COLORREF c);
void d2_blit_bitmap_to_surface(const D2Bitmap *bmp, IDirectDrawSurface7 *surf);

#endif
