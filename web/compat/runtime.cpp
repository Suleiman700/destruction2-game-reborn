// Web runtime for Destruction II: implements the Win32 / DirectX 7 subset declared in compat/*.h on
// top of Emscripten (canvas presentation, Web Audio, keyboard/gamepad events, IndexedDB-backed files).
#include <windows.h>
#include <ddraw.h>
#include <dsound.h>
#include <dinput.h>
#include <fstream.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <math.h>
#include <map>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "runtime.h"

int d2_app_init(HINSTANCE hInstance, int nCmdShow);
void d2_app_frame();

static const char *kAssetRoot = "/game";
static const char *kHomeRoot = "/home/d2";

// ===========================================================================================
// Small CRT / Win32 helpers
// ===========================================================================================
static char *itoa_impl(long long value, char *str, int radix) {
    char buf[72];
    int i = 0;
    bool neg = value < 0 && radix == 10;
    unsigned long long v = neg ? -(unsigned long long)value : (radix == 10 ? (unsigned long long)value : (unsigned long)value);
    do {
        int d = (int)(v % radix);
        buf[i++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
        v /= radix;
    } while (v);
    int j = 0;
    if (neg) str[j++] = '-';
    while (i) str[j++] = buf[--i];
    str[j] = 0;
    return str;
}
char *_itoa(int value, char *str, int radix) { return itoa_impl(value, str, radix); }
char *_ltoa(long value, char *str, int radix) { return itoa_impl(value, str, radix); }
char *_strupr(char *s) { for (char *p = s; *p; p++) *p = (char)toupper((unsigned char)*p); return s; }
char *_strlwr(char *s) { for (char *p = s; *p; p++) *p = (char)tolower((unsigned char)*p); return s; }
BOOL SetRect(LPRECT r, int l, int t, int ri, int b) { r->left = l; r->top = t; r->right = ri; r->bottom = b; return TRUE; }

void OutputDebugString(LPCSTR s) { emscripten_log(EM_LOG_CONSOLE, "%s", s); }
void Sleep(DWORD) {}
DWORD GetTickCount(void) { return (DWORD)(unsigned long long)emscripten_get_now(); }
DWORD timeGetTime(void) { return GetTickCount(); }
BOOL QueryPerformanceFrequency(LARGE_INTEGER *f) { f->QuadPart = 1000000; return TRUE; }
BOOL QueryPerformanceCounter(LARGE_INTEGER *c) { c->QuadPart = (LONGLONG)(emscripten_get_now() * 1000.0); return TRUE; }
HLOCAL LocalAlloc(UINT, size_t n) { return calloc(1, n); }
HLOCAL LocalFree(HLOCAL p) { free(p); return NULL; }
LPVOID GlobalLock(HGLOBAL h) { return h; }
BOOL GlobalUnlock(HGLOBAL) { return TRUE; }
BOOL OpenClipboard(HWND) { return FALSE; }
BOOL CloseClipboard(void) { return TRUE; }
HANDLE GetClipboardData(UINT) { return NULL; }
HRESULT CoInitialize(LPVOID) { return S_OK; }
void CoUninitialize(void) {}
HMODULE GetModuleHandle(LPCSTR) { return (HMODULE)1; }
DWORD GetModuleFileName(HMODULE, LPSTR s, DWORD n) { snprintf(s, n, "%s/D2.exe", kHomeRoot); return (DWORD)strlen(s); }
HRSRC FindResource(HMODULE, LPCSTR, LPCSTR) { return NULL; }
HGLOBAL LoadResource(HMODULE, HRSRC) { return NULL; }
LPVOID LockResource(HGLOBAL) { return NULL; }
BOOL SystemParametersInfo(UINT, UINT, PVOID, UINT) { return TRUE; }
BOOL MessageBeep(UINT) { return TRUE; }
BOOL Beep(DWORD, DWORD) { return TRUE; }

EM_JS(void, js_open_url, (const char *url), { window.open(UTF8ToString(url), '_blank', 'noopener'); });
HINSTANCE ShellExecute(HWND, LPCSTR, LPCSTR file, LPCSTR, LPCSTR, int) {
    if (file && (!strncmp(file, "http", 4) || !strncmp(file, "mailto:", 7))) js_open_url(file);
    return (HINSTANCE)33;
}

EM_JS(void, js_message_box, (const char *caption, const char *text), {
    var c = UTF8ToString(caption), t = UTF8ToString(text);
    console.warn('[Destruction II] ' + c + ': ' + t);
    if (Module.onMessageBox) Module.onMessageBox(c, t); else alert(c + '\n\n' + t);
});
int MessageBox(HWND, LPCSTR text, LPCSTR caption, UINT) {
    js_message_box(caption ? caption : "", text ? text : "");
    return 1;
}

// ===========================================================================================
// Files: Windows-relative paths -> virtual FS ("Sound\x.wav" lives with the assets, everything else
// under the persistent home directory). Lookup is case-insensitive like NTFS/FAT.
// ===========================================================================================
static std::string resolve_ci(const std::string &path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) return path;
    std::string cur;
    size_t i = 0;
    if (path[0] == '/') { cur = ""; i = 1; }
    while (i <= path.size()) {
        size_t j = path.find('/', i);
        if (j == std::string::npos) j = path.size();
        std::string comp = path.substr(i, j - i);
        std::string dir = cur.empty() ? "/" : cur;
        std::string next = cur + "/" + comp;
        if (stat(next.c_str(), &st) != 0) {
            if (DIR *d = opendir(dir.c_str())) {
                while (dirent *e = readdir(d)) {
                    if (!strcasecmp(e->d_name, comp.c_str())) { next = cur + "/" + e->d_name; break; }
                }
                closedir(d);
            }
        }
        cur = next;
        i = j + 1;
    }
    return cur;
}

const char *d2_path(const char *winPath, char *out, size_t outSize) {
    std::string p(winPath ? winPath : "");
    for (char &c : p) if (c == '\\') c = '/';
    while (p.size() > 1 && p[0] == '.' && p[1] == '/') p = p.substr(2);
    std::string full;
    if (!p.empty() && p[0] == '/') full = p;
    else if (!strncasecmp(p.c_str(), "Sound/", 6) || !strncasecmp(p.c_str(), "Bitmaps/", 8)) full = std::string(kAssetRoot) + "/" + p;
    else full = std::string(kHomeRoot) + "/" + p;
    full = resolve_ci(full);
    snprintf(out, outSize, "%s", full.c_str());
    return out;
}

static bool g_fsDirty = false;
static double g_lastSync = 0;
namespace d2io {
void mark_dirty() { g_fsDirty = true; }
bool read_file(const char *winPath, std::string &out) {
    char p[MAX_PATH];
    d2_path(winPath, p, sizeof p);
    FILE *f = fopen(p, "rb");
    if (!f) return false;
    out.clear();
    char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    fclose(f);
    return true;
}
std::string text_from_raw(const std::string &raw) {
    // MSVC CRT text mode: CR LF -> LF, and Ctrl-Z (0x1A) marks end of file.
    std::string t;
    t.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); i++) {
        char c = raw[i];
        if (c == 0x1A) break;
        if (c == '\r' && i + 1 < raw.size() && raw[i + 1] == '\n') continue;
        t.push_back(c);
    }
    return t;
}
}

BOOL CreateDirectory(LPCSTR p, LPSECURITY_ATTRIBUTES) { char b[MAX_PATH]; return mkdir(d2_path(p, b, sizeof b), 0777) == 0; }
BOOL DeleteFile(LPCSTR p) { char b[MAX_PATH]; g_fsDirty = true; return unlink(d2_path(p, b, sizeof b)) == 0; }
BOOL MoveFile(LPCSTR a, LPCSTR b) { char x[MAX_PATH], y[MAX_PATH]; g_fsDirty = true; return rename(d2_path(a, x, sizeof x), d2_path(b, y, sizeof y)) == 0; }
DWORD GetCurrentDirectory(DWORD n, LPSTR s) { snprintf(s, n, "%s", kHomeRoot); return (DWORD)strlen(s); }
BOOL SetCurrentDirectory(LPCSTR) { return TRUE; }
int _lopen(LPCSTR p, int) { char b[MAX_PATH]; FILE *f = fopen(d2_path(p, b, sizeof b), "rb"); return f ? (int)(intptr_t)f : -1; }
UINT _lread(int h, LPVOID buf, UINT n) { return (UINT)fread(buf, 1, n, (FILE *)(intptr_t)h); }
int _lclose(int h) { return fclose((FILE *)(intptr_t)h); }

EM_JS(void, js_sync_fs, (void), {
    if (Module.d2Syncing) { Module.d2SyncAgain = true; return; }
    Module.d2Syncing = true;
    FS.syncfs(false, function(err) {
        Module.d2Syncing = false;
        if (err) console.error('Destruction II: saving failed', err);
        if (Module.d2SyncAgain) { Module.d2SyncAgain = false; _d2_request_sync(); }
    });
});
extern "C" EMSCRIPTEN_KEEPALIVE void d2_request_sync() { g_fsDirty = true; }

