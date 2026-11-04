#pragma once
// [CHOSEN, not recovered] The DirectInput 8 surface the client uses, backed by SDL so the
// recovered buffered keyboard and mouse sampling stays unchanged. Game controllers enumerate as none.
#include "windows.h"
#include <cstddef>
typedef struct { DWORD dwOfs, dwData, dwTimeStamp, dwSequence; uintptr_t uAppData; } DIDEVICEOBJECTDATA;
typedef struct { DWORD dwSize, dwHeaderSize, dwObj, dwHow; } DIPROPHEADER;
typedef struct { DIPROPHEADER diph; DWORD dwData; } DIPROPDWORD;
typedef struct { DIPROPHEADER diph; LONG lMin, lMax; } DIPROPRANGE;
typedef struct { DIPROPHEADER diph; wchar_t wsz[260]; } DIPROPSTRING;
struct DIDEVCAPS { DWORD dwSize, dwFlags, dwDevType, dwAxes, dwButtons, dwPOVs; };
typedef struct { GUID guidInstance, guidProduct; char tszProductName[260]; } DIDEVICEINSTANCEA;
typedef struct { GUID guidType; DWORD dwOfs, dwType; char tszName[260]; } DIDEVICEOBJECTINSTANCEA;
typedef struct { LONG lX, lY, lZ, lRx, lRy, lRz, rglSlider[2]; DWORD rgdwPOV[4]; BYTE rgbButtons[128]; LONG rest[44]; } DIJOYSTATE2;
typedef const DIDEVICEINSTANCEA* LPCDIDEVICEINSTANCEA;
typedef const DIDEVICEOBJECTINSTANCEA* LPCDIDEVICEOBJECTINSTANCEA;
typedef BOOL (*LPDIENUMDEVICESCALLBACKA)(LPCDIDEVICEINSTANCEA, LPVOID);
typedef BOOL (*LPDIENUMDEVICEOBJECTSCALLBACKA)(LPCDIDEVICEOBJECTINSTANCEA, LPVOID);
enum { DI_OK = 0, DI_BUFFEROVERFLOW = 1, DIENUM_STOP = 0, DIENUM_CONTINUE = 1, DIPH_DEVICE = 0, DIPH_BYOFFSET = 1, DIPH_BYID = 2,
    DISCL_EXCLUSIVE = 1, DISCL_NONEXCLUSIVE = 2, DISCL_FOREGROUND = 4, DISCL_BACKGROUND = 8, DI8DEVCLASS_GAMECTRL = 4,
    DIEDFL_ATTACHEDONLY = 1, DIDFT_AXIS = 3, DIMOFS_X = 0, DIMOFS_Y = 4, DIMOFS_Z = 8, DIMOFS_BUTTON0 = 12, DIMOFS_BUTTON1, DIMOFS_BUTTON2,
    DIMOFS_BUTTON3, DIMOFS_BUTTON4, DIMOFS_BUTTON5, DIMOFS_BUTTON6, DIMOFS_BUTTON7 };
#define DIERR_INPUTLOST ((HRESULT)0x8007001EL)
#define DIERR_INVALIDPARAM ((HRESULT)0x80070057L)
#define DIERR_OUTOFMEMORY ((HRESULT)0x8007000EL)
#define DIERR_BETADIRECTINPUTVERSION ((HRESULT)0x80070481L)
#define DIERR_OLDDIRECTINPUTVERSION ((HRESULT)0x8007047EL)
#define DIJOFS_BUTTON(n) (offsetof(DIJOYSTATE2, rgbButtons) + (n))
#define DIJOFS_POV(n) (offsetof(DIJOYSTATE2, rgdwPOV) + (n) * 4)
#define DIPROP_BUFFERSIZE 1
#define DIPROP_RANGE 4
#define DIPROP_KEYNAME 20
extern const GUID GUID_SysKeyboard, GUID_SysMouse, GUID_XAxis, GUID_YAxis, GUID_ZAxis, GUID_RxAxis, GUID_RyAxis, GUID_RzAxis, GUID_Slider, IID_IDirectInput8A;
extern const int c_dfDIKeyboard, c_dfDIMouse2, c_dfDIJoystick2;
struct IDirectInputDevice8A {
    int kind = 0;
    HRESULT Acquire(); HRESULT Unacquire(); ULONG Release();
    HRESULT SetCooperativeLevel(HWND, DWORD); HRESULT SetDataFormat(const int*);
    HRESULT SetProperty(intptr_t, const DIPROPHEADER*); HRESULT GetProperty(intptr_t, DIPROPHEADER*);
    HRESULT GetDeviceData(DWORD, DIDEVICEOBJECTDATA*, DWORD*, DWORD); HRESULT GetCapabilities(DIDEVCAPS*);
    HRESULT GetDeviceState(DWORD, LPVOID); HRESULT Poll(); HRESULT EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACKA, LPVOID, DWORD);
};
struct IDirectInput8A {
    ULONG Release(); HRESULT CreateDevice(const GUID&, IDirectInputDevice8A**, void*);
    HRESULT EnumDevices(DWORD, LPDIENUMDEVICESCALLBACKA, LPVOID, DWORD);
};
HRESULT DirectInput8Create(HINSTANCE, DWORD, const GUID&, void**, void*);
