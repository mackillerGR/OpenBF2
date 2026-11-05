// [CHOSEN, not recovered] The three Win32/CRT imports the menu player keeps on portable hosts.
// [PLATFORM] Win32 builds resolve these from USER32 and the CRT and do not compile this file.

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_keycode.h>

#include <ctime>

// The menu's busy cursor has no meaning on a touch screen, and the portable host draws no OS cursor.
extern "C" void* SetCursor(void*)
{
    return nullptr;
}

// Only the Caps Lock toggle (virtual key 20) is ever queried; the low bit is the toggle state.
extern "C" short GetKeyState(int virtualKey)
{
    return (virtualKey == 20 && (SDL_GetModState() & SDL_KMOD_CAPS)) ? 1 : 0;
}

extern "C" long long _time64(long long* out)
{
    const long long now = static_cast<long long>(std::time(nullptr));
    if (out)
        *out = now;
    return now;
}
