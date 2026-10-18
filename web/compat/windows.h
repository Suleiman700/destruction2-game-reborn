// Minimal Win32 API surface needed by Destruction II, implemented for the web (Emscripten).
#ifndef D2WEB_WINDOWS_H
#define D2WEB_WINDOWS_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <ctype.h>

// ---------------------------------------------------------------------------
// Calling conventions / decorations
// ---------------------------------------------------------------------------
#define WINAPI
#define APIENTRY
#define CALLBACK
#define PASCAL
#define FAR
#define NEAR
#define far
#define near
#define __stdcall
#define __cdecl
#define STDMETHODCALLTYPE
#define CONST const
#define VOID void
#define IN
#define OUT
#define OPTIONAL

// ---------------------------------------------------------------------------
// Base types (Win32 / ILP32 layout; wasm32 longs are 32-bit like Windows)
// ---------------------------------------------------------------------------
typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef long LONG;
typedef unsigned long ULONG;
typedef unsigned int UINT;
typedef int INT;
typedef short SHORT;
typedef unsigned short USHORT;
typedef char CHAR;
typedef unsigned char UCHAR;
typedef wchar_t WCHAR;
typedef float FLOAT;
typedef long long LONGLONG;
typedef unsigned long long ULONGLONG;
typedef long HRESULT;
typedef DWORD COLORREF;
typedef UINT WPARAM;
typedef LONG LPARAM;
typedef LONG LRESULT;
typedef char TCHAR;
typedef BYTE BOOLEAN;
typedef DWORD *LPDWORD, *PDWORD;
typedef BYTE *LPBYTE, *PBYTE;
typedef WORD *LPWORD;
typedef LONG *LPLONG;
typedef BOOL *LPBOOL;
typedef void *LPVOID, *PVOID;
typedef const void *LPCVOID;
typedef char *LPSTR, *PSTR, *LPTSTR, *HPSTR;
typedef const char *LPCSTR, *LPCTSTR, *PCSTR;
typedef int *LPINT;
typedef UINT *LPUINT;
typedef DWORD FOURCC;
typedef uintptr_t ULONG_PTR, DWORD_PTR, UINT_PTR;
typedef intptr_t LONG_PTR, INT_PTR;
typedef unsigned short ATOM;

typedef union _LARGE_INTEGER {
    struct { DWORD LowPart; LONG HighPart; };
    LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef struct _GUID { DWORD Data1; WORD Data2; WORD Data3; BYTE Data4[8]; } GUID, IID, CLSID;
typedef const GUID &REFGUID;
typedef const IID &REFIID;
typedef const CLSID &REFCLSID;
typedef GUID *LPGUID;

// Handles: opaque pointers
struct D2Handle_;
typedef void *HANDLE;
typedef HANDLE HWND, HINSTANCE, HMODULE, HICON, HCURSOR, HMENU, HGLOBAL, HLOCAL, HRSRC, HMMIO, LOCALHANDLE;
typedef struct D2GdiObject *HGDIOBJ;
typedef HGDIOBJ HFONT, HPEN, HBRUSH, HBITMAP, HPALETTE, HRGN;
typedef struct D2DC *HDC;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef NULL
#define NULL 0
#endif
#define MAX_PATH 260
#define INFINITE 0xFFFFFFFF

#define S_OK ((HRESULT)0L)
#define S_FALSE ((HRESULT)1L)
#define E_FAIL ((HRESULT)0x80004005L)
#define E_INVALIDARG ((HRESULT)0x80070057L)
#define E_OUTOFMEMORY ((HRESULT)0x8007000EL)
#define E_NOINTERFACE ((HRESULT)0x80004002L)
#define E_POINTER ((HRESULT)0x80004003L)
#define E_ACCESSDENIED ((HRESULT)0x80070005L)
#define E_PENDING ((HRESULT)0x8000000AL)
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr) (((HRESULT)(hr)) < 0)
#define MAKE_HRESULT(sev, fac, code) ((HRESULT)(((unsigned long)(sev) << 31) | ((unsigned long)(fac) << 16) | ((unsigned long)(code))))

#define LOWORD(l) ((WORD)((DWORD_PTR)(l) & 0xffff))
#define HIWORD(l) ((WORD)((DWORD_PTR)(l) >> 16))
#define LOBYTE(w) ((BYTE)((DWORD_PTR)(w) & 0xff))
#define HIBYTE(w) ((BYTE)((DWORD_PTR)(w) >> 8))
#define MAKEWORD(a, b) ((WORD)(((BYTE)(a)) | ((WORD)((BYTE)(b))) << 8))
#define MAKELONG(a, b) ((LONG)(((WORD)(a)) | ((DWORD)((WORD)(b))) << 16))

