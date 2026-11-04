#pragma once
// [CHOSEN, not recovered] The small part of the Win32 type surface the client's headers
// name. Window, input and registry behaviour live in the SDL platform layer on this port.
#include <cstdint>
#include <cstring>
typedef void* HWND; typedef void* HANDLE; typedef void* HINSTANCE; typedef void* HMODULE;
typedef void* HICON; typedef void* HCURSOR; typedef void* HBRUSH; typedef void* HDC;
typedef void* LPVOID; typedef const void* LPCVOID; typedef void* PVOID;
typedef char* LPSTR; typedef const char* LPCSTR; typedef const wchar_t* LPCWSTR;
typedef uint32_t DWORD; typedef uint16_t WORD; typedef uint8_t BYTE; typedef int32_t BOOL;
typedef uint32_t UINT; typedef int32_t LONG; typedef uint32_t ULONG; typedef int64_t LONGLONG;
typedef uintptr_t WPARAM; typedef intptr_t LPARAM; typedef intptr_t LRESULT;
typedef DWORD* LPDWORD; typedef BYTE* LPBYTE; typedef long HRESULT;
typedef union { struct { DWORD LowPart; LONG HighPart; }; LONGLONG QuadPart; } LARGE_INTEGER;
typedef struct { LONG x, y; } POINT;
typedef struct { LONG left, top, right, bottom; } RECT;
typedef struct { DWORD dwLowDateTime, dwHighDateTime; } FILETIME;
typedef struct { WORD wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds; } SYSTEMTIME;
typedef struct { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; DWORD time; POINT pt; } MSG;
typedef struct { DWORD Data1; WORD Data2, Data3; BYTE Data4[8]; } GUID;
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
#define WINAPI
#define CALLBACK
#define APIENTRY
#define MAX_PATH 260
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr) (((HRESULT)(hr)) < 0)
#define S_OK ((HRESULT)0)
#define LOWORD(l) ((WORD)((uintptr_t)(l) & 0xffff))
#define HIWORD(l) ((WORD)(((uintptr_t)(l) >> 16) & 0xffff))