// ===========================================================================================
// Window & messages
// ===========================================================================================
static WNDPROC g_wndproc = NULL;
static HWND g_hwnd = (HWND)0x1000;
static bool g_hasFocus = true;
static bool g_quit = false;
static std::vector<MSG> g_posted;

ATOM RegisterClass(const WNDCLASS *wc) { g_wndproc = wc->lpfnWndProc; return 1; }
HWND CreateWindowEx(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID) {
    if (g_wndproc) g_wndproc(g_hwnd, WM_CREATE, 0, 0);
    return g_hwnd;
}
LRESULT DefWindowProc(HWND, UINT, WPARAM, LPARAM) { return 0; }
BOOL ShowWindow(HWND, int) { return TRUE; }
BOOL UpdateWindow(HWND) { return TRUE; }
HWND SetFocus(HWND h) {
    if (g_wndproc) g_wndproc(h, WM_SETFOCUS, 0, 0);
    return h;
}
int GetSystemMetrics(int i) { return i == SM_CXSCREEN ? 800 : 600; }
LONG GetWindowLong(HWND, int) { return 1; }
BOOL GetClientRect(HWND, LPRECT r) { return SetRect(r, 0, 0, 800, 600); }
HICON LoadIcon(HINSTANCE, LPCSTR) { return NULL; }
HCURSOR LoadCursor(HINSTANCE, LPCSTR) { return NULL; }
HCURSOR SetCursor(HCURSOR) { return NULL; }
int ShowCursor(BOOL) { return 0; }
BOOL PeekMessage(LPMSG, HWND, UINT, UINT, UINT) { return FALSE; }
BOOL GetMessage(LPMSG, HWND, UINT, UINT) { return TRUE; }
BOOL TranslateMessage(const MSG *) { return TRUE; }
LRESULT DispatchMessage(const MSG *m) { return g_wndproc ? g_wndproc(m->hwnd, m->message, m->wParam, m->lParam) : 0; }
BOOL WaitMessage(void) { return TRUE; }
BOOL PostMessage(HWND h, UINT msg, WPARAM w, LPARAM l) {
    MSG m = {h, msg, w, l, 0, {0, 0}};
    g_posted.push_back(m);
    return TRUE;
}
void PostQuitMessage(int) { g_quit = true; }

// ===========================================================================================
// GDI objects and device contexts (software, 8-bit)
// ===========================================================================================
enum { GDI_FONT = 1, GDI_PEN, GDI_BRUSH };
struct D2GdiObject {
    int type;
    bool stock;
    LOGFONT lf;
    int penStyle, penWidth;
    COLORREF color;
    UINT brushStyle;
    // resolved font metrics
    bool resolved;
    std::string css;
    float scaleX;
    int ascent, descent;
    int id;
};
struct D2DC {
    IDirectDrawSurface7 *surf;
    D2GdiObject *font, *pen, *brush;
    COLORREF textColor, bkColor;
    int bkMode;
    UINT align;
    POINT cur;
};

static int g_nextFontId = 1;
// Live GDI handles. Like Win32, DeleteObject/SelectObject ignore handles that are not valid objects
// (the game deletes g_hFontAngled[2], one past the end of its array, at shutdown).
static std::unordered_set<D2GdiObject *> g_liveObjects;
static D2GdiObject *make_obj(int type) {
    D2GdiObject *o = new D2GdiObject();
    g_liveObjects.insert(o);
    o->type = type;
    o->stock = false;
    o->resolved = false;
    o->scaleX = 1;
    o->ascent = o->descent = 0;
    o->id = 0;
    memset(&o->lf, 0, sizeof o->lf);
    o->penStyle = PS_SOLID;
    o->penWidth = 1;
    o->color = 0;
    o->brushStyle = BS_SOLID;
    return o;
}

HFONT CreateFontIndirect(const LOGFONT *lf) {
    D2GdiObject *o = make_obj(GDI_FONT);
    o->lf = *lf;
    o->id = g_nextFontId++;
    return o;
}
HFONT CreateFont(int h, int w, int esc, int orient, int weight, DWORD italic, DWORD underline, DWORD strike,
                 DWORD charset, DWORD outprec, DWORD clipprec, DWORD quality, DWORD pitch, LPCSTR face) {
    LOGFONT lf;
    memset(&lf, 0, sizeof lf);
    lf.lfHeight = h; lf.lfWidth = w; lf.lfEscapement = esc; lf.lfOrientation = orient; lf.lfWeight = weight;
    lf.lfItalic = (BYTE)italic; lf.lfUnderline = (BYTE)underline; lf.lfStrikeOut = (BYTE)strike;
    lf.lfCharSet = (BYTE)charset; lf.lfOutPrecision = (BYTE)outprec; lf.lfClipPrecision = (BYTE)clipprec;
    lf.lfQuality = (BYTE)quality; lf.lfPitchAndFamily = (BYTE)pitch;
    snprintf(lf.lfFaceName, LF_FACESIZE, "%s", face ? face : "");
    return CreateFontIndirect(&lf);
}
HPEN CreatePen(int style, int width, COLORREF color) {
    D2GdiObject *o = make_obj(GDI_PEN);
    o->penStyle = style; o->penWidth = width < 1 ? 1 : width; o->color = color;
    return o;
}
HBRUSH CreateBrushIndirect(const LOGBRUSH *lb) {
    D2GdiObject *o = make_obj(GDI_BRUSH);
    o->brushStyle = lb->lbStyle; o->color = lb->lbColor;
    return o;
}
HBRUSH CreateSolidBrush(COLORREF c) { LOGBRUSH lb = {BS_SOLID, c, 0}; return CreateBrushIndirect(&lb); }

static D2GdiObject *g_stock[9];
static D2GdiObject *g_systemFont;
static void init_stock() {
    if (g_systemFont) return;
    COLORREF bc[5] = {RGB(255, 255, 255), RGB(192, 192, 192), RGB(128, 128, 128), RGB(64, 64, 64), RGB(0, 0, 0)};
    for (int i = 0; i < 5; i++) { g_stock[i] = (D2GdiObject *)CreateSolidBrush(bc[i]); g_stock[i]->stock = true; }
    LOGBRUSH nb = {BS_NULL, 0, 0};
    g_stock[NULL_BRUSH] = (D2GdiObject *)CreateBrushIndirect(&nb); g_stock[NULL_BRUSH]->stock = true;
    g_stock[WHITE_PEN] = (D2GdiObject *)CreatePen(PS_SOLID, 1, RGB(255, 255, 255)); g_stock[WHITE_PEN]->stock = true;
    g_stock[BLACK_PEN] = (D2GdiObject *)CreatePen(PS_SOLID, 1, RGB(0, 0, 0)); g_stock[BLACK_PEN]->stock = true;
    g_stock[NULL_PEN] = (D2GdiObject *)CreatePen(PS_NULL, 1, 0); g_stock[NULL_PEN]->stock = true;
    // The Windows "System" font: bold, 16 pixels.
    g_systemFont = (D2GdiObject *)CreateFont(16, 0, 0, 0, FW_BOLD, 0, 0, 0, 0, 0, 0, 0, 0, "System");
    g_systemFont->stock = true;
}
HGDIOBJ GetStockObject(int i) { init_stock(); return (i >= 0 && i < 9) ? g_stock[i] : NULL; }
BOOL DeleteObject(HGDIOBJ obj) {
    if (!obj || !g_liveObjects.count(obj) || obj->stock) return FALSE;
    g_liveObjects.erase(obj);
    delete obj;
    return TRUE;
}

static D2DC *new_dc(IDirectDrawSurface7 *s) {
    init_stock();
    D2DC *dc = new D2DC();
    dc->surf = s;
    // Every GetDC on a DirectDraw surface yields a fresh DC with default state.
    dc->font = g_systemFont;
    dc->pen = g_stock[BLACK_PEN];
    dc->brush = g_stock[WHITE_BRUSH];
    dc->textColor = RGB(0, 0, 0);
    dc->bkColor = RGB(255, 255, 255);
    dc->bkMode = OPAQUE;
    dc->align = TA_LEFT | TA_TOP;
    dc->cur.x = dc->cur.y = 0;
    return dc;
}

HGDIOBJ SelectObject(HDC hdc, HGDIOBJ obj) {
    if (!hdc || !obj || !g_liveObjects.count(obj)) return NULL;
    D2GdiObject *prev = NULL;
    switch (obj->type) {
    case GDI_FONT: prev = hdc->font; hdc->font = obj; break;
    case GDI_PEN: prev = hdc->pen; hdc->pen = obj; break;
    case GDI_BRUSH: prev = hdc->brush; hdc->brush = obj; break;
    }
    return prev;
}
COLORREF SetTextColor(HDC hdc, COLORREF c) { if (!hdc) return CLR_INVALID; COLORREF o = hdc->textColor; hdc->textColor = c; return o; }
COLORREF SetBkColor(HDC hdc, COLORREF c) { if (!hdc) return CLR_INVALID; COLORREF o = hdc->bkColor; hdc->bkColor = c; return o; }
int SetBkMode(HDC hdc, int m) { if (!hdc) return 0; int o = hdc->bkMode; hdc->bkMode = m; return o; }
UINT SetTextAlign(HDC hdc, UINT a) { if (!hdc) return 0; UINT o = hdc->align; hdc->align = a; return o; }