#define ZeroMemory(d, l) memset((d), 0, (l))
#define FillMemory(d, l, f) memset((d), (f), (l))
#define CopyMemory(d, s, l) memcpy((d), (s), (l))
#define MoveMemory(d, s, l) memmove((d), (s), (l))

#define lstrlen(s) ((int)strlen(s))
#define lstrcpy strcpy
#define lstrcat strcat
#define lstrcmp strcmp
#define lstrcmpi strcasecmp
#define stricmp strcasecmp
#define _stricmp strcasecmp
#define strcmpi strcasecmp
#define strnicmp strncasecmp
#define _strnicmp strncasecmp
#define wsprintf sprintf
#define _snprintf snprintf
#define _vsnprintf vsnprintf

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

char *_itoa(int value, char *str, int radix);
char *_ltoa(long value, char *str, int radix);
char *_strupr(char *s);
char *_strlwr(char *s);
#define itoa _itoa
#define ltoa _ltoa
#define strupr _strupr
#define strlwr _strlwr

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------
typedef struct tagRECT { LONG left, top, right, bottom; } RECT, *LPRECT, *PRECT;
typedef const RECT *LPCRECT;
typedef struct tagPOINT { LONG x, y; } POINT, *LPPOINT, *PPOINT;
typedef struct tagSIZE { LONG cx, cy; } SIZE, *LPSIZE, *PSIZE;
BOOL SetRect(LPRECT r, int l, int t, int ri, int b);

// ---------------------------------------------------------------------------
// Colors / GDI
// ---------------------------------------------------------------------------
#define RGB(r, g, b) ((COLORREF)(((BYTE)(r) | ((WORD)((BYTE)(g)) << 8)) | (((DWORD)(BYTE)(b)) << 16)))
#define GetRValue(rgb) ((BYTE)(rgb))
#define GetGValue(rgb) ((BYTE)(((WORD)(rgb)) >> 8))
#define GetBValue(rgb) ((BYTE)((rgb) >> 16))
#define PALETTEINDEX(i) ((COLORREF)(0x01000000 | (DWORD)(WORD)(i)))
#define CLR_INVALID 0xFFFFFFFF

typedef struct tagPALETTEENTRY { BYTE peRed, peGreen, peBlue, peFlags; } PALETTEENTRY, *LPPALETTEENTRY;
typedef struct tagRGBQUAD { BYTE rgbBlue, rgbGreen, rgbRed, rgbReserved; } RGBQUAD;

#pragma pack(push, 2)
typedef struct tagBITMAPFILEHEADER { WORD bfType; DWORD bfSize; WORD bfReserved1, bfReserved2; DWORD bfOffBits; } BITMAPFILEHEADER;
#pragma pack(pop)
typedef struct tagBITMAPINFOHEADER {
    DWORD biSize; LONG biWidth; LONG biHeight; WORD biPlanes; WORD biBitCount; DWORD biCompression;
    DWORD biSizeImage; LONG biXPelsPerMeter; LONG biYPelsPerMeter; DWORD biClrUsed; DWORD biClrImportant;
} BITMAPINFOHEADER, *LPBITMAPINFOHEADER;
typedef struct tagBITMAP { LONG bmType, bmWidth, bmHeight, bmWidthBytes; WORD bmPlanes, bmBitsPixel; LPVOID bmBits; } BITMAP;

typedef struct tagLOGBRUSH { UINT lbStyle; COLORREF lbColor; ULONG_PTR lbHatch; } LOGBRUSH;
#define LF_FACESIZE 32
typedef struct tagLOGFONT {
    LONG lfHeight, lfWidth, lfEscapement, lfOrientation, lfWeight;
    BYTE lfItalic, lfUnderline, lfStrikeOut, lfCharSet, lfOutPrecision, lfClipPrecision, lfQuality, lfPitchAndFamily;
    CHAR lfFaceName[LF_FACESIZE];
} LOGFONT, *PLOGFONT, *LPLOGFONT;

#define FW_DONTCARE 0
#define FW_THIN 100
#define FW_LIGHT 300
#define FW_NORMAL 400
#define FW_REGULAR 400
#define FW_MEDIUM 500
#define FW_SEMIBOLD 600
#define FW_BOLD 700
#define FW_HEAVY 900
#define ANSI_CHARSET 0
#define DEFAULT_CHARSET 1
#define OUT_DEFAULT_PRECIS 0
#define OUT_TT_PRECIS 4
#define CLIP_DEFAULT_PRECIS 0
#define CLIP_TT_ALWAYS (2 << 4)
#define DEFAULT_QUALITY 0
#define DRAFT_QUALITY 1
#define PROOF_QUALITY 2
#define NONANTIALIASED_QUALITY 3
#define ANTIALIASED_QUALITY 4
#define DEFAULT_PITCH 0
#define FIXED_PITCH 1
#define VARIABLE_PITCH 2
#define FF_DONTCARE (0 << 4)
#define FF_ROMAN (1 << 4)
#define FF_SWISS (2 << 4)
#define FF_MODERN (3 << 4)
#define TMPF_TRUETYPE 0x04

