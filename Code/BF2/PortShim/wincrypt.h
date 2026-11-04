#pragma once
// [CHOSEN, not recovered] No machine-bound DPAPI here: protected values fail to store.
#include "windows.h"
typedef struct _CRYPTOAPI_BLOB { DWORD cbData; BYTE* pbData; } DATA_BLOB;
#define CRYPTPROTECT_UI_FORBIDDEN 1
BOOL CryptProtectData(DATA_BLOB*, const wchar_t*, DATA_BLOB*, void*, void*, DWORD, DATA_BLOB*);
BOOL CryptUnprotectData(DATA_BLOB*, wchar_t**, DATA_BLOB*, void*, void*, DWORD, DATA_BLOB*);