// ---- palette / colour matching -------------------------------------------------------------
static IDirectDrawSurface7 *g_primary = NULL;
static unsigned g_paletteVersion = 1;

static const PALETTEENTRY *system_palette() {
    return (g_primary && g_primary->palette) ? g_primary->palette->entries : NULL;
}
static BYTE nearest_index(const PALETTEENTRY *pal, int r, int g, int b) {
    int best = 0, bestD = 0x7fffffff;
    for (int i = 0; i < 256; i++) {
        int dr = pal[i].peRed - r, dg = pal[i].peGreen - g, db = pal[i].peBlue - b;
        int d = dr * dr + dg * dg + db * db;
        if (d < bestD) { bestD = d; best = i; if (!d) break; }
    }
    return (BYTE)best;
}
BYTE d2_match_color(COLORREF c) {
    if ((c & 0xFF000000) == 0x01000000) return (BYTE)(c & 0xFF);  // PALETTEINDEX
    static unsigned cacheVersion = 0;
    static std::unordered_map<DWORD, BYTE> cache;
    const PALETTEENTRY *pal = system_palette();
    if (!pal) return (BYTE)(c & 0xFF);
    if (cacheVersion != g_paletteVersion) { cache.clear(); cacheVersion = g_paletteVersion; }
    DWORD key = c & 0xFFFFFF;
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    BYTE idx = nearest_index(pal, GetRValue(c), GetGValue(c), GetBValue(c));
    cache[key] = idx;
    return idx;
}

// ---- pixel primitives ---------------------------------------------------------------------------
static inline void put_px(IDirectDrawSurface7 *s, int x, int y, BYTE c) {
    if (x >= 0 && y >= 0 && x < s->width && y < s->height) s->pixels[y * s->pitch + x] = c;
}
static void fill_rect(IDirectDrawSurface7 *s, int l, int t, int r, int b, BYTE c) {
    if (l < 0) l = 0;
    if (t < 0) t = 0;
    if (r > s->width) r = s->width;
    if (b > s->height) b = s->height;
    for (int y = t; y < b; y++) memset(s->pixels + y * s->pitch + l, c, r > l ? r - l : 0);
}

COLORREF SetPixel(HDC hdc, int x, int y, COLORREF c) {
    if (!hdc) return CLR_INVALID;
    BYTE i = d2_match_color(c);
    put_px(hdc->surf, x, y, i);
    const PALETTEENTRY *p = system_palette();
    return p ? RGB(p[i].peRed, p[i].peGreen, p[i].peBlue) : c;
}
BOOL SetPixelV(HDC hdc, int x, int y, COLORREF c) { SetPixel(hdc, x, y, c); return TRUE; }
COLORREF GetPixel(HDC hdc, int x, int y) {
    IDirectDrawSurface7 *s = hdc ? hdc->surf : NULL;
    if (!s || x < 0 || y < 0 || x >= s->width || y >= s->height) return CLR_INVALID;
    BYTE i = s->pixels[y * s->pitch + x];
    const PALETTEENTRY *p = system_palette();
    return p ? RGB(p[i].peRed, p[i].peGreen, p[i].peBlue) : PALETTEINDEX(i);
}

// Rounded rectangle membership test (rw/rh are the corner ellipse sizes; 0 = square).
static bool in_rrect(int x, int y, int l, int t, int r, int b, int rw, int rh) {
    if (x < l || x >= r || y < t || y >= b) return false;
    if (rw <= 1 || rh <= 1) return true;
    double ax = rw / 2.0, ay = rh / 2.0;
    double cx = x + 0.5, cy = y + 0.5;
    double ex = cx < l + ax ? l + ax : (cx > r - ax ? r - ax : cx);
    double ey = cy < t + ay ? t + ay : (cy > b - ay ? b - ay : cy);
    double dx = (cx - ex) / ax, dy = (cy - ey) / ay;
    return dx * dx + dy * dy <= 1.0;
}
static void draw_rrect(HDC hdc, int l, int t, int r, int b, int rw, int rh) {
    if (!hdc) return;
    IDirectDrawSurface7 *s = hdc->surf;
    if (r < l) { int tmp = l; l = r; r = tmp; }
    if (b < t) { int tmp = t; t = b; b = tmp; }
    bool pen = hdc->pen->penStyle != PS_NULL;
    int pw = pen ? hdc->pen->penWidth : 0;
    // Wide geometric pens are centred on the outline.
    int ol = l - pw / 2, ot = t - pw / 2, orr = r + (pw - 1) / 2, ob = b + (pw - 1) / 2;
    if (!pen) { ol = l; ot = t; orr = r - 1; ob = b - 1; }  // without a pen GDI shrinks the fill by one
    int il = ol + pw, it = ot + pw, ir = orr - pw, ib = ob - pw;
    BYTE penC = pen ? d2_match_color(hdc->pen->color) : 0;
    bool fill = hdc->brush->brushStyle != BS_NULL;
    BYTE brushC = fill ? d2_match_color(hdc->brush->color) : 0;
    int y0 = ot < 0 ? 0 : ot, y1 = ob > s->height ? s->height : ob;
    int x0 = ol < 0 ? 0 : ol, x1 = orr > s->width ? s->width : orr;
    for (int y = y0; y < y1; y++) {
        BYTE *row = s->pixels + y * s->pitch;
        for (int x = x0; x < x1; x++) {
            if (!in_rrect(x, y, ol, ot, orr, ob, rw, rh)) continue;
            bool inner = in_rrect(x, y, il, it, ir, ib, rw - 2 * pw, rh - 2 * pw);
            if (!inner) { if (pen) row[x] = penC; }
            else if (fill) row[x] = brushC;
        }
    }
}
BOOL Rectangle(HDC hdc, int l, int t, int r, int b) { draw_rrect(hdc, l, t, r, b, 0, 0); return TRUE; }
BOOL RoundRect(HDC hdc, int l, int t, int r, int b, int w, int h) { draw_rrect(hdc, l, t, r, b, w, h); return TRUE; }
BOOL MoveToEx(HDC hdc, int x, int y, LPPOINT old) {
    if (!hdc) return FALSE;
    if (old) *old = hdc->cur;
    hdc->cur.x = x; hdc->cur.y = y;
    return TRUE;
}
BOOL LineTo(HDC hdc, int x1, int y1) {
    if (!hdc) return FALSE;
    int x0 = hdc->cur.x, y0 = hdc->cur.y;
    hdc->cur.x = x1; hdc->cur.y = y1;
    if (hdc->pen->penStyle == PS_NULL) return TRUE;
    BYTE c = d2_match_color(hdc->pen->color);
    int w = hdc->pen->penWidth;
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, err = dx + dy;
    // GDI lines exclude the final pixel.
    while (!(x0 == x1 && y0 == y1)) {
        if (w <= 1) put_px(hdc->surf, x0, y0, c);
        else fill_rect(hdc->surf, x0 - w / 2, y0 - w / 2, x0 - w / 2 + w, y0 - w / 2 + w, c);
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
    return TRUE;
}

// ---- text ----------------------------------------------------------------------------------------
EM_JS(void, js_font_metrics, (const char *css, float *out), {
    var c = Module.d2TextCanvas || (Module.d2TextCanvas = document.createElement('canvas'));
    var ctx = c.getContext('2d');
    ctx.font = UTF8ToString(css);
    var m = ctx.measureText('Hg');
    HEAPF32[(out >> 2)] = m.fontBoundingBoxAscent !== undefined ? m.fontBoundingBoxAscent : m.actualBoundingBoxAscent;
    HEAPF32[(out >> 2) + 1] = m.fontBoundingBoxDescent !== undefined ? m.fontBoundingBoxDescent : m.actualBoundingBoxDescent;
});
EM_JS(float, js_text_width, (const char *css, const char *text), {
    var c = Module.d2TextCanvas || (Module.d2TextCanvas = document.createElement('canvas'));
    var ctx = c.getContext('2d');
    ctx.font = UTF8ToString(css);
    return ctx.measureText(UTF8ToString(text)).width;
});
// Renders text into a w*h mask (1 = ink). Glyphs are thresholded: GDI never antialiases on 8-bit DCs.
EM_JS(void, js_text_mask, (const char *css, const char *text, int w, int h, float originX, float baseline, float scaleX, unsigned char *out), {
    var c = Module.d2MaskCanvas || (Module.d2MaskCanvas = document.createElement('canvas'));
    if (c.width < w) c.width = w;
    if (c.height < h) c.height = h;
    var ctx = c.getContext('2d', { willReadFrequently: true });
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    ctx.clearRect(0, 0, w, h);
    ctx.font = UTF8ToString(css);
    ctx.fillStyle = '#fff';
    ctx.textBaseline = 'alphabetic';
    ctx.setTransform(scaleX, 0, 0, 1, originX, 0);
    ctx.fillText(UTF8ToString(text), 0, baseline);
    var d = ctx.getImageData(0, 0, w, h).data;
    for (var i = 0, n = w * h; i < n; i++) HEAPU8[out + i] = d[i * 4 + 3] >= 128 ? 1 : 0;
});

static std::string latin1_to_utf8(const char *s, int n) {
    std::string o;
    for (int i = 0; i < n && s[i]; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80) o.push_back((char)c);
        else { o.push_back((char)(0xC0 | (c >> 6))); o.push_back((char)(0x80 | (c & 0x3F))); }
    }
    return o;
}