#define PS_SOLID 0
#define PS_DASH 1
#define PS_DOT 2
#define PS_NULL 5
#define BS_SOLID 0
#define BS_NULL 1
#define BS_HOLLOW BS_NULL

#define TRANSPARENT 1
#define OPAQUE 2
#define TA_NOUPDATECP 0
#define TA_UPDATECP 1
#define TA_LEFT 0
#define TA_RIGHT 2
#define TA_CENTER 6
#define TA_TOP 0
#define TA_BOTTOM 8
#define TA_BASELINE 24

#define WHITE_BRUSH 0
#define LTGRAY_BRUSH 1
#define GRAY_BRUSH 2
#define DKGRAY_BRUSH 3
#define BLACK_BRUSH 4
#define NULL_BRUSH 5
#define HOLLOW_BRUSH NULL_BRUSH
#define WHITE_PEN 6
#define BLACK_PEN 7
#define NULL_PEN 8

#define SRCCOPY 0x00CC0020
#define IMAGE_BITMAP 0
#define LR_DEFAULTCOLOR 0
#define LR_LOADFROMFILE 0x10
#define LR_CREATEDIBSECTION 0x2000
#define RT_BITMAP ((LPCSTR)2)

HFONT CreateFont(int h, int w, int esc, int orient, int weight, DWORD italic, DWORD underline, DWORD strike,
                 DWORD charset, DWORD outprec, DWORD clipprec, DWORD quality, DWORD pitch, LPCSTR face);
HFONT CreateFontIndirect(const LOGFONT *lf);
HPEN CreatePen(int style, int width, COLORREF color);
HBRUSH CreateBrushIndirect(const LOGBRUSH *lb);
HBRUSH CreateSolidBrush(COLORREF color);
HGDIOBJ GetStockObject(int i);
HGDIOBJ SelectObject(HDC hdc, HGDIOBJ obj);
BOOL DeleteObject(HGDIOBJ obj);
COLORREF SetTextColor(HDC hdc, COLORREF c);
COLORREF SetBkColor(HDC hdc, COLORREF c);
int SetBkMode(HDC hdc, int mode);
UINT SetTextAlign(HDC hdc, UINT align);
BOOL TextOut(HDC hdc, int x, int y, LPCSTR s, int n);
BOOL GetTextExtentPoint(HDC hdc, LPCSTR s, int n, LPSIZE sz);
#define GetTextExtentPoint32 GetTextExtentPoint
BOOL Rectangle(HDC hdc, int l, int t, int r, int b);
BOOL RoundRect(HDC hdc, int l, int t, int r, int b, int w, int h);
BOOL MoveToEx(HDC hdc, int x, int y, LPPOINT old);
BOOL LineTo(HDC hdc, int x, int y);
COLORREF SetPixel(HDC hdc, int x, int y, COLORREF c);
BOOL SetPixelV(HDC hdc, int x, int y, COLORREF c);
COLORREF GetPixel(HDC hdc, int x, int y);
int GetObject(HGDIOBJ h, int n, LPVOID out);
HANDLE LoadImage(HINSTANCE, LPCSTR name, UINT type, int cx, int cy, UINT flags);
// Win32 LoadBitmap. The game calls it as LoadBitmap(&surface, name) inside Game methods; in the shipped
// D2.exe this resolved to USER32 LoadBitmapA with a bogus HINSTANCE (verified via REA: callers
// 0x406f5f and 0x40e53c) and therefore always failed without side effects.
inline HBITMAP LoadBitmap(const void *, LPCSTR) { return NULL; }
HDC CreateCompatibleDC(HDC);
BOOL DeleteDC(HDC);
BOOL StretchBlt(HDC, int, int, int, int, HDC, int, int, int, int, DWORD);

