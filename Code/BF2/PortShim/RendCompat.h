#pragma once
// [CHOSEN, not recovered] Win32 surface the renderer plugin uses, on top of DXVK's windows.h.
#include <windows.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <SDL3/SDL_timer.h>
typedef pthread_mutex_t CRITICAL_SECTION;
inline void InitializeCriticalSection(CRITICAL_SECTION* c) { pthread_mutexattr_t a; pthread_mutexattr_init(&a); pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE); pthread_mutex_init(c, &a); }
inline void EnterCriticalSection(CRITICAL_SECTION* c) { pthread_mutex_lock(c); }
inline void LeaveCriticalSection(CRITICAL_SECTION* c) { pthread_mutex_unlock(c); }
inline void DeleteCriticalSection(CRITICAL_SECTION* c) { pthread_mutex_destroy(c); }
typedef struct { WORD wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds; } SYSTEMTIME;
inline void GetLocalTime(SYSTEMTIME* t) { time_t n = time(0); struct tm m; localtime_r(&n, &m); *t = SYSTEMTIME{(WORD)(m.tm_year + 1900), (WORD)(m.tm_mon + 1), (WORD)m.tm_wday, (WORD)m.tm_mday, (WORD)m.tm_hour, (WORD)m.tm_min, (WORD)m.tm_sec, 0}; }
inline DWORD GetCurrentThreadId() { return (DWORD)syscall(SYS_gettid); }
inline DWORD GetEnvironmentVariableA(const char* n, char* b, DWORD s) { const char* v = getenv(n); if (!v) return 0; snprintf(b, s, "%s", v); return (DWORD)strlen(v); }
inline void OutputDebugStringA(const char* s) { fputs(s, stderr); }
inline BOOL QueryPerformanceCounter(LARGE_INTEGER* v) { v->QuadPart = (LONGLONG)SDL_GetPerformanceCounter(); return 1; }
inline BOOL QueryPerformanceFrequency(LARGE_INTEGER* v) { v->QuadPart = (LONGLONG)SDL_GetPerformanceFrequency(); return 1; }
inline HMODULE LoadLibraryA(const char* n) { return (HMODULE)dlopen(n, RTLD_NOW); }
inline void* GetProcAddress(HMODULE m, const char* n) { return dlsym((void*)m, n); }
typedef struct { DWORD dmSize; DWORD dmPelsWidth, dmPelsHeight, dmBitsPerPel, dmDisplayFrequency, dmDisplayFlags; } DEVMODEA;
inline BOOL EnumDisplaySettingsA(const char*, DWORD, DEVMODEA*) { return 0; }
typedef struct { DWORD cbSize; RECT rcWindow, rcClient; DWORD dwStyle, dwExStyle; } WINDOWINFO;
inline BOOL GetWindowInfo(HWND, WINDOWINFO* w) { memset(w, 0, sizeof(*w)); return 1; }
#include <stdarg.h>
inline int fopen_s(FILE** f, const char* n, const char* m) { *f = fopen(n, m); return *f ? 0 : 1; }
inline int _vsnprintf_s(char* b, size_t n, size_t, const char* f, va_list a) { return vsnprintf(b, n, f, a); }
inline BOOL IsDebuggerPresent() { return 0; }
inline void DebugBreak() { abort(); }
enum { SM_CXSIZEFRAME = 32, SM_CYSIZEFRAME = 33, SM_CYCAPTION = 4, SWP_NOMOVE = 2, SM_CXSCREEN = 0, SM_CYSCREEN = 1 };
int GetSystemMetrics(int);
BOOL SetWindowPos(HWND, HWND, int, int, int, int, UINT);
typedef uintptr_t DWORD_PTR;
inline HANDLE GetCurrentProcess() { return (HANDLE)(intptr_t)-1; }
inline BOOL GetProcessAffinityMask(HANDLE, DWORD_PTR* a, DWORD_PTR* b) { *a = *b = 1; return 1; }
inline BOOL SetProcessAffinityMask(HANDLE, DWORD_PTR) { return 1; }
inline void Sleep(DWORD ms) { usleep(ms * 1000); }
void GlobalMemoryStatus(MEMORYSTATUS*);
inline BOOL InflateRect(RECT* r, int x, int y) { r->left -= x; r->right += x; r->top -= y; r->bottom += y; return 1; }
#ifndef _TRUNCATE
#define _TRUNCATE ((size_t)-1)
#endif
inline int strncpy_s(char* d, size_t n, const char* s, size_t c) { if (!n) return 22; size_t l = strnlen(s, c); if (l >= n) l = n - 1; memcpy(d, s, l); d[l] = 0; return 0; }
template <size_t N> inline int strncpy_s(char (&d)[N], const char* s, size_t c) { return strncpy_s(d, N, s, c); }
inline DWORD GetModuleFileNameA(HMODULE, char* b, DWORD n) { if (n) b[0] = 0; return 0; }
inline DWORD GetCurrentDirectoryA(DWORD n, char* b) { return getcwd(b, n) ? (DWORD)strlen(b) : 0; }
#include <sys/stat.h>
inline BOOL CreateDirectoryA(const char* p, void*) { return mkdir(p, 0755) == 0; }