static void resolve_font(D2GdiObject *f) {
    if (f->resolved) return;
    const char *face = f->lf.lfFaceName;
    std::string family;
    if (!strcasecmp(face, "Arial")) family = "Arial, 'Liberation Sans', Helvetica, sans-serif";
    else if (!strcasecmp(face, "MS Sans Serif")) family = "'Microsoft Sans Serif', 'MS Sans Serif', Tahoma, Arial, sans-serif";
    else if (!strcasecmp(face, "System")) family = "'Microsoft Sans Serif', Tahoma, Arial, sans-serif";
    else family = std::string("'") + face + "', Arial, sans-serif";
    int weight = f->lf.lfWeight ? f->lf.lfWeight : FW_NORMAL;
    char probe[256];
    snprintf(probe, sizeof probe, "%s%d 100px %s", f->lf.lfItalic ? "italic " : "", weight, family.c_str());
    float m[2];
    js_font_metrics(probe, m);
    float asc100 = m[0] > 0 ? m[0] : 90.5f, desc100 = m[1] > 0 ? m[1] : 21.2f;
    int h = f->lf.lfHeight;
    float em;
    if (h > 0) em = h * 100.0f / (asc100 + desc100);  // positive height = cell height
    else if (h < 0) em = (float)-h;                    // negative height = character (em) height
    else em = 16.0f * 100.0f / (asc100 + desc100);
    f->ascent = (int)floorf(asc100 * em / 100.0f + 0.5f);
    f->descent = (h > 0) ? h - f->ascent : (int)floorf(desc100 * em / 100.0f + 0.5f);
    f->scaleX = 1.0f;
    if (f->lf.lfWidth > 0) {
        // GDI scales glyphs so the average character width equals lfWidth (Arial xAvgCharWidth = 904/2048 em).
        float avg = !strcasecmp(face, "Arial") ? em * 904.0f / 2048.0f : em * 0.5f;
        f->scaleX = f->lf.lfWidth / avg;
    }
    char css[256];
    snprintf(css, sizeof css, "%s%d %.3fpx %s", f->lf.lfItalic ? "italic " : "", weight, em, family.c_str());
    f->css = css;
    f->resolved = true;
}

struct TextMask {
    int w, h;       // mask size
    int originX;    // x offset of the reference point inside the mask
    int advance;    // GDI text extent (cx)
    std::vector<unsigned char> bits;
};
static std::unordered_map<std::string, TextMask> g_textCache;

static const TextMask &text_mask(D2GdiObject *f, const char *s, int n) {
    resolve_font(f);
    std::string key = std::to_string(f->id) + '\x01' + std::string(s, n);
    auto it = g_textCache.find(key);
    if (it != g_textCache.end()) return it->second;
    if (g_textCache.size() > 4096) g_textCache.clear();
    std::string utf8 = latin1_to_utf8(s, n);
    TextMask tm;
    float adv = js_text_width(f->css.c_str(), utf8.c_str()) * f->scaleX;
    tm.advance = (int)floorf(adv + 0.5f);
    int pad = (f->ascent + f->descent) / 3 + 2;  // room for glyph overhang
    tm.originX = pad;
    tm.w = tm.advance + 2 * pad;
    tm.h = f->ascent + f->descent;
    if (tm.w < 1) tm.w = 1;
    if (tm.h < 1) tm.h = 1;
    tm.bits.assign((size_t)tm.w * tm.h, 0);
    if (!utf8.empty()) js_text_mask(f->css.c_str(), utf8.c_str(), tm.w, tm.h, (float)pad, (float)f->ascent, f->scaleX, tm.bits.data());
    return g_textCache.emplace(key, std::move(tm)).first->second;
}

BOOL GetTextExtentPoint(HDC hdc, LPCSTR s, int n, LPSIZE sz) {
    if (!hdc || !s) return FALSE;
    const TextMask &tm = text_mask(hdc->font, s, n);
    sz->cx = tm.advance;
    sz->cy = hdc->font->ascent + hdc->font->descent;
    return TRUE;
}

BOOL TextOut(HDC hdc, int x, int y, LPCSTR s, int n) {
    if (!hdc || !s || n <= 0) return FALSE;
    D2GdiObject *f = hdc->font;
    const TextMask &tm = text_mask(f, s, n);
    // Reference point offset in text space (u along the baseline, v downwards).
    int u0 = 0, v0 = 0;
    UINT ha = hdc->align & 6, va = hdc->align & 24;
    if (ha == TA_CENTER) u0 = -tm.advance / 2;
    else if (ha == TA_RIGHT) u0 = -tm.advance;
    if (va == TA_BOTTOM) v0 = -(f->ascent + f->descent);
    else if (va == TA_BASELINE) v0 = -f->ascent;
    int esc = ((f->lf.lfEscapement % 3600) + 3600) % 3600;
    // Rotation (counter-clockwise, screen y down): dx = u*cos + v*sin, dy = -u*sin + v*cos.
    int cs = 1, sn = 0;
    if (esc == 900) { cs = 0; sn = 1; }
    else if (esc == 1800) { cs = -1; sn = 0; }
    else if (esc == 2700) { cs = 0; sn = -1; }
    IDirectDrawSurface7 *surf = hdc->surf;
    if (hdc->bkMode == OPAQUE) {
        BYTE bk = d2_match_color(hdc->bkColor);
        for (int v = 0; v < tm.h; v++)
            for (int u = 0; u < tm.advance; u++) {
                int uu = u0 + u, vv = v0 + v;
                put_px(surf, x + uu * cs + vv * sn, y - uu * sn + vv * cs, bk);
            }
    }
    BYTE fg = d2_match_color(hdc->textColor);
    for (int v = 0; v < tm.h; v++) {
        const unsigned char *row = &tm.bits[(size_t)v * tm.w];
        for (int mx = 0; mx < tm.w; mx++) {
            if (!row[mx]) continue;
            int uu = u0 + mx - tm.originX, vv = v0 + v;
            put_px(surf, x + uu * cs + vv * sn, y - uu * sn + vv * cs, fg);
        }
    }
    if (hdc->align & TA_UPDATECP) hdc->cur.x += tm.advance;
    return TRUE;
}

// ---- bitmaps ---------------------------------------------------------------------------------
int GetObject(HGDIOBJ h, int n, LPVOID out) {
    const D2Bitmap *b = (const D2Bitmap *)h;
    if (!b || n < (int)sizeof(BITMAP)) return 0;
    BITMAP *bm = (BITMAP *)out;
    memset(bm, 0, sizeof *bm);
    bm->bmWidth = b->width; bm->bmHeight = b->height; bm->bmBitsPixel = (WORD)b->bpp; bm->bmPlanes = 1;
    return sizeof(BITMAP);
}
HANDLE LoadImage(HINSTANCE, LPCSTR name, UINT, int, int, UINT) { return (HANDLE)d2_find_bitmap(name); }
HDC CreateCompatibleDC(HDC) { return NULL; }
BOOL DeleteDC(HDC) { return TRUE; }
BOOL StretchBlt(HDC, int, int, int, int, HDC, int, int, int, int, DWORD) { return FALSE; }

