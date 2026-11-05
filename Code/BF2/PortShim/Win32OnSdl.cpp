// [CHOSEN, not recovered] The Win32 and DirectInput subset the client calls, implemented on SDL3.
#include "windows.h"
#include "dinput.h"
#include "wincrypt.h"
#include <SDL3/SDL.h>
#include <dlfcn.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <string>

static WNDPROC g_wndProc = 0;
static SDL_Window* g_window = 0;
static std::deque<DIDEVICEOBJECTDATA> g_keyEvents, g_mouseEvents;
static DWORD g_sequence = 0;
static bool g_keyAcquired = false, g_mouseAcquired = false;
static std::deque<MSG> g_messages;

// The DIK code of an SDL scancode: DirectInput numbers keys by the PC set-1 scan code.
static int dikFromScancode(SDL_Scancode s) {
    if (s >= SDL_SCANCODE_1 && s <= SDL_SCANCODE_0) return 0x02 + (s - SDL_SCANCODE_1);
    if (s >= SDL_SCANCODE_F1 && s <= SDL_SCANCODE_F10) return 0x3B + (s - SDL_SCANCODE_F1);
    static const struct { SDL_Scancode s; int d; } t[] = {
        {SDL_SCANCODE_ESCAPE,1},{SDL_SCANCODE_MINUS,0x0C},{SDL_SCANCODE_EQUALS,0x0D},{SDL_SCANCODE_BACKSPACE,0x0E},{SDL_SCANCODE_TAB,0x0F},
        {SDL_SCANCODE_Q,0x10},{SDL_SCANCODE_W,0x11},{SDL_SCANCODE_E,0x12},{SDL_SCANCODE_R,0x13},{SDL_SCANCODE_T,0x14},{SDL_SCANCODE_Y,0x15},
        {SDL_SCANCODE_U,0x16},{SDL_SCANCODE_I,0x17},{SDL_SCANCODE_O,0x18},{SDL_SCANCODE_P,0x19},{SDL_SCANCODE_LEFTBRACKET,0x1A},{SDL_SCANCODE_RIGHTBRACKET,0x1B},
        {SDL_SCANCODE_RETURN,0x1C},{SDL_SCANCODE_LCTRL,0x1D},{SDL_SCANCODE_A,0x1E},{SDL_SCANCODE_S,0x1F},{SDL_SCANCODE_D,0x20},{SDL_SCANCODE_F,0x21},
        {SDL_SCANCODE_G,0x22},{SDL_SCANCODE_H,0x23},{SDL_SCANCODE_J,0x24},{SDL_SCANCODE_K,0x25},{SDL_SCANCODE_L,0x26},{SDL_SCANCODE_SEMICOLON,0x27},
        {SDL_SCANCODE_APOSTROPHE,0x28},{SDL_SCANCODE_GRAVE,0x29},{SDL_SCANCODE_LSHIFT,0x2A},{SDL_SCANCODE_BACKSLASH,0x2B},{SDL_SCANCODE_Z,0x2C},{SDL_SCANCODE_X,0x2D},
        {SDL_SCANCODE_C,0x2E},{SDL_SCANCODE_V,0x2F},{SDL_SCANCODE_B,0x30},{SDL_SCANCODE_N,0x31},{SDL_SCANCODE_M,0x32},{SDL_SCANCODE_COMMA,0x33},{SDL_SCANCODE_PERIOD,0x34},
        {SDL_SCANCODE_SLASH,0x35},{SDL_SCANCODE_RSHIFT,0x36},{SDL_SCANCODE_LALT,0x38},{SDL_SCANCODE_SPACE,0x39},{SDL_SCANCODE_CAPSLOCK,0x3A},
        {SDL_SCANCODE_F11,0x57},{SDL_SCANCODE_F12,0x58},{SDL_SCANCODE_RCTRL,0x9D},{SDL_SCANCODE_RALT,0xB8},{SDL_SCANCODE_HOME,0xC7},{SDL_SCANCODE_UP,0xC8},
        {SDL_SCANCODE_PAGEUP,0xC9},{SDL_SCANCODE_LEFT,0xCB},{SDL_SCANCODE_RIGHT,0xCD},{SDL_SCANCODE_END,0xCF},{SDL_SCANCODE_DOWN,0xD0},{SDL_SCANCODE_PAGEDOWN,0xD1},
        {SDL_SCANCODE_INSERT,0xD2},{SDL_SCANCODE_DELETE,0xD3}};
    for (const auto& e : t) if (e.s == s) return e.d;
    return 0;
}
static void pushInput(std::deque<DIDEVICEOBJECTDATA>& q, DWORD ofs, DWORD data) {
    DIDEVICEOBJECTDATA e{ofs, data, (DWORD)SDL_GetTicks(), ++g_sequence, 0};
    q.push_back(e);
    if (q.size() > 512) q.pop_front();
}
static void post(UINT m, WPARAM w, LPARAM l) { MSG msg{}; msg.hwnd = g_window; msg.message = m; msg.wParam = w; msg.lParam = l; g_messages.push_back(msg); }
static float g_pointerX = 0.0f, g_pointerY = 0.0f;
static bool g_pointerSeen = false;
static void pumpSdl() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_EVENT_QUIT: post(WM_QUIT, 0, 0); break;
            case SDL_EVENT_KEY_DOWN: case SDL_EVENT_KEY_UP: {
                if (e.key.repeat) break;
                const int dik = dikFromScancode(e.key.scancode);
                if (dik && g_keyAcquired) pushInput(g_keyEvents, dik, e.key.down ? 0x80 : 0);
                break;
            }
            case SDL_EVENT_TEXT_INPUT:
                for (const char* p = e.text.text; *p; ++p) post(WM_CHAR, (unsigned char)*p, 0);
                break;
            case SDL_EVENT_MOUSE_MOTION:
                g_pointerX = e.motion.x; g_pointerY = e.motion.y; g_pointerSeen = true;
                if (g_mouseAcquired) {
                    if (e.motion.xrel != 0) pushInput(g_mouseEvents, DIMOFS_X, (DWORD)(LONG)e.motion.xrel);
                    if (e.motion.yrel != 0) pushInput(g_mouseEvents, DIMOFS_Y, (DWORD)(LONG)e.motion.yrel);
                }
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                if (g_mouseAcquired) pushInput(g_mouseEvents, DIMOFS_Z, (DWORD)(LONG)(e.wheel.y * 120));
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN: case SDL_EVENT_MOUSE_BUTTON_UP: {
                static const int map[] = {0, 0, 2, 1, 3, 4};
                if (g_mouseAcquired && e.button.button <= 5 && e.button.button > 0)
                    pushInput(g_mouseEvents, DIMOFS_BUTTON0 + map[e.button.button], e.button.down ? 0x80 : 0);
                break;
            }
            case SDL_EVENT_WINDOW_FOCUS_GAINED: post(WM_ACTIVATE, WA_ACTIVE, 0); break;
            case SDL_EVENT_WINDOW_FOCUS_LOST: post(WM_ACTIVATE, 0, 0); break;
            default: break;
        }
    }
}

