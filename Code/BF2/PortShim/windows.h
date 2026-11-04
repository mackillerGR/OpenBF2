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