static std::map<std::string, D2Bitmap *> g_bitmaps;
const D2Bitmap *d2_find_bitmap(const char *name) {
    if (!name) return NULL;
    std::string key(name);
    for (char &c : key) c = (char)toupper((unsigned char)c);
    auto it = g_bitmaps.find(key);
    if (it != g_bitmaps.end()) return it->second;
    std::string path = std::string(kAssetRoot) + "/Bitmaps/" + key + ".bmp";
    std::string data;
    D2Bitmap *bmp = NULL;
    if (d2io::read_file(path.c_str(), data) && data.size() > 54 && data[0] == 'B' && data[1] == 'M') {
        BITMAPFILEHEADER bf;
        BITMAPINFOHEADER bi;
        memcpy(&bf, data.data(), sizeof bf);
        memcpy(&bi, data.data() + 14, sizeof bi);
        if ((bi.biBitCount == 8 || bi.biBitCount == 24) && bi.biCompression == 0) {
            bmp = new D2Bitmap();
            bmp->width = bi.biWidth;
            bmp->height = bi.biHeight < 0 ? -bi.biHeight : bi.biHeight;
            bmp->bpp = bi.biBitCount;
            bmp->paletteCount = bi.biBitCount == 8 ? (bi.biClrUsed ? (int)bi.biClrUsed : 256) : 0;
            if (bmp->paletteCount > 256) bmp->paletteCount = 256;
            memset(bmp->palette, 0, sizeof bmp->palette);
            memcpy(bmp->palette, data.data() + 14 + bi.biSize, bmp->paletteCount * 4);
            int bpp = bi.biBitCount / 8;
            int stride = ((bmp->width * bpp) + 3) & ~3;
            bmp->pixels = new BYTE[(size_t)bmp->width * bmp->height * bpp];
            for (int y = 0; y < bmp->height; y++) {
                int srcRow = bi.biHeight > 0 ? bmp->height - 1 - y : y;
                memcpy(bmp->pixels + (size_t)y * bmp->width * bpp, data.data() + bf.bfOffBits + (size_t)srcRow * stride, bmp->width * bpp);
            }
        } else {
            emscripten_log(EM_LOG_ERROR, "Unsupported bitmap format: %s", path.c_str());
        }
    }
    g_bitmaps[key] = bmp;
    return bmp;
}

// GDI StretchBlt from a DIB section onto an 8-bit DirectDraw surface DC: every source colour is matched
// to the nearest entry of the system palette.
void d2_blit_bitmap_to_surface(const D2Bitmap *bmp, IDirectDrawSurface7 *s) {
    const PALETTEENTRY *pal = system_palette();
    BYTE map[256];
    for (int i = 0; i < 256; i++) {
        const RGBQUAD &q = bmp->palette[i];
        map[i] = pal ? nearest_index(pal, q.rgbRed, q.rgbGreen, q.rgbBlue) : (BYTE)i;
    }
    std::unordered_map<DWORD, BYTE> rgbCache;
    for (int y = 0; y < s->height; y++) {
        int sy = (int)((long long)y * bmp->height / s->height);
        BYTE *dst = s->pixels + y * s->pitch;
        for (int x = 0; x < s->width; x++) {
            int sx = (int)((long long)x * bmp->width / s->width);
            if (bmp->bpp == 8) {
                dst[x] = map[bmp->pixels[(size_t)sy * bmp->width + sx]];
            } else {
                const BYTE *p = bmp->pixels + ((size_t)sy * bmp->width + sx) * 3;
                DWORD key = RGB(p[2], p[1], p[0]);
                auto it = rgbCache.find(key);
                if (it == rgbCache.end()) it = rgbCache.emplace(key, pal ? nearest_index(pal, p[2], p[1], p[0]) : 0).first;
                dst[x] = it->second;
            }
        }
    }
}

// ===========================================================================================
// DirectDraw
// ===========================================================================================
static int g_displayW = 800, g_displayH = 600;

IDirectDrawPalette::IDirectDrawPalette() : refs(1) { memset(entries, 0, sizeof entries); }
ULONG IDirectDrawPalette::AddRef() { return ++refs; }
ULONG IDirectDrawPalette::Release() {
    ULONG r = --refs;
    if (!r) delete this;
    return r;
}
HRESULT IDirectDrawPalette::GetEntries(DWORD, DWORD start, DWORD count, LPPALETTEENTRY out) {
    if (start + count > 256) return DDERR_INVALIDPARAMS;
    memcpy(out, entries + start, count * sizeof(PALETTEENTRY));
    return DD_OK;
}
// Without DDPCAPS_ALLOW256, DirectDraw reserves entries 0 and 255 for black and white.
static void reserve_entries(PALETTEENTRY *e) {
    e[0].peRed = e[0].peGreen = e[0].peBlue = 0;
    e[255].peRed = e[255].peGreen = e[255].peBlue = 255;
}
HRESULT IDirectDrawPalette::SetEntries(DWORD, DWORD start, DWORD count, LPPALETTEENTRY in) {
    if (start + count > 256) return DDERR_INVALIDPARAMS;
    memcpy(entries + start, in, count * sizeof(PALETTEENTRY));
    reserve_entries(entries);
    g_paletteVersion++;
    return DD_OK;
}

IDirectDrawSurface7::IDirectDrawSurface7(int w, int h)
    : width(w), height(h), pitch(w), hasColorKey(FALSE), colorKey(0), isPrimary(FALSE), backBuffer(NULL), palette(NULL), refs(1), dc(NULL) {
    pixels = new BYTE[(size_t)w * h];
    memset(pixels, 0, (size_t)w * h);
}
IDirectDrawSurface7::~IDirectDrawSurface7() {
    if (this == g_primary) g_primary = NULL;
    if (backBuffer) backBuffer->Release();
    if (palette) palette->Release();
    delete dc;
    delete[] pixels;
}
ULONG IDirectDrawSurface7::AddRef() { return ++refs; }
ULONG IDirectDrawSurface7::Release() {
    ULONG r = --refs;
    if (!r) delete this;
    return r;
}

static bool rect_ok(const RECT &r, int w, int h) {
    return r.left >= 0 && r.top >= 0 && r.right <= w && r.bottom <= h && r.left < r.right && r.top < r.bottom;
}

HRESULT IDirectDrawSurface7::Blt(LPRECT dst, IDirectDrawSurface7 *src, LPRECT srcRect, DWORD flags, LPDDBLTFX fx) {
    RECT d = dst ? *dst : RECT{0, 0, width, height};
    if (!rect_ok(d, width, height)) return DDERR_INVALIDPARAMS;  // no clipper attached: DirectDraw refuses
    if (flags & DDBLT_COLORFILL) {
        if (!fx) return DDERR_INVALIDPARAMS;
        fill_rect(this, d.left, d.top, d.right, d.bottom, (BYTE)fx->dwFillColor);
        return DD_OK;
    }
    if (!src) return DDERR_INVALIDPARAMS;
    RECT s = srcRect ? *srcRect : RECT{0, 0, src->width, src->height};
    if (!rect_ok(s, src->width, src->height)) return DDERR_INVALIDPARAMS;
    bool key = (flags & DDBLT_KEYSRC) && src->hasColorKey;
    int dw = d.right - d.left, dh = d.bottom - d.top, sw = s.right - s.left, sh = s.bottom - s.top;
    std::vector<BYTE> tmp;
    const BYTE *sp = src->pixels;
    int spitch = src->pitch;
    if (src == this) { tmp.assign(pixels, pixels + (size_t)pitch * height); sp = tmp.data(); }
    for (int y = 0; y < dh; y++) {
        const BYTE *srow = sp + (s.top + y * sh / dh) * spitch + s.left;
        BYTE *drow = pixels + (d.top + y) * pitch + d.left;
        for (int x = 0; x < dw; x++) {
            BYTE c = srow[x * sw / dw];
            if (!key || c != src->colorKey) drow[x] = c;
        }
    }
    return DD_OK;
}

HRESULT IDirectDrawSurface7::BltFast(DWORD x, DWORD y, IDirectDrawSurface7 *src, LPRECT srcRect, DWORD trans) {
    if (!src) return DDERR_INVALIDPARAMS;
    RECT s = srcRect ? *srcRect : RECT{0, 0, src->width, src->height};
    if (!rect_ok(s, src->width, src->height)) return DDERR_INVALIDPARAMS;
    int w = s.right - s.left, h = s.bottom - s.top;
    RECT d = {(LONG)x, (LONG)y, (LONG)x + w, (LONG)y + h};
    if (!rect_ok(d, width, height)) return DDERR_INVALIDPARAMS;  // BltFast never clips
    bool key = (trans & DDBLTFAST_SRCCOLORKEY) && src->hasColorKey;
    std::vector<BYTE> tmp;
    const BYTE *sp = src->pixels;
    if (src == this) { tmp.assign(pixels, pixels + (size_t)pitch * height); sp = tmp.data(); }
    for (int row = 0; row < h; row++) {
        const BYTE *srow = sp + (s.top + row) * src->pitch + s.left;
        BYTE *drow = pixels + (d.top + row) * pitch + d.left;
        if (!key) { memcpy(drow, srow, w); continue; }
        BYTE ck = src->colorKey;
        for (int i = 0; i < w; i++)
            if (srow[i] != ck) drow[i] = srow[i];
    }
    return DD_OK;
}

HRESULT IDirectDrawSurface7::Lock(LPRECT rect, LPDDSURFACEDESC2 desc, DWORD, HANDLE) {
    RECT r = rect ? *rect : RECT{0, 0, width, height};
    if (!rect_ok(r, width, height)) return DDERR_INVALIDPARAMS;
    GetSurfaceDesc(desc);
    desc->lpSurface = pixels + r.top * pitch + r.left;
    return DD_OK;
}
HRESULT IDirectDrawSurface7::Unlock(LPRECT) { return DD_OK; }
HRESULT IDirectDrawSurface7::GetDC(HDC *out) {
    delete dc;
    dc = new_dc(this);
    *out = dc;
    return DD_OK;
}
HRESULT IDirectDrawSurface7::ReleaseDC(HDC) { return DD_OK; }