// ---------------------------------------------------------------------------
// Window / message stubs (the browser drives the loop)
// ---------------------------------------------------------------------------
typedef LRESULT (*WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef struct tagMSG { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; DWORD time; POINT pt; } MSG, *LPMSG;
typedef struct tagWNDCLASS {
    UINT style; WNDPROC lpfnWndProc; int cbClsExtra, cbWndExtra; HINSTANCE hInstance; HICON hIcon; HCURSOR hCursor;
    HBRUSH hbrBackground; LPCSTR lpszMenuName; LPCSTR lpszClassName;
} WNDCLASS;

#define WM_CREATE 0x0001
#define WM_DESTROY 0x0002
#define WM_SETFOCUS 0x0007
#define WM_KILLFOCUS 0x0008
#define WM_QUIT 0x0012
#define WM_ENDSESSION 0x0016
#define WM_ACTIVATEAPP 0x001C
#define WM_SETCURSOR 0x0020
#define WM_KEYDOWN 0x0100
#define WM_KEYUP 0x0101
#define WM_COMMAND 0x0111
#define VK_F12 0x7B
#define VK_ESCAPE 0x1B
#define CS_HREDRAW 2
#define CS_VREDRAW 1
#define WS_POPUP 0x80000000L
#define WS_EX_TOPMOST 0x00000008L
#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
#define GWL_HINSTANCE (-6)
#define IDC_ARROW ((LPCSTR)32512)
#define MAKEINTRESOURCE(i) ((LPSTR)((ULONG_PTR)((WORD)(i))))
#define PM_NOREMOVE 0
#define PM_REMOVE 1
#define MB_OK 0
#define MB_ICONERROR 0x10
#define MB_SYSTEMMODAL 0x1000
#define MB_ICONINFORMATION 0x40
#define SW_SHOW 5
#define SW_SHOWNORMAL 1
#define SPI_SCREENSAVERRUNNING 97
#define CF_TEXT 1
#define LPTR 0x0040
#define LMEM_FIXED 0
#define GMEM_MOVEABLE 2
#define OF_READ 0

ATOM RegisterClass(const WNDCLASS *);
HWND CreateWindowEx(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
LRESULT DefWindowProc(HWND, UINT, WPARAM, LPARAM);
BOOL ShowWindow(HWND, int);
BOOL UpdateWindow(HWND);
HWND SetFocus(HWND);
int GetSystemMetrics(int);
LONG GetWindowLong(HWND, int);
BOOL GetClientRect(HWND, LPRECT);
HICON LoadIcon(HINSTANCE, LPCSTR);
HCURSOR LoadCursor(HINSTANCE, LPCSTR);
HCURSOR SetCursor(HCURSOR);
int ShowCursor(BOOL);
BOOL PeekMessage(LPMSG, HWND, UINT, UINT, UINT);
BOOL GetMessage(LPMSG, HWND, UINT, UINT);
BOOL TranslateMessage(const MSG *);
LRESULT DispatchMessage(const MSG *);
BOOL WaitMessage(void);
BOOL PostMessage(HWND, UINT, WPARAM, LPARAM);
void PostQuitMessage(int);
int MessageBox(HWND, LPCSTR text, LPCSTR caption, UINT type);
BOOL MessageBeep(UINT);
BOOL Beep(DWORD freq, DWORD dur);
BOOL SystemParametersInfo(UINT, UINT, PVOID, UINT);
HMODULE GetModuleHandle(LPCSTR);
DWORD GetModuleFileName(HMODULE, LPSTR, DWORD);
void OutputDebugString(LPCSTR);
void Sleep(DWORD ms);
DWORD GetTickCount(void);
DWORD timeGetTime(void);
BOOL QueryPerformanceCounter(LARGE_INTEGER *);
BOOL QueryPerformanceFrequency(LARGE_INTEGER *);
HLOCAL LocalAlloc(UINT, size_t);
HLOCAL LocalFree(HLOCAL);
LPVOID GlobalLock(HGLOBAL);
BOOL GlobalUnlock(HGLOBAL);
BOOL OpenClipboard(HWND);
BOOL CloseClipboard(void);
HANDLE GetClipboardData(UINT);
HINSTANCE ShellExecute(HWND, LPCSTR op, LPCSTR file, LPCSTR params, LPCSTR dir, int show);
HRSRC FindResource(HMODULE, LPCSTR, LPCSTR);
HGLOBAL LoadResource(HMODULE, HRSRC);
LPVOID LockResource(HGLOBAL);

// Files
typedef struct _SECURITY_ATTRIBUTES *LPSECURITY_ATTRIBUTES;
BOOL CreateDirectory(LPCSTR, LPSECURITY_ATTRIBUTES);
BOOL DeleteFile(LPCSTR);
BOOL MoveFile(LPCSTR, LPCSTR);
DWORD GetCurrentDirectory(DWORD, LPSTR);
BOOL SetCurrentDirectory(LPCSTR);
int _lopen(LPCSTR, int);
UINT _lread(int, LPVOID, UINT);
int _lclose(int);
// Translate a Windows relative path ("Data\\x") into the virtual filesystem path.
const char *d2_path(const char *winPath, char *out, size_t outSize);

// COM
HRESULT CoInitialize(LPVOID);
void CoUninitialize(void);
struct IUnknown {
    virtual HRESULT QueryInterface(REFIID, void **) { return E_NOINTERFACE; }
    virtual ULONG AddRef() { return 1; }
    virtual ULONG Release() = 0;
    virtual ~IUnknown() {}
};
typedef IUnknown *LPUNKNOWN;
#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) static const GUID name = {l, w1, w2, {b1, b2, b3, b4, b5, b6, b7, b8}}

#endif