HMODULE GetModuleHandleA(LPCSTR) { return (HMODULE)&g_window; }
HMODULE LoadLibraryA(LPCSTR name) { return dlopen(name, RTLD_NOW); }
FARPROC GetProcAddress(HMODULE m, LPCSTR n) { return (FARPROC)dlsym(m, n); }
BOOL FreeLibrary(HMODULE m) { return m && dlclose(m) == 0; }
// [CHOSEN, not recovered] The latest pointer position across the window, each axis 0..1. Touch
// screens and test drivers report positions, which DirectInput's relative deltas cannot carry.
bool portablePointerPosition(float* x, float* y) {
    int width = 0, height = 0;
    if (!g_pointerSeen || !g_window || !SDL_GetWindowSize(g_window, &width, &height) || width <= 0 || height <= 0)
        return false;
    *x = g_pointerX / width;
    *y = g_pointerY / height;
    return true;
}
DWORD GetTickCount() { return (DWORD)SDL_GetTicks(); }
DWORD GetCurrentProcessId() { return (DWORD)getpid(); }
// Renderer TUs see the 32-bit MEMORYSTATUS from the D3D headers, so they ask for the size directly.
extern "C" unsigned int portableSystemRamMB() { return (unsigned int)SDL_GetSystemRAM(); }
void GlobalMemoryStatus(MEMORYSTATUS* s) { *s = MEMORYSTATUS(); s->dwTotalPhys = (size_t)SDL_GetSystemRAM() * 1048576u; s->dwAvailPhys = s->dwTotalPhys; }
WORD RegisterClassExA(const WNDCLASSEXA* c) { g_wndProc = c->lpfnWndProc; return 1; }
HWND CreateWindowExA(DWORD, LPCSTR, LPCSTR title, DWORD style, int, int, int w, int h, HWND, HMENU, HINSTANCE, LPVOID) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD)) { std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 0; }
    SDL_WindowFlags flags = SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (style & WS_POPUP) flags |= SDL_WINDOW_FULLSCREEN;
#if defined(__ANDROID__)
    flags |= SDL_WINDOW_FULLSCREEN;     // hides the system bars