static double g_flipDeadline = 0;
HRESULT IDirectDrawSurface7::Flip(IDirectDrawSurface7 *, DWORD) {
    if (!isPrimary || !backBuffer) return DDERR_INVALIDPARAMS;
    BYTE *t = pixels;
    pixels = backBuffer->pixels;
    backBuffer->pixels = t;
    return DD_OK;
}
// The game keeps simulating while a flip is pending (waiting for vertical blank). The runtime sets
// the deadline for each browser frame; see frame().
HRESULT IDirectDrawSurface7::GetFlipStatus(DWORD flags) {
    if ((flags & DDGFS_ISFLIPDONE) && emscripten_get_now() < g_flipDeadline) return DDERR_WASSTILLDRAWING;
    return DD_OK;
}
HRESULT IDirectDrawSurface7::SetPalette(IDirectDrawPalette *pal) {
    if (pal) pal->AddRef();
    if (palette) palette->Release();
    palette = pal;
    g_paletteVersion++;
    return DD_OK;
}
HRESULT IDirectDrawSurface7::GetPalette(IDirectDrawPalette **pal) {
    if (!palette) return DDERR_NOTFOUND;
    palette->AddRef();
    *pal = palette;
    return DD_OK;
}
HRESULT IDirectDrawSurface7::SetColorKey(DWORD, LPDDCOLORKEY key) {
    hasColorKey = key != NULL;
    colorKey = key ? (BYTE)key->dwColorSpaceLowValue : 0;
    return DD_OK;
}
HRESULT IDirectDrawSurface7::Restore() { return DD_OK; }
HRESULT IDirectDrawSurface7::IsLost() { return DD_OK; }
HRESULT IDirectDrawSurface7::GetSurfaceDesc(LPDDSURFACEDESC2 desc) {
    if (!desc) return DDERR_INVALIDPARAMS;
    DWORD size = desc->dwSize;
    memset(desc, 0, sizeof *desc);
    desc->dwSize = size;
    desc->dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PITCH | DDSD_PIXELFORMAT;
    desc->dwWidth = width;
    desc->dwHeight = height;
    desc->lPitch = pitch;
    desc->ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
    desc->ddpfPixelFormat.dwFlags = DDPF_PALETTEINDEXED8;
    desc->ddpfPixelFormat.dwRGBBitCount = 8;
    desc->ddsCaps.dwCaps = isPrimary ? (DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX) : DDSCAPS_OFFSCREENPLAIN;
    return DD_OK;
}
HRESULT IDirectDrawSurface7::GetAttachedSurface(LPDDSCAPS2 caps, IDirectDrawSurface7 **out) {
    if (!backBuffer || !(caps->dwCaps & DDSCAPS_BACKBUFFER)) return DDERR_NOTFOUND;
    backBuffer->AddRef();
    *out = backBuffer;
    return DD_OK;
}

ULONG IDirectDraw7::Release() { delete this; return 0; }
HRESULT IDirectDraw7::SetCooperativeLevel(HWND, DWORD) { return DD_OK; }
HRESULT IDirectDraw7::SetDisplayMode(DWORD w, DWORD h, DWORD bpp, DWORD, DWORD) {
    if (bpp != 8) return DDERR_INVALIDPARAMS;
    g_displayW = (int)w;
    g_displayH = (int)h;
    return DD_OK;
}
HRESULT IDirectDraw7::RestoreDisplayMode() { return DD_OK; }
HRESULT IDirectDraw7::CreateSurface(LPDDSURFACEDESC2 desc, IDirectDrawSurface7 **out, IUnknown *) {
    if (desc->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) {
        IDirectDrawSurface7 *p = new IDirectDrawSurface7(g_displayW, g_displayH);
        p->isPrimary = TRUE;
        if (desc->dwFlags & DDSD_BACKBUFFERCOUNT && desc->dwBackBufferCount >= 1)
            p->backBuffer = new IDirectDrawSurface7(g_displayW, g_displayH);
        g_primary = p;
        *out = p;
        return DD_OK;
    }
    if (!(desc->dwFlags & DDSD_WIDTH) || !(desc->dwFlags & DDSD_HEIGHT) || !desc->dwWidth || !desc->dwHeight)
        return DDERR_INVALIDPARAMS;
    *out = new IDirectDrawSurface7((int)desc->dwWidth, (int)desc->dwHeight);
    if (desc->dwFlags & DDSD_CKSRCBLT) (*out)->SetColorKey(DDCKEY_SRCBLT, &desc->ddckCKSrcBlt);
    return DD_OK;
}
HRESULT IDirectDraw7::CreatePalette(DWORD flags, LPPALETTEENTRY entries, IDirectDrawPalette **out, IUnknown *) {
    IDirectDrawPalette *p = new IDirectDrawPalette();
    memcpy(p->entries, entries, sizeof p->entries);
    if (!(flags & DDPCAPS_ALLOW256)) reserve_entries(p->entries);
    *out = p;
    return DD_OK;
}
HRESULT IDirectDraw7::TestCooperativeLevel() { return g_hasFocus ? DD_OK : DDERR_NOEXCLUSIVEMODE; }
HRESULT IDirectDraw7::GetCaps(LPDDCAPS hw, LPDDCAPS hel) {
    if (hw) { DWORD s = hw->dwSize; memset(hw, 0, sizeof *hw); hw->dwSize = s; }
    if (hel) { DWORD s = hel->dwSize; memset(hel, 0, sizeof *hel); hel->dwSize = s; }
    return DD_OK;
}
HRESULT IDirectDraw7::RestoreAllSurfaces() { return DD_OK; }
HRESULT IDirectDraw7::WaitForVerticalBlank(DWORD, HANDLE) { return DD_OK; }
HRESULT DirectDrawCreateEx(GUID *, LPVOID *out, REFIID, IUnknown *) {
    *out = new IDirectDraw7();
    return DD_OK;
}

// ---- presentation ----------------------------------------------------------------------------------
EM_JS(void, js_present, (const unsigned char *rgba, int w, int h), {
    var c = Module.canvas;
    if (c.width !== w || c.height !== h) { c.width = w; c.height = h; Module.d2Image = null; }
    var ctx = Module.d2Ctx || (Module.d2Ctx = c.getContext('2d', { alpha: false }));
    if (!Module.d2Image) Module.d2Image = ctx.createImageData(w, h);
    Module.d2Image.data.set(HEAPU8.subarray(rgba, rgba + w * h * 4));
    ctx.putImageData(Module.d2Image, 0, 0);
});
static std::vector<uint32_t> g_rgba;
static void present() {
    if (!g_primary) return;
    int w = g_primary->width, h = g_primary->height;
    g_rgba.resize((size_t)w * h);
    uint32_t lut[256];
    const PALETTEENTRY *pal = system_palette();
    for (int i = 0; i < 256; i++) {
        if (pal) lut[i] = 0xFF000000u | (pal[i].peBlue << 16) | (pal[i].peGreen << 8) | pal[i].peRed;
        else lut[i] = 0xFF000000u | (i << 16) | (i << 8) | i;
    }
    for (int y = 0; y < h; y++) {
        const BYTE *src = g_primary->pixels + y * g_primary->pitch;
        uint32_t *dst = &g_rgba[(size_t)y * w];
        for (int x = 0; x < w; x++) dst[x] = lut[src[x]];
    }
    js_present((const unsigned char *)g_rgba.data(), w, h);
}

// ===========================================================================================
// DirectSound: software mixer pulled by Web Audio
// ===========================================================================================
static IDirectSoundBuffer *g_buffers = NULL;  // all secondary buffers
static int g_outRate = 48000;
static bool g_audioRunning = false;

