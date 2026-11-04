#pragma once
// [CHOSEN, not recovered] The CPU benchmark's counters on any architecture. CPUID reports nothing.
#include <SDL3/SDL_timer.h>
#include <string.h>
inline void cpuidPortable(int* regs, int) { memset(regs, 0, 16); }
#undef __cpuid
#define __cpuid cpuidPortable
inline unsigned long long rdtscPortable() { return SDL_GetPerformanceCounter(); }
#define __rdtsc rdtscPortable