#endif
    g_window = SDL_CreateWindow(title, w, h, flags);
    if (!g_window) std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
    else SDL_StartTextInput(g_window);
    return g_window;
}
LRESULT DefWindowProcA(HWND, UINT, WPARAM, LPARAM) { return 0; }
BOOL DestroyWindow(HWND w) { SDL_DestroyWindow((SDL_Window*)w); if (w == g_window) g_window = 0; return 1; }
BOOL ShowWindow(HWND w, int cmd) { return cmd == SW_HIDE ? SDL_HideWindow((SDL_Window*)w) : SDL_ShowWindow((SDL_Window*)w); }
BOOL UpdateWindow(HWND) { return 1; }
BOOL SetForegroundWindow(HWND w) { return SDL_RaiseWindow((SDL_Window*)w); }
HWND SetFocus(HWND w) { return w; }
HWND GetFocus() { return g_window; }
HWND GetForegroundWindow() { return g_window; }
void PostQuitMessage(int) { post(WM_QUIT, 0, 0); }
int GetSystemMetrics(int i) {
    const SDL_DisplayMode* m = SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay());
    if (i == SM_CXSCREEN) return m ? m->w : 1280;
    if (i == SM_CYSCREEN) return m ? m->h : 720;
    return 0;
}
BOOL SystemParametersInfoA(UINT, UINT, PVOID, UINT) { return 1; }
BOOL SetWindowPos(HWND w, HWND, int, int, int cx, int cy, UINT) { return SDL_SetWindowSize((SDL_Window*)w, cx, cy); }
LONG SetWindowLongA(HWND, int, LONG) { return 0; }
HICON LoadIconA(HINSTANCE, LPCSTR) { return 0; }
HCURSOR LoadCursorA(HINSTANCE, LPCSTR) { return 0; }
HCURSOR LoadCursorFromFileA(LPCSTR) { return 0; }
void* LoadImageA(HINSTANCE, LPCSTR, UINT, int, int, UINT) { return 0; }
HCURSOR SetCursor(HCURSOR) { return 0; }
int ShowCursor(BOOL show) { if (show) SDL_ShowCursor(); else SDL_HideCursor(); return show ? 0 : -1; }
HBRUSH CreateSolidBrush(DWORD) { return 0; }
BOOL PeekMessageA(MSG* m, HWND, UINT, UINT, UINT flags) {
    pumpSdl();
    if (g_messages.empty()) return 0;
    *m = g_messages.front();
    if (flags & PM_REMOVE) g_messages.pop_front();
    return 1;
}
BOOL TranslateMessage(const MSG*) { return 1; }
LRESULT DispatchMessageA(const MSG* m) { return g_wndProc ? g_wndProc(m->hwnd, m->message, m->wParam, m->lParam) : 0; }
short GetAsyncKeyState(int vk) {
    const SDL_Keymod mod = SDL_GetModState();
    if (vk == VK_CONTROL) return (mod & SDL_KMOD_CTRL) ? (short)0x8000 : 0;
    if (vk == VK_MENU) return (mod & SDL_KMOD_ALT) ? (short)0x8000 : 0;
    return 0;
}
BOOL GetKeyboardState(BYTE* out) { std::memset(out, 0, 256); return 1; }
UINT MapVirtualKeyExA(UINT code, UINT, HKL) { return code; }
int ToAsciiEx(UINT, UINT, const BYTE*, WORD*, UINT, HKL) { return 0; }
HKL GetKeyboardLayout(DWORD) { return 0; }
int MessageBoxA(HWND, LPCSTR text, LPCSTR title, UINT) { std::fprintf(stderr, "[%s] %s\n", title ? title : "", text ? text : ""); return 1; }
static std::string g_clipboard;
BOOL OpenClipboard(HWND) { return 1; }
BOOL CloseClipboard() { return 1; }
BOOL EmptyClipboard() { return 1; }
HANDLE SetClipboardData(UINT, HANDLE h) { SDL_SetClipboardText((const char*)h); return h; }
HANDLE GetClipboardData(UINT) { char* t = SDL_GetClipboardText(); g_clipboard = t ? t : ""; SDL_free(t); return (HANDLE)g_clipboard.c_str(); }
BOOL IsClipboardFormatAvailable(UINT) { return SDL_HasClipboardText(); }
HGLOBAL GlobalAlloc(UINT, size_t n) { return std::calloc(1, n); }
HGLOBAL GlobalFree(HGLOBAL h) { std::free(h); return 0; }
LPVOID GlobalLock(HGLOBAL h) { return h; }
BOOL GlobalUnlock(HGLOBAL) { return 1; }
LONG RegOpenKeyExA(HKEY, LPCSTR, DWORD, DWORD, HKEY*) { return 2; }
LONG RegCloseKey(HKEY) { return 0; }
LONG RegQueryValueExA(HKEY, LPCSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD) { return 2; }
BOOL CryptProtectData(DATA_BLOB*, const wchar_t*, DATA_BLOB*, void*, void*, DWORD, DATA_BLOB*) { return 0; }
BOOL CryptUnprotectData(DATA_BLOB*, wchar_t**, DATA_BLOB*, void*, void*, DWORD, DATA_BLOB*) { return 0; }