IDirectSoundBuffer::IDirectSoundBuffer(LPCDSBUFFERDESC desc)
    : primary((desc->dwFlags & DSBCAPS_PRIMARYBUFFER) != 0), data(NULL), size(0), pos(0), playing(FALSE), looping(FALSE),
      volume(0), pan(0), refs(1), nextBuffer(NULL) {
    memset(&fmt, 0, sizeof fmt);
    if (!primary) {
        size = desc->dwBufferBytes;
        data = new BYTE[size ? size : 1];
        memset(data, desc->lpwfxFormat && desc->lpwfxFormat->wBitsPerSample == 8 ? 128 : 0, size);
        if (desc->lpwfxFormat) memcpy(&fmt, desc->lpwfxFormat, sizeof(WAVEFORMATEX) - sizeof(WORD));
        nextBuffer = g_buffers;
        g_buffers = this;
    }
}
IDirectSoundBuffer::~IDirectSoundBuffer() {
    for (IDirectSoundBuffer **pp = &g_buffers; *pp; pp = &(*pp)->nextBuffer)
        if (*pp == this) { *pp = nextBuffer; break; }
    delete[] data;
}
ULONG IDirectSoundBuffer::Release() {
    ULONG r = --refs;
    if (!r) delete this;
    return r;
}
HRESULT IDirectSoundBuffer::SetFormat(LPCWAVEFORMATEX) { return DS_OK; }
HRESULT IDirectSoundBuffer::Play(DWORD, DWORD, DWORD flags) { playing = TRUE; looping = (flags & DSBPLAY_LOOPING) != 0; return DS_OK; }
HRESULT IDirectSoundBuffer::Stop() { playing = FALSE; return DS_OK; }
HRESULT IDirectSoundBuffer::SetVolume(LONG v) { volume = v < DSBVOLUME_MIN ? DSBVOLUME_MIN : (v > 0 ? 0 : v); return DS_OK; }
HRESULT IDirectSoundBuffer::SetPan(LONG p) { pan = p < DSBPAN_LEFT ? DSBPAN_LEFT : (p > DSBPAN_RIGHT ? DSBPAN_RIGHT : p); return DS_OK; }
HRESULT IDirectSoundBuffer::GetVolume(LPLONG v) { *v = volume; return DS_OK; }
HRESULT IDirectSoundBuffer::GetPan(LPLONG p) { *p = pan; return DS_OK; }
HRESULT IDirectSoundBuffer::SetFrequency(DWORD f) { if (f) fmt.nSamplesPerSec = f; return DS_OK; }
HRESULT IDirectSoundBuffer::GetStatus(LPDWORD s) { *s = (playing ? DSBSTATUS_PLAYING : 0) | (playing && looping ? DSBSTATUS_LOOPING : 0); return DS_OK; }
HRESULT IDirectSoundBuffer::SetCurrentPosition(DWORD bytes) {
    int ba = fmt.nBlockAlign ? fmt.nBlockAlign : 1;
    pos = (double)(bytes / ba);
    return DS_OK;
}
HRESULT IDirectSoundBuffer::GetCurrentPosition(LPDWORD play, LPDWORD write) {
    int ba = fmt.nBlockAlign ? fmt.nBlockAlign : 1;
    DWORD p = (DWORD)pos * ba;
    if (play) *play = p;
    if (write) *write = size ? (p + (DWORD)(fmt.nSamplesPerSec * 15 / 1000) * ba) % size : 0;
    return DS_OK;
}
HRESULT IDirectSoundBuffer::Lock(DWORD off, DWORD bytes, LPVOID *p1, LPDWORD n1, LPVOID *p2, LPDWORD n2, DWORD) {
    if (!size || off >= size || bytes > size) return DSERR_INVALIDPARAM;
    DWORD first = bytes < size - off ? bytes : size - off;
    *p1 = data + off;
    *n1 = first;
    if (p2) *p2 = bytes > first ? data : NULL;
    if (n2) *n2 = bytes - first;
    return DS_OK;
}
HRESULT IDirectSoundBuffer::Unlock(LPVOID, DWORD, LPVOID, DWORD) { return DS_OK; }
HRESULT IDirectSoundBuffer::Restore() { return DS_OK; }

ULONG IDirectSound::Release() { delete this; return 0; }
HRESULT IDirectSound::SetCooperativeLevel(HWND, DWORD) { return DS_OK; }
HRESULT IDirectSound::CreateSoundBuffer(LPCDSBUFFERDESC desc, IDirectSoundBuffer **out, LPUNKNOWN) {
    if (!(desc->dwFlags & DSBCAPS_PRIMARYBUFFER) && (!desc->lpwfxFormat || !desc->dwBufferBytes)) return DSERR_INVALIDPARAM;
    *out = new IDirectSoundBuffer(desc);
    return DS_OK;
}
HRESULT IDirectSound::GetCaps(LPDSCAPS caps) {
    if (!caps) return E_POINTER;
    memset(caps, 0, sizeof *caps);
    caps->dwMaxHwMixingStreamingBuffers = 64;
    return DS_OK;
}
HRESULT DirectSoundCreate(LPGUID, LPDIRECTSOUND *out, LPUNKNOWN) { *out = new IDirectSound(); return DS_OK; }

static inline float sample_at(IDirectSoundBuffer *b, long frame, int ch) {
    int ba = b->fmt.nBlockAlign;
    const BYTE *p = b->data + frame * ba;
    if (b->fmt.wBitsPerSample == 8) return ((int)p[ch] - 128) / 128.0f;
    int16_t v;
    memcpy(&v, p + ch * 2, 2);
    return v / 32768.0f;
}

// Mixes `frames` output frames. L/R may be NULL to only advance play cursors.
static void mix(float *L, float *R, int frames) {
    if (L) { memset(L, 0, frames * sizeof(float)); memset(R, 0, frames * sizeof(float)); }
    for (IDirectSoundBuffer *b = g_buffers; b; b = b->nextBuffer) {
        if (!b->playing || !b->fmt.nBlockAlign || !b->fmt.nSamplesPerSec) continue;
        long total = (long)(b->size / b->fmt.nBlockAlign);
        if (total <= 0) { b->playing = FALSE; continue; }
        double step = (double)b->fmt.nSamplesPerSec / g_outRate;
        float g = powf(10.0f, b->volume / 2000.0f);
        float gl = g * (b->pan > 0 ? powf(10.0f, -b->pan / 2000.0f) : 1.0f);
        float gr = g * (b->pan < 0 ? powf(10.0f, b->pan / 2000.0f) : 1.0f);
        bool stereo = b->fmt.nChannels >= 2;
        for (int i = 0; i < frames; i++) {
            if (L) {
                long f0 = (long)b->pos;
                double frac = b->pos - f0;
                long f1 = f0 + 1;
                if (f1 >= total) f1 = b->looping ? 0 : f0;
                float l0 = sample_at(b, f0, 0), l1 = sample_at(b, f1, 0);
                float l = l0 + (float)frac * (l1 - l0);
                float r = l;
                if (stereo) { float r0 = sample_at(b, f0, 1), r1 = sample_at(b, f1, 1); r = r0 + (float)frac * (r1 - r0); }
                L[i] += l * gl;
                R[i] += r * gr;
            }
            b->pos += step;
            if (b->pos >= total) {
                if (b->looping) b->pos -= total;
                else { b->playing = FALSE; b->pos = 0; break; }  // a finished one-shot buffer rewinds
            }
        }
    }
    if (L)
        for (int i = 0; i < frames; i++) {
            L[i] = L[i] > 1 ? 1 : (L[i] < -1 ? -1 : L[i]);
            R[i] = R[i] > 1 ? 1 : (R[i] < -1 ? -1 : R[i]);
        }
}
extern "C" EMSCRIPTEN_KEEPALIVE void d2_mix(float *L, float *R, int frames, int rate) {
    g_outRate = rate;
    g_audioRunning = true;
    mix(L, R, frames);
}

// ===========================================================================================
// DirectInput
// ===========================================================================================
static BYTE g_keys[256];
static BYTE g_keyPendingRelease[256];
static BYTE g_keySeen[256];
const DIDATAFORMAT c_dfDIKeyboard = {1}, c_dfDIJoystick = {2}, c_dfDIJoystick2 = {3}, c_dfDIMouse = {4};

extern "C" EMSCRIPTEN_KEEPALIVE void d2_key(int dik, int down, int vk) {
    if (dik > 0 && dik < 256) {
        if (down) { g_keys[dik] = 0x80; g_keyPendingRelease[dik] = 0; g_keySeen[dik] = 0; }
        else if (g_keySeen[dik]) g_keys[dik] = 0;
        else g_keyPendingRelease[dik] = 1;  // tap shorter than a poll: report it once before releasing
    }
    if (vk && g_wndproc) g_wndproc(g_hwnd, down ? WM_KEYDOWN : WM_KEYUP, (WPARAM)vk, 0);
}
static void clear_keys() {
    memset(g_keys, 0, sizeof g_keys);
    memset(g_keyPendingRelease, 0, sizeof g_keyPendingRelease);
}

EM_JS(int, js_pad_count, (void), {
    var pads = navigator.getGamepads ? navigator.getGamepads() : [];
    var n = 0;
    Module.d2Pads = [];
    for (var i = 0; i < pads.length; i++) if (pads[i]) { Module.d2Pads.push(pads[i].index); n++; }
    return n;
});
// out: [axes0..5, buttons0..31] as floats
EM_JS(int, js_pad_state, (int slot, float *out), {
    var pads = navigator.getGamepads ? navigator.getGamepads() : [];
    var idx = Module.d2Pads ? Module.d2Pads[slot] : -1;
    var p = idx !== undefined && idx >= 0 ? pads[idx] : null;
    if (!p) return 0;
    var a = p.axes, b = p.buttons, f = out >> 2;
    function bv(i) { return b[i] ? (typeof b[i] === 'object' ? b[i].value : b[i]) : 0; }
    // DirectInput layout for a standard pad: X/Y left stick, Rx/Ry right stick, Z = triggers.
    HEAPF32[f + 0] = a[0] || 0; HEAPF32[f + 1] = a[1] || 0;
    HEAPF32[f + 2] = bv(7) - bv(6);
    HEAPF32[f + 3] = a[2] || 0; HEAPF32[f + 4] = a[3] || 0; HEAPF32[f + 5] = 0;
    for (var i = 0; i < 32; i++) HEAPF32[f + 6 + i] = b[i] && (b[i].pressed || bv(i) > 0.5) ? 1 : 0;
    return 1;
});