// --- Messages, window constants and the Win32-on-SDL surface (Win32OnSdl.cpp).
typedef void* HKEY; typedef void* HKL; typedef void* HGLOBAL; typedef void* HMENU;
typedef void* HGDIOBJ; typedef intptr_t (*FARPROC)();
typedef LRESULT (*WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef struct { UINT cbSize, style; WNDPROC lpfnWndProc; int cbClsExtra, cbWndExtra; HINSTANCE hInstance;
    HICON hIcon; HCURSOR hCursor; HBRUSH hbrBackground; LPCSTR lpszMenuName, lpszClassName; HICON hIconSm; } WNDCLASSEXA;
typedef struct { DWORD dwLength, dwMemoryLoad; size_t dwTotalPhys, dwAvailPhys, dwTotalPageFile,
    dwAvailPageFile, dwTotalVirtual, dwAvailVirtual; } MEMORYSTATUS;
enum { WM_DESTROY=0x2, WM_SIZE=0x5, WM_ACTIVATE=0x6, WM_SETFOCUS=0x7, WM_KILLFOCUS=0x8, WM_CLOSE=0x10,
    WM_QUIT=0x12, WM_ACTIVATEAPP=0x1C, WM_SETCURSOR=0x20, WM_NCHITTEST=0x84, WM_KEYDOWN=0x100, WM_KEYUP=0x101,
    WM_CHAR=0x102, WM_SYSKEYDOWN=0x104, WM_COMMAND=0x111, WM_SYSCOMMAND=0x112, WM_MOUSEMOVE=0x200 };
enum { WA_ACTIVE=1, HTCLIENT=1, SIZE_MINIMIZED=1, SIZE_MAXHIDE=4, SC_SIZE=0xF000, SC_MOVE=0xF010,
    SC_MAXIMIZE=0xF030, SC_SCREENSAVE=0xF140, SC_MONITORPOWER=0xF170 };
enum { VK_CONTROL=0x11, VK_MENU=0x12, VK_F10=0x79, SW_HIDE=0, SW_SHOW=5, SW_MINIMIZE=6, SM_CXSCREEN=0,
    SM_CYSCREEN=1, SM_CXFRAME=32, SM_CYFRAME=33, SM_CYCAPTION=4, SWP_NOMOVE=2, GWL_STYLE=-16, GWL_EXSTYLE=-20,
    SPIF_UPDATEINIFILE=1, SPIF_SENDCHANGE=2, CS_DBLCLKS=8, IMAGE_CURSOR=2, LR_LOADFROMFILE=0x10, LR_SHARED=0x8000,
    CF_TEXT=1, CF_OEMTEXT=7, MAPVK_VSC_TO_VK=1, ERROR_SUCCESS=0, KEY_QUERY_VALUE=1, PM_NOREMOVE=0, PM_REMOVE=1, PM_NOYIELD=2 };
#define WS_POPUP 0x80000000u
#define WS_VISIBLE 0x10000000u
#define WS_OVERLAPPEDWINDOW 0x00CF0000u
#define WS_EX_TOPMOST 0x8u
#define IDC_ARROW ((LPCSTR)(intptr_t)0x7F00)
#define MAKEINTRESOURCEA(i) ((LPCSTR)(intptr_t)(i))
#define HKEY_LOCAL_MACHINE ((HKEY)(intptr_t)0x80000002)
#define GHND 0x42
#define GMEM_MOVEABLE 2

HMODULE GetModuleHandleA(LPCSTR); HMODULE LoadLibraryA(LPCSTR); FARPROC GetProcAddress(HMODULE, LPCSTR); BOOL FreeLibrary(HMODULE);
DWORD GetTickCount(); DWORD GetCurrentProcessId(); void GlobalMemoryStatus(MEMORYSTATUS*);
HWND CreateWindowExA(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
WORD RegisterClassExA(const WNDCLASSEXA*); LRESULT DefWindowProcA(HWND, UINT, WPARAM, LPARAM); BOOL DestroyWindow(HWND);
BOOL ShowWindow(HWND, int); BOOL UpdateWindow(HWND); BOOL SetForegroundWindow(HWND); HWND SetFocus(HWND); HWND GetFocus();
HWND GetForegroundWindow(); void PostQuitMessage(int); int GetSystemMetrics(int); BOOL SystemParametersInfoA(UINT, UINT, PVOID, UINT);
BOOL SetWindowPos(HWND, HWND, int, int, int, int, UINT); LONG SetWindowLongA(HWND, int, LONG);
HICON LoadIconA(HINSTANCE, LPCSTR); HCURSOR LoadCursorA(HINSTANCE, LPCSTR); HCURSOR LoadCursorFromFileA(LPCSTR);
void* LoadImageA(HINSTANCE, LPCSTR, UINT, int, int, UINT); HCURSOR SetCursor(HCURSOR); int ShowCursor(BOOL); HBRUSH CreateSolidBrush(DWORD);
BOOL PeekMessageA(MSG*, HWND, UINT, UINT, UINT); BOOL TranslateMessage(const MSG*); LRESULT DispatchMessageA(const MSG*);
short GetAsyncKeyState(int); BOOL GetKeyboardState(BYTE*); UINT MapVirtualKeyExA(UINT, UINT, HKL); int ToAsciiEx(UINT, UINT, const BYTE*, WORD*, UINT, HKL); HKL GetKeyboardLayout(DWORD);
int MessageBoxA(HWND, LPCSTR, LPCSTR, UINT);
BOOL OpenClipboard(HWND); BOOL CloseClipboard(); BOOL EmptyClipboard(); HANDLE SetClipboardData(UINT, HANDLE); HANDLE GetClipboardData(UINT); BOOL IsClipboardFormatAvailable(UINT);
HGLOBAL GlobalAlloc(UINT, size_t); HGLOBAL GlobalFree(HGLOBAL); LPVOID GlobalLock(HGLOBAL); BOOL GlobalUnlock(HGLOBAL);
LONG RegOpenKeyExA(HKEY, LPCSTR, DWORD, DWORD, HKEY*); LONG RegCloseKey(HKEY); LONG RegQueryValueExA(HKEY, LPCSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
typedef struct { WORD wVersion, wHighVersion; } WSADATA;
inline int WSAStartup(WORD, WSADATA*) { return 0; }
inline int WSACleanup() { return 0; }

typedef intptr_t INT_PTR; typedef wchar_t* LPWSTR; typedef void* HLOCAL;
enum { SW_SHOWNORMAL = 1, FORMAT_MESSAGE_ALLOCATE_BUFFER = 0x100, FORMAT_MESSAGE_FROM_SYSTEM = 0x1000,
    FORMAT_MESSAGE_IGNORE_INSERTS = 0x200, LANG_NEUTRAL = 0, SUBLANG_DEFAULT = 1 };
#define MAKELANGID(p, s) ((((WORD)(s)) << 10) | (WORD)(p))
DWORD GetModuleFileNameA(HMODULE, LPSTR, DWORD); HLOCAL LocalFree(HLOCAL); DWORD GetLastError();
DWORD FormatMessageA(DWORD, LPCVOID, DWORD, DWORD, LPSTR, DWORD, void*);