const GUID GUID_SysKeyboard{1}, GUID_SysMouse{2}, GUID_XAxis{3}, GUID_YAxis{4}, GUID_ZAxis{5}, GUID_RxAxis{6}, GUID_RyAxis{7}, GUID_RzAxis{8}, GUID_Slider{9}, IID_IDirectInput8A{10};
const int c_dfDIKeyboard = 1, c_dfDIMouse2 = 2, c_dfDIJoystick2 = 3;
HRESULT DirectInput8Create(HINSTANCE, DWORD, const GUID&, void** out, void*) { *out = new IDirectInput8A; return DI_OK; }
ULONG IDirectInput8A::Release() { delete this; return 0; }
HRESULT IDirectInput8A::CreateDevice(const GUID& g, IDirectInputDevice8A** out, void*) {
    IDirectInputDevice8A* d = new IDirectInputDevice8A;
    d->kind = (int)g.Data1;
    *out = d;
    return DI_OK;
}
HRESULT IDirectInput8A::EnumDevices(DWORD, LPDIENUMDEVICESCALLBACKA, LPVOID, DWORD) { return DI_OK; }
HRESULT IDirectInputDevice8A::Acquire() {
    (kind == 1 ? g_keyAcquired : g_mouseAcquired) = true;
    if (kind == 2 && g_window) SDL_SetWindowRelativeMouseMode(g_window, true);
    return DI_OK;
}
HRESULT IDirectInputDevice8A::Unacquire() {
    (kind == 1 ? g_keyAcquired : g_mouseAcquired) = false;
    if (kind == 2 && g_window) SDL_SetWindowRelativeMouseMode(g_window, false);
    return DI_OK;
}
ULONG IDirectInputDevice8A::Release() { delete this; return 0; }
HRESULT IDirectInputDevice8A::SetCooperativeLevel(HWND, DWORD) { return DI_OK; }
HRESULT IDirectInputDevice8A::SetDataFormat(const int*) { return DI_OK; }
HRESULT IDirectInputDevice8A::SetProperty(intptr_t, const DIPROPHEADER*) { return DI_OK; }
HRESULT IDirectInputDevice8A::GetProperty(intptr_t, DIPROPHEADER* h) {
    DIPROPSTRING* p = (DIPROPSTRING*)h;
    const char* n = SDL_GetScancodeName((SDL_Scancode)h->dwObj);
    size_t i = 0; for (; n && n[i] && i < 259; ++i) p->wsz[i] = (wchar_t)n[i];
    p->wsz[i] = 0;
    return DI_OK;
}
HRESULT IDirectInputDevice8A::GetDeviceData(DWORD, DIDEVICEOBJECTDATA* out, DWORD* count, DWORD) {
    pumpSdl();
    std::deque<DIDEVICEOBJECTDATA>& q = kind == 1 ? g_keyEvents : g_mouseEvents;
    DWORD n = 0;
    while (n < *count && !q.empty()) { out[n++] = q.front(); q.pop_front(); }
    *count = n;
    return DI_OK;
}
HRESULT IDirectInputDevice8A::GetCapabilities(DIDEVCAPS* c) { c->dwAxes = 3; c->dwButtons = 8; c->dwPOVs = 0; return DI_OK; }
HRESULT IDirectInputDevice8A::GetDeviceState(DWORD n, LPVOID out) { std::memset(out, 0, n); return DI_OK; }
HRESULT IDirectInputDevice8A::Poll() { return DI_OK; }
HRESULT IDirectInputDevice8A::EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACKA, LPVOID, DWORD) { return DI_OK; }

DWORD GetModuleFileNameA(HMODULE, LPSTR out, DWORD n) { if (n) out[0] = 0; return 0; }
HLOCAL LocalFree(HLOCAL h) { std::free(h); return 0; }
DWORD GetLastError() { return 1; }
DWORD FormatMessageA(DWORD, LPCVOID, DWORD, DWORD, LPSTR, DWORD, void*) { return 0; }