EM_JS(int, js_debug_enabled, (void), { return /[?&]debug/.test(location.search) ? 1 : 0; });
extern "C" int d2_debug_status();

IDirectInputDevice7::IDirectInputDevice7(int k, int pad) : kind(k), padIndex(pad), rangeMin(0), rangeMax(65535), acquired(FALSE) {}
ULONG IDirectInputDevice7::Release() { delete this; return 0; }
HRESULT IDirectInputDevice7::SetDataFormat(LPCDIDATAFORMAT) { return DI_OK; }
HRESULT IDirectInputDevice7::SetCooperativeLevel(HWND, DWORD) { return DI_OK; }
HRESULT IDirectInputDevice7::SetProperty(const GUID *prop, LPCDIPROPHEADER hdr) {
    if (prop == DIPROP_RANGE) {
        const DIPROPRANGE *r = (const DIPROPRANGE *)hdr;
        rangeMin = r->lMin;
        rangeMax = r->lMax;
    }
    return DI_OK;
}
HRESULT IDirectInputDevice7::Acquire() { acquired = TRUE; return DI_OK; }
HRESULT IDirectInputDevice7::Unacquire() { acquired = FALSE; return DI_OK; }
HRESULT IDirectInputDevice7::Poll() { return DI_OK; }
HRESULT IDirectInputDevice7::GetDeviceState(DWORD size, LPVOID data) {
    if (kind == 0) {
        if (size < 256) return E_INVALIDARG;
        memcpy(data, g_keys, 256);
        if (js_debug_enabled())
            for (int i = 0; i < 256; i++)
                if (g_keys[i] && !g_keySeen[i]) emscripten_log(EM_LOG_CONSOLE, "[d2] poll sees DIK 0x%02x down (status %d)", i, d2_debug_status());
        for (int i = 0; i < 256; i++) {
            if (g_keys[i]) g_keySeen[i] = 1;
            if (g_keyPendingRelease[i]) { g_keys[i] = 0; g_keyPendingRelease[i] = 0; }
        }
        return DI_OK;
    }
    DIJOYSTATE *js = (DIJOYSTATE *)data;
    memset(js, 0, size);
    float st[38];
    if (!js_pad_state(padIndex, st)) return DIERR_INPUTLOST;
    LONG *axes[6] = {&js->lX, &js->lY, &js->lZ, &js->lRx, &js->lRy, &js->lRz};
    for (int i = 0; i < 6; i++) *axes[i] = (LONG)(rangeMin + (st[i] + 1.0f) * 0.5f * (rangeMax - rangeMin));
    js->rgdwPOV[0] = js->rgdwPOV[1] = js->rgdwPOV[2] = js->rgdwPOV[3] = 0xFFFFFFFF;
    for (int i = 0; i < 32; i++) js->rgbButtons[i] = st[6 + i] > 0 ? 0x80 : 0;
    return DI_OK;
}
HRESULT IDirectInputDevice7::GetCapabilities(LPDIDEVCAPS caps) {
    caps->dwAxes = kind ? 6 : 0;
    caps->dwButtons = kind ? 32 : 256;
    caps->dwPOVs = 0;
    return DI_OK;
}
HRESULT IDirectInputDevice7::EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACK cb, LPVOID ctx, DWORD) {
    if (kind != 1) return DI_OK;
    for (int i = 0; i < 6; i++) {
        DIDEVICEOBJECTINSTANCE oi;
        memset(&oi, 0, sizeof oi);
        oi.dwSize = sizeof oi;
        oi.dwOfs = i * 4;
        if (cb(&oi, ctx) == DIENUM_STOP) break;
    }
    return DI_OK;
}
ULONG IDirectInput7::Release() { delete this; return 0; }
HRESULT IDirectInput7::CreateDevice(REFGUID guid, LPDIRECTINPUTDEVICE *out, LPUNKNOWN) {
    *out = new IDirectInputDevice7(0, 0);
    return DI_OK;
}
HRESULT IDirectInput7::CreateDeviceEx(REFGUID guid, REFIID, LPVOID *out, LPUNKNOWN) {
    *out = new IDirectInputDevice7(1, (int)guid.Data1);
    return DI_OK;
}
HRESULT IDirectInput7::EnumDevices(DWORD type, LPDIENUMDEVICESCALLBACK cb, LPVOID ctx, DWORD) {
    if (type != DIDEVTYPE_JOYSTICK) return DI_OK;
    int n = js_pad_count();
    for (int i = 0; i < n; i++) {
        DIDEVICEINSTANCE di;
        memset(&di, 0, sizeof di);
        di.dwSize = sizeof di;
        di.guidInstance.Data1 = i;
        snprintf(di.tszProductName, MAX_PATH, "Gamepad %d", i + 1);
        if (cb(&di, ctx) == DIENUM_STOP) break;
    }
    return DI_OK;
}
HRESULT DirectInputCreateEx(HINSTANCE, DWORD, REFIID, LPVOID *out, LPUNKNOWN) { *out = new IDirectInput7(); return DI_OK; }

// ===========================================================================================
// ofstream helpers, focus, main loop
// ===========================================================================================
extern "C" EMSCRIPTEN_KEEPALIVE void d2_focus(int focused) {
    if (g_quit) return;
    if (focused) {
        g_hasFocus = true;
        if (g_wndproc) g_wndproc(g_hwnd, WM_SETFOCUS, 0, 0);
    } else {
        g_hasFocus = false;
        clear_keys();
        if (g_wndproc) g_wndproc(g_hwnd, WM_KILLFOCUS, 0, 0);
    }
}

EM_JS(double, js_frame_budget, (void), { return Module.d2FrameBudget || 10; });
EM_JS(void, js_on_exit, (void), { if (Module.onGameExit) Module.onGameExit(); });

// WM_DESTROY tears the whole game down. Quitting mid-round makes the original's ~Game() touch
// objects that were already released; on Windows the process was terminating at that point anyway
// (the message loop dereferences the deleted TheGame next). Treat a trap there as process exit.
extern "C" EMSCRIPTEN_KEEPALIVE void d2_dispatch_destroy() {
    if (g_wndproc) g_wndproc(g_hwnd, WM_DESTROY, 0, 0);
}
EM_JS(void, js_shutdown, (void), {
    try { _d2_dispatch_destroy(); }
    catch (e) { console.warn('Destruction II: error during shutdown (the original process exits here too):', e); }
    FS.syncfs(false, function (err) { if (err) console.error('Destruction II: saving failed', err); });
    if (Module.onGameExit) Module.onGameExit();
});

static double g_lastFrame = 0;
static void frame() {
    double now = emscripten_get_now();
    // Keep sound cursors moving even when Web Audio is not running (e.g. no output device).
    if (!g_audioRunning && g_lastFrame > 0) {
        int frames = (int)((now - g_lastFrame) * g_outRate / 1000.0);
        if (frames > 0) mix(NULL, NULL, frames);
    }
    g_lastFrame = now;

    std::vector<MSG> posted;
    posted.swap(g_posted);
    for (const MSG &m : posted) {
        if (m.message == WM_DESTROY) { g_quit = true; break; }
        DispatchMessage(&m);
    }
    if (g_quit) {
        emscripten_cancel_main_loop();
        js_shutdown();
        return;
    }

    // The original keeps running game logic while the flip waits for vertical blank. Give it a slice
    // of this browser frame for that, leaving the rest to the browser.
    g_flipDeadline = now + js_frame_budget();
    d2_app_frame();
    present();

    if (g_fsDirty && now - g_lastSync > 1000) {
        g_fsDirty = false;
        g_lastSync = now;
        js_sync_fs();
    }
}

static void ensure_dir(const char *p) { mkdir(p, 0777); }
static void copy_default(const char *rel) {
    std::string dst = std::string(kHomeRoot) + "/" + rel;
    struct stat st;
    if (stat(dst.c_str(), &st) == 0) return;
    std::string data;
    if (!d2io::read_file((std::string(kAssetRoot) + "/" + rel).c_str(), data)) return;
    FILE *f = fopen(dst.c_str(), "wb");
    if (f) { fwrite(data.data(), 1, data.size(), f); fclose(f); g_fsDirty = true; }
}

int main() {
    ensure_dir(kHomeRoot);
    ensure_dir((std::string(kHomeRoot) + "/Data").c_str());
    ensure_dir((std::string(kHomeRoot) + "/Players").c_str());
    ensure_dir((std::string(kHomeRoot) + "/Save").c_str());
    copy_default("Data/default.config");
    copy_default("Data/Keys.config");
    chdir(kHomeRoot);
    if (!d2_app_init((HINSTANCE)1, SW_SHOWNORMAL)) {
        js_on_exit();
        return 0;
    }
    emscripten_set_main_loop(frame, 0, false);
    return 0;
}
